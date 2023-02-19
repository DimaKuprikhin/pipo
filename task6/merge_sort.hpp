#include <algorithm>
#include <thread>
#include <vector>
#include <future>
#include <optional>

#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>

// Тип вычислителя.
enum struct ProcessorType {
  Thread,
  Process
};

namespace {

// Вспомогательные функции для проверки результата выполнения системных
// вызовов. Две перегрузки для функций, которые возвращают int и void*.
int CheckResult(int return_value, const std::string& operation) {
  if (return_value < 0) {
    throw std::runtime_error(operation + ": " + strerror(errno));
  }
  return return_value;
}

void* CheckResult(void* return_value, const std::string& operation) {
  if (return_value < (void*)0) {
    throw std::runtime_error(operation + ": " + strerror(errno));
  }
  return return_value;
}

// Астрактный класс вычислителя, который выполняет одну задачу.
class Processor {
public:
  // Функция, блокирующая выполнение до момента завершения вычислителя.
  virtual void Join() = 0;
};

// Класс вычислителя, создающий новый поток для выполнения задачи.
class ThreadProcessor : public Processor {
public:
  ThreadProcessor(std::function<void()> task) : thread(std::thread(task)) {}
  void Join() override { thread.join(); };
private:
  std::thread thread;
};

// Класс вычислителя, создающий новый процесс для выполнения задачи.
class ProcessProcessor : public Processor {
public:
  ProcessProcessor(std::function<void()> task) {
    pid = CheckResult(fork(), "fork");
    if (pid == 0) {
      // Выполняем задачу и завершаем выполнение дочернего процесса.
      task();
      exit(0);
    }
  }

  void Join() override {
    int status;
    // Ждем, пока процесс с нужным идентификатором не завершится.
    while (CheckResult(waitpid(pid, &status, 0), "waitpid") != pid) {}
  }

private:
  int pid;
};

// Параметры для внутренней функции `merge_sort_impl()`.
struct Params {
  size_t processors_count = 1;
  ProcessorType processor_type;
  // Если processor_type == ProcessorType::Process, shmid должно быть > 0.
  int shmid = -1;
};

// Функция, выполняющая слияние двух частей массива.
template <typename T, typename Compare>
void merge(T* first, T* middle, T* last, Compare comp) {
  std::vector<T> tmp;
  tmp.reserve(last - first);

  T* left_iter = first;
  T* right_iter = middle;
  while (left_iter < middle && right_iter < last) {
    if (comp(*left_iter, *right_iter)) {
      tmp.push_back(std::move(*left_iter));
      ++left_iter;
    } else {
      tmp.push_back(std::move(*right_iter));
      ++right_iter;
    }
  }
  while (left_iter < middle) {
    tmp.push_back(std::move(*left_iter));
    ++left_iter;
  }
  while (right_iter < last) {
    tmp.push_back(std::move(*right_iter));
    ++right_iter;
  }

  T* iter = first;
  for (auto& entry : tmp) {
    *iter = std::move(entry);
    ++iter;
  }
}

// Функция, вызывающая `merge_sort_impl()` в новом вычислителе.
template<typename T, typename Compare>
std::unique_ptr<Processor> merge_sort_async(T* data, size_t first, size_t last,
                                            Params params, Compare comp) {
  if (params.processor_type == ProcessorType::Thread) {
    // Запускаем функцию `merge_sort_impl()` с переданными параметрами.
    std::function<void()> task = [=]() {
        merge_sort_impl(data, first, last, params, comp);
    };
    return std::make_unique<ThreadProcessor>(ThreadProcessor(task));
  }
  if (params.processor_type == ProcessorType::Process) {
    // В новом процессе нужно приаттачить сегмент разделяемой памяти и передать
    // функции `merge_sort_impl()` новый указатель.
    std::function<void()> task = [=]() {
        void* shm_data = CheckResult(shmat(params.shmid, NULL, 0), "shmat");
        merge_sort_impl((T*)shm_data, first, last, params, comp);
        shmdt(data);
    };
    return std::make_unique<ProcessProcessor>(ProcessProcessor(task));
  }
  // Unreachable.
  return {};
}

// Функция, реализующая сортировку слиянием с использованием нескольких
// вычислителей. При каждом выполнении функции, сортируемый диапазон массива
// делится на две равный части и сортируется отдельно. При этом, если есть
// свободные вычислители, левая часть массива сортируется в новом вычислителе,
// а правая - в текущем.
template <typename T, typename Compare>
void merge_sort_impl(T* data, size_t first, size_t last, Params params,
                     Compare comp) {
  if (last - first <= 1) {
    return;
  }
  size_t middle = first + (last - first) / 2;
  std::unique_ptr<Processor> processor;

  // Если есть свободные вычислители, то сортируем левую часть асинхронно в
  // новом потоке/процессе.
  if (params.processors_count < 2) {
    merge_sort_impl(data, first, middle, params, comp);
  }
  else {
    // Передаем в новый вычислитель половину количества свободных вычислителей.
    Params new_params{params.processors_count / 2, params.processor_type, params.shmid};
    params.processors_count -= params.processors_count / 2;
    processor = merge_sort_async(data, first, middle, new_params, comp);
  }

  // Правую часть всегда сортируем в этом же потоке/процессе.
  merge_sort_impl(data, middle, last, params, comp);

  // Если левая часть сортировалась не в этом потоке/процессе, ожидаем
  // завершения выполнения асинхронной задачи.
  if (processor) {
    processor->Join();
  }
  // Сливаем обе части массива.
  merge(data + first, data + middle, data + last, comp);
}

}  // namespace

// Сортировка слиянием массива с использованием нескольких вычислителей.
// `first` и `last` - указатели на первый и последний (невключительно) элементы
// сортируемого массива типа T.
// `comp` - компаратор для типа T.
// `processors_count` - количество вычислителей (включая текущий
// поток/процесс), которые могут быть использованы для сортировки.
// `processor_type` - тип вычислителей, который будут использоваться для
// сортировки.
template <typename T, typename Compare = std::less<T>>
void merge_sort(T* first, T* last, Compare comp = Compare{},
                size_t processors_count = 1,
                ProcessorType processor_type = ProcessorType::Thread) {
  Params params{processors_count, processor_type};
  // Если при сортировки будут создаваться новые процессы, необходимо создать
  // сегмент разделяемой памяти для коммуникации между процессами.
  if (processor_type == ProcessorType::Process && processors_count > 1) {
    const size_t size = sizeof(T) * (last - first);
    params.shmid = CheckResult(shmget(IPC_PRIVATE, size, IPC_CREAT | 0660), "shmget");
    void* addr = CheckResult(shmat(params.shmid, NULL, 0), "shmat");

    // Копируем массив в разделяемую память.
    memcpy(addr, (void*)first, size);
    // Запускаем сортировку.
    merge_sort_impl((T*)addr, 0, last - first, params, comp);
    // Копируем данные из разделяемой памяти обратно в массив.
    memcpy((void*)first, addr, size);

    // Удаляем сегмент.
    shmctl(params.shmid, IPC_RMID, 0);
  }
  else {
    // Иначе, запускаем сортировку без каких-либо дополнительных действий.
    merge_sort_impl(first, 0, last - first, params, comp);
  }
}
