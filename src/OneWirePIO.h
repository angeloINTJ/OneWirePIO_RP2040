/**
 * @file OneWirePIO.h
 * @brief Hardware-accelerated 1-Wire Physical Layer (PHY) using RP2040 PIO.
 *
 * This class offloads all critical 1-Wire timing (Reset, Write, Read) to the
 * RP2040's PIO coprocessor, achieving deterministic, jitter-free communication
 * independent of CPU load. No bit-banging, no interrupt-disabling required.
 *
 * Features:
 *   - Dynamic PIO program loading (no hardcoded offsets)
 *   - RAII resource management (automatic cleanup on destruction)
 *   - Runtime pin reassignment via setPin()
 *   - Supports both pio0 and pio1 via dependency injection
 *
 * Hardware Requirements:
 *   - Raspberry Pi Pico / Pico W (RP2040)
 *   - DS18B20 or any 1-Wire device with 4.7kΩ pull-up resistor
 *
 * @author Ângelo
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <Arduino.h>
#include "hardware/pio.h"

/**
 * @class OneWirePIO
 * @brief 1-Wire Physical Layer implemented via the RP2040 PIO state machine.
 *
 * Manages PIO resource allocation (state machine + instruction memory) and
 * provides low-level byte-oriented 1-Wire communication primitives.
 *
 * @note One instance controls one PIO state machine. The RP2040 has 8 state
 *       machines total (4 per PIO block), so up to 8 independent buses can
 *       coexist.
 *
 * Usage:
 * @code
 *   OneWirePIO bus(pio0);        // Use PIO block 0
 *   if (!bus.begin(GP_PIN)) {
 *       // Handle: no free state machine or instruction memory
 *   }
 *   bus.sendReset();
 *   if (bus.isSensorPresent()) {
 *       bus.writeByte(0xCC);     // Skip ROM
 *       bus.writeByte(0x44);     // Convert T
 *   }
 * @endcode
 */
class OneWirePIO {
public:
    /**
     * @brief Construct a new OneWirePIO instance.
     * @param pio_instance PIO block to use: pio0 (default) or pio1.
     *
     * No hardware is touched until begin() is called. This allows safe
     * construction of global objects before the runtime is fully initialized.
     */
    OneWirePIO(PIO pio_instance = pio0);

    /**
     * @brief Destructor — releases all PIO resources (RAII).
     *
     * Disables the state machine, unclaims it, and removes the PIO program
     * from instruction memory. Safe to call even if begin() was never called
     * or if it failed.
     */
    ~OneWirePIO();

    /**
     * @brief Initialize the PIO hardware for 1-Wire communication.
     *
     * Dynamically loads the PIO program, claims an unused state machine,
     * configures clock divider to 1 µs/tick, and sets up the data pin.
     *
     * @param pin GPIO pin number connected to the 1-Wire data line.
     * @return true  Initialization successful.
     * @return false No free state machine or instruction memory available.
     */
    bool begin(uint pin);

    /**
     * @brief Change the active GPIO pin at runtime.
     *
     * Temporarily disables the state machine, reconfigures pin mapping,
     * and re-enables it. Useful for multiplexing multiple sensors on
     * different pins with a single PIO instance.
     *
     * @param pin New GPIO pin number.
     */
    void setPin(uint pin);

    /**
     * @brief Send a 1-Wire Reset pulse.
     *
     * Pulls the bus LOW for 480 µs, then releases and waits for the
     * presence pulse. Call isSensorPresent() immediately after to check
     * if any device responded.
     */
    void sendReset();

    /**
     * @brief Check if a device responded to the last Reset pulse.
     *
     * Must be called immediately after sendReset(). The PIO state machine
     * samples the bus during the presence detection window and pushes the
     * result to the RX FIFO.
     *
     * @return true  A device pulled the bus LOW (presence detected).
     * @return false No response within the timeout window.
     */
    bool isSensorPresent();

    /**
     * @brief Write one byte to the 1-Wire bus (LSB first).
     * @param data Byte to transmit.
     */
    void writeByte(uint8_t data);

    /**
     * @brief Read one byte from the 1-Wire bus (LSB first).
     *
     * Sends 8 read time-slots by writing 0xFF and capturing the bus state.
     *
     * @return The byte read from the device, or 0xFF on timeout.
     */
    uint8_t readByte();

private:
    uint        _sm;              ///< Claimed PIO state machine index
    uint        _offset;          ///< PIO instruction memory offset
    PIO         _pio;             ///< PIO block instance (pio0 or pio1)
    pio_sm_config _c;             ///< Cached state machine configuration
    bool        _isInitialized;   ///< Safety flag for RAII destructor

    /**
     * @brief Busy-wait for data in the PIO RX FIFO with timeout.
     * @return true  Data available in the FIFO.
     * @return false Timed out after PIO_TIMEOUT_US microseconds.
     */
    bool _waitForRx();
};
