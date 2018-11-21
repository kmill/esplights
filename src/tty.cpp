#include "tty.hpp"
#include <Stream.h>
#include <cstdint>

// TELNET rfc https://tools.ietf.org/html/rfc854
// option rfcs https://www.iana.org/assignments/telnet-options/telnet-options.xhtml

enum { TSTATE_START, TSTATE_IAC, TSTATE_READ, TSTATE_BUFFER,
       TSTATE_WILL, TSTATE_WONT, TSTATE_DO, TSTATE_DONT,
       TSTATE_SB, TSTATE_EOL };

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

#define TELNET_OPT_ECHO 1
#define TELNET_OPT_SGA 3

WiFiClientTTY::WiFiClientTTY(WiFiClient client)
  : _client(client),
    telnetState(TSTATE_START),
    _negotiations()
{
  client.setNoDelay(true);

  sendWill(TELNET_OPT_ECHO);
  sendDont(TELNET_OPT_ECHO);
  sendWill(TELNET_OPT_SGA);
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
  uint8_t buffer[2];
  switch(c) {
  case '\n':
    buffer[0] = TELNET_CR;
    buffer[1] = TELNET_LF;
    return _client.write(buffer, 2) > 0;
  case '\r':
    buffer[0] = TELNET_CR;
    buffer[1] = 0;
    return _client.write(buffer, 2) > 0;
  case TELNET_IAC:
    buffer[0] = TELNET_IAC;
    buffer[1] = TELNET_IAC;
    return _client.write(buffer, 2) > 0;
  default:
    return _client.write(c);
  }
}
size_t WiFiClientTTY::write(const uint8_t *buffer, size_t size) {
  size_t tsize = telnet_buffer_size(buffer, size);

  if (tsize == size) {
    return _client.write(buffer, size);
  }

  uint8_t *buffer2 = nullptr;
  if (tsize <= sizeof(free_buffer)) {
    buffer2 = free_buffer;
  } else {
    buffer2 = static_cast<uint8_t *>(malloc(tsize));
  }
  if (!buffer2) {
    // fallback
    for (size_t i = 0; i < size; i++) {
      if (!write(buffer[i])) {
        return i;
      }
    }
    return size;
  }
  size_t j = 0; // index into buffer2
  for (size_t i = 0; i < size; i++) {
    switch(buffer[i]) {
    case '\n':
      buffer2[j++] = TELNET_CR;
      buffer2[j++] = TELNET_LF;
      break;
    case '\r':
      buffer2[j++] = TELNET_CR;
      buffer2[j++] = 0;
      break;
    case TELNET_IAC:
      buffer2[j++] = TELNET_IAC;
      buffer2[j++] = TELNET_IAC;
      break;
    default:
      buffer2[j++] = buffer[i];
      break;
    }
  }
  size_t twritten = _client.write(buffer2, tsize);
  if (buffer2 != free_buffer) {
    free(buffer2);
  }
  if (twritten < tsize) {
    // estimate how many bytes were written. partially written escapes count
    size_t ts = 0;
    size_t written = 0;
    for (; ts < twritten; written++) {
      switch(buffer[written]) {
      case '\n':
      case '\r':
      case TELNET_IAC:
        ts += 2;
        break;
      default:
        ts += 1;
        break;
      }
    }
    return written;
  }
  return size;
}

size_t WiFiClientTTY::telnet_buffer_size(const uint8_t *buffer, size_t size) {
  size_t tsize = size;
  for (size_t i = 0; i < size; i++) {
    switch (buffer[i]) {
    case '\n':
    case '\r':
    case TELNET_IAC:
      tsize += 1;
      break;
    default:
      break;
    }
  }
  return tsize;
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
        dumpNegotiations(); // A hack: using AYT to show current status over serial.
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
      recvWill(telnetCode);
      telnetState = TSTATE_START;
      break;

    case TSTATE_WONT:
      telnetCode = _read();
      recvWont(telnetCode);
      telnetState = TSTATE_START;
      break;

    case TSTATE_DO:
      telnetCode = _read();
      recvDo(telnetCode);
      telnetState = TSTATE_START;
      break;

    case TSTATE_DONT:
      telnetCode = _read();
      recvDont(telnetCode);
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

void WiFiClientTTY::recvDo(uint8_t code) {
  Serial.printf("telnet: DO %d\n", code);
  if (code == TELNET_OPT_ECHO) {
    sendWill(TELNET_OPT_ECHO);
  } else if (code == TELNET_OPT_SGA) {
    sendWill(TELNET_OPT_SGA);
  } else {
    sendWont(code);
  }
}
void WiFiClientTTY::recvDont(uint8_t code) {
  Serial.printf("telnet: DONT %d\n", code);
  sendWont(code);
}
void WiFiClientTTY::recvWill(uint8_t code) {
  Serial.printf("telnet: WILL %d\n", code);
  if (code == TELNET_OPT_SGA) {
    sendDo(TELNET_OPT_SGA);
  } else {
    sendDont(code);
  }
}
void WiFiClientTTY::recvWont(uint8_t code) {
  Serial.printf("telnet: WONT %d\n", code);
  sendDont(code);
}

void WiFiClientTTY::sendDo(uint8_t code) {
  for (size_t i = 0; i < _negotiations.size(); i++) {
    auto pair = _negotiations.at(i);
    if (pair.first == code) {
      if (pair.second == TELNET_DO) {
        return;
      } else if (pair.second == TELNET_DONT) {
        return;
      }
    }
  }
  _negotiations.push_back(std::make_pair(code, TELNET_DO));

  uint8_t buffer[3];
  buffer[0] = TELNET_IAC;
  buffer[1] = TELNET_DO;
  buffer[2] = code;
  _client.write(buffer, 3);

  Serial.printf("telnet: sent DO %d\n", code);
}

void WiFiClientTTY::sendDont(uint8_t code) {
  for (size_t i = 0; i < _negotiations.size(); i++) {
    auto pair = _negotiations.at(i);
    if (pair.first == code) {
      if (pair.second == TELNET_DO) {
        pair.second = TELNET_DONT;
        goto skip;
      } else if (pair.second == TELNET_DONT) {
        return;
      }
    }
  }
  _negotiations.push_back(std::make_pair(code, TELNET_DONT));
 skip:
  uint8_t buffer[3];
  buffer[0] = TELNET_IAC;
  buffer[1] = TELNET_DONT;
  buffer[2] = code;
  _client.write(buffer, 3);

  Serial.printf("telnet: sent DONT %d\n", code);
}

void WiFiClientTTY::sendWill(uint8_t code) {
  for (size_t i = 0; i < _negotiations.size(); i++) {
    auto pair = _negotiations.at(i);
    if (pair.first == code) {
      if (pair.second == TELNET_WILL) {
        return;
      } else if (pair.second == TELNET_WONT) {
        return;
      }
    }
  }
  _negotiations.push_back(std::make_pair(code, TELNET_WILL));

  uint8_t buffer[3];
  buffer[0] = TELNET_IAC;
  buffer[1] = TELNET_WILL;
  buffer[2] = code;
  _client.write(buffer, 3);

  Serial.printf("telnet: sent WILL %d\n", code);
}

void WiFiClientTTY::sendWont(uint8_t code) {
  for (size_t i = 0; i < _negotiations.size(); i++) {
    auto pair = _negotiations.at(i);
    if (pair.first == code) {
      if (pair.second == TELNET_WILL) {
        pair.second = TELNET_WONT;
        goto skip;
      } else if (pair.second == TELNET_WONT) {
        return;
      }
    }
  }
  _negotiations.push_back(std::make_pair(code, TELNET_WONT));
 skip:
  uint8_t buffer[3];
  buffer[0] = TELNET_IAC;
  buffer[1] = TELNET_WONT;
  buffer[2] = code;
  _client.write(buffer, 3);

  Serial.printf("telnet: sent WONT %d\n", code);
}

void WiFiClientTTY::dumpNegotiations() {
  Serial.println("-------");
  for (auto p = _negotiations.begin(); p != _negotiations.end(); ++p) {
    Serial.printf("%d ", p->first);
    switch (p->second) {
    case TELNET_WILL: Serial.printf("WILL"); break;
    case TELNET_WONT: Serial.printf("WONT"); break;
    case TELNET_DO: Serial.printf("DO"); break;
    case TELNET_DONT: Serial.printf("DONT"); break;
    }
    Serial.println();
  }
  Serial.println("-------");
}
