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
        activeTasks--;
        workFinished.notify_all();
      }
      while (running && queue.empty()) {
        workAdded.wait(guard);
      }
      if (queue.any()) task = queue.popBack();
    }
    if (task) {
      task->state = Task::State::Running;
      if(task->func) task->func(task->cancel);
      task->~Task();
      task->state = Task::State::Uninitialized;
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
  auto * task = new(taskPool.alloc()) Task();
  task->generation = generation++;
  if (generation == 0) generation++;
  task->func = func;
  task->state = Task::State::Queued;
  queue.pushBack(task);
  activeTasks++;
  workAdded.notify_one();
  auto index = taskPool.getIndex(task);
  assert(index <= 0xFFFFu);

  return TaskId{ uint16_t(index), task->generation };
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
  while (true) {
    auto * task = taskPool.fromIndex(id.index);
    if (task->generation != id.generation) return true;
    if (task->state != Task::State::Running && task->state != Task::State::Queued) return true;
    logger(0, "waiting.. state=%d", task->state);
    workFinished.wait(guard);
    logger(0, "woken");
  }
  return true;
}

void Tasks::waitAll()
{
  std::unique_lock<std::mutex> guard(lock);
  while (true) {
    if (activeTasks == 0) return;
    workFinished.wait(guard);
  }
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
