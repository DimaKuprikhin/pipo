#pragma once

#include <stdexcept>
#include <string>

#include <string.h>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

// Вспомогательные функции, оборачивающие обработку ошибок c style функций.
int CheckResult(int result, const std::string& operation) {
  if (result == -1) {
    throw std::runtime_error("Error while " + operation + ": " + strerror(errno));
  }
  return result;
}

void* CheckResult(void* result, const std::string& operation) {
  if (result == (void*)(-1)) {
    throw std::runtime_error("Error while " + operation + ": " + strerror(errno));
  }
  return result;
}

struct Times : public std::unordered_map<std::string, int> {
  static Times LoadFromFile(const std::string& filepath) {
    std::ifstream reader(filepath);
    if (!reader) {
      throw std::runtime_error("Couldn't read file " + filepath);
    }

    Times times;
    std::string buffer;
    while (std::getline(reader, buffer)) {
      std::istringstream line(buffer);
      std::string dish_type;
      char colon;
      std::string time_str;
      int time;

      if (!(line >> dish_type)) {
        throw std::runtime_error("Expected dish type");
      }
      if (!(line >> colon) || (colon != ':')) {
        throw std::runtime_error("Expected ':' delimiter");
      }
      if (!(line >> time_str)) {
        throw std::runtime_error("Expected operation time");
      }
      try {
        time = std::stoi(time_str);
        if (time < 0) {
          throw std::exception();
        }
      }
      catch (...) {
        throw std::runtime_error("Invalid operation time");
      }
      times.insert({dish_type, time});
    }
    return times;
  }
};

struct WashTask {
  std::string dish_type;
  int count;
};

struct WashTaskQueue : public std::queue<WashTask> {
  static WashTaskQueue LoadFromFile(const std::string& filepath) {
    std::ifstream reader(filepath);
    if (!reader) {
      throw std::runtime_error("Couldn't open file " + filepath);
    }

    WashTaskQueue queue;
    std::string buffer;
    while (std::getline(reader, buffer)) {
      std::istringstream line(buffer);
      std::string dish_type;
      char colon;
      std::string count_str;
      int count;

      if (!(line >> dish_type)) {
        throw std::runtime_error("Expected dish type");
      }
      if (!(line >> colon) || (colon != ':')) {
        throw std::runtime_error("Expected ':' delimiter");
      }
      if (!(line >> count_str)) {
        throw std::runtime_error("Expected count of dishes");
      }
      try {
        count = std::stoi(count_str);
        if (count < 0) {
          throw std::exception();
        }
      }
      catch (...) {
        throw std::runtime_error("Invalid number of dishes to wash");
      }
      queue.push({dish_type, count});
    }
    return queue;
  }
};
