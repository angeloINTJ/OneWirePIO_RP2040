/**
 * @file BasicTemperature.ino
 * @brief Minimal example — read DS18B20 temperature using PIO-accelerated 1-Wire.
 *
 * Wiring:
 *   GP2 ──┬── DS18B20 DQ
 *         │
 *        4.7kΩ
 *         │
 *        3.3V
 *
 * This is the simplest possible sketch: initialize, request, wait, read.
 * Open Serial Monitor at 115200 baud to see the output.
 */

#include <OneWirePIO.h>
#include <DS18B20PIO.h>

/// GPIO pin connected to the DS18B20 data line
static const uint SENSOR_PIN = 2;

/// PIO-backed 1-Wire bus (using PIO block 0)
OneWirePIO bus(pio0);

/// DS18B20 high-level driver
DS18B20PIO sensor(bus);

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }  // Wait for USB Serial (Pico native USB)

    Serial.println("=== OneWirePIO_RP2040 — Basic Temperature ===\n");

    // Initialize the PIO state machine and assign the GPIO pin
    if (!bus.begin(SENSOR_PIN)) {
        Serial.println("FATAL: Could not initialize PIO (no free SM or memory).");
        while (true) { delay(1000); }
    }

    Serial.println("PIO initialized. Reading temperature every 2 seconds...\n");
}

void loop() {
    // Step 1: Start temperature conversion
    sensor.requestTemperatures(SENSOR_PIN);

    // Step 2: Wait for the conversion to complete
    delay(sensor.getConversionDelay());

    // Step 3: Read the validated result
    float temperature;
    if (sensor.getTemperatureValidated(SENSOR_PIN, temperature)) {
        Serial.print("Temperature: ");
        Serial.print(temperature, 2);
        Serial.println(" °C");
    } else {
        // Print a descriptive error message
        char errorMsg[64];
        sensor.getLastErrorString(errorMsg, sizeof(errorMsg), SENSOR_PIN);
        Serial.println(errorMsg);
    }

    delay(2000);
}
