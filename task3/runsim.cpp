#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

struct Args {
  int processes;

  static Args Parse(int argc, char** argv) {
    if (argc != 2) {
      throw std::runtime_error("Expected 1 argument");
    }
    return {std::stoi(argv[1])};
  }
};

struct Command {
  std::string program_path;
  std::vector<std::string> args;

  static Command Parse(const std::string& line) {
    std::istringstream stream(line);

    Command command;
    if (!(stream >> command.program_path)) {
      throw std::runtime_error("Expected program path");
    }

    std::string arg;
    while (stream >> arg) {
      command.args.push_back(arg);
    }

    return command;
  }
};

int CheckResult(int result, std::string operation) {
  if (result == -1) {
    throw std::runtime_error("Error while " + operation + ": " + strerror(errno));
  }
  return result;
}

class Process {
public:
  void Run(const Command& command) {
    pid = CheckResult(fork(), "Error while fork: ");
    if (pid == 0) {
      // Перенаправляем stdin в /dev/null.
      int fd = CheckResult(open("/dev/null", O_RDWR), "open");
      dup2(fd, 0);

      char** args = ConstructArgs(command);
      // Заменяем текущий процесс на программу из `task`.
      CheckResult(execve(command.program_path.c_str(), args, NULL), "execve");
    }
  }

  bool IsRunning() {
    if (pid == 0) {
      return false;
    }
    if (CheckResult(waitpid(pid, NULL, WNOHANG), "waitpid") == pid) {
      pid = 0;
      return false;
    }
    return true;
  }

private:
  char** ConstructArgs(const Command& task) {
    char** args = new char*[task.args.size() + 2];
    args[0] = new char[task.program_path.size() + 1];
    strcpy(args[0], task.program_path.c_str());
    for (size_t i = 0; i < task.args.size(); ++i) {
      args[i + 1] = new char[task.args[i].size() + 1];
      strcpy(args[i + 1], task.args[i].c_str());
    }
    args[task.args.size() + 1] = NULL;
    return args;
  }

  int pid = 0;
};

int main(int argc, char** argv) {
  const Args args = Args::Parse(argc, argv);

  std::vector<Process> processes(args.processes);

  std::string buffer;
  while (std::getline(std::cin, buffer)) {
    const Command command = Command::Parse(buffer);

    bool has_spare_process = false;
    for (size_t i = 0; i < processes.size(); ++i) {
      Process& process = processes[i];
      if (!process.IsRunning()) {
        std::cout << "Run " << command.program_path << " in process " << i << std::endl;
        process.Run(command);
        has_spare_process = true;
        break;
      }
    }

    if (!has_spare_process) {
      std::cout << "You reached the limit of programs running simultaneously" << std::endl;
    }
  }
}
