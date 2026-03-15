/**
 * @file ResolutionBenchmark.ino
 * @brief Benchmark all four DS18B20 resolution modes and compare timing vs precision.
 *
 * Cycles through 9, 10, 11, and 12-bit resolution, measuring the actual
 * conversion time and displaying the resulting temperature precision.
 *
 * Wiring: same as BasicTemperature (GP2 + 4.7kΩ pull-up to 3.3V).
 */

#include <OneWirePIO.h>
#include <DS18B20PIO.h>

static const uint SENSOR_PIN = 2;

OneWirePIO bus(pio0);
DS18B20PIO sensor(bus);

/// Resolution presets to cycle through
static const DS18B20PIO::Resolution resolutions[] = {
    DS18B20PIO::RES_9_BIT,
    DS18B20PIO::RES_10_BIT,
    DS18B20PIO::RES_11_BIT,
    DS18B20PIO::RES_12_BIT
};

/// Human-readable labels for each resolution
static const char *resLabels[] = {
    " 9-bit (0.5000 °C)",
    "10-bit (0.2500 °C)",
    "11-bit (0.1250 °C)",
    "12-bit (0.0625 °C)"
};

static const uint NUM_RES = sizeof(resolutions) / sizeof(resolutions[0]);

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== OneWirePIO_RP2040 — Resolution Benchmark ===\n");

    if (!bus.begin(SENSOR_PIN)) {
        Serial.println("FATAL: PIO init failed.");
        while (true) { delay(1000); }
    }
}

void loop() {
    for (uint i = 0; i < NUM_RES; i++) {
        // Set resolution
        if (!sensor.setResolution(SENSOR_PIN, resolutions[i])) {
            Serial.print("Failed to set ");
            Serial.println(resLabels[i]);
            continue;
        }

        // Request and time the conversion
        unsigned long t0 = millis();
        sensor.requestTemperatures(SENSOR_PIN);
        delay(sensor.getConversionDelay());

        float temp;
        bool ok = sensor.getTemperatureValidated(SENSOR_PIN, temp);
        unsigned long elapsed = millis() - t0;

        // Report
        Serial.print(resLabels[i]);
        Serial.print("  →  ");

        if (ok) {
            Serial.print(temp, 4);
            Serial.print(" °C");
        } else {
            Serial.print("READ FAILED");
        }

        Serial.print("  [");
        Serial.print(elapsed);
        Serial.println(" ms]");
    }

    Serial.println("\n--- Cycle complete, repeating in 5 s ---\n");
    delay(5000);
}
