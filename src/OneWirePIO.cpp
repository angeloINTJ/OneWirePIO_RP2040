/**
 * @file OneWirePIO.cpp
 * @brief Implementation of the hardware-accelerated 1-Wire PHY layer.
 * @see OneWirePIO.h for full API documentation.
 */

#include "OneWirePIO.h"
#include "onewire_protocol.pio.h"
#include "hardware/clocks.h"

/// Maximum time (µs) to wait for the PIO state machine RX FIFO response.
static const uint32_t PIO_TIMEOUT_US = 5000;

// --------------------------------------------------------------------
// Construction / Destruction (RAII)
// --------------------------------------------------------------------

OneWirePIO::OneWirePIO(PIO pio_instance)
    : _sm(0)
    , _offset(0)
    , _pio(pio_instance)
    , _c{}
    , _isInitialized(false)
{
}

OneWirePIO::~OneWirePIO() {
    if (_isInitialized) {
        pio_sm_set_enabled(_pio, _sm, false);
        pio_sm_unclaim(_pio, _sm);
        pio_remove_program(_pio, &onewire_protocol_program, _offset);
        _isInitialized = false;
    }
}

// --------------------------------------------------------------------
// Initialization
// --------------------------------------------------------------------

bool OneWirePIO::begin(uint pin) {
    // Check if there is room in instruction memory for the PIO program
    if (!pio_can_add_program(_pio, &onewire_protocol_program)) {
        return false;
    }

    // Try to claim an unused state machine (non-blocking)
    int sm = pio_claim_unused_sm(_pio, false);
    if (sm < 0) {
        return false;
    }

    _sm     = static_cast<uint>(sm);
    _offset = pio_add_program(_pio, &onewire_protocol_program);

    // Build the base configuration
    _c = pio_get_default_sm_config();
    sm_config_set_wrap(&_c, _offset + 0, _offset + 26);
    sm_config_set_out_shift(&_c, true, false, 32);   // Shift right, no autopull
    sm_config_set_in_shift(&_c, true, false, 32);     // Shift right, no autopush

    // Clock divider: 1 tick = 1 µs (required by the PIO timing program)
    float clk_div = static_cast<float>(clock_get_hz(clk_sys)) / 1000000.0f;
    sm_config_set_clkdiv(&_c, clk_div);

    // Configure the GPIO and start the state machine
    setPin(pin);

    _isInitialized = true;
    return true;
}

// --------------------------------------------------------------------
// Pin Management
// --------------------------------------------------------------------

void OneWirePIO::setPin(uint pin) {
    pio_sm_set_enabled(_pio, _sm, false);

    // Hand GPIO control to the PIO block
    pio_gpio_init(_pio, pin);
    pio_sm_set_pins_with_mask(_pio, _sm, 0, (1u << pin));      // Output LOW
    pio_sm_set_pindirs_with_mask(_pio, _sm, 0, (1u << pin));   // Direction: input

    // Map SET and IN to the chosen pin
    sm_config_set_set_pins(&_c, pin, 1);
    sm_config_set_in_pins(&_c, pin);

    // Apply configuration and restart
    pio_sm_init(_pio, _sm, _offset, &_c);
    pio_sm_set_enabled(_pio, _sm, true);
}

// --------------------------------------------------------------------
// Low-Level Bus Operations
// --------------------------------------------------------------------

bool OneWirePIO::_waitForRx() {
    uint32_t start = micros();
    while (pio_sm_is_rx_fifo_empty(_pio, _sm)) {
        if (micros() - start > PIO_TIMEOUT_US) {
            return false;
        }
        tight_loop_contents();  // Hint: keep the core in a tight spin
    }
    return true;
}

void OneWirePIO::sendReset() {
    pio_sm_clear_fifos(_pio, _sm);

    // Command word encoding for the PIO program:
    //   bit  0     : 1 = reset command
    //   bits 1-10  : tRSTL  = 480 µs (bus LOW duration)
    //   bits 11-17 : tPDHIGH = 70 µs (wait before sampling presence)
    //   bits 18+   : tPDLOW  = 410 µs (remaining slot time)
    uint32_t command = 1u | (480u << 1) | (70u << 11) | (410u << 18);
    pio_sm_put_blocking(_pio, _sm, command);
}

bool OneWirePIO::isSensorPresent() {
    if (!_waitForRx()) {
        return false;
    }
    // PIO pushes 0 when presence pulse detected (bus pulled LOW by device)
    return (pio_sm_get(_pio, _sm) == 0);
}

void OneWirePIO::writeByte(uint8_t data) {
    // Command word: data bits shifted left by 1, bit 0 = 0 (write/read mode)
    pio_sm_put_blocking(_pio, _sm, (static_cast<uint32_t>(data) << 1) | 0u);

    // Drain the echo byte pushed back by the PIO program
    if (_waitForRx()) {
        pio_sm_get(_pio, _sm);
    } else {
        pio_sm_clear_fifos(_pio, _sm);
    }
}

uint8_t OneWirePIO::readByte() {
    // Send 0xFF to generate 8 read time-slots
    pio_sm_put_blocking(_pio, _sm, (0xFFu << 1) | 0u);

    if (!_waitForRx()) {
        pio_sm_clear_fifos(_pio, _sm);
        return 0xFF;  // Timeout — return all-ones (bus idle state)
    }

    // PIO shifts bits in from the MSB side; the byte sits in the top 8 bits
    return static_cast<uint8_t>(pio_sm_get(_pio, _sm) >> 24);
}
