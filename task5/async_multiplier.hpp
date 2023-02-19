#include <memory>
#include <vector>

// Абстрактный класс асинхронного вычислителя произведения чисел от `from` до
// `to`.
class AsyncMultiplier {
public:
  // Вызов метода запускает асинхронное вычисление произедения.
  virtual void SetTask(uint64_t from, uint64_t to) = 0;
  // Функция возвращает результат вычисления, запущенного вызовом метода
  // `SetTask()`. Если асинхронное вычисление еще не завершилось, выполнение
  // блокируется до момент появления результата.
  // Если метод вызывается до какого-либо вызова `SetTask()`, результатом будет
  // 0. Результат вычисления произведения можно получить лишь единожды, все
  // последующие вызовы метода будут возвращать 0.
  virtual uint64_t GetResult() = 0;
  // Завершает выполнение вычислителя и блокирует выполнение до момента
  // завершения выполнения вычислителя.
  virtual void Finish() = 0;
};

// Функция, возвращающая массив асинхронных вычислителей. Если `is_threads`
// истинно, вернется массив вычислителей, использующих потоки. Иначе,
// вычислители будут использовать процессы.
std::vector<std::unique_ptr<AsyncMultiplier>> CreateMultipliers(int count, bool is_threads);
