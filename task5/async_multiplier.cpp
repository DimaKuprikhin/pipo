#include "async_multiplier.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

#include <string.h>
#include <sys/wait.h>

namespace {

// Функция, вычисляющая произведение чисел от `from` до `to`.
uint64_t Multiply(uint64_t from, uint64_t to) {
  if (from > to) {
    throw std::runtime_error("`to` should be equal or greater than `from`");
  }
  uint64_t result = from;
  for (uint64_t i = from + 1; i <= to; ++i) {
    result *= i;
  }
  return result;
}
    
// Реализация асинхронного вычислителя произведения, использующая потоки.
// Для межпотокового взаимодействия используются мьютекс для предотвращения
// гонок и условная переманная для ожидания нужных состояний.
class ThreadAcyncMultiplier : public AsyncMultiplier {
public:
  ThreadAcyncMultiplier() : thread(&ThreadAcyncMultiplier::Run, this) {}

  void SetTask(uint64_t from, uint64_t to) override {
    // Захватываем мьютекс, задаем параметры для вычисления и оповещаем поток
    // вычислителя о появлении задачи.
    std::unique_lock lock(mutex);
    this->from = from;
    this->to = to;
    lock.unlock();
    cv.notify_one();
  }

  uint64_t GetResult() override {
    std::unique_lock lock(mutex);
    // Ожидаем на условной переменной, пока в поле `result` не появится
    // ненулевое значение.
    while (result == 0) {
      // Если поля `from` и `result` равны 0, у вычислителя нет результата
      // какого-либо вычисления и нет выполняющейся задачи. В этом случае
      // возвращаем 0.
      if (from == 0) {
        return 0;
      }
      cv.wait(lock);
    }
    uint64_t result_ = result;
    result = 0;
    lock.unlock();
    return result_;
  }

  void Finish() override {
    std::unique_lock lock(mutex);
    // Переключаем значение поля `should_finish` и оповещаем поток вычислителя.
    should_finish = true;
    lock.unlock();
    cv.notify_one();
    // Ждем завершения выполнения потока.
    thread.join();
  }

private:
  void Run() {
    // Функция, выполняющаяся вычислителем. Ожидаем задачи в бесконечном цикле,
    // пока не получим сигнал о завершении.
    while (true) {
      std::unique_lock lock(mutex);
      // Ждем на условной переменной появления задачи.
      while (from == 0) {
        cv.wait(lock);
        // Если был вызван метод `Finish()`, завершаем выполнение.
        if (should_finish) {
          return;
        }
      }
      // Вычисляем результат и оповещаем родительский поток.
      result = Multiply(from, to);
      from = to = 0;
      lock.unlock();
      cv.notify_one();
    }
  }

  // Поля `from` и `to` хранят данные о задаче на вычисление произведения.
  // Если `from` равно 0, у вычислителя в данный момент нет никакой
  // незавершенной задачи.
  uint64_t from = 0;
  uint64_t to = 0;
  // Поле `result` хранит результат вычисления. Если `result` равно 0, значит
  // вычислитель еще не получал ни одной задачи или результат уже был получен
  // вызовом метода `GetResult()`. 
  uint64_t result = 0;
  std::thread thread;
  std::mutex mutex;
  std::condition_variable cv;
  bool should_finish = false;
};

// Реализация асинхронного вычислителя произведения, использующая процессы.
// Для межпроцессорного взаимодействия используются пары pipe'ов.
class ProcessAsyncMultiplier : public AsyncMultiplier {
public:
  ProcessAsyncMultiplier() {
    // Создаем два pipe'а - один для передачи данных вычислителю, другой - от
    // вычислителя.
    CreatePipe(to_child_read_end, to_child_write_end);
    CreatePipe(to_parent_read_end, to_parent_write_end);

    // Создаем процесс.
    pid = fork();
    if (pid == -1) {
      throw std::runtime_error(strerror(errno));
    }
    if (pid == 0) {
      // В дочернем процессе закрываем неиспользуемые файловые дескрипторы
      // и запускаем выполнение главного метода.
      close(to_child_write_end);
      close(to_parent_read_end);
      Task();
      // Закрываем оставшиеся дескрипторы и завершаем выполнение.
      close(to_child_read_end);
      close(to_parent_write_end);
      exit(0);
    }
    // В главном процессе также закрываем неиспользуемые дескрипторы.
    close(to_child_read_end);
    close(to_parent_write_end);
  }

  void SetTask(uint64_t from, uint64_t to) override {
    // Отправляем в pipe 3 значения - флаг о завершении выполнения и два числа.
    bool should_finish = false;
    write(to_child_write_end, &should_finish, sizeof(bool));
    write(to_child_write_end, &from, sizeof(uint64_t));
    write(to_child_write_end, &to, sizeof(uint64_t));
    expects_result = true;
  }

  uint64_t GetResult() override {
    if (!expects_result) {
      return 0;
    }
    // Блокируемся на чтении из pipe результата.
    uint64_t result;
    read(to_parent_read_end, &result, sizeof(uint64_t));
    expects_result = false;
    return result;
  }

  void Finish() override {
    // Пишем в pipe флаг о завершении выполнения процесса и закрываем созданные
    // дескрипторы.
    bool should_finish = true;
    write(to_child_write_end, &should_finish, sizeof(bool));
    close(to_child_write_end);
    close(to_parent_read_end);
    // Ждем завершения дочернего процесса.
    int status;
    while (waitpid(pid, &status, 0) != pid) {}
  }

private:
  void CreatePipe(int& read_end_fd, int& write_end_fd) {
    int pipefds[2];
    if (pipe(pipefds) == -1) {
      throw std::runtime_error(strerror(errno));
    }
    read_end_fd = pipefds[0];
    write_end_fd = pipefds[1];
  }

  void Task() {
    // В бесконечном цикле получаем значение флага о завершении выполнения
    // и параметры задачи.
    while (true) {
      bool should_finish = false;
      read(to_child_read_end, &should_finish, sizeof(bool));
      if (should_finish) {
        return;
      }

      uint64_t from, to;
      read(to_child_read_end, &from, sizeof(uint64_t));
      read(to_child_read_end, &to, sizeof(uint64_t));

      uint64_t result = Multiply(from, to);
      // Отправляем в главный процесс результат вычисления.
      write(to_parent_write_end, &result, sizeof(uint64_t));
    }
  }

  int to_child_read_end;
  int to_child_write_end;
  int to_parent_read_end;
  int to_parent_write_end;
  int pid;
  // Поле `expects_result` равно true, если вычислитель в данный момент
  // выполняет задачу или есть неполученный результат вычисления.
  bool expects_result = false;
};

}

std::vector<std::unique_ptr<AsyncMultiplier>> CreateMultipliers(int count, bool is_threads) {
  std::vector<std::unique_ptr<AsyncMultiplier>> result;
  for (int i = 0; i < count; ++i) {
    if (is_threads) {
        result.push_back(std::make_unique<ThreadAcyncMultiplier>());
    }
    else {
        result.push_back(std::make_unique<ProcessAsyncMultiplier>());
    }
  }
  return result;
}
