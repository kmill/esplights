#pragma once

#include "task.hpp"
#include <cstdint>
#include "ntp.hpp"

#define MAX_INPUT_LINE  128
#define MAX_CMD_ARGS 16

class TerminalTask : public Task {
public:
  TerminalTask(const char *name, std::shared_ptr<TTY> tty);
  ~TerminalTask();
  void run() override;
private:
  void showPrompt();
  char line_buf[MAX_INPUT_LINE+1];
  int line_buf_idx = 0;
  char *parsed_argv[MAX_CMD_ARGS];
  int parsed_argc;
  void parse_line();
  uint8_t last_char;
};

typedef int (Command)(int argc, char **argv);

void add_command(const char *name, Command *c);
Command *lookup_command(const char *name);
