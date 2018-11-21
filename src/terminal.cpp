#include "terminal.hpp"
#include<vector>
#include<cstring>
#include<time.h>

TerminalTask::TerminalTask(const char *name, std::shared_ptr<TTY> tty) : Task(name) {
  setTTY(tty);
  last_char = 0;
  tty->print("espled (core "); tty->print(ESP.getCoreVersion());
  tty->print("; esp sdk "); tty->print(ESP.getSdkVersion());
  tty->print("; chip ID "); tty->print(ESP.getChipId());
  tty->print(")\n");
  showPrompt();
}

TerminalTask::~TerminalTask() {
  Serial.println("Terminal closed.");
}

void TerminalTask::showPrompt() {
  uint32_t t = time(nullptr);
  int s = t % 60; int _m = t / 60;
  int m = _m % 60; int _h = _m / 60;
  int h = _h % 24;
  tty->printf("%2u:%02u:%02u> ", h, m, s);
}

void TerminalTask::run() {
  while (tty->available() > 0) {
    bool process = false;

    bool was_cr = last_char == '\r';
    char c = tty->read();
    last_char = c;

    switch (c) {

    case '\n':
      if (was_cr) {
        break;
      }
      /* fallthrough */
    case '\r':
      process = true;
      /* fallthrough */
    case 3: // ^C
      if (process && line_buf_idx > 0) {
        line_buf[line_buf_idx++] = '\0';
        tty->println();
        parse_line();
      } else {
        tty->println();
      }
      showPrompt();
      line_buf_idx = 0;
      break;

    case '\b':
    case 127: // backspace
      if (line_buf_idx > 0) {
        tty->write("\b \b", 3);
        line_buf_idx--;
      }
      break;
    default:
      if (0x20 <= c && c <= 0x7E && line_buf_idx < MAX_INPUT_LINE) {
        line_buf[line_buf_idx++] = c;
        tty->write(c);
      }
    }
  }
}

void TerminalTask::parse_line() {
  int i;
  parsed_argv[0] = strtok(line_buf, " ");
  for (i = 0; i + 1 < MAX_CMD_ARGS && parsed_argv[i] != nullptr; i++) {
    parsed_argv[i + 1] = strtok(nullptr, " ");
  }
  parsed_argc = i;

  Command *cmd = lookup_command(parsed_argv[0]);
  if (cmd == nullptr) {
    tty->printf("command not found: %s\n", parsed_argv[0]);
  } else {
    int err = cmd(parsed_argc, parsed_argv);
    if (err) {
      tty->printf("(error code %d)\n", err);
    }
  }
}


extern "C" {
  typedef struct {
    const char *name;
    Command *command;
  } Cmd;
}

int help_command(int argc, char **argv);

static std::vector<Cmd> commands{
  {"help", help_command}
};

void add_command(const char *name, Command *c) {
  for (auto it = commands.begin(); it != commands.end(); ++it) {
    if (strcmp(name, it->name) == 0) {
      it->command = c;
      return;
    }
  }
  commands.push_back({name, c});
}
Command *lookup_command(const char *name) {
  for (auto it = commands.begin(); it != commands.end(); ++it) {
    if (strcmp(name, it->name) == 0) {
      return it->command;
    }
  }
  return nullptr;
}


int help_command(int argc, char **argv) {
  cur_tty->printf("Commands:\n");
  for (auto it = commands.begin(); it != commands.end(); ++it) {
    cur_tty->printf("  %s\n", it->name);
  }
  return 0;
}
