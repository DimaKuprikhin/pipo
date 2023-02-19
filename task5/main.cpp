#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "async_multiplier.hpp"

struct Args {
  uint64_t processors;
  bool use_threads;
};

Args ParseArgs(int argc, char** argv) {
  Args args{std::thread::hardware_concurrency() + 1, true};
  if (argc >= 2) {
    if (argv[1] == std::string("--use-processes")) {
      args.use_threads = false;
      if (argc >= 3) {
        throw std::runtime_error("Too many arguments");
      }
    }
    else {
      try {
        args.processors = std::stoull(argv[1]);
      }
      catch (...) {
        throw std::runtime_error("First argument should be either --use-processes or number of processors");
      }
      if (argc >= 3) {
        if (argv[2] != std::string("--use-processes")) {
          throw std::runtime_error(std::string("Unknown argument: ") + argv[2]);
        }
        args.use_threads = false;
      }
    }
  }
  if (argc > 3) {
    throw std::runtime_error("Too many arguments");
  }
  std::cout << "Program will use " << args.processors << " "
      << (args.use_threads ? "threads" : "processes") << std::endl;
  return args;
}

int main(int argc, char** argv) {
  Args args = ParseArgs(argc, argv);

  auto multipliers = CreateMultipliers(args.processors, args.use_threads);

  std::string input;
  while (std::cin >> input) {
    uint64_t value = std::stoll(input);
    
    uint64_t quotient = value / args.processors;
    uint64_t remainder = value % args.processors;

    // Распределяем задачи по вычислителям.
    uint64_t from = 1;
    for (uint64_t i = 0; i < args.processors; ++i) {
      if (quotient == 0 && i >= remainder) {
        continue;
      }
      uint64_t to = from + quotient - 1;
      if (i < remainder) {
        ++to;
      }
      multipliers[i]->SetTask(from, to);
      from = to + 1;
    }

    // Ожидаем результатов вычислений.
    uint64_t result = 1;
    for (auto& multiplier : multipliers) {
      uint64_t task_result = multiplier->GetResult();
      if (task_result != 0) {
        result*= task_result;
      }
    }

    std::cout << value << "! = " << result << std::endl;
  }

  for (auto& multiplier : multipliers) {
    multiplier->Finish();
  }
  
  return 0;
}
