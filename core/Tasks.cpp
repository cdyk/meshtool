#include "Common.h"
#include <list>
#include <thread>

// Mock-up of a task system - justs spawns a thread per task for now

struct Task
{
  Task(TaskFunc& func, TaskId id) : func(func), thread(&Task::run, this), id(id) {}

  TaskFunc func;
  std::thread thread;
  TaskId id;
  bool done = false;

  void run()
  {
    func();
    done = true;
  }

};

struct TasksImpl
{
  Logger logger;
  std::list<Task> tasks;

  TaskId nextId = 0;
};


void Tasks::init(Logger logger)
{
  impl = new TasksImpl;
}

Tasks::~Tasks()
{
  cleanup();
  delete  impl;
  impl = nullptr;
}

TaskId Tasks::enqueue(TaskFunc& func)
{
  impl->tasks.emplace_back(func, impl->nextId);
  return impl->nextId++;
}

void  Tasks::wait(TaskId id)
{
  for (auto it = impl->tasks.begin(); it != impl->tasks.end();) {
    if (it->id == id) {
      it->thread.join();
      it = impl->tasks.erase(it);
    }
    else {
      ++it;
    }
  }
}

void  Tasks::update()
{
  for (auto it = impl->tasks.begin(); it != impl->tasks.end();) {
    if (it->done) {
      it->thread.join();
      it = impl->tasks.erase(it);
    }
    else {
      ++it;
    }
  }

}


void  Tasks::cleanup()
{
  for (auto it = impl->tasks.begin(); it != impl->tasks.end();) {
    if(it->thread.joinable()) it->thread.join();
  }
  impl->tasks.clear();
}
