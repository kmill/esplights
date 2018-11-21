#include "ntp.hpp"
#include <time.h>
#include "task.hpp"

void initializeNTP() {
  configTime(0, 0, "time.nist.gov", "pool.ntp.org");
}
