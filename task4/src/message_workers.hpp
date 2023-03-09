#pragma once

#include <sys/ipc.h>
#include <sys/msg.h>

#include "utils.hpp"
#include "workers.hpp"

// Структура сообщений для очереди, хранящей количество свободного места на
// столе. Одно сообщение представляет собой как минимум 1 свободное место -
// определяется по значению поля `count` - это позволяет сократить
// количество записей в очередь сообщений при инициализации.
struct RemainingSpaceMessage {
    long   mtype;
    int    count;
};
// Структура сообщений для очереди, хранящей данные о посуде на столе.
struct DishesMessage {
    long   mtype;
    // Флаг, указывающий на то, является ли эта посуда последней.
    bool   last;
    char   dish_type[256];
};

// Класс разделяемого состояния для мойщика и вытирателя, использующих очереди
// сообщений для своей коммуникации.
class MessageSharedState : SharedState {
public:
  MessageSharedState(int table_limit) {
    // Создаем очередь сообщений.
    key_t key = CheckResult(ftok(".", 0), "ftok");
    msg_id = CheckResult(msgget(key, IPC_CREAT | IPC_EXCL | 0660), "msgget");
    // Пишем сообщение о количестве свободного места на столе.
    RemainingSpaceMessage message{.mtype = 1, .count = table_limit};
    CheckResult(msgsnd(msg_id, &message, sizeof(int), 0), "msgsnd");
  }

  int MsgId() { return msg_id; }

  ~MessageSharedState() {
    msgctl(msg_id, IPC_RMID, 0);
  }

private:
  int msg_id;
};

// Класс мойщика, использующего в своей реализации очереди сообщений.
class MessageWasher : public Washer {
public:
  MessageWasher(const Times& washing_times, std::shared_ptr<MessageSharedState> shared_state)
    : Washer(washing_times), shared_state(shared_state) {}

private:
  void BeforeWork() override {}

  void PutDish(const std::string& dish_type, bool is_last) override {
    // Ждем, пока появится свободное место на столе.
    RemainingSpaceMessage message;
    CheckResult(msgrcv(shared_state->MsgId(), &message, sizeof(int), 1, 0), "msgrcv");
    if (--message.count > 0) {
      // Если сообщение указывает на несколько свободных мест, записываем его
      // обратно с уменьшенным количеством.
      CheckResult(msgsnd(shared_state->MsgId(), &message, sizeof(int), 0), "msgsnd");
    }
    // Записываем данные о вымытой посуде.
    DishesMessage dish_message{.mtype = 2, .last = is_last};
    strcpy(dish_message.dish_type, dish_type.c_str());
    CheckResult(msgsnd(shared_state->MsgId(), &dish_message, sizeof(DishesMessage) - sizeof(long), 0), "msgsnd");
  }

  void AfterWork() override {}

private:
  std::shared_ptr<MessageSharedState> shared_state;
};

// Класс вытирателя, использующего в своей реализации очереди сообщений.
class MessageWiper : public Wiper {
public:
  MessageWiper(const Times& wiping_times, std::shared_ptr<MessageSharedState> shared_state)
    : Wiper(wiping_times), shared_state(shared_state) {}

private:
  void BeforeWork() override {}

  bool IsWorkDone() override { return took_last; }

  std::string TakeDish() override {
    // Пишем сообщение о появившемся свободном месте на столе.
    RemainingSpaceMessage message{.mtype = 1, .count = 1};
    CheckResult(msgsnd(shared_state->MsgId(), &message, sizeof(int), 0), "msgsnd");
    // Читаем данные о вымытой посуде.
    DishesMessage dish_message;
    CheckResult(msgrcv(shared_state->MsgId(), &dish_message, sizeof(DishesMessage) - sizeof(long), 2, 0), "msgrcv");
    took_last = dish_message.last;
    std::string dish_type;
    dish_type.resize(strlen(dish_message.dish_type));
    strcpy(dish_type.data(), dish_message.dish_type);
    return dish_type;
  }

  void AfterWork() override {}

private:
  std::shared_ptr<MessageSharedState> shared_state;
  bool took_last = false;
};
