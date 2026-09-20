#if !defined(ARDUINO_ARCH_ESP8266)
#error "Select NodeMCU 1.0 (ESP-12E Module) or another ESP8266 board."
#endif

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadiationD.h>

#include <stdio.h>

// NodeMCU v3 pin names: D5 is GPIO14, D2 is GPIO4, and D1 is GPIO5.
constexpr uint8_t GEIGER_PIN = D5;
constexpr uint8_t OLED_SDA = D2;
constexpr uint8_t OLED_SCL = D1;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int8_t OLED_RESET = -1;
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RadiationD geiger;

uint32_t lastDisplayUpdate = 0;

void printTotalCount(uint64_t totalCount) {
  char text[21];
  snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(totalCount));
  display.print(text);
}

void renderReadout(const RadiationDReading& reading) {
  const float mSvH = reading.usvH / 1000.0f;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("RADIATION MONITOR");
  display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

  display.setCursor(0, 14);
  display.print("CPM");
  display.setCursor(72, 14);
  display.print("mSv/h");

  display.setTextSize(2);
  display.setCursor(0, 25);
  display.print(reading.cpm, 1);

  display.setTextSize(1);
  display.setCursor(72, 28);
  display.print(mSvH, 4);

  display.drawFastHLine(0, 45, SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(0, 51);
  display.print("AVG:");
  display.print(reading.sampledSeconds);
  display.print("s  COUNT:");
  printTotalCount(reading.totalCount);

  display.display();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 not found at I2C address 0x3C.");
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print("Starting Geiger counter...");
  display.display();

  if (!geiger.begin(GEIGER_PIN, RadiationD::J321_CPM_PER_USVH, 60)) {
    Serial.println("RadiationD initialization failed.");
    display.clearDisplay();
    display.setCursor(0, 24);
    display.print("Geiger init failed");
    display.display();
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  if (millis() - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = millis();

    const RadiationDReading reading = geiger.getReading();
    renderReadout(reading);

    Serial.printf("CPM: %.2f | %.6f mSv/h | Total: %llu\n",
                  reading.cpm,
                  reading.usvH / 1000.0f,
                  static_cast<unsigned long long>(reading.totalCount));
  }
}
