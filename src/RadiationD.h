#pragma once

#include <Arduino.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "RadiationD supports ESP32 boards only."
#endif

#include <freertos/FreeRTOS.h>
#include <esp_timer.h>

/**
 * Configuration for a RadiationD-v1.1 compatible Geiger-Muller counter.
 */
struct RadiationDConfig {
  uint8_t pin = 4;
  float cpmPerUSvH = 153.8f;
  uint16_t averagingSeconds = 60;
  bool enableInternalPullup = false;
};

/**
 * Atomic snapshot of the current measurement.
 */
struct RadiationDReading {
  float cpm;
  float usvH;
  uint64_t totalCount;
  uint16_t sampledSeconds;
};

/**
 * Interrupt-driven RadiationD-v1.1 Geiger counter driver for ESP32 Arduino.
 *
 * The library samples the pulse counter once per second from an ESP timer, so
 * the sketch does not need to call an update function in loop().
 */
class RadiationD {
 public:
  static constexpr float J321_CPM_PER_USVH = 153.8f;
  static constexpr uint16_t MIN_AVERAGING_SECONDS = 1;
  static constexpr uint16_t MAX_AVERAGING_SECONDS = 3600;

  RadiationD() = default;
  ~RadiationD();

  RadiationD(const RadiationD&) = delete;
  RadiationD& operator=(const RadiationD&) = delete;

  /**
   * Start pulse counting and the one-second sampling timer.
   * Returns false if the configuration is invalid or the ESP timer cannot be
   * allocated.
   */
  bool begin(const RadiationDConfig& config);

  /**
   * Convenience overload for the common setup.
   */
  bool begin(uint8_t pin,
             float cpmPerUSvH = J321_CPM_PER_USVH,
             uint16_t averagingSeconds = 60,
             bool enableInternalPullup = false);

  /** Stop the timer, detach the interrupt, and release the rolling buffer. */
  void end();

  bool isRunning() const;

  /** Counts per minute calculated from the rolling average. */
  float getCPM() const;

  /** Dose rate in micro-sieverts per hour, calculated from CPM. */
  float getDoseRateUSvH() const;

  /** Return CPM, dose rate, count total, and filled averaging period together. */
  RadiationDReading getReading() const;

  /** Total accepted pulses since begin() or resetTotalCount(). */
  uint64_t getTotalCount() const;

  /** Reset only the lifetime pulse counter; the rolling measurement continues. */
  void resetTotalCount();

 private:
  static constexpr uint64_t SAMPLE_PERIOD_US = 1000000ULL;

  static void IRAM_ATTR pulseISR(void* arg);
  static void sampleTimerCallback(void* arg);
  void sample();
  void clearState();

  RadiationDConfig _config{};
  uint32_t* _samples = nullptr;
  esp_timer_handle_t _sampleTimer = nullptr;

  mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
  volatile uint32_t _pulsesSinceSample = 0;
  uint64_t _totalCount = 0;
  uint64_t _countsInWindow = 0;
  uint16_t _sampleIndex = 0;
  uint16_t _sampledSeconds = 0;
  float _cpm = 0.0f;
  float _usvH = 0.0f;
  bool _interruptAttached = false;
  bool _running = false;
};
