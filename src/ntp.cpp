#include "ntp.hpp"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "task.hpp"

// TODO use <time.h>

#define NTP_PACKET_SIZE 48

static WiFiUDP ntpUDP;
static const char *ntpServerName = "time.nist.gov";
static uint32_t lastNtpMillis = 0;
static uint32_t ntpUnixTime = 0;
static uint32_t ntpMillisOffset = 0;
static IPAddress ntpIP;

class NTPSendTask : public Task {
public:
  NTPSendTask() : Task("NTPSend") {
    setInterval((uint32_t)20*60*1000*1000);
    setActive(true);
    sendNTPRequest();
  }
  void run() override {
    sendNTPRequest();
  }
  void sendNTPRequest() {
    lastNtpMillis = millis();
    tty->print("Sending NTP request to "); tty->println(ntpIP);
    byte ntpBuffer[NTP_PACKET_SIZE];
    memset(ntpBuffer, 0, NTP_PACKET_SIZE);
    ntpBuffer[0] = 0b11100011; // LI, Version, Mode
    ntpUDP.beginPacket(ntpIP, 123);
    ntpUDP.write(ntpBuffer, NTP_PACKET_SIZE);
    ntpUDP.endPacket();
  
    WiFi.hostByName(ntpServerName, ntpIP);
  }
};

class NTPReceiveTask : public Task {
public:
  NTPReceiveTask() : Task("NTPReceive") {
    setActive(true);
    new NTPSendTask();
  }
  void run() override {
    if (ntpUDP.parsePacket() != 0) {
      byte ntpBuffer[NTP_PACKET_SIZE];
      if (NTP_PACKET_SIZE == ntpUDP.read(ntpBuffer, NTP_PACKET_SIZE)) {
        uint32_t ntpTime = (ntpBuffer[40] << 24) | (ntpBuffer[41] << 16) | (ntpBuffer[42] << 8) | ntpBuffer[43];
        ntpUnixTime = ntpTime - 2208988800UL;
        ntpMillisOffset = millis();
        tty->printf("ntp unix time is %lu\n", ntpUnixTime);
      } else {
        tty->printf("recieved small ntp packet");
      }
    }
  }
};

void initializeNTP() {
  WiFi.hostByName(ntpServerName, ntpIP);
  ntpUDP.begin(123);
  new NTPReceiveTask();
}

uint32_t unixTime() {
  return ntpUnixTime + (millis() - ntpMillisOffset)/1000;
}
