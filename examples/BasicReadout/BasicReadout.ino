#include <RadiationD.h>

constexpr uint8_t GEIGER_PIN = 4;

// For a J321 / M4011 tube, 153.8 CPM per micro-sievert per hour is a common
// starting point. Confirm the correct factor for your tube before relying on
// the dose-rate calculation.
RadiationD geiger;

void setup() {
  Serial.begin(115200);

  if (!geiger.begin(GEIGER_PIN, RadiationD::J321_CPM_PER_USVH, 60)) {
    Serial.println("RadiationD initialization failed.");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("RadiationD started. Counting pulses...");
}

void loop() {
  const RadiationDReading reading = geiger.getReading();

  Serial.printf("CPM: %.2f | Dose rate: %.4f uSv/h | Total pulses: %llu | Window: %u s\n",
                reading.cpm,
                reading.usvH,
                static_cast<unsigned long long>(reading.totalCount),
                reading.sampledSeconds);

  delay(5000);
}
