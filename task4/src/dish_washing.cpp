#include <memory>

#include "utils.hpp"
#include "fifo_workers.hpp"
#include "pipe_workers.hpp"
#include "message_workers.hpp"
#include "socket_workers.hpp"
#include "shm_workers.hpp"

struct Args {
  std::string washing_times_filepath;
  std::string wiping_times_filepath;
  std::string dishes_filepath;
  int table_limit;
  std::string workers_type;

  static Args Parse(int argc, char** argv) {
    if (argc != 5) {
      throw std::runtime_error("Incorrect number of arguments");
    }

    Args args;
    args.washing_times_filepath = std::string(argv[1]);
    args.wiping_times_filepath = std::string(argv[2]);
    args.dishes_filepath = std::string(argv[3]);
    args.workers_type = std::string(argv[4]);

    char* table_limit_var = getenv("TABLE_LIMIT");
    if (table_limit_var == NULL) {
      throw std::runtime_error("Couldn't get TABLE_LIMIT env variable");
    }
    args.table_limit = std::stoi(table_limit_var);

    return args;
  }
};

// Возвращает требуемую пару работников согласно переданному типу.
std::pair<std::unique_ptr<Washer>, std::unique_ptr<Wiper>>
CreateWorkers(const Times& washing_times,
              const Times& wiping_times,
              int table_limit,
              const std::string& type) {
  if (type == "fifo") {
    auto shared_state = std::make_shared<FifoSharedState>(table_limit);
    return {std::make_unique<FifoWasher>(washing_times, shared_state),
            std::make_unique<FifoWiper>(wiping_times, shared_state)};
  }
  if (type == "pipe") {
    auto shared_state = std::make_shared<PipeSharedState>(table_limit);
    return {std::make_unique<PipeWasher>(washing_times, shared_state),
            std::make_unique<PipeWiper>(wiping_times, shared_state)};
  }
  if (type == "msg") {
    auto shared_state = std::make_shared<MessageSharedState>(table_limit);
    return {std::make_unique<MessageWasher>(washing_times, shared_state),
            std::make_unique<MessageWiper>(wiping_times, shared_state)};
  }
  if (type == "shm") {
    auto shared_state = std::make_shared<ShmSharedState>(table_limit);
    return {std::make_unique<ShmWasher>(washing_times, shared_state),
            std::make_unique<ShmWiper>(wiping_times, shared_state)};
  }
  if (type == "socket") {
    auto shared_state = std::make_shared<SocketSharedState>(table_limit);
    return {std::make_unique<SocketWasher>(washing_times, shared_state),
            std::make_unique<SocketWiper>(wiping_times, shared_state)};
  }
  throw std::runtime_error("Unexpected type of workers: " + type);
}

int main(int argc, char** argv) {
  const Args args = Args::Parse(argc, argv);

  Times washing_times = Times::LoadFromFile(args.washing_times_filepath);
  Times wiping_times = Times::LoadFromFile(args.wiping_times_filepath);
  WashTaskQueue queue = WashTaskQueue::LoadFromFile(args.dishes_filepath);

  auto workers = CreateWorkers(washing_times, wiping_times, args.table_limit, args.workers_type);
  workers.second->Work();
  workers.first->Work(queue);
  workers.second->Join();
}
