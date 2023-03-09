#pragma once

#include <string>

#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <unistd.h>

#include "utils.hpp"
#include "workers.hpp"

// Класс разделяемого состояния для мойщика и вытирателя, использующих FIFO
// для своей коммуникации.
class FifoSharedState : public SharedState {
public:
  FifoSharedState(int table_limit) : SharedState() {
    // Создаем FIFO.
    CheckResult(mkfifo(fifo_path.c_str(), O_RDWR | 0660), "mkfifo");

    // Создаем множество из 2 семафоров: значение первого будет отражать
    // количество вымытой (но не вытертой) посуды на столе, значение второго
    // будет отражать количетсво свободного места для посуды на столе.
    // Washer будет ставить вымытую посуду на стол, пока значение второго
    // семафора больше 0. Wiper будет забирать посуду со стола, если значение
    // первого семафора больше 0.
    union semun {
      int              val;    /* Value for SETVAL */
      struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
      unsigned short  *array;  /* Array for GETALL, SETALL */
      struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                  (Linux-specific) */
    };
    key_t key = CheckResult(ftok(".", 0), "ftok");
    sem_id = CheckResult(semget(key, 2, IPC_CREAT | IPC_EXCL | 0660), "semget");
    union semun s;
    s.val = 0;
    semctl(sem_id, 0, SETVAL, s);
    s.val = table_limit;
    semctl(sem_id, 1, SETVAL, s);
  }

  int SemId() { return sem_id; }

  ~FifoSharedState() override {
    // Удаляем FIFO и множество семафоров.
    unlink(fifo_path.c_str());
    semctl(sem_id, 0, IPC_RMID);
  }

  // Путь к созданному FIFO.
  const std::string fifo_path = "dish_washing_fifo";

private:
  // Идентификатор множества семафоров, который будут использоваться для
  // коммуникации между мойщиком и вытирателем.
  int sem_id;
};

// Класс мойщика, использующего в своей реализации FIFO и межпроцессные
// семафоры.
class FifoWasher : public Washer {
public:
  FifoWasher(const Times& washing_times, std::shared_ptr<FifoSharedState> shared_state)
    : Washer(washing_times), shared_state(shared_state) {}

private:
  void BeforeWork() override {
    fifo_fd = CheckResult(open(shared_state->fifo_path.c_str(), O_WRONLY), "open");
  }

  void PutDish(const std::string& dish_type, bool is_last) override {
    // Ждем, пока на столе будет свободное место.
    static sembuf sops[] = {{.sem_num = 0, .sem_op = 1}, {.sem_num = 1, .sem_op = -1}};
    CheckResult(semop(shared_state->SemId(), sops, 2), "semop");
    // Записываем сообщение в FIFO по определенному формату: сначала записывается
    // логическое значение, указывающее на то, является ли данное сообщение
    // последним в FIFO. Затем записывается число, равное длине строки типа
    // посуды. После этого, записывается строка типа посуды.
    CheckResult(write(fifo_fd, &is_last, sizeof(bool)), "write");
    int size = dish_type.size();
    CheckResult(write(fifo_fd, &size, sizeof(int)), "write");
    CheckResult(write(fifo_fd, dish_type.data(), size), "write");
  }

  void AfterWork() override {
    CheckResult(close(fifo_fd), "close");
  }

  std::shared_ptr<FifoSharedState> shared_state;
  // Файловый дескриптор FIFO.
  int fifo_fd;
};

// Класс вытирателя, использующего в своей реализации FIFO и межпроцессные
// семафоры.
class FifoWiper : public Wiper {
public:
  FifoWiper(const Times& wiping_times, std::shared_ptr<FifoSharedState> shared_state)
    : Wiper(wiping_times), shared_state(shared_state) {}

private:
  void BeforeWork() override {
    fifo_fd = CheckResult(open(shared_state->fifo_path.c_str(), O_RDONLY), "open fifo in wiper");
  }

  bool IsWorkDone() override {
    return took_last;
  }

  std::string TakeDish() override {
    // Уменьшаем значение первого семафора и увеличиваем значение второго.
    static sembuf sops[] = {{.sem_num = 0, .sem_op = -1}, {.sem_num = 1, .sem_op = 1}};
    CheckResult(semop(shared_state->SemId(), sops, 2), "semop");
    // Читаем данные из FIFO.
    CheckResult(read(fifo_fd, &took_last, sizeof(bool)), "read");
    int size;
    CheckResult(read(fifo_fd, &size, sizeof(int)), "read");
    std::string dish_type(size, 0);
    CheckResult(read(fifo_fd, dish_type.data(), size), "read");
    return dish_type;
  }

  void AfterWork() override {
    CheckResult(close(fifo_fd), "close");
  }

  std::shared_ptr<FifoSharedState> shared_state;
  // Файловый дескриптор FIFO.
  int fifo_fd;
  // Взял ли вытиратель последную посуду со стола.
  bool took_last = false;
};
