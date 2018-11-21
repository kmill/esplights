#pragma once
#include "task.hpp"
#include <memory>

void initializeTelnetSpawner(Task *(*spawner)(std::shared_ptr<TTY>));
