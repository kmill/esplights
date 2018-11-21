#pragma once

#include <memory>
#include "tty.hpp"

#define MAX_TASKS 256

extern std::shared_ptr<TTY> cur_tty;

class Task;

struct TaskRef {
  Task *task;
  int exit_code;

  TaskRef(Task *t) : task(t), exit_code(-1) {
  }
  void trySetExit(uint8_t code) {
    if (exit_code < 0) {
      task = nullptr;
      exit_code = code;
    }
  }

  bool isDone() {
    return task == nullptr;
  }
};

class Task {
public:
  Task(const char *task_name);
  virtual ~Task();

  virtual void run() {}

  void setTTY(std::shared_ptr<TTY> tty) {
    this->tty = tty;
  }

  /**
     Turn task into top-level task (no parent).
   */
  void detach();

  /**
     Whether the task will be scheduled. Default false.
     Reschedule if interval and making active.
   */
  void setActive(bool active) {
    if (active && !this->active) {
      setInterval(interval);
    }
    this->active = active;
  }
  /**
     Whether the task does not die if the TTY closes. Default false.
   */
  void setBackground(bool background) {
    this->background = background;
  }
  /**
     Whether the task waits on active nonbackground noninterval subtasks. Default true.
   */
  void setWaits(bool waits) {
    this->waits = waits;
  }

  /**
     Set the wakeup interval (in microseconds).  0 to disable wakeup intervals.
   */
  void setInterval(uint32_t interval);
  void setIntervalFPS(float fps) {
    setInterval(static_cast<uint32_t>(1000000.0/fps));
  }

  /**
     Set exit code and delete task.
   */
  void exit(uint8_t code) {
    this->_taskref->trySetExit(code);
    deathmark = true;
  }

  /**
     Gives a handle object that can be used to find the exit code on exit.
     Grab this when creating a task.
   */
  std::shared_ptr<TaskRef> ref() {
    return this->_taskref;
  }

  uint8_t get_tid() {
    return tid;
  }
  const char *get_name() {
    return name;
  }
  bool get_active() {
    return active;
  }
  bool get_background() {
    return background;
  }
  bool get_waits() {
    return waits;
  }
  uint32_t get_interval() {
    return interval;
  }
  uint32_t get_scheduled() {
    return scheduled;
  }
  Task *get_parent() {
    return parent;
  }
  std::shared_ptr<TTY> get_tty() {
    return tty;
  }

  /**
     get total runtime in units of 1024 us ("milliseconds").  for interval tasks, is reset each time.
   */
  uint32_t get_ms_cost() {
    return ms_cost;
  }
  /**
     get maximum time late in units of 1024 us ("milliseconds"), for interval tasks.
   */
  uint32_t get_ms_late() {
    return ms_late;
  }


  /**
     Run active tasks in round-robin once-through, for at most some number of microseconds.
   */
  static void run_tasks(uint32_t usecs);

  static void kill_current();

  static Task *get(uint8_t tid);

  static Task *current();

protected:
  uint8_t tid;
  const char *name;
  std::shared_ptr<TTY> tty;
private:
  bool active;
  bool background;
  bool waits;
  uint32_t interval;
  uint32_t scheduled;
  bool deathmark;
  // statistics
  uint32_t ms_cost;
  uint32_t ms_late;

  void remove_child(Task *child);
  void add_child(Task *task);
  void detach_parent();

  bool should_run(uint32_t time);
  bool should_die();
  void reschedule();

  std::shared_ptr<TaskRef> _taskref;
  /* task tree */
  Task *parent;
  Task *child;
  Task *nextChild;

  static Task *task_list[256];
  static void task_list_push(Task *task);
  static void task_list_remove(Task *task);
  static uint8_t scheduler_next_task;
};

