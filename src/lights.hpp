#pragma once

#include <NeoPixelBus.h>
#include <memory>

class LEDSegment;

class LEDSystem {
public:
  LEDSystem(int pixel_count);

  std::shared_ptr<LEDSegment> requestSegment();

private:
  size_t _pixel_count;
  NeoEsp8266Dma800KbpsMethod _dma;

  std::shared_ptr<LEDSegment> _cur_seg;

  void send(const LEDSegment *seg, bool wait);
  
  friend LEDSegment;
};

class LEDSegment {
public:
  ~LEDSegment() {
    delete[] _buffer;
  }

  /**
     The light system is able to deactivate the LEDSegment, signaling
     to a user of this object that they no longer have control of the
     lights.
   */
  bool isActive() const {
    return _active;
  }
  operator bool() const {
    return _active;
  }

  /**
     Sends the segment to the LED system, if active.  If 'wait' is
     false, then allow the LED system to skip this frame if it hasn't
     yet finished with the previous frame.
   */
  void send(bool wait=false) {
    if (_active) {
      _led_system->send(this, wait);
    }
  }

  /**
     Get an editable RGB buffer of the lights.  Pixels come
     sequentially in trios of bytes, each in red-green-blue order.
   */
  uint8_t *getBuffer() const {
    return _buffer;
  }

  /**
     Set the color of one of the pixels.
  */
  void set(size_t idx, NeoRgbFeature::ColorObject color) {
    if (idx < _pixel_count) {
      NeoRgbFeature::applyPixelColor(_buffer, idx, color);
    }
  }
  /**
     Get the color of one of the pixels. If outside bounds, returns black.
  */
  NeoRgbFeature::ColorObject get(size_t idx) {
    if (idx < _pixel_count) {
      return NeoRgbFeature::retrievePixelColor(_buffer, idx);
    } else {
      return 0;
    }
  }
  /**
     Get the number of pixels in the segment.
  */
  size_t length() const {
    return _pixel_count;
  }

  /**
     Clear the segment to black.
  */
  void clear() {
    memset(_buffer, 0, 3*_pixel_count);
  }

private:
  LEDSegment(size_t pixel_count, LEDSystem *led_system)
    : _active(true),
      _pixel_count(pixel_count),
      _led_system(led_system)
  {
   _buffer = new uint8_t[3*pixel_count]();
  }
  void deactivate() {
    _active = false;
  }


  bool _active;
  size_t _pixel_count;
  uint8_t *_buffer;
  LEDSystem *_led_system;

  friend LEDSystem;
};

void initialize_lights();
/**
   Get a new LEDSegment object.
 */
std::shared_ptr<LEDSegment> requestLEDSegment();
