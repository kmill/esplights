#include "ota.hpp"
#include <ArduinoOTA.h>
#include "task.hpp"

class OTATask : public Task {
public:
  OTATask() : Task("ota") {
  }
  void run() override {
    ArduinoOTA.handle();
  }
};

void initializeOTA(const char *hostname, const char *password) {
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd OTA");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA progress: %u%%\r", (int)(progress / (total / 100.0)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error %u:", error);
    switch (error) {
      case OTA_AUTH_ERROR: Serial.println("OTA_AUTH_ERROR"); break;
      case OTA_BEGIN_ERROR: Serial.println("OTA_BEGIN_ERROR"); break;
      case OTA_CONNECT_ERROR: Serial.println("OTA_CONNECT_ERROR"); break;
      case OTA_RECEIVE_ERROR: Serial.println("OTA_RECEIVE_ERROR"); break;
      case OTA_END_ERROR: Serial.println("OTA_END_ERROR"); break;
    }
  });
  ArduinoOTA.begin();

  Task *t = new OTATask();
  t->setActive(true);

  Serial.println("OTA ready");
}
