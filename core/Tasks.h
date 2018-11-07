#pragma once
#include <mutex>
#include <condition_variable>
#include "mem/Allocators.h"

typedef std::function<void(bool&)> TaskFunc;

struct TaskId
{
  uint32_t index;
  uint32_t generation;
};

struct Task
{
  enum struct State {
    Uninitialized,
    Queued,
    Running
  };
  uint32_t generation = 0;
  State state = State::Uninitialized;
  bool cancel = false;
  TaskFunc func;
};

class Tasks
{
public:
  ~Tasks();

  void init(Logger logger);
  TaskId enqueue(TaskFunc& func);
  bool poll(TaskId id);
  bool wait(TaskId id);
  void cleanup();

private:
  Logger logger = nullptr;
  std::mutex lock;
  std::condition_variable workAdded;
  std::condition_variable workFinished;
  Pool<Task> taskPool;
  uint32_t generation = 1;
  Vector<Task*> queue;
  Vector<std::thread> workers;
  bool running = true;
  void worker();

};