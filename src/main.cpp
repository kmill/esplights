#include <memory>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <FS.h>

#include <WiFiUdp.h>

#include "task.hpp"
#include "terminal.hpp"
#include "telnet.hpp"
#include "ntp.hpp"
#include "ota.hpp"
#include "lights.hpp"
#include "commands.hpp"

#include "config.hpp"

// status led
//#define LED 2

#define macro_xstr(a) macro_str(a)
#define macro_str(a) #a
const char *MDNS_HOSTNAME = macro_xstr(M_MDNS_HOSTNAME);

const int OSC_PORT = 8000;

//ESP8266WebServer httpServer(80);

class TimeSayerTask : public Task {
public:
  TimeSayerTask() : Task("time-sayer") {
    setInterval(60*1000*1000);
    setActive(true);
  }
  void run() override {
    uint32_t time = unixTime();
    tty->printf("unix time is %lu; ", time);
    int s = time % 60; int _m = time / 60;
    int m = _m % 60; int _h = _m / 60;
    int h = _h % 24;
    tty->printf("%2u:%02u:%02u\n", h, m, s);
  }
};


/*void httpHandleRoot();
  void httpHandleNotFound();*/

void setup() {
  Serial.begin(115200);
  delay(1);
  Serial.println("\nin setup()!\n");

  /// WIFI ///

  WiFi.mode(WIFI_STA);
  WiFi.begin(M_WIFI_SSID, M_WIFI_PASSWORD);
  {
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print("no wifi ("); Serial.print(++i); Serial.print(" s)\n");
    }
  }

  Serial.print("SSID: "); Serial.println(WiFi.SSID());
  Serial.print("IP address: "); Serial.print(WiFi.localIP()); Serial.println();

  /// SPIFFS ///

  SPIFFS.begin();

  /// MDNS ///

  if (!MDNS.begin(MDNS_HOSTNAME)) {
    Serial.println("Failed to set up mDNS responder.");
  } else {
    Serial.print("mDNS hostname: "); Serial.println(MDNS_HOSTNAME);
  }
  MDNS.addService("telnet", "tcp", 23);
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("osc", "udp", OSC_PORT);

  /// OTA ///

  initializeOTA(MDNS_HOSTNAME, M_OTA_PASS);

  /// HTTP ///

  /*
  httpServer.on("/", HTTP_GET, httpHandleRoot);
  httpServer.onNotFound(httpHandleNotFound);
  httpServer.begin();*/

  /// TELNET ///

  initializeTelnetSpawner([](std::shared_ptr<TTY> tty) -> Task* {
                            return new TerminalTask("telnet-terminal", tty);
                          });

  // NTP //

  initializeNTP();
  new TimeSayerTask();

  // LED STRIP //

  //  initialize_lights();

  //  strip.Begin();
  /*  for (int i = 0; i < strip.PixelCount(); i++) {
    HsbColor col(static_cast<float>(i)/strip.PixelCount(), 1.0, 0.1);
    strip.SetPixelColor(i, col);
    }*/
  //  strip.Show();

  /// OTHER STUFF ///

  //  pinMode(LED, OUTPUT);
  //  analogWrite(LED, 1023.0);
  //ledTicker.attach(0.01, ledUpdate);
 
  {
    std::shared_ptr<TTY> serialTTY(new StreamTTY(&Serial));
    Task *t = new TerminalTask("serial-terminal", serialTTY);
    t->setActive(true);
  }

 initialize_commands();
}
/*
float brightness = 0;
float amount = 2.0 / 500.0;

void ledUpdate() {
  float x = 0.4*brightness;
  analogWrite(LED, min(1023.0, floor(1024 * (1.0 - x * x))));
  brightness += amount;
  if (brightness < 0) {
    brightness = -brightness;
    amount = -amount;
  } else if (brightness > 1) {
    brightness = 1.0 - (brightness - 1.0);
    amount = -amount;
  }
  }*/




void loop() {
  Task::run_tasks(2*1000); // for 2 milliseconds

  //  httpServer.handleClient();
}


/*
void httpHandleRoot() {
  httpServer.send(200, "text/html", "<p><em>Hello</em>, world!</p>");
}

void httpHandleNotFound() {
  httpServer.send(404, "text/plain", "404: Not Found");
  }*/

