#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include "utils.hpp"

// Класс, использующийся для инициализации и освобождения ресурсов, общих для
// двух работников. Каждый работник из пары хранит `shared_ptr` на объект
// класса, что гарантирует освобождение выделенных ресурсов только в момент
// разрушения последнего из работников, пользующихся этими русурсами.
struct SharedState {
  virtual ~SharedState() = default;
};

// Базовый класс работника мойки. Предоставляет дочерним классам метод
// `Sleep()` для приостановки выполнения процесса (используется для симуляции
// какой-то работы (мытье, вытирание)) и средства для логгирования.
class Worker {
public:
  Worker(const std::string& name) : name(name) {}
  virtual ~Worker() = default;

protected:
  // Метод, позволяющий процессу заснуть на `secs` секунд.
  void Sleep(int secs) {
    while (true) {
      int seconds_passed = CheckResult(sleep(secs), "sleep");
      if (seconds_passed == 0) {
        return;
      }
      else {
        std::cerr << "Sleep didn't return 0" << std::endl;
        secs -= seconds_passed;
      }
    }
  }

  // Класс логгера, использующийся дочерними классами для логирования своих
  // действий.
  struct Logger {
    Logger(const std::string& value) {
      std::cout << value;
    }
    template<typename T>
    Logger& operator<<(const T& value) {
      std::cout << value;
      return *this;
    }
    ~Logger() { std::cout << std::endl; }
  };

  // Метод, выводящий имя работника и текущее время и возвращающий класс логгера.
  Logger Log() {
    return Logger(name + " " + std::to_string(GetSecond()) + " sec: ");
  }

private:
  // Возвращает количество секунд с момента создания объекта класса.
  int GetSecond() {
    auto now = std::chrono::steady_clock::now();
    int ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    return (ms + 100) / 1000;
  }

  // Имя(вид) работника, использующееся для логгирования.
  std::string name;
  // Время создания объекта для логгирования.
  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

// Абстрактный класс мойщика. Имеет абстрактный метод `Work()`, симулирующий
// мытье посуды согласно очереди задач. Выполнение метода `Work()`
// подразумевается в том же процессе, из которого этот метод был запущен.
// Метод `Work()` вызывает в себе несколько других методов, представляющих
// различные стадии работы, которые должны быть переопределены в наследниках
// класса и реализовывать логику работы с каким-то конкретным средством
// синхронизации процессов.
class Washer : public Worker {
public:
  Washer(const Times& washing_times) : Worker("WASHER"), washing_times(washing_times) {}

  void Work(WashTaskQueue queue) {
    BeforeWork();
    while (!queue.empty()) {
      WashTask task = queue.front();
      queue.pop();
      while (task.count > 0) {
        // Моем посуду.
        Wash(task.dish_type);
        --task.count;
        // Ставим вымытую посуду на стол (если стол полон, ждем, пока Wiper
        // заберет одну).
        Log() << "Trying to put " << task.dish_type << " on the table";
        PutDish(task.dish_type, queue.empty() && task.count == 0);
        Log() << "Put " << task.dish_type << " on the table";
      }
    }
    Log() << "Finished work";
    AfterWork();
  }

protected:
  // Выполняет необходимые действия до выполнения работы.
  virtual void BeforeWork() = 0;
  // Выполняет действия для передачи данных о вымытой посуде.
  virtual void PutDish(const std::string& dish_type, bool is_last) = 0;
  // Выполняет необходимые действия после выполнения работы.
  virtual void AfterWork() = 0;

  // Метод, симулирующий мытье посуды типа `dish_type`.
  void Wash(const std::string& dish_type) {
    const int washing_time = washing_times.at(dish_type);
    Log() << "Wash " << dish_type << " for " << washing_time << " seconds";
    Sleep(washing_time);
  }

private:
  // Маппинг из типа посуды в количество времени, требующееся для мытья этого
  // типа посуды.
  Times washing_times;
};

// Абстрактный класс вытирателя. Имеет абстрактный метод `Work()`, симулирующий
// вытирание посуды, лежащей на столе. Метод `Work()` создает новый процесс,
// в котором выполнятся работа по вытиранию посуды. Метод `Work()` вызывает в
// себе несколько других методов, представляющих различные стадии работы,
// которые должны быть переопределены в наследниках класса и реализовывать
// логику работы с каким-то конкретным средством синхронизации процессов.
class Wiper : public Worker {
public:
  Wiper(const Times& wiping_times) : Worker("WIPER "), wiping_times(wiping_times) {}

  void Work() {
    pid = CheckResult(fork(), "fork");
    if (pid == 0) {
      BeforeWork();
      while (!IsWorkDone()) {
        // Ждем, пока на столе будет хотя бы одна посуда, и забираем ее.
        Log() << "Trying to get dish from the table";
        std::string dish_type = TakeDish();
        Log() << "Got " << dish_type << " from the table";
        Wipe(dish_type);
      }
      Log() << "Finished work";
      AfterWork();
      exit(0);
    }
  }

  // Ожидает завершения выполнения процесса.
  void Join() {
    while (waitpid(pid, NULL, 0) != pid) {}
  }

protected:
  // Выполняет необходимые действия до начала работы.
  virtual void BeforeWork() = 0;
  // Возвращает true, если не ожидается появления новой посуды от мойщика.
  virtual bool IsWorkDone() = 0;
  // Выполняет действия для получения данных об очередной вымытой поусде.
  virtual std::string TakeDish() = 0;
  // Выполняет необходимые действия после выполнения работы.
  virtual void AfterWork() = 0;

  // Метод, симулирующий вытирание посуды типа `dish_type`.
  void Wipe(const std::string& dish_type) {
    const int wiping_time = wiping_times.at(dish_type);
    Log() << "Wipe " << dish_type << " for " << wiping_time << " seconds";
    Sleep(wiping_time);
  }

private:
  // Маппинг из типа посуды в количество времени, требующееся для вытирания
  // этого типа посуды.
  Times wiping_times;
  // Идентификатор созданного процесса.
  int pid;
};
