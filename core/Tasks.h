#pragma once
#include <mutex>
#include <condition_variable>
#include "mem/Allocators.h"

typedef std::function<void(bool&)> TaskFunc;

struct TaskId
{
  uint16_t index = 0;
  uint16_t generation = 0;  // a valid task will never get generation=1;
};

struct Task
{
  TaskFunc func;
  enum struct State {
    Uninitialized,
    Queued,
    Running
  };
  uint16_t generation = 0;
  State state = State::Uninitialized;
  bool cancel = false;
};

class Tasks
{
public:
  ~Tasks();

  void init(Logger logger);
  TaskId enqueue(TaskFunc& func);
  bool poll(TaskId id);
  bool wait(TaskId id);
  void waitAll();
  void cleanup();

private:
  Logger logger = nullptr;
  std::mutex lock;
  uint32_t activeTasks = 0;
  std::condition_variable workAdded;
  std::condition_variable workFinished;
  Pool<Task> taskPool;
  uint16_t generation = 1;
  Vector<Task*> queue;
  Vector<std::thread> workers;
  bool running = true;
  void worker();

};