#pragma once

#include <semaphore.h>
#include <sys/shm.h>

#include "utils.hpp"
#include "workers.hpp"

// Класс разделяемого состояния для мойщика и вытирателя, использующих
// разделяемую память для своей коммуникации. В разделяемой памяти инициируется
// два семафора для синхронизации процессов, а также несолько вспомогательных
// полей для хранения состояния очереди записей о посуде.
// Этот класс инкапсулирует всю логику работы с разделяемой памятью от классов
// работников.
class ShmSharedState : public SharedState {
public:
  ShmSharedState(int table_limit) : table_limit(table_limit) {
    // Инициализируем участок разделяемой памяти.
    key_t key = CheckResult(ftok(".", 0), "ftok");
    // `dishes_size` - количество записей о посуды, поставленной на стол,
    // которое мы можем хранить в разделяемой памяти. Это значение на 1 больше,
    // чем `TABLE_LIMIT`, так как `Washer` может записать данные о посуде, даже
    // если стол заполнен, чтобы избежать гонки процессов.
    dishes_size = table_limit + 1;
    // Размер выделяемой памяти - метаданные + массив записей о посуде.
    int shm_size = sizeof(ShmMetadata) + dishes_size * sizeof(DishType);
    shm_id = CheckResult(shmget(key, shm_size, IPC_CREAT | IPC_EXCL | 0660), "shmget");
    shm_addr = (char*)CheckResult(shmat(shm_id, NULL, 0), "shmat");
    shm_metadata = (ShmMetadata*)shm_addr;
    // Инициализируем поля метаданных.
    CheckResult(sem_init(&shm_metadata->dishes_sem, 1, 0), "sem_init");
    CheckResult(sem_init(&shm_metadata->remaining_space_sem, 1, table_limit), "sem_init");
    shm_metadata->dishes_head = -1;
    shm_metadata->dishes_tail = 0;
    shm_metadata->has_last_dish = false;
    dishes = (DishType*)(shm_addr + sizeof(ShmMetadata));
  }

  ~ShmSharedState() override {
    sem_destroy(&shm_metadata->dishes_sem);
    sem_destroy(&shm_metadata->remaining_space_sem);
    shmdt(shm_addr);
    shmctl(shm_id, IPC_RMID, NULL);
  }

  // Записывает данные о вымытой посуде в свободный слот разделяемой памяти
  // согласно положению головы очереди. Блокирует выполнение, если на столе
  // нет свободного места.
  void PutDish(const std::string& dish_type, bool is_last) {
    // Ждем, пока на столе не будет свободного места.
    CheckResult(sem_post(&shm_metadata->dishes_sem), "sem_post");
    CheckResult(sem_wait(&shm_metadata->remaining_space_sem), "sem_wait");
    // Записываем данные в разделяемую память.
    // Записанной посуды в разделяемую память может стать больше (на 1), чем
    // размер стола `TABLE_LIMIT` (в методе `TakeDish()` увеличили значение
    // семафора, указывающего на количество свободного места, но не успели
    // прочитать данные, а в этом методе уже записываем новые), но мы это учли
    // при инициализации разделяемой памяти.
    shm_metadata->has_last_dish = is_last;
    shm_metadata->dishes_head = (shm_metadata->dishes_head + 1) % dishes_size;
    dishes[shm_metadata->dishes_head].size = dish_type.size();
    strcpy(dishes[shm_metadata->dishes_head].dish_type, dish_type.c_str());
  }

  // Читает данные о вымытой посуде из разделяемой памяти согласно положению
  // хвоста очереди. Блокирует выполнение, если в данный момент нет нужных
  // данных.
  std::string TakeDish() {
    // Ждем, пока появятся данные.
    CheckResult(sem_post(&shm_metadata->remaining_space_sem), "sem_post");
    CheckResult(sem_wait(&shm_metadata->dishes_sem), "sem_wait");
    // Получаем данные.
    int size = dishes[shm_metadata->dishes_tail].size;
    if (size == 0) {
      // Should never happen.
      throw std::runtime_error("Dish is not ready");
    }
    std::string dish_type(size, 0);
    memcpy(dish_type.data(), dishes[shm_metadata->dishes_tail].dish_type, size);
    dishes[shm_metadata->dishes_tail].size = 0;
    shm_metadata->dishes_tail = (shm_metadata->dishes_tail + 1) % dishes_size;
    return dish_type;
  }

  bool IsAllDishesTaken() {
    return shm_metadata->has_last_dish && dishes[shm_metadata->dishes_tail].size == 0;
  }

private:
  // Метаданные для синхронизации процессов мойщика и вытирателя.
  struct ShmMetadata {
    // Семафор, значение которого равно количеству вымытой посуды на столе.
    // Используется вытирателем для ожидания появление вымытой посуды.
    sem_t dishes_sem;
    // Семафор, значение которого равно количеству свободного места на столе
    // для посуды. Используется мойщиком для ожидания появления свободного
    // места на столе.
    sem_t remaining_space_sem;
    // Индекс записи о посуде, являющейся головой очереди. По этому индексу
    // хранится запись, добавленная последним вызовом `PutDish()`.
    int dishes_head;
    // Индекс записи о посуде, являющейся хвостом очереди. По этому индексу
    // хранится запись, которая будет извлечена следующим вызовом `TakeDish()`.
    int dishes_tail;
    // Флаг, хранящий сигнал об окончании потока посуды.
    bool has_last_dish;
  };

  // Структура записи о посуде.
  struct DishType {
    int size;
    char dish_type[256];
  };

  int table_limit;
  int dishes_size;
  int shm_id;
  char* shm_addr;
  ShmMetadata* shm_metadata;
  DishType* dishes;
};

// Класс мойщика, использующего в своей реализации разделяемую память.
class ShmWasher : public Washer {
public:
  ShmWasher(const Times& washing_times, std::shared_ptr<ShmSharedState> shared_state)
    : Washer(washing_times), shared_state(shared_state) {}

private:
  void BeforeWork() override {}

  void AfterWork() override {}

  void PutDish(const std::string& dish_type, bool is_last) override {
    shared_state->PutDish(dish_type, is_last);
  }

private:
  std::shared_ptr<ShmSharedState> shared_state;
};

// Класс вытирателя, использующего в своей реализации разделяемую память.
class ShmWiper : public Wiper {
public:
  ShmWiper(const Times& wiping_times, std::shared_ptr<ShmSharedState> shared_state)
    : Wiper(wiping_times), shared_state(shared_state) {}

private:
  void BeforeWork() override {}

  void AfterWork() override {}

  bool IsWorkDone() override {
    return shared_state->IsAllDishesTaken();
  }

  std::string TakeDish() override {
    return shared_state->TakeDish();
  }

private:
  std::shared_ptr<ShmSharedState> shared_state;
};
