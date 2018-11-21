#include "telnet.hpp"
#include "task.hpp"
#include <WiFiServer.h>

class TelnetSpawnerTask : public Task {
public:
  TelnetSpawnerTask(const char *name, Task *(*spawner)(std::shared_ptr<TTY>)) :
    Task(name),
    telnetServer(23),
    _spawner(spawner)
  {
    telnetServer.begin();
    telnetServer.setNoDelay(true);
  }
  void run() override {
    if (telnetServer.hasClient()) {
      tty->println("Telnet client connected.");
      std::shared_ptr<TTY> serialTTY(new WiFiClientTTY(telnetServer.available()));
      Task *t = _spawner(serialTTY);
      t->setActive(true);
    }
  }
private:
  WiFiServer telnetServer;
  Task *(*_spawner)(std::shared_ptr<TTY>);
};

void initializeTelnetSpawner(Task *(*spawner)(std::shared_ptr<TTY>)) {
  Task *t = new TelnetSpawnerTask("telnet-spawner", spawner);
  t->setActive(true);
  t->setWaits(false);
}
