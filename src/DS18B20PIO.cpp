/**
 * @file DS18B20PIO.cpp
 * @brief Implementation of the DS18B20 sensor driver.
 * @see DS18B20PIO.h for full API documentation.
 */

#include "DS18B20PIO.h"

// ====================================================================
// Construction
// ====================================================================

DS18B20PIO::DS18B20PIO(OneWirePIO &bus)
    : _bus(bus)
    , _state(OK)
    , _conversionDelay(750)
{
}

// ====================================================================
// Private Helpers
// ====================================================================

float DS18B20PIO::convertRawToScale(int16_t raw, TemperatureScale scale) {
    float celsius = static_cast<float>(raw) * 0.0625f;

    switch (scale) {
        case FAHRENHEIT: return (celsius * 1.8f) + 32.0f;
        case KELVIN:     return celsius + 273.15f;
        default:         return celsius;
    }
}

uint8_t DS18B20PIO::calculateCRC(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;

    while (len--) {
        uint8_t inbyte = *data++;
        for (uint8_t i = 8; i; i--) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;  // Reflected polynomial for Dallas CRC-8
            }
            inbyte >>= 1;
        }
    }

    return crc;
}

// ====================================================================
// Resolution Configuration
// ====================================================================

bool DS18B20PIO::setResolution(uint pin, Resolution res) {
    _bus.setPin(pin);
    _bus.sendReset();

    if (!_bus.isSensorPresent()) {
        _state = ERROR_NO_SENSOR;
        return false;
    }

    // Read the current scratchpad to preserve TH/TL alarm registers
    _bus.writeByte(CMD_SKIP_ROM);
    _bus.writeByte(CMD_READ_SCRATCHPAD);

    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = _bus.readByte();
    }

    // Modify only the resolution bits (bits 6:5) in the config register
    uint8_t config = scratchpad[4] & 0x9F;  // Clear bits 6:5

    switch (res) {
        case RES_9_BIT:
            // config bits 6:5 = 00 (already cleared)
            _conversionDelay = 94;
            break;
        case RES_10_BIT:
            config |= 0x20;
            _conversionDelay = 188;
            break;
        case RES_11_BIT:
            config |= 0x40;
            _conversionDelay = 375;
            break;
        case RES_12_BIT:
            config |= 0x60;
            _conversionDelay = 750;
            break;
    }

    // Write the modified config back to the sensor
    _bus.sendReset();
    if (!_bus.isSensorPresent()) {
        _state = ERROR_NO_SENSOR;
        return false;
    }

    _bus.writeByte(CMD_SKIP_ROM);
    _bus.writeByte(CMD_WRITE_SCRATCHPAD);
    _bus.writeByte(scratchpad[2]);  // TH (alarm high)
    _bus.writeByte(scratchpad[3]);  // TL (alarm low)
    _bus.writeByte(config);         // Configuration register

    _state = OK;
    return true;
}

unsigned long DS18B20PIO::getConversionDelay() {
    return _conversionDelay;
}

// ====================================================================
// Temperature Conversion
// ====================================================================

void DS18B20PIO::requestTemperatures(uint pin) {
    _bus.setPin(pin);
    _bus.sendReset();

    if (_bus.isSensorPresent()) {
        _bus.writeByte(CMD_SKIP_ROM);
        _bus.writeByte(CMD_CONVERT_T);
        _state = OK;
    } else {
        _state = ERROR_NO_SENSOR;
    }
}

// ====================================================================
// ROM Identification
// ====================================================================

bool DS18B20PIO::readROM(uint pin, uint8_t *romOut) {
    _bus.setPin(pin);
    _bus.sendReset();

    if (!_bus.isSensorPresent()) {
        _state = ERROR_NO_SENSOR;
        return false;
    }

    _bus.writeByte(CMD_READ_ROM);
    for (int i = 0; i < 8; i++) {
        romOut[i] = _bus.readByte();
    }

    _state = OK;
    return true;
}

// ====================================================================
// Temperature Reading (Validated)
// ====================================================================

bool DS18B20PIO::getTemperatureValidated(uint pin, float &temperaturaOut,
                                          TemperatureScale scale) {
    _bus.setPin(pin);
    _bus.sendReset();

    if (!_bus.isSensorPresent()) {
        _state = ERROR_NO_SENSOR;
        return false;
    }

    _bus.writeByte(CMD_SKIP_ROM);
    _bus.writeByte(CMD_READ_SCRATCHPAD);

    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = _bus.readByte();
    }

    // Validate data integrity with CRC-8
    if (calculateCRC(scratchpad, 8) != scratchpad[8]) {
        _state = ERROR_CRC;
        return false;
    }

    // Reconstruct the signed 16-bit temperature value
    int16_t tempRaw = (static_cast<int16_t>(scratchpad[1]) << 8) | scratchpad[0];
    temperaturaOut = convertRawToScale(tempRaw, scale);

    _state = OK;
    return true;
}

// ====================================================================
// Diagnostic Read
// ====================================================================

bool DS18B20PIO::getDiagnosticData(uint pin, uint8_t *rawScratchpad,
                                    float &tempOut, bool &crcError,
                                    TemperatureScale scale) {
    crcError = false;

    _bus.setPin(pin);
    _bus.sendReset();

    if (!_bus.isSensorPresent()) {
        _state = ERROR_NO_SENSOR;
        return false;
    }

    _bus.writeByte(CMD_SKIP_ROM);
    _bus.writeByte(CMD_READ_SCRATCHPAD);

    for (int i = 0; i < 9; i++) {
        rawScratchpad[i] = _bus.readByte();
    }

    // Check CRC but still return the data for inspection
    if (calculateCRC(rawScratchpad, 8) != rawScratchpad[8]) {
        crcError = true;
        _state = ERROR_CRC;
        return true;  // Communication succeeded; data may be corrupt
    }

    int16_t tempRaw = (static_cast<int16_t>(rawScratchpad[1]) << 8) | rawScratchpad[0];
    tempOut = convertRawToScale(tempRaw, scale);

    _state = OK;
    return true;
}

// ====================================================================
// Error Reporting
// ====================================================================

DS18B20PIO::State DS18B20PIO::getState() {
    return _state;
}

void DS18B20PIO::getLastErrorString(char *buffer, size_t maxLength, uint pin) {
    if (buffer == nullptr || maxLength == 0) {
        return;
    }

    switch (_state) {
        case OK:
            snprintf(buffer, maxLength, "OK: Idle/Success on pin %u", pin);
            break;
        case ERROR_NO_SENSOR:
            snprintf(buffer, maxLength,
                     "Error: DS18B20 not detected on pin %u", pin);
            break;
        case ERROR_CRC:
            snprintf(buffer, maxLength,
                     "Error: CRC mismatch on pin %u — data corrupted", pin);
            break;
        default:
            snprintf(buffer, maxLength,
                     "Error: Unknown state on pin %u", pin);
            break;
    }
}
