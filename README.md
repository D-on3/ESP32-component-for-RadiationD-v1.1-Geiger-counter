# RadiationD Arduino Library for ESP32 and ESP8266

An interrupt-driven Arduino library for **RadiationD-v1.1** and compatible
Geiger-Muller counter boards used with an ESP32 or ESP8266. It counts
active-low pulses, calculates CPM, and converts the rolling average to a
configurable dose rate in micro-sieverts per hour (uSv/h).

This repository is an **ESP32 and ESP8266 Arduino project and library**. It
has no ESP-IDF or CMake project files.

## Features

- GPIO falling-edge interrupt: no polling and no `loop()` update call required.
- Platform one-second timer: ESP32 uses `esp_timer`; ESP8266 uses a scheduled
  `Ticker` callback.
- Configurable rolling average from 1 to 3600 seconds.
- Thread-safe reading snapshot with CPM, uSv/h, total pulses, and current
  filled averaging window.
- Supports several independent `RadiationD` objects when each uses its own GPIO.

## Wiring

RadiationD-v1.1 type boards normally expose `5V`, `GND`, and `VIN`/`OUT`.

| RadiationD board | ESP32 example | NodeMCU v3 / ESP8266 example |
| --- | --- | --- |
| `GND` | `GND` | `GND` |
| `VIN` / `OUT` | GPIO 4 | `D5` / GPIO 14 |
| `5V` | 5 V supply, if required by the board | 5 V supply, if required by the board |

ESP32 and ESP8266 GPIO pins are **3.3 V only**. If the counter's pulse output
reaches 5 V, add a proper level shifter or resistor divider before connecting
it to the microcontroller. Always share ground between the module and board.

The pulse is expected to be active low, therefore the library attaches a
falling-edge interrupt. Leave the internal pull-up disabled for a board with a
normal push-pull output; enable it only for an open-drain or otherwise floating
signal.

## Install in Arduino IDE

1. Download this repository as a ZIP file.
2. In Arduino IDE choose **Sketch -> Include Library -> Add .ZIP Library...**.
3. Select the correct board package:
   - **ESP32 by Espressif Systems** for ESP32 boards;
   - **ESP8266 by ESP8266 Community** and **NodeMCU 1.0 (ESP-12E Module)** for
     NodeMCU v3.
4. Open the matching example from **File -> Examples -> RadiationD**.

Alternatively, clone the repository into your Arduino sketchbook's
`libraries/RadiationD` directory.

## Project layout

- `examples/OLEDReadout/OLEDReadout.ino` — complete ESP32 OLED project.
- `examples/OLEDReadoutESP8266/OLEDReadoutESP8266.ino` — complete NodeMCU v3
  / ESP8266 OLED project.
- `examples/BasicReadout/BasicReadout.ino` — serial-monitor-only example.
- `src/` — the reusable `RadiationD` Arduino library.

For NodeMCU v3, open **File -> Examples -> RadiationD -> OLEDReadoutESP8266**.

## OLED readout (SSD1306, 128x64)

Both OLED examples use a 0.96-inch, 128x64 I2C SSD1306 display at address
`0x3C`, following the wiring style in the
[reference tutorial](https://randomnerdtutorials.com/micropython-oled-display-esp32-esp8266/).

| SSD1306 OLED | ESP32 | NodeMCU v3 / ESP8266 |
| --- | --- | --- |
| `VCC` / `VIN` | `3.3 V` | `3.3 V` |
| `GND` | `GND` | `GND` |
| `SCL` | GPIO 22 | `D1` / GPIO 5 |
| `SDA` | GPIO 21 | `D2` / GPIO 4 |

Install **Adafruit SSD1306** and **Adafruit GFX Library** through Arduino IDE's
Library Manager if they are not installed automatically. The display shows
CPM, dose rate in **mSv/h** (`uSv/h / 1000`), averaging time, and total pulses.

If the display remains blank, run an I2C scanner and change `OLED_ADDRESS` to
`0x3D` when required by the module.

## Basic use

```cpp
#include <RadiationD.h>

RadiationD geiger;

void setup() {
  Serial.begin(115200);
  if (!geiger.begin(4, RadiationD::J321_CPM_PER_USVH, 60)) {
    while (true) { delay(1000); }
  }
}

void loop() {
  RadiationDReading reading = geiger.getReading();
  Serial.printf("%.2f CPM | %.4f uSv/h\n", reading.cpm, reading.usvH);
  delay(5000);
}
```

`begin()` starts the platform timer that samples the interrupt counter every
second. There is no `update()` call to add to `loop()`.

## Calibration and averaging

The conversion factor is the number of counts per minute that corresponds to
1 uSv/h:

```cpp
geiger.begin(4, 153.8f, 300);  // pin, CPM per uSv/h, 5-minute average
```

Typical starting values are shown below, but use the datasheet of the actual
tube and validate it against a known reference instrument when accuracy matters.

| Tube | Typical CPM per uSv/h |
| --- | ---: |
| J321 / M4011 | 151-154 |
| SBM-20 | about 175 |
| LND-712 | about 123 |

A 60-second window reacts more quickly; a 300-second window gives a steadier
background-radiation reading. During startup, the library calculates from the
seconds collected so far instead of treating unfilled samples as zero.

## API

| Call | Purpose |
| --- | --- |
| `begin(pin, factor, seconds, pullup)` | Starts the counter and returns `false` on invalid configuration or timer allocation failure. |
| `getCPM()` | Returns the rolling-average count rate. |
| `getDoseRateUSvH()` | Returns CPM divided by the configured conversion factor. |
| `getReading()` | Returns a consistent `RadiationDReading` snapshot. |
| `getTotalCount()` / `resetTotalCount()` | Reads or clears the accepted-pulse total. |
| `end()` | Stops the ESP timer, detaches the interrupt, and releases memory. |

This project provides an indication from a hobby counter, not a calibrated
dosimeter or a radiation-safety instrument.
