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
          cur_tty->printf("; scheduled for %u", t->get_scheduled());
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
    setBackground(true);
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

static int cmd_hsb(int argc, char **argv) {
  if (argc != 4) {
    cur_tty->printf("%s h s b", argv[0]);
    return 1;
  }
  float h, s, b;
  h = clamp(atof(argv[1]), 0.0, 1.0);
  s = clamp(atof(argv[2]), 0.0, 1.0);
  b = clamp(atof(argv[3]), 0.0, 1.0);
  auto seg = requestLEDSegment();
  for (size_t i = 0; i < seg->length(); i++) {
    seg->set(i, HsbColor{h, s, b});
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
  TwinkleTask(int upspeed = 4, int downspeed = 2)
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

class FireTask : public LightTask {
public:
  FireTask(size_t rows, float decay, float heat, float loss, float keep, float fps)
    : LightTask("fire", fps)
  {
    width = 2+seg->length();
    this->rows = rows;
    fire = new uint8_t[width*rows]();

    _decay = static_cast<unsigned int>(decay * 256);
    _heat = static_cast<unsigned int>(heat * 256);
    _loss = static_cast<unsigned int>(256 * loss);
    _keep = static_cast<unsigned int>(256 * keep);
  }
  ~FireTask() {
    delete[] fire;
  }
  void update() override {
    for (size_t j = 1; j < width-1; j++) {
      fire[width*(rows-1) + j] = fire[width*(rows-1) + j] * _decay / 256;
      if (static_cast<unsigned int>(random(256)) <= _heat) {
        fire[width*(rows-1) + j] = 100+random(256-100);
      }
    }
    for (size_t i = 0; i+1 < rows; i++) {
      for (size_t j = 1; j < width-1; j++) {
        unsigned int sum = 0;
        sum += fire[width*(i+1) + j-1];
        sum += fire[width*(i+1) + j];
        sum += fire[width*(i+1) + j+1];
        if (i+2 < rows) {
          sum += fire[width*(i+2) + j];
        }
        sum *= (256-_keep);
        sum += _keep * fire[width*i + j];
        fire[width*i + j] = sum / _loss;
      }
    }
    for (size_t j = 1; j < width-1; j++) {
      seg->set(j-1, palette(fire[j]));
    }
    seg->send();
  }
  RgbColor palette(uint8_t value) {
    double f = value/255.0;
    return HsbColor(f*(0.1-0.015) + 0.015, 1.0, std::min(1.0, f*2.0));
  }
private:
  size_t width;
  size_t rows;
  uint8_t *fire; // i*width + j for row i column j.

  unsigned int _decay;
  unsigned int _heat;
  unsigned int _loss;
  unsigned int _keep;
};

static int cmd_fire(int argc, char **argv) {
  size_t rows = 10;
  float decay = 0.9;
  float heat = 0.1;
  float loss = 4.0;
  float keep = 0.2;
  float fps = 22;
  for (char **arg = &argv[1]; *arg; ) {
    if (strcmp(*arg, "-r") == 0) {
      arg++;
      rows = static_cast<size_t>(std::max(2, std::min(25, atoi(*arg++))));
    } else if (strcmp(*arg, "-d") == 0) {
      arg++;
      decay = atof(*arg++);
    } else if (strcmp(*arg, "-e") == 0) {
      arg++;
      heat = atof(*arg++);
    } else if (strcmp(*arg, "-l") == 0) {
      arg++;
      loss = atof(*arg++);
    } else if (strcmp(*arg, "-k") == 0) {
      arg++;
      keep = atof(*arg++);
    } else if (strcmp(*arg, "-f") == 0) {
      arg++;
      fps = atof(*arg++);
    } else {
      cur_tty->printf("%s [-r rows] [-d decay] [-e heat] [-l loss] [-k keep] [-f fps]\n", argv[0]);
      cur_tty->printf("rows is 2-25\ndecay is 0.0-1.0\nheat is 0.0-1.0\nloss is 1.0 or more\nkeep is 0.0-1.0\n");
      return 1;
    }
  }
  new FireTask(rows, decay, heat, loss, keep, fps);
  return 0;
}

class TwfireTask : public LightTask {
public:
  TwfireTask()
    : LightTask("twfire")
  {
    width = seg->length();
    rows = 10;
    upspeed = 4*256;
    downspeed = 2*256;
    keep = static_cast<int>(256*0.8);

    fire = new uint16_t[(width+2)*3*rows]();
  }
  ~TwfireTask() {
    delete[] fire;
  }
  void update() override {
    uint16_t r = random(100) + random(100) + random(100);
    if (r < 120) {
      uint16_t i = random(3*width);
      pos(0, i) = 65535;
    }
    for (size_t j = 0; j < 3*width; j++) {
      uint16_t &target = pos(0, j);
      uint16_t &current = pos(1, j);
      if (current < target) {
        current = iclamp(static_cast<int>(current) + upspeed, 0, target);
      } else {
        current = target = iclamp(static_cast<int>(current) - downspeed, 0, 65535);
      }
    }
    for (size_t i = 2; i < rows; i++) {
      for (size_t j = 0; j < 3*width; j++) {
        unsigned int sum = 0;
        sum += pos(i-1, j-3);
        sum += pos(i-1, j);
        sum += pos(i-1, j+3);
        sum += pos(i-2, j);
        sum *= (256-keep);
        sum += 4*keep*pos(i,j);
        pos(i, j) = iclamp(sum / (4*256), 0, 65535);
      }
    }
    for (size_t j = 0; j < width; j++) {
      seg->set(j, RgbColor{
          static_cast<uint8_t>(pos(rows-1,3*j)/256),
            static_cast<uint8_t>(pos(rows-1,3*j+1)/256),
            static_cast<uint8_t>(pos(rows-1,3*j+2)/256)});
    }
    seg->send();
  }
protected:
  inline uint16_t& pos(int row, int col) {
    return fire[3*(width+2)*row + 3 + col];
  }
private:
  size_t width;
  size_t rows;
  uint16_t *fire;
  int upspeed;
  int downspeed;
  int keep;
};
int cmd_twfire(int argc, char **argv) {
  new TwfireTask();
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
  add_command("hsb", cmd_hsb);
  add_command("rainbow", cmd_rainbow);
  add_command("twinkle", cmd_twinkle);
  add_command("fire", cmd_fire);
  add_command("twfire", cmd_twfire);
}
