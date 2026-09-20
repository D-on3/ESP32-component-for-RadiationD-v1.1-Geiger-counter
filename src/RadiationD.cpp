#include "RadiationD.h"

#include <stdlib.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_err.h>
#endif

// Definitions keep the constants link-safe for C++11 Arduino toolchains when
// a sketch takes their address or otherwise odr-uses them.
constexpr float RadiationD::J321_CPM_PER_USVH;
constexpr uint16_t RadiationD::MIN_AVERAGING_SECONDS;
constexpr uint16_t RadiationD::MAX_AVERAGING_SECONDS;
constexpr uint32_t RadiationD::SAMPLE_PERIOD_MS;
#if defined(ARDUINO_ARCH_ESP32)
constexpr uint64_t RadiationD::SAMPLE_PERIOD_US;
#endif

RadiationD::~RadiationD() {
  end();
}

bool RadiationD::begin(uint8_t pin,
                       float cpmPerUSvH,
                       uint16_t averagingSeconds,
                       bool enableInternalPullup) {
  RadiationDConfig config;
  config.pin = pin;
  config.cpmPerUSvH = cpmPerUSvH;
  config.averagingSeconds = averagingSeconds;
  config.enableInternalPullup = enableInternalPullup;
  return begin(config);
}

bool RadiationD::begin(const RadiationDConfig& config) {
  end();

  if (config.cpmPerUSvH <= 0.0f ||
      config.averagingSeconds < MIN_AVERAGING_SECONDS ||
      config.averagingSeconds > MAX_AVERAGING_SECONDS) {
    return false;
  }

  _samples = static_cast<uint32_t*>(calloc(config.averagingSeconds,
                                             sizeof(uint32_t)));
  if (_samples == nullptr) {
    return false;
  }

  _config = config;
  clearState();

  pinMode(_config.pin,
          _config.enableInternalPullup ? INPUT_PULLUP : INPUT);

#if defined(ARDUINO_ARCH_ESP32)
  esp_timer_create_args_t timerArgs = {};
  timerArgs.callback = &RadiationD::sampleTimerCallback;
  timerArgs.arg = this;
  timerArgs.dispatch_method = ESP_TIMER_TASK;
  timerArgs.name = "radiationd";

  if (esp_timer_create(&timerArgs, &_sampleTimer) != ESP_OK) {
    end();
    return false;
  }

  portENTER_CRITICAL(&_mux);
  _running = true;
  portEXIT_CRITICAL(&_mux);
#else
  noInterrupts();
  _running = true;
  interrupts();
#endif

  attachInterruptArg(_config.pin, &RadiationD::pulseISR, this, FALLING);
  _interruptAttached = true;

#if defined(ARDUINO_ARCH_ESP32)
  if (esp_timer_start_periodic(_sampleTimer, SAMPLE_PERIOD_US) != ESP_OK) {
    end();
    return false;
  }
#else
  _sampleTicker.attach_ms_scheduled(SAMPLE_PERIOD_MS, [this]() { sample(); });
#endif

  return true;
}

void RadiationD::end() {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_mux);
  _running = false;
  portEXIT_CRITICAL(&_mux);
#else
  noInterrupts();
  _running = false;
  interrupts();
#endif

  if (_interruptAttached) {
    detachInterrupt(_config.pin);
    _interruptAttached = false;
  }

#if defined(ARDUINO_ARCH_ESP32)
  if (_sampleTimer != nullptr) {
    esp_timer_stop(_sampleTimer);
    esp_timer_delete(_sampleTimer);
    _sampleTimer = nullptr;
  }
#else
  _sampleTicker.detach();
#endif

  if (_samples != nullptr) {
    free(_samples);
    _samples = nullptr;
  }

  clearState();
}

bool RadiationD::isRunning() const {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_mux);
  const bool running = _running;
  portEXIT_CRITICAL(&_mux);
#else
  noInterrupts();
  const bool running = _running;
  interrupts();
#endif
  return running;
}

float RadiationD::getCPM() const {
  return getReading().cpm;
}

float RadiationD::getDoseRateUSvH() const {
  return getReading().usvH;
}

RadiationDReading RadiationD::getReading() const {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_mux);
#else
  noInterrupts();
#endif
  const RadiationDReading reading = {
      _cpm,
      _usvH,
      _totalCount,
      _sampledSeconds,
  };
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&_mux);
#else
  interrupts();
#endif
  return reading;
}

uint64_t RadiationD::getTotalCount() const {
  return getReading().totalCount;
}

void RadiationD::resetTotalCount() {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_mux);
#else
  noInterrupts();
#endif
  _totalCount = 0;
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&_mux);
#else
  interrupts();
#endif
}

void IRAM_ATTR RadiationD::pulseISR(void* arg) {
  RadiationD* const counter = static_cast<RadiationD*>(arg);
  if (counter == nullptr) {
    return;
  }

#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL_ISR(&counter->_mux);
#endif
  if (counter->_running) {
    ++counter->_pulsesSinceSample;
    ++counter->_totalCount;
  }
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL_ISR(&counter->_mux);
#endif
}

#if defined(ARDUINO_ARCH_ESP32)
void RadiationD::sampleTimerCallback(void* arg) {
  RadiationD* const counter = static_cast<RadiationD*>(arg);
  if (counter != nullptr) {
    counter->sample();
  }
}
#endif

void RadiationD::sample() {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_mux);
#else
  noInterrupts();
#endif

  if (!_running || _samples == nullptr) {
#if defined(ARDUINO_ARCH_ESP32)
    portEXIT_CRITICAL(&_mux);
#else
    interrupts();
#endif
    return;
  }

  const uint32_t countThisSecond = _pulsesSinceSample;
  _pulsesSinceSample = 0;

  _countsInWindow -= _samples[_sampleIndex];
  _countsInWindow += countThisSecond;
  _samples[_sampleIndex] = countThisSecond;
  _sampleIndex = (_sampleIndex + 1) % _config.averagingSeconds;

  if (_sampledSeconds < _config.averagingSeconds) {
    ++_sampledSeconds;
  }

  _cpm = static_cast<float>(_countsInWindow) * 60.0f /
         static_cast<float>(_sampledSeconds);
  _usvH = _cpm / _config.cpmPerUSvH;

#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&_mux);
#else
  interrupts();
#endif
}

void RadiationD::clearState() {
#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_mux);
#else
  noInterrupts();
#endif
  _pulsesSinceSample = 0;
  _totalCount = 0;
  _countsInWindow = 0;
  _sampleIndex = 0;
  _sampledSeconds = 0;
  _cpm = 0.0f;
  _usvH = 0.0f;
#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&_mux);
#else
  interrupts();
#endif
}
