/**
 * @file MultiSensor.ino
 * @brief Read multiple DS18B20 sensors on different GPIO pins with a single PIO instance.
 *
 * Demonstrates the setPin() feature: one PIO state machine is shared across
 * multiple sensors by switching the active GPIO pin at runtime.
 *
 * Wiring (each sensor needs its own 4.7kΩ pull-up to 3.3V):
 *   GP2 ──┬── DS18B20 #1 DQ     GP3 ──┬── DS18B20 #2 DQ
 *       4.7kΩ                       4.7kΩ
 *       3.3V                        3.3V
 */

#include <OneWirePIO.h>
#include <DS18B20PIO.h>

/// GPIO pins for each sensor
static const uint SENSOR_PINS[] = { 2, 3 };
static const uint NUM_SENSORS   = sizeof(SENSOR_PINS) / sizeof(SENSOR_PINS[0]);

OneWirePIO bus(pio0);
DS18B20PIO sensor(bus);

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== OneWirePIO_RP2040 — Multi-Sensor ===\n");

    if (!bus.begin(SENSOR_PINS[0])) {
        Serial.println("FATAL: PIO init failed.");
        while (true) { delay(1000); }
    }
}

void loop() {
    // Request conversion on all sensors first (parallel conversion)
    for (uint i = 0; i < NUM_SENSORS; i++) {
        sensor.requestTemperatures(SENSOR_PINS[i]);
    }

    // Wait for the slowest conversion (only once)
    delay(sensor.getConversionDelay());

    // Read results from each sensor
    for (uint i = 0; i < NUM_SENSORS; i++) {
        float temp;
        if (sensor.getTemperatureValidated(SENSOR_PINS[i], temp)) {
            Serial.print("Sensor #");
            Serial.print(i);
            Serial.print(" (GP");
            Serial.print(SENSOR_PINS[i]);
            Serial.print("): ");
            Serial.print(temp, 2);
            Serial.println(" °C");
        } else {
            char err[64];
            sensor.getLastErrorString(err, sizeof(err), SENSOR_PINS[i]);
            Serial.println(err);
        }
    }

    Serial.println("---");
    delay(3000);
}
