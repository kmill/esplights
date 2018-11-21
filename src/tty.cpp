#include "tty.hpp"
#include <Stream.h>
#include <cstdint>

enum { TSTATE_START, TSTATE_IAC, TSTATE_READ, TSTATE_BUFFER, TSTATE_WILL, TSTATE_WONT, TSTATE_DO, TSTATE_DONT, TSTATE_SB, TSTATE_EOL };

#define TELNET_IAC 255
#define TELNET_SE 240
#define TELNET_NOP 241
#define TELNET_DATA_MARK 242
#define TELNET_BREAK 243
#define TELNET_INTERRUPT 244
#define TELNET_ABORT_OUTPUT 245
#define TELNET_AYT 246
#define TELNET_ERASE_CHAR 247
#define TELNET_ERASE_LINE 248
#define TELNET_GO_AHEAD 249
#define TELNET_SB 250
#define TELNET_WILL 251
#define TELNET_WONT 252
#define TELNET_DO 253
#define TELNET_DONT 254

#define TELNET_CR 13
#define TELNET_LF 10

WiFiClientTTY::WiFiClientTTY(WiFiClient client) :
  _client(client),
  telnetState(TSTATE_START)
{
  client.setNoDelay(true);

  // TODO handle negotiation properly
  // echo
  sendWill(1);
  sendDont(1);
  // suppress go ahead
  sendDo(3);
}

bool WiFiClientTTY::connected() {
  return _client.connected();
}
void WiFiClientTTY::close() {
  _client.stop();
}
void WiFiClientTTY::flush() {
  _client.flush();
}
size_t WiFiClientTTY::write(uint8_t c) {
  if (c == '\n') {
    _client.write(TELNET_CR);
    return _client.write(TELNET_LF);
  }
  if (c == '\r') {
    _client.write(TELNET_CR);
    return _client.write((uint8_t)0);
  }
  if (c == TELNET_IAC) {
    _client.write(TELNET_IAC);
    // return value not quite correct in escaped case
  }
  return _client.write(c);
}
size_t WiFiClientTTY::write(const uint8_t *buffer, size_t size) {
  for (size_t i = 0; i < size; i++) {
    if (buffer[i] == TELNET_IAC || buffer[i] == TELNET_CR || buffer[i] == TELNET_LF) {
      size_t out = 0;
      for (size_t j = 0;
           j < size && write(buffer[j]);
           j++, out++);
      return out;
    }
  }
  return _client.write(buffer, size);
}
int WiFiClientTTY::available() {
  handleTelnet();
  return telnetState == TSTATE_BUFFER || (telnetState == TSTATE_READ && _client.available());
}

int WiFiClientTTY::_read() {
  int c = _client.read();
  //Serial.printf("_client.read() == %d\n", c);
  return c;
}
int WiFiClientTTY::_peek() {
  int c = _client.peek();
  return c;
}

int WiFiClientTTY::read() {
  handleTelnet();
  switch (telnetState) {
  case TSTATE_READ:
    telnetState = TSTATE_START;
    return _read();
  case TSTATE_BUFFER:
    telnetState = TSTATE_START;
    return telnetCode;
  default:
    return -1;
  }
}
int WiFiClientTTY::peek() {
  handleTelnet();
  switch (telnetState) {
  case TSTATE_READ:
    return _peek();
  case TSTATE_BUFFER:
    return telnetCode;
  default:
    return -1;
  }
}

void WiFiClientTTY::handleTelnet() {
  while (_client.available() > 0) {
    switch (telnetState) {

    case TSTATE_START:
      if (_peek() == TELNET_IAC) {
        _read();
        telnetState = TSTATE_IAC;
      } else if (_peek() == TELNET_CR) {
        _read();
        telnetState = TSTATE_EOL;
      } else {
        telnetState = TSTATE_READ;
      }
      break;

    case TSTATE_EOL:
      if (_peek() == 0) {
        _read();
        telnetCode = '\r';
        telnetState = TSTATE_BUFFER;
      } else if (_peek() == TELNET_LF) {
        _read();
        telnetCode = '\n';
        telnetState = TSTATE_BUFFER;
      } else { // ill-formed
        telnetCode = '\r';
        telnetState = TSTATE_BUFFER;
      }
      break;

    case TSTATE_IAC:
      //Serial.printf("IAC %d\n", _peek());
      switch (_peek()) {
      case TELNET_SE:
      case TELNET_NOP:
      case TELNET_DATA_MARK:
      case TELNET_ABORT_OUTPUT:
      case TELNET_GO_AHEAD:
      case TELNET_BREAK:
        _read();
        telnetState = TSTATE_START;
        break;

      case TELNET_INTERRUPT:
        _read();
        telnetCode = 3; // ^C
        telnetState = TSTATE_BUFFER;
        break;

      case TELNET_AYT:
        _read();
        _client.write(7); // BEL
        telnetState = TSTATE_START;
        break;

      case TELNET_ERASE_CHAR:
        _read();
        telnetCode = 8; // ^H, backspace
        telnetState = TSTATE_BUFFER;
        break;

      case TELNET_ERASE_LINE:
        _read();
        telnetCode = 21; // ^U, NAK
        telnetState = TSTATE_BUFFER;
        break;

      case TELNET_WILL:
        _read();
        telnetState = TSTATE_WILL;
        break;

      case TELNET_WONT:
        _read();
        telnetState = TSTATE_WONT;
        break;

      case TELNET_DO:
        _read();
        telnetState = TSTATE_DO;
        break;

      case TELNET_DONT:
        _read();
        telnetState = TSTATE_DONT;
        break;

      case TELNET_SB:
        _read();
        telnetState = TSTATE_SB;
        break;

      case TELNET_IAC:
        telnetState = TSTATE_READ;
        break;

      default:
        Serial.printf("telnet: Unknown IAC %d\n", _peek());
        _read();
        telnetState = TSTATE_START;
        break;
      }
      break;

    case TSTATE_WILL:
      telnetCode = _read();
      Serial.printf("telnet: WILL %d\n", telnetCode);
      sendDont(telnetCode);
      telnetState = TSTATE_START;
      break;

    case TSTATE_WONT:
      telnetCode = _read();
      Serial.printf("telnet: WONT %d\n", telnetCode);
      sendDont(telnetCode);
      telnetState = TSTATE_START;
      break;

    case TSTATE_DO:
      telnetCode = _read();
      Serial.printf("telnet: DO %d\n", telnetCode);
      telnetState = TSTATE_START;
      break;

    case TSTATE_DONT:
      telnetCode = _read();
      Serial.printf("telnet: DONT %d\n", telnetCode);
      telnetState = TSTATE_START;
      break;

    case TSTATE_SB:
      telnetCode = _read();
      Serial.printf("telnet: SB %d\n", telnetCode);
      telnetState = TSTATE_START;
      break;

    case TSTATE_READ:
    case TSTATE_BUFFER:
      return;

    default:
      Serial.printf("telnet: Missing telnet state! %d\n", telnetState);
      return;
    }
  }
}

void WiFiClientTTY::sendDo(uint8_t code) {
  uint8_t buffer[3];
  buffer[0] = TELNET_IAC;
  buffer[1] = TELNET_DO;
  buffer[2] = code;
  _client.write(buffer, 3);
}

void WiFiClientTTY::sendDont(uint8_t code) {
  uint8_t buffer[3];
  buffer[0] = TELNET_IAC;
  buffer[1] = TELNET_DONT;
  buffer[2] = code;
  _client.write(buffer, 3);
}

void WiFiClientTTY::sendWill(uint8_t code) {
  uint8_t buffer[3];
  buffer[0] = TELNET_IAC;
  buffer[1] = TELNET_WILL;
  buffer[2] = code;
  _client.write(buffer, 3);
}

void WiFiClientTTY::sendWont(uint8_t code) {
  uint8_t buffer[3];
  buffer[0] = TELNET_IAC;
  buffer[1] = TELNET_WONT;
  buffer[2] = code;
  _client.write(buffer, 3);
}
