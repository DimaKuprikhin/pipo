#pragma once

#include "utils.hpp"
#include "workers.hpp"

// Класс разделяемого состояния для мойщика и вытирателя, использующих pipe'ы
// для своей коммуникации.
class PipeSharedState : public SharedState {
public:
  PipeSharedState(int table_limit) {
    // Создаем два pipe: первый будет передавать данные о посуде, второй будет
    // передавать данные о свободном месте на столе (количество непрочитанные
    // байтов в pipe = количество свободного места).
    pipe_fds.resize(4);
    CheckResult(pipe(pipe_fds.data()), "pipe");
    CheckResult(pipe(pipe_fds.data() + 2), "pipe");
    // Заполняем второй pipe текущим количеством свободных мест.
    std::vector<char> table_spare_space(table_limit, 0);
    CheckResult(write(RemainingSpaceWriteEnd(), table_spare_space.data(), table_limit), "write");
  }

  int DishesPipeReadEnd() { return pipe_fds[0]; }
  int DishesPipeWriteEnd() { return pipe_fds[1]; }
  int RemainingSpaceReadEnd() { return pipe_fds[2]; }
  int RemainingSpaceWriteEnd() { return pipe_fds[3]; }

private:
  std::vector<int> pipe_fds;
};

// Класс мойщика, использующего в своей реализации pipe'ы.
class PipeWasher : public Washer {
public:
  PipeWasher(const Times& washing_times, std::shared_ptr<PipeSharedState> shared_state)
    : Washer(washing_times), shared_state(shared_state) {}

  ~PipeWasher() override {
    // Закрываем оставшиеся концы pipe. Делаем это в деструкторе, а не в
    // `AfterWork()`, так как вытиратель может работать дольше, чем мойщик,
    // а мойщик будет разрушен только тогда, когда оба работника завершат
    // работу.
    CheckResult(close(shared_state->DishesPipeWriteEnd()), "close");
    CheckResult(close(shared_state->RemainingSpaceReadEnd()), "close");
  }

private:
  void BeforeWork() override {
    // Закрываем неиспользуемые концы pipe.
    CheckResult(close(shared_state->DishesPipeReadEnd()), "close");
    CheckResult(close(shared_state->RemainingSpaceWriteEnd()), "close");
  }

  void PutDish(const std::string& dish_type, bool is_last) override {
    // Ждем, пока на столе появится свободное место.
    bool byte;
    CheckResult(read(shared_state->RemainingSpaceReadEnd(), &byte, sizeof(bool)), "read");
    // Записываем сообщение в pipe по определенному формату: сначала записывается
    // логическое значение, указывающее на то, является ли данное сообщение
    // последним в pipe. Затем записывается число, равное длине строки типа
    // посуды. После этого, записывается строка типа посуды.
    CheckResult(write(shared_state->DishesPipeWriteEnd(), &is_last, sizeof(bool)), "write");
    int size = dish_type.size();
    CheckResult(write(shared_state->DishesPipeWriteEnd(), &size, sizeof(int)), "write");
    CheckResult(write(shared_state->DishesPipeWriteEnd(), dish_type.data(), size), "write");
  }

  void AfterWork() override {}

private:
  std::shared_ptr<PipeSharedState> shared_state;
};

// Класс вытирателя, использующего в своей реализации pipe'ы.
class PipeWiper : public Wiper {
public:
  PipeWiper(const Times& wiping_times, std::shared_ptr<PipeSharedState> shared_state)
    : Wiper(wiping_times), shared_state(shared_state) {}

private:
  void BeforeWork() override {
    // Закрываем неиспользуемые концы pipe.
    CheckResult(close(shared_state->DishesPipeWriteEnd()), "close");
    CheckResult(close(shared_state->RemainingSpaceReadEnd()), "close");
  }

  bool IsWorkDone() override {
    return took_last;
  }

  std::string TakeDish() override {
    // Ждем, пока на столе окажется вымытая посуда, и читаем данные о ней.
    CheckResult(read(shared_state->DishesPipeReadEnd(), &took_last, sizeof(bool)), "read");
    int size;
    CheckResult(read(shared_state->DishesPipeReadEnd(), &size, sizeof(int)), "read");
    std::string dish_type(size, 0);
    CheckResult(read(shared_state->DishesPipeReadEnd(), dish_type.data(), size), "read");
    // Добавляем одно свободное место на стол.
    bool byte;
    CheckResult(write(shared_state->RemainingSpaceWriteEnd(), &byte, sizeof(bool)), "write");
    return dish_type;
  }

  void AfterWork() override {
    // Закрываем оставшиеся концы pipe'ов. Можем это сделать в методе
    // `AfterWork()`, так как к этому моменту мойщик гарантированно закончил
    // свою работу.
    CheckResult(close(shared_state->DishesPipeReadEnd()), "close");
    CheckResult(close(shared_state->RemainingSpaceWriteEnd()), "close");
  }

private:
  std::shared_ptr<PipeSharedState> shared_state;
  bool took_last = false;
};
