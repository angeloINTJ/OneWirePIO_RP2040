/**
 * @file DS18B20PIO.h
 * @brief High-level DS18B20 temperature sensor driver over PIO-accelerated 1-Wire.
 *
 * Built on top of OneWirePIO, this class provides a complete DS18B20 interface
 * with CRC-8 validation, configurable resolution, multi-scale output, and
 * built-in diagnostics.
 *
 * Features:
 *   - Validated temperature reads with CRC-8 integrity check
 *   - Configurable resolution: 9-bit (94 ms) to 12-bit (750 ms)
 *   - Output in Celsius, Fahrenheit, or Kelvin
 *   - ROM code reading for device identification
 *   - Raw scratchpad access for advanced diagnostics
 *   - State machine for structured error tracking
 *
 * Wiring:
 * @code
 *   Pico GP pin ──┬── DS18B20 DQ (Data)
 *                 │
 *                4.7kΩ
 *                 │
 *                3.3V
 * @endcode
 *
 * @author Ângelo
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <Arduino.h>
#include "OneWirePIO.h"

/**
 * @class DS18B20PIO
 * @brief DS18B20 sensor driver with CRC validation and diagnostics.
 *
 * This class does NOT manage the OneWirePIO lifetime — it receives a reference
 * via dependency injection. The caller is responsible for calling
 * OneWirePIO::begin() before using any DS18B20PIO methods.
 *
 * Usage:
 * @code
 *   OneWirePIO bus(pio0);
 *   bus.begin(2);
 *
 *   DS18B20PIO sensor(bus);
 *   sensor.requestTemperatures(2);
 *   delay(sensor.getConversionDelay());
 *
 *   float temp;
 *   if (sensor.getTemperatureValidated(2, temp)) {
 *       Serial.println(temp);
 *   }
 * @endcode
 */
class DS18B20PIO {
public:
    // ----------------------------------------------------------------
    // Enumerations
    // ----------------------------------------------------------------

    /**
     * @brief Operational state for structured error tracking.
     */
    enum State {
        OK,               ///< Last operation succeeded
        ERROR_NO_SENSOR,  ///< No device responded to the Reset pulse
        ERROR_CRC         ///< CRC-8 mismatch — data corruption detected
    };

    /**
     * @brief ADC resolution presets.
     *
     * Higher resolution yields more decimal precision but requires a
     * longer conversion time.
     */
    enum Resolution {
        RES_9_BIT  = 9,   ///<  0.5°C   precision,  ~94 ms conversion
        RES_10_BIT = 10,   ///<  0.25°C  precision, ~188 ms conversion
        RES_11_BIT = 11,   ///<  0.125°C precision, ~375 ms conversion
        RES_12_BIT = 12    ///<  0.0625°C precision, ~750 ms conversion (default)
    };

    /**
     * @brief Temperature output unit.
     */
    enum TemperatureScale {
        CELSIUS,     ///< °C (default)
        FAHRENHEIT,  ///< °F = °C × 1.8 + 32
        KELVIN       ///< K  = °C + 273.15
    };

    // ----------------------------------------------------------------
    // 1-Wire ROM / Function Commands
    // ----------------------------------------------------------------

    static const uint8_t CMD_SKIP_ROM         = 0xCC;  ///< Address all devices
    static const uint8_t CMD_READ_ROM         = 0x33;  ///< Read 64-bit ROM code
    static const uint8_t CMD_READ_SCRATCHPAD  = 0xBE;  ///< Read 9-byte scratchpad
    static const uint8_t CMD_WRITE_SCRATCHPAD = 0x4E;  ///< Write TH, TL, Config
    static const uint8_t CMD_CONVERT_T        = 0x44;  ///< Start temperature conversion

    // ----------------------------------------------------------------
    // Public API
    // ----------------------------------------------------------------

    /**
     * @brief Construct with a reference to an initialized OneWirePIO bus.
     * @param bus Reference to the PIO-backed 1-Wire bus (must outlive this object).
     */
    DS18B20PIO(OneWirePIO &bus);

    /**
     * @brief Set the ADC resolution of the sensor.
     *
     * Reads the current scratchpad, modifies the config register, and
     * writes it back. Also updates the internal conversion delay.
     *
     * @param pin  GPIO pin the sensor is connected to.
     * @param res  Desired resolution (RES_9_BIT .. RES_12_BIT).
     * @return true  Resolution set successfully.
     * @return false Sensor not detected.
     */
    bool setResolution(uint pin, Resolution res);

    /**
     * @brief Get the expected conversion time for the current resolution.
     * @return Conversion delay in milliseconds.
     */
    unsigned long getConversionDelay();

    /**
     * @brief Issue a Convert T command to start temperature measurement.
     *
     * After calling this, wait at least getConversionDelay() milliseconds
     * before reading the result.
     *
     * @param pin GPIO pin the sensor is connected to.
     */
    void requestTemperatures(uint pin);

    /**
     * @brief Read the 64-bit ROM code (family + serial + CRC).
     *
     * Only works with a single device on the bus (uses Read ROM command).
     *
     * @param pin    GPIO pin the sensor is connected to.
     * @param romOut Pointer to an 8-byte buffer to receive the ROM code.
     * @return true  ROM read successfully.
     * @return false Sensor not detected.
     */
    bool readROM(uint pin, uint8_t *romOut);

    /**
     * @brief Read temperature with CRC-8 validation.
     *
     * This is the recommended method for production use. Returns false
     * if the sensor is missing or if the scratchpad CRC check fails.
     *
     * @param pin             GPIO pin the sensor is connected to.
     * @param temperaturaOut  [out] Temperature value on success.
     * @param scale           Output unit (default: CELSIUS).
     * @return true  Valid temperature stored in temperaturaOut.
     * @return false Read failed — check getState() for details.
     */
    bool getTemperatureValidated(uint pin, float &temperaturaOut,
                                 TemperatureScale scale = CELSIUS);

    /**
     * @brief Read temperature with full diagnostic output.
     *
     * Returns the raw 9-byte scratchpad, computed temperature, and
     * CRC error flag. Useful for debugging sensor or wiring issues.
     *
     * @param pin             GPIO pin the sensor is connected to.
     * @param rawScratchpad   [out] 9-byte scratchpad buffer.
     * @param tempOut         [out] Computed temperature.
     * @param crcError        [out] true if CRC mismatch detected.
     * @param scale           Output unit (default: CELSIUS).
     * @return true  Communication succeeded (check crcError for data validity).
     * @return false Sensor not detected on the bus.
     */
    bool getDiagnosticData(uint pin, uint8_t *rawScratchpad, float &tempOut,
                           bool &crcError, TemperatureScale scale = CELSIUS);

    /**
     * @brief Get the state from the last operation.
     * @return Current State (OK, ERROR_NO_SENSOR, or ERROR_CRC).
     */
    State getState();

    /**
     * @brief Format a human-readable error string from the current state.
     *
     * Uses snprintf() internally — safe against buffer overflows.
     *
     * @param buffer    Destination character buffer.
     * @param maxLength Size of the buffer in bytes.
     * @param pin       GPIO pin number (included in the message for context).
     */
    void getLastErrorString(char *buffer, size_t maxLength, uint pin);

private:
    OneWirePIO    &_bus;              ///< Reference to the underlying 1-Wire bus
    State          _state;            ///< Last operation result
    unsigned long  _conversionDelay;  ///< Current conversion time (ms)

    /**
     * @brief Compute Dallas/Maxim CRC-8 (polynomial 0x8C, reflected).
     * @param data Pointer to the data buffer.
     * @param len  Number of bytes to process.
     * @return Computed CRC-8 value.
     */
    uint8_t calculateCRC(const uint8_t *data, uint8_t len);

    /**
     * @brief Convert a raw 16-bit temperature register to the desired scale.
     * @param raw   Signed 16-bit value from scratchpad bytes 0-1.
     * @param scale Target temperature unit.
     * @return Temperature as a floating-point value.
     */
    float convertRawToScale(int16_t raw, TemperatureScale scale);
};
