#include "task.hpp"
extern "C" {
#include "user_interface.h"
}

std::shared_ptr<TTY> cur_tty(new StreamTTY(&Serial));
static std::shared_ptr<TaskRef> current_taskref = nullptr;

Task *Task::task_list[256] = {nullptr};
uint8_t Task::scheduler_next_task = 0;

Task *Task::get(uint8_t tid) {
  return task_list[tid];
}

void Task::task_list_push(Task *task) {
  for (int i = 1; i < MAX_TASKS; i++) {
    if (!task_list[i]) {
      task_list[i] = task;
      task->tid = i;
      return;
    }
  }
  // Otherwise assign to task 0, which is auto-killed
  Task *t0 = task_list[0];
  task_list[0] = nullptr;
  delete t0; // TODO: unsafe, since this might be the task that called task_list_push
  task_list[0] = task;
  task->tid = 0;
}
void Task::task_list_remove(Task *task) {
  if (task_list[task->tid] == task) {
    task_list[task->tid] = nullptr;
  }
}

Task::Task(const char *task_name) :
  tid(0),
  name(task_name),
  tty(cur_tty),
  active(false),
  background(false),
  waits(true),
  interval(0),
  scheduled(0),
  deathmark(false),
  ms_cost(0),
  ms_late(0),
  _taskref(new TaskRef(this)),
  parent(nullptr),
  child(nullptr),
  nextChild(nullptr)
{
  task_list_push(this);
  if (current_taskref && current_taskref->task) {
    current_taskref->task->add_child(this);
  }
}

Task::~Task() {
  this->detach_parent();

  while (this->child) {
    Task *t = this->child;
    this->remove_child(t);
    delete t;
  }

  task_list_remove(this);

  _taskref->trySetExit(0);
}

void Task::detach() {
  this->detach_parent();
}

void Task::detach_parent() {
  if (this->parent) {
    this->parent->remove_child(this);
  }
}

void Task::remove_child(Task *child) {
  Task **t = &this->child;
  while (*t) {
    if (*t == child) {
      *t = (*t)->nextChild;
      child->parent = nullptr;
      child->nextChild = nullptr;
      return;
    }
    t = &(*t)->nextChild;
  }
}

void Task::add_child(Task *task) {
  if (this == task) {
    return;
  }
  task->detach_parent();
  task->parent = this;
  task->nextChild = this->child;
  this->child = task;
}

void Task::setInterval(uint32_t interval) {
  if (interval == 0) {
    this->interval = 0;
  } else {
    this->interval = interval;
    this->scheduled = system_get_time() + interval;
  }
}

void Task::reschedule() {
  if (interval > 0) {
    uint32_t now = system_get_time();
    do {
      scheduled += interval;
    } while (static_cast<int32_t>(scheduled - now) <= 0);
  }
}

bool Task::should_run(uint32_t time) {
  if (!active) {
    return false;
  }
  if (waits) {
    for (Task *child = this->child; child; child = child->nextChild) {
      if (!child->background && child->interval == 0 && child->active) {
        return false;
      }
    }
  }
  if (interval > 0 && static_cast<int32_t>(scheduled - time) > 0) {
    return false;
  }
  return true;
}

bool Task::should_die() {
  return deathmark || (!background && tty && !tty->connected());
}

void Task::run_tasks(uint32_t usecs) {
  std::shared_ptr<TTY> old_cur_tty = cur_tty;

  uint32_t start = system_get_time();
  uint32_t now = start;
  uint8_t start_tid = scheduler_next_task;

  do {
    Task *task = task_list[scheduler_next_task++];
    if (task) {
      if (task->should_die()) {
        delete task;
      } else if (task->should_run(now)) {
        if (task->interval > 0) {
          task->ms_late = max(task->ms_late, (system_get_time() - task->scheduled) >> 10);
        }

        current_taskref = task->ref();
        cur_tty = task->tty;
        task->run();
        task->reschedule();
        cur_tty = old_cur_tty;
        current_taskref = nullptr;

        uint32_t this_ms = (system_get_time() - now) >> 10;
        if (task->interval > 0) {
          task->ms_cost = this_ms;
        } else {
          task->ms_cost += this_ms;
        }
      }
    }
    now = system_get_time();
  } while (static_cast<int32_t>(start+usecs - now) > 0 && start_tid != scheduler_next_task);
}


Task *Task::current() {
  if (current_taskref && current_taskref->task) {
    return current_taskref->task;
  } else {
    return nullptr;
  }
}

void Task::kill_current() {
  if (current_taskref && current_taskref->task) {
    current_taskref->task->deathmark = true;
  }
}
