#pragma once
#include <cstdint>
#include <functional>

typedef void(*Logger)(unsigned level, const char* msg, ...);


bool readObj(Logger logger, const void * ptr, size_t size);

typedef std::function<void(void)> TaskFunc;
typedef unsigned TaskId;
struct TasksImpl;
struct Tasks
{
  ~Tasks();

  void init(Logger logger);
  TaskId enqueue(TaskFunc& func);
  void wait(TaskId id);
  void update();
  void cleanup();

  TasksImpl* impl = nullptr;
};

