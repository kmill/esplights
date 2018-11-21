#include "lights.hpp"

#include <NeoPixelBus.h>

#define LED_COUNT 240

LEDSystem *led_system = nullptr;

typedef NeoGrbFeature TheColorFeature;

void initialize_lights() {
  led_system = new LEDSystem(LED_COUNT);
}

std::shared_ptr<LEDSegment> requestLEDSegment() {
  if (!led_system) {
    initialize_lights();
  }
  return led_system->requestSegment();
}

LEDSystem::LEDSystem(int pixel_count)
  : _pixel_count(pixel_count),
    _dma(pixel_count, 3),
    _cur_seg(nullptr)
{
  _dma.Initialize();
}

std::shared_ptr<LEDSegment> LEDSystem::requestSegment() {
  if (_cur_seg) {
    _cur_seg->deactivate();
  }
  _cur_seg.reset(new LEDSegment(_pixel_count, this));
  return _cur_seg;
}

void LEDSystem::send(const LEDSegment *seg, bool wait) {
  if (seg->isActive() && (wait || _dma.IsReadyToUpdate())) {
    uint8_t *ps = _dma.getPixels();
    for (size_t i = 0; i < _pixel_count; i++) {
      TheColorFeature::applyPixelColor
        (ps, i,
         NeoRgbFeature::retrievePixelColor(seg->_buffer, i));
    }
    _dma.Update();
  }
}
