# esplights

This is an "operating system" for the ESP8266 to control WS2812B-based
lighting.  It includes

* a task system with support for scheduling tasks that wake up on an interval.
* a command line that one can connect to over serial or Telnet.
* mDNS support

In the future it will have

* an HTTP server
* an OSC server

There also will potentially be cooperative multitasking using `cont_t`
and `yield()` (see `core_esp8266_main.cpp` from the ESP8266 Arduino
library).  However, it will take some care to make sure there are no
resource leaks when killing such a task.

## Getting started

These instructions will get you set up to develop and upload the software to the device.

### Prerequisites

Git clone `https://github.com/kmill/esplights.git`.

Install [`platformio`](https://platformio.org/).  Then in the project directory,

```
platformio lib install NeoPixelBus
```

### Compiling

Copy `src/config.hpp.example` to `src/config.hpp` and modify the file for the WiFi and OTA settings.

To compile,
```
platformio run
```

### Deployment

```
platformio run -t upload
```

Or, with a particular environment as defined in `platformio.ini`,
```
platformio run -e envname -t upload
```

### Telnet usage

Using mDNS, run `telnet mdnsname.local` to connect to the onboard
Telnet server.  Type `help` to get a list of commands.

### Wiring

We use the ESP8266's I2S "DMA" mode for interfacing with the WS2812B
LED strip.  The method uses the GPIO3/RXD0 pin for communication.  On
the NodeMCU v1.0, this is the pin labeled RX.

**Note:** Once the lights are initialized, the USB serial receive is
deactivated, since it uses the same pin.

## Built with

* [`platformio`](https://platformio.org/)
* [`NeoPixelBus`](https://github.com/Makuna/NeoPixelBus)
* [ESP8266 core for Arduino](https://github.com/esp8266/Arduino)

## Authors

* **Kyle Miller** - [kmill](https://github.com/kmill)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.
