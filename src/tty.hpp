#pragma once

#include <Stream.h>
#include <HardwareSerial.h>
#include <WiFiClient.h>
#include <cstdint>
#include <vector>
#include <utility>

class TTY : public Stream {
public:
  virtual bool connected() = 0;
  virtual void close() = 0;
  virtual void flush() = 0;
};

class StreamTTY : public TTY {
public:
  StreamTTY(Stream *stream) : _stream(stream), _open(true) {
  }
  bool connected() override {
    return _open;
  }
  void close() override {
    _open = false;
  }
  void flush() override {
    _stream->flush();
  }
  size_t write(uint8_t c) override {
    return _stream->write(c);
  }
  size_t write(const uint8_t *buffer, size_t size) override {
    return _stream->write(buffer, size);
  }
  int available() override {
    return _stream->available();
  }
  int read() override {
    return _stream->read();
  }
  int peek() override {
    return _stream->peek();
  }

private:
  Stream *_stream;
  bool _open;
};

/**
   Communicates over the WiFiClient as a telnet server
 */
class WiFiClientTTY : public TTY {
public:
  WiFiClientTTY(WiFiClient client);

  bool connected() override;
  void close() override;
  void flush() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  int available() override;
  int read() override;
  int peek() override;

private:
  WiFiClient _client;

  void handleTelnet();
  uint8_t telnetState;
  uint8_t telnetCode;
  /**
     list of pairs of {option,do/dont/will/wont} from our point of view.
   */
  std::vector<std::pair<uint8_t, uint8_t>> _negotiations;

  void recvDo(uint8_t code);
  void recvDont(uint8_t code);
  void recvWill(uint8_t code);
  void recvWont(uint8_t code);

  void sendDo(uint8_t code);
  void sendDont(uint8_t code);
  void sendWill(uint8_t code);
  void sendWont(uint8_t code);

  int _read();
  int _peek();

  void dumpNegotiations();

  /**
     Gives the size of a buffer needed to escape the string for telnet.
   */
  static size_t telnet_buffer_size(const uint8_t *buffer, size_t size);

  uint8_t free_buffer[128];
};
