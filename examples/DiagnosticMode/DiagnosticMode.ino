/**
 * @file DiagnosticMode.ino
 * @brief Advanced example — read ROM code and raw scratchpad with CRC check.
 *
 * Use this sketch to:
 *   - Identify sensors by their unique 64-bit ROM code
 *   - Inspect the raw 9-byte scratchpad for debugging
 *   - Verify CRC integrity of the communication link
 *
 * Wiring: same as BasicTemperature (GP2 + 4.7kΩ pull-up to 3.3V).
 */

#include <OneWirePIO.h>
#include <DS18B20PIO.h>

static const uint SENSOR_PIN = 2;

OneWirePIO bus(pio0);
DS18B20PIO sensor(bus);

/**
 * @brief Print a byte array as hexadecimal.
 */
void printHex(const uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        if (i < len - 1) Serial.print(" ");
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Serial.println("=== OneWirePIO_RP2040 — Diagnostic Mode ===\n");

    if (!bus.begin(SENSOR_PIN)) {
        Serial.println("FATAL: PIO init failed.");
        while (true) { delay(1000); }
    }

    // --- Read and display the 64-bit ROM code ---
    uint8_t rom[8];
    if (sensor.readROM(SENSOR_PIN, rom)) {
        Serial.print("ROM Code: ");
        printHex(rom, 8);
        Serial.println();

        Serial.print("  Family:  0x");
        Serial.println(rom[0], HEX);

        Serial.print("  Serial:  ");
        printHex(rom + 1, 6);
        Serial.println();

        Serial.print("  CRC:     0x");
        Serial.println(rom[7], HEX);
    } else {
        Serial.println("Could not read ROM — sensor not detected.");
    }

    Serial.println();
}

void loop() {
    // Start conversion
    sensor.requestTemperatures(SENSOR_PIN);
    delay(sensor.getConversionDelay());

    // Read with full diagnostics
    uint8_t scratchpad[9];
    float temp;
    bool crcError;

    if (sensor.getDiagnosticData(SENSOR_PIN, scratchpad, temp, crcError)) {
        Serial.print("Scratchpad: ");
        printHex(scratchpad, 9);
        Serial.println();

        if (crcError) {
            Serial.println("  *** CRC MISMATCH — data may be corrupt ***");
        } else {
            Serial.print("  Temperature: ");
            Serial.print(temp, 4);
            Serial.println(" °C");

            Serial.print("  TH Alarm:    ");
            Serial.println(static_cast<int8_t>(scratchpad[2]));

            Serial.print("  TL Alarm:    ");
            Serial.println(static_cast<int8_t>(scratchpad[3]));

            uint8_t config = (scratchpad[4] >> 5) & 0x03;
            Serial.print("  Resolution:  ");
            Serial.print(9 + config);
            Serial.println(" bit");
        }
    } else {
        char err[64];
        sensor.getLastErrorString(err, sizeof(err), SENSOR_PIN);
        Serial.println(err);
    }

    Serial.println("---");
    delay(5000);
}
