/**
 * @file TemperatureScales.ino
 * @brief Display temperature simultaneously in Celsius, Fahrenheit, and Kelvin.
 *
 * Demonstrates the TemperatureScale enum by reading the sensor once and
 * performing three validated reads in different units.
 *
 * Wiring: same as BasicTemperature (GP2 + 4.7kΩ pull-up to 3.3V).
 */

#include <OneWirePIO.h>
#include <DS18B20PIO.h>

static const uint SENSOR_PIN = 2;

OneWirePIO bus(pio0);
DS18B20PIO sensor(bus);

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== OneWirePIO_RP2040 — Temperature Scales ===\n");

    if (!bus.begin(SENSOR_PIN)) {
        Serial.println("FATAL: PIO init failed.");
        while (true) { delay(1000); }
    }
}

void loop() {
    // Single conversion
    sensor.requestTemperatures(SENSOR_PIN);
    delay(sensor.getConversionDelay());

    // Read in all three scales
    float celsius, fahrenheit, kelvin;

    bool ok_c = sensor.getTemperatureValidated(SENSOR_PIN, celsius,    DS18B20PIO::CELSIUS);

    // Re-request needed because each getTemperatureValidated reads the scratchpad
    sensor.requestTemperatures(SENSOR_PIN);
    delay(sensor.getConversionDelay());
    bool ok_f = sensor.getTemperatureValidated(SENSOR_PIN, fahrenheit, DS18B20PIO::FAHRENHEIT);

    sensor.requestTemperatures(SENSOR_PIN);
    delay(sensor.getConversionDelay());
    bool ok_k = sensor.getTemperatureValidated(SENSOR_PIN, kelvin,     DS18B20PIO::KELVIN);

    if (ok_c && ok_f && ok_k) {
        Serial.print("Celsius:    ");
        Serial.print(celsius, 2);
        Serial.println(" °C");

        Serial.print("Fahrenheit: ");
        Serial.print(fahrenheit, 2);
        Serial.println(" °F");

        Serial.print("Kelvin:     ");
        Serial.print(kelvin, 2);
        Serial.println(" K");
    } else {
        char err[64];
        sensor.getLastErrorString(err, sizeof(err), SENSOR_PIN);
        Serial.println(err);
    }

    Serial.println("---");
    delay(5000);
}
