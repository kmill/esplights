#include "commands.hpp"
#include "terminal.hpp"
#include "task.hpp"
extern "C" {
#include "user_interface.h"
}
#include <cstdlib>

// Reminder: add new commands to initialize_commands at the end.

static int cmd_print_args(int argc, char **argv) {
  cur_tty->printf("received %d arguments\n", argc);
  for (int i = 0; i < argc; i++) {
    cur_tty->printf("%d: '%s'\n", i, argv[i]);
  }
  return 0;
}

static int cmd_tasks(int argc, char **argv) {
  int have_task = 0;
  for (int i = 0; i < MAX_TASKS; i++) {
    Task *t = Task::get(i);
    if (t) {
      have_task = 1;
      cur_tty->printf("%d. %s (", t->get_tid(), t->get_name());
      cur_tty->print(t->get_active() ? "a" : "");
      cur_tty->print(t->get_background() ? "b" : "");
      cur_tty->print(t->get_waits() ? "w" : "");
      cur_tty->print(")");
      if (t->get_parent()) {
        cur_tty->printf("[%d]", t->get_parent()->get_tid());
      }
      if (t->get_interval() > 0) {
        cur_tty->printf(" (every %u us", t->get_interval());
        if (t->get_active()) {
          cur_tty->printf("; scheduled for %u)", t->get_scheduled());
        }
        cur_tty->printf(")");
      }
      if (t->get_tty()) {
        cur_tty->printf(" (tty %p", t->get_tty().get());
        if (!t->get_tty()->connected()) {
          cur_tty->printf(" disconnected");
        }
        cur_tty->printf(")");
      }
      cur_tty->printf(" (runtime %u ms", t->get_ms_cost() * 1000/1024);
      if (t->get_interval() > 0) {
        cur_tty->printf("; late %u ms", t->get_ms_late() * 1000/1024);
      }
      cur_tty->printf(")");
      cur_tty->print("\n");
    }
  }
  if (!have_task) {
    cur_tty->printf("(none)\n");
  } else {
    cur_tty->printf("a=active, b=background, w=waits. [parent]\n");
  }
  cur_tty->printf("Current time: %u us\n",  system_get_time());
  return 0;
}

static int cmd_kill(int argc, char **argv) {
  if (argc < 2) {
    cur_tty->printf("Usage: %s [-c exitcode] taskid taskid ...\n", argv[0]);
    return 1;
  }
  int exitcode = 22;
  for (int i = 1; i < argc; i++) {
    if (strcmp("-c", argv[i]) == 0) {
      i++;
      if (i < argc) {
        exitcode = atoi(argv[i]);
      }
    } else {
      Task *t = Task::get(atoi(argv[i]));
      if (t) {
        cur_tty->printf("Killing task %d with code %d\n", t->get_tid(), exitcode);
        t->exit(exitcode);
      }
    }
  }
  return 0;
}

static int cmd_exit(int argc, char **argv) {
  cur_tty->printf("Bye!\n");
  cur_tty->close();
  return 0;
}

static int cmd_reset(int argc, char **argv) {
  cur_tty->printf("Resetting.\n");
  cur_tty->close();
  delay(5); // to give enough time for telnet to close
  ESP.restart();
  return 1;
}

#include "lights.hpp"
static int cmd_clear(int argc, char **argv) {
  auto seg = requestLEDSegment();
  seg->clear();
  seg->send(true);
  return 0;
}
static int cmd_stop(int argc, char **argv) {
  auto seg = requestLEDSegment();
  return 0;
}

class LightTask : public Task {
public:
  LightTask(const char *name, float fps=30.0f)
    : Task(name),
      seg(requestLEDSegment())
  {
    detach();
    setIntervalFPS(fps);
    setActive(true);
  }
  void run() override {
    if (!seg->isActive()) {
      exit(0);
      return;
    }
    update();
  }
  virtual void update() = 0;
protected:
  std::shared_ptr<LEDSegment> seg;
};

class RainbowTask : public LightTask {
public:
  RainbowTask(float speed, float mul, float s, float b)
    : LightTask("rainbow"),
      _speed(speed),
      _mul(mul),
      _s(s),
      _b(b)
  {}
  void update() override {
    for (size_t i = 0; i < seg->length(); i++) {
      HsbColor c(fmod(hue + _mul*static_cast<float>(i)/seg->length(), 1.0f), _s, _b);
      seg->set(i, c);
    }
    seg->send();
    hue = fmod(hue - _speed, 1.0f);
  }
private:
  float _speed, _mul, _s, _b;
  float hue = 0.0;
};

double clamp(double a, double lo, double hi) {
  if (a > hi) {
    return hi;
  } else if (a <= hi) {
    if (a < lo) {
      return lo;
    } else {
      return a;
    }
  } else {
    // NaN
    return lo;
  }
}
float clamp(float a, float lo, float hi) {
  if (a > hi) {
    return hi;
  } else if (a <= hi) {
    if (a < lo) {
      return lo;
    } else {
      return a;
    }
  } else {
    // NaN
    return lo;
  }
}

static int cmd_rainbow(int argc, char **argv) {
  float speed = 0.01, mul = 1.0, s = 1.0, b = 1.0;
  for (char **arg = &argv[1]; *arg; ) {
    if (strcmp(*arg, "-f") == 0) {
      arg++;
      speed = atof(*arg++);
    } else if (strcmp(*arg, "-m") == 0) {
      arg++;
      mul = atof(*arg++);
    } else if (strcmp(*arg, "-s") == 0) {
      arg++;
      s = clamp(atof(*arg++), 0.0, 1.0);
    } else if (strcmp(*arg, "-b") == 0) {
      arg++;
      b = clamp(atof(*arg++), 0.0, 1.0);
    } else {
      cur_tty->printf("%s [-f speed] [-m spatial_multiplier] [-s saturation] [-b brightness]\n", argv[0]);
      return 1;
    }
  }
  new RainbowTask(speed, mul, s, b);
  return 0;
}

static int cmd_rgb(int argc, char **argv) {
  if (argc != 4) {
    cur_tty->printf("%s r g b", argv[0]);
    return 1;
  }
  float r, g, b;
  r = clamp(atof(argv[1]), 0.0, 1.0);
  g = clamp(atof(argv[2]), 0.0, 1.0);
  b = clamp(atof(argv[3]), 0.0, 1.0);
  auto seg = requestLEDSegment();
  for (size_t i = 0; i < seg->length(); i++) {
    seg->set(i, RgbColor{
        static_cast<uint8_t>(r*255.99f),
          static_cast<uint8_t>(g*255.99f),
          static_cast<uint8_t>(b*255.99f)});
  }
  seg->send(true);
  return 0;
}

int iclamp(int x, int lo, int hi) {
  if (x < lo) {
    return lo;
  } else if (x > hi) {
    return hi;
  } else {
    return x;
  }
}

class TwinkleTask : public LightTask {
public:
  TwinkleTask()
    : LightTask("twinkle")
  {
    targets = new RgbColor[seg->length()];
    for (size_t i = 0; i < seg->length(); i++) {
      targets[i] = 0;
    }
  }
  ~TwinkleTask() {
    delete[] targets;
  }
  void update() override {
    int upspeed = 4;
    int downspeed = 2;
    for (size_t j = 0; j < seg->length(); j++) {
      RgbColor c = seg->get(j);
      RgbColor c2 = targets[j];
      if (c.R < c2.R || c.G < c2.G || c.B < c2.B) {
        if (c.R < c2.R) {
          c.R = iclamp(c.R + upspeed, 0, c2.R);
        }
        if (c.G < c2.G) {
          c.G = iclamp(c.G + upspeed, 0, c2.G);
        }
        if (c.B < c2.B) {
          c.B = iclamp(c.B + upspeed, 0, c2.B);
        }
      } else {
        c.R = iclamp((int)c.R - downspeed, 0, 255);
        c.G = iclamp((int)c.G - downspeed, 0, 255);
        c.B = iclamp((int)c.B - downspeed, 0, 255);
        c2 = c;
      }
      seg->set(j, c);
      targets[j] = c2;
    }
    uint16_t r = random(100) + random(100) + random(100);
    if (r < 150) {
      uint16_t i = random(seg->length());
      targets[i] = HsbColor{static_cast<float>(random(1000)/1000.0), 1.0, 1.0};
    }
    seg->send();
  }
private:
  RgbColor *targets;
};
int cmd_twinkle(int argc, char **argv) {
  new TwinkleTask();
  return 0;
}

void initialize_commands() {
  add_command("print_args", cmd_print_args);
  add_command("tasks", cmd_tasks);
  add_command("kill", cmd_kill);
  add_command("exit", cmd_exit); add_command("quit", cmd_exit);
  add_command("reset", cmd_reset);

  add_command("clear", cmd_clear);
  add_command("stop", cmd_stop);
  add_command("rgb", cmd_rgb);
  add_command("rainbow", cmd_rainbow);
  add_command("twinkle", cmd_twinkle);
}
