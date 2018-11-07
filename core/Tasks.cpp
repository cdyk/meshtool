#include "Common.h"
#include "Tasks.h"
#include <list>
#include <thread>
#include <cassert>


void Tasks::init(Logger logger)
{
  assert(this->logger == nullptr);
  this->logger = logger;

  workers.resize(std::thread::hardware_concurrency());
  for (auto & w : workers) {
    w = std::move(std::thread(&Tasks::worker, this));
  }
  logger(0, "Tasks: initialized with %d worker threads.", workers.size());
}

void Tasks::worker()
{
  Task * task = nullptr;
  while (running)
  {
    {
      std::unique_lock<std::mutex> guard(lock);
      if (task) {
        taskPool.release(task);
        task = nullptr;
      }
      while (running && queue.empty()) {
        workAdded.wait(guard);
      }
      if (queue.any()) task = queue.popBack();
    }
    if (task) {
      task->state = Task::State::Running;
      task->func(task->cancel);
      workFinished.notify_all();
    }
  }
}


Tasks::~Tasks()
{
  cleanup();
}

TaskId Tasks::enqueue(TaskFunc& func)
{
  std::lock_guard<std::mutex> guard(lock);
  auto * task = taskPool.alloc();
  task->generation = generation++;
  task->func = func;
  task->state = Task::State::Queued;
  queue.pushBack(task);
  workAdded.notify_one();
  return TaskId{ taskPool.getIndex(task), task->generation };
}

bool Tasks::poll(TaskId id)
{
  std::unique_lock<std::mutex> guard(lock);
  auto * task = taskPool.fromIndex(id.index);
  if (task->generation != id.generation) return true;
  if (task->state != Task::State::Running && task->state != Task::State::Queued) return true;
  return false;
}

bool Tasks::wait(TaskId id)
{
  std::unique_lock<std::mutex> guard(lock);
  auto * task = taskPool.fromIndex(id.index);
  if (task->generation != id.generation) return true;
  if (task->state != Task::State::Running && task->state != Task::State::Queued) return true;

  workFinished.wait(guard, [task]() { return task->state != Task::State::Running && task->state != Task::State::Queued; });

  return true;
}


void  Tasks::cleanup()
{
  {
    std::lock_guard<std::mutex> guard(lock);
    for (auto * item : queue) {
      item->cancel = true;
    }
    running = false;
    workAdded.notify_all();
    workFinished.notify_all();
  }
  for (auto & w : workers) {
    w.join();
  }
  workers.clear();
}
