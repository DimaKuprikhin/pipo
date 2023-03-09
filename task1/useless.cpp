#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <vector>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace logging_details {
bool enabled = true;
}

class Log {
public:
  template<typename T>
  Log& operator<<(const T& value) {
    if (logging_details::enabled) {
      std::cout << value;
    }
    return *this;
  }

  ~Log() {
    if (logging_details::enabled) {
      std::cout << std::endl;
    }
  }
};

struct Args {
  std::string filepath;

  static Args ParseArgs(int argc, char** argv) {
    if (argc < 2) {
      throw std::runtime_error("Expected filepath to file with commands");
    }
    if (argc > 3) {
      throw std::runtime_error("Too many arguments");
    }
    if (argc == 3 && std::string(argv[2]) == "--no-logs") {
      logging_details::enabled = false;
    }
    return Args{argv[1]};
  }
};

struct Task {
  int delay;
  std::string program;
  std::vector<std::string> args;
};

std::vector<Task> ReadTasksFromFile(const std::string& filepath) {
  std::ifstream reader(filepath);
  if (!reader) {
    throw std::runtime_error("Failed to open file " + filepath);
  }

  std::vector<Task> tasks;
  std::string buffer;
  while (std::getline(reader, buffer)) {
    std::istringstream line(buffer);
    std::string word;
    Task task;

    line >> word;
    task.delay = std::stoi(word);
    
    if (!(line >> task.program)) {
      throw std::runtime_error("Line doesn't contain command");
    }

    while (line >> word) {
      task.args.push_back(word);
    }

    tasks.push_back(std::move(task));
  }
  return tasks;
}

char** ConstructArgs(const Task& task) {
  char** args = new char*[task.args.size() + 2];
  args[0] = new char[task.program.size() + 1];
  strcpy(args[0], task.program.c_str());
  for (size_t i = 0; i < task.args.size(); ++i) {
    args[i + 1] = new char[task.args[i].size() + 1];
    strcpy(args[i + 1], task.args[i].c_str());
  }
  args[task.args.size() + 1] = NULL;
  return args;
}

void DestructArgs(char** args) {
  for (size_t i = 0; args[i] != NULL; ++i) {
    delete[] args[i];
  }
  delete[] args;
}

void Execute(const Task& task) {
  int fork_result = fork();
  if (fork_result == 0) {
    // Перенаправляем stdin, stdout и stderr в /dev/null. 
    int fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
      throw std::runtime_error(std::string("Error while open: ") + strerror(errno));
    }
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);

    char** args = ConstructArgs(task);
    // Заменяем текущий процесс на программу из `task`.
    if (execve(task.program.c_str(), args, NULL) == -1) {
      DestructArgs(args);
      throw std::runtime_error(std::string("Error while execve: ") + strerror(errno));
    }
  }
  else if (fork_result == -1) {
    throw std::runtime_error(std::string("Couldn't fork: ") + strerror(errno));
  }
}

int main(int argc, char** argv) {
  const auto args = Args::ParseArgs(argc, argv);

  auto tasks = ReadTasksFromFile(args.filepath);

  std::sort(tasks.begin(), tasks.end(), [](const Task& lhs, const Task& rhs)->bool {
    return lhs.delay < rhs.delay || (lhs.delay == rhs.delay && lhs.program < rhs.program);
  });
  
  // Итератор, указывающий на первую в данный момент задачу в очереди на выполнение.
  size_t iter = 0;
  // Счетчик, хранящий количество секунд, прошедших с начала выполнения программы.
  int seconds_passed = 0;

  while (iter < tasks.size()) {
    Task& task = tasks[iter];

    // Вычисляем время, через которое должна выполниться следующая задача.
    int seconds_to_exec_next_task = task.delay - seconds_passed;
    if (seconds_to_exec_next_task > 0) {
      Log() << "Sleep for " << seconds_to_exec_next_task << " seconds...";
      int seconds_slept = sleep(seconds_to_exec_next_task);
      if (seconds_slept != 0) {
        seconds_passed += seconds_slept;
        Log() << "sleep() didn't return 0";
      }
      else {
        seconds_passed += seconds_to_exec_next_task;
      }
      continue;
    }

    Log() << "Executing " << task.program << " after " << seconds_passed << " seconds";
    Execute(task);
    ++iter;
  }
}
