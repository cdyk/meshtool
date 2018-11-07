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

        for (uint8_t i = 0; i < task->succcessorCount; i++) {
          auto & succ = task->successors[i];
          auto * succTask = taskPool.fromIndex(succ.index);
          assert(succTask->generation == succ.generation);

          if (--succTask->predecessors == 0) {
            // fixme when it pops up in profiler.
            for (uint32_t t = 0; t < waiting.size32(); t++) {
              if (waiting[t] == succTask) {
                waiting[t] = waiting.back();
                break;
              }
            }
            waiting.popBack();
            ready.pushBack(succTask);
            workAdded.notify_one();
          }
        }

        task->~Task();
        task->state = Task::State::Uninitialized;

        taskPool.release(task);
        task = nullptr;
        activeTasks--;
        workFinished.notify_all();
      }
      while (running && ready.empty()) {
        workAdded.wait(guard);
      }
      if (ready.any()) task = ready.popBack();
    }
    if (task) {
      task->state = Task::State::Running;
      if(task->func) task->func(task->cancel);
    }
  }
}


Tasks::~Tasks()
{
  cleanup();
}

TaskId Tasks::enqueue(TaskFunc& func, TaskId* predecessors, uint32_t predecessors_count)
{
  std::lock_guard<std::mutex> guard(lock);
  auto * task = new(taskPool.alloc()) Task();
  task->generation = generation++;
  if (generation == 0) generation++;
  task->func = func;
  task->state = Task::State::Queued;
  activeTasks++;

  auto index = taskPool.getIndex(task);
  assert(index <= 0xFFFFu);

  auto taskId = TaskId{ uint16_t(index), task->generation };

  if (predecessors && predecessors_count) {

    for (uint32_t i = 0; i < predecessors_count; i++) {
      auto & pred = predecessors[i];
      if (pred.generation != 0) {
        auto * predecessorTask = taskPool.fromIndex(pred.index);
        if (pred.generation == predecessorTask->generation) {
          assert(predecessorTask->succcessorCount < 4);
          predecessorTask->successors[predecessorTask->succcessorCount++] = taskId;
          task->predecessors++;
        }
      }
    }
  }
  if (task->predecessors) {
    waiting.pushBack(task);
  }
  else {
    ready.pushBack(task);
    workAdded.notify_one();
  }
  return taskId;
}

TaskId Tasks::enqueueFence(TaskId* predecessors, uint32_t predecessors_count)
{
  TaskFunc f;
  return enqueue(f, predecessors, predecessors_count);
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
    for (auto * item : ready) {
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
