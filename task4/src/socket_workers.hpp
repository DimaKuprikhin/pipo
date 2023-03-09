#pragma once

#include <string>
#include <vector>

#include <sys/socket.h>

#include "utils.hpp"
#include "workers.hpp"

// Класс разделяемого состояния для мойщика и вытирателя, использующих
// локальные сокеты для своей коммуникации.
class SocketSharedState : public SharedState {
public:
  SocketSharedState(int table_limit) {
    // Создаем две пары сокетов - первая используется для передачи данные о
    // вымытой посуде, вторая для передачи данных о количестве свободного
    // места на столе.
    CheckResult(socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fds.data()), "socketpair");
    CheckResult(socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fds.data() + 2), "socketpair");
    // Записываем количество свободного места.
    std::string remaining_space(table_limit, 0);
    CheckResult(write(RemainingSpaceSocketWriteEnd(), remaining_space.data(), table_limit), "write");
  }

  int DishesSocketReadEnd() { return socket_fds[0]; }
  int DishesSocketWriteEnd() { return socket_fds[1]; }
  int RemainingSpaceSocketReadEnd() { return socket_fds[2]; }
  int RemainingSpaceSocketWriteEnd() { return socket_fds[3]; }

private:
  std::vector<int> socket_fds{4, 0};
};

class SocketWasher : public Washer {
public:
  SocketWasher(const Times& washing_times, std::shared_ptr<SocketSharedState> shared_state)
    : Washer(washing_times), shared_state(shared_state) {}

  ~SocketWasher() override {
    // Закрываем оставшиеся конце сокетов. Делаем это в деструкторе, а не в
    // методе `AfterWork()`, так как вытиратель может работать дольше, чем
    // мойщик.
    CheckResult(close(shared_state->DishesSocketWriteEnd()), "close");
    CheckResult(close(shared_state->RemainingSpaceSocketReadEnd()), "close");
  }

private:
  void BeforeWork() override {
    // Закрываем неиспользуемые концы сокетов.
    CheckResult(close(shared_state->DishesSocketReadEnd()), "close");
    CheckResult(close(shared_state->RemainingSpaceSocketWriteEnd()), "close");
  }

  void AfterWork() override {}

  void PutDish(const std::string& dish_type, bool is_last) override {
    // Ждем, пока появится свободное место на столе.
    bool byte;
    CheckResult(read(shared_state->RemainingSpaceSocketReadEnd(), &byte, sizeof(bool)), "read");
    // Записываем данные о посуде.
    CheckResult(write(shared_state->DishesSocketWriteEnd(), &is_last, sizeof(bool)), "write");
    int size = dish_type.size();
    CheckResult(write(shared_state->DishesSocketWriteEnd(), &size, sizeof(int)), "write");
    CheckResult(write(shared_state->DishesSocketWriteEnd(), dish_type.data(), size), "write");
  }

private:
  std::shared_ptr<SocketSharedState> shared_state;
};

class SocketWiper : public Wiper {
public:
  SocketWiper(const Times& wiping_times, std::shared_ptr<SocketSharedState> shared_state)
    : Wiper(wiping_times), shared_state(shared_state) {}

private:
  void BeforeWork() override {
    // Закрываем неиспользуемые концы сокетов.
    CheckResult(close(shared_state->DishesSocketWriteEnd()), "close");
    CheckResult(close(shared_state->RemainingSpaceSocketReadEnd()), "close");
  }

  void AfterWork() override {
    // Закрываем оставшиеся конце сокетов.
    CheckResult(close(shared_state->DishesSocketReadEnd()), "close");
    CheckResult(close(shared_state->RemainingSpaceSocketWriteEnd()), "close");
  }

  bool IsWorkDone() override {
    return took_last;
  }

  std::string TakeDish() override {
    // Доавляем свободное место на столе.
    bool byte;
    CheckResult(write(shared_state->RemainingSpaceSocketWriteEnd(), &byte, sizeof(bool)), "write");
    // Читаем данные о посуде.
    CheckResult(read(shared_state->DishesSocketReadEnd(), &took_last, sizeof(bool)), "read");
    int size;
    CheckResult(read(shared_state->DishesSocketReadEnd(), &size, sizeof(int)), "read");
    std::string dish_type(size, 0);
    CheckResult(read(shared_state->DishesSocketReadEnd(), dish_type.data(), size), "read");
    return dish_type;
  }

private:
  std::shared_ptr<SocketSharedState> shared_state;
  bool took_last = false;
};
