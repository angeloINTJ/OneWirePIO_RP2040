# OneWirePIO_RP2040

**Hardware-accelerated 1-Wire & DS18B20 library for the Raspberry Pi Pico / Pico W.**

All critical 1-Wire timing is offloaded to the RP2040's PIO coprocessor — zero bit-banging, zero interrupt-disabling, deterministic communication even under heavy CPU load.

---

## Features

- **PIO-accelerated** — Reset, Write, and Read are handled entirely by hardware state machines
- **Jitter-free** — timing is independent of CPU load, ISRs, or Wi-Fi stack activity
- **RAII resource management** — PIO state machines and instruction memory are automatically released on destruction
- **Runtime pin switching** — multiplex multiple sensors through a single PIO instance via `setPin()`
- **CRC-8 validation** — every temperature read is integrity-checked
- **Configurable resolution** — 9-bit (94 ms) to 12-bit (750 ms)
- **Multi-scale output** — Celsius, Fahrenheit, or Kelvin
- **Full diagnostics** — raw scratchpad access, ROM code reading, structured error states
- **Minimal footprint** — 27 PIO instructions, lightweight C++ classes

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| Raspberry Pi Pico / Pico W | RP2040-based board |
| DS18B20 | Dallas 1-Wire temperature sensor |
| 4.7 kΩ resistor | Pull-up between data line and 3.3V |

### Wiring Diagram

```
Pico GP2 ──┬── DS18B20 DQ (Data)
           │
         4.7kΩ
           │
         3.3V

DS18B20 VDD ── 3.3V
DS18B20 GND ── GND
```

## Installation

### Arduino IDE

1. Download this repository as a `.zip` file
2. Go to **Sketch → Include Library → Add .ZIP Library...**
3. Select the downloaded file
4. Select your board: **Tools → Board → Raspberry Pi Pico / Pico W**

### PlatformIO

Add to your `platformio.ini`:

```ini
[env:pico]
platform = raspberrypi
board = pico
framework = arduino
lib_deps =
    https://github.com/angeloINTJ/OneWirePIO_RP2040.git
```

### Arduino Library Manager

Open the Arduino IDE, go to **Sketch → Include Library → Manage Libraries...**, search for `OneWirePIO_RP2040` and click **Install**.

## Quick Start

```cpp
#include <OneWirePIO.h>
#include <DS18B20PIO.h>

OneWirePIO bus(pio0);
DS18B20PIO sensor(bus);

void setup() {
    Serial.begin(115200);
    bus.begin(2);  // GP2
}

void loop() {
    sensor.requestTemperatures(2);
    delay(sensor.getConversionDelay());

    float temp;
    if (sensor.getTemperatureValidated(2, temp)) {
        Serial.print("Temperature: ");
        Serial.print(temp, 2);
        Serial.println(" °C");
    }
    delay(2000);
}
```

## Examples

| Example | Description |
|---------|-------------|
| [BasicTemperature](examples/BasicTemperature/) | Minimal read loop — start here |
| [MultiSensor](examples/MultiSensor/) | Multiple DS18B20 on different pins with one PIO instance |
| [DiagnosticMode](examples/DiagnosticMode/) | ROM code, raw scratchpad, CRC inspection |
| [ResolutionBenchmark](examples/ResolutionBenchmark/) | Compare 9/10/11/12-bit timing and precision |
| [TemperatureScales](examples/TemperatureScales/) | Simultaneous Celsius, Fahrenheit, and Kelvin output |

## API Reference

### OneWirePIO (Physical Layer)

```cpp
OneWirePIO bus(pio0);       // or pio1
bool ok = bus.begin(pin);   // Initialize PIO + claim state machine
bus.setPin(pin);             // Switch GPIO at runtime
bus.sendReset();             // 480 µs reset pulse
bool present = bus.isSensorPresent();  // Check presence after reset
bus.writeByte(0xCC);         // Write one byte (LSB first)
uint8_t b = bus.readByte();  // Read one byte (LSB first)
```

### DS18B20PIO (Sensor Driver)

```cpp
DS18B20PIO sensor(bus);

// Temperature
sensor.requestTemperatures(pin);
delay(sensor.getConversionDelay());
float temp;
sensor.getTemperatureValidated(pin, temp, DS18B20PIO::CELSIUS);

// Resolution
sensor.setResolution(pin, DS18B20PIO::RES_9_BIT);  // 9, 10, 11, or 12

// Identification
uint8_t rom[8];
sensor.readROM(pin, rom);

// Diagnostics
uint8_t scratch[9];
float temp;
bool crcErr;
sensor.getDiagnosticData(pin, scratch, temp, crcErr);

// Error handling
DS18B20PIO::State state = sensor.getState();
char msg[64];
sensor.getLastErrorString(msg, sizeof(msg), pin);
```

## How It Works

The RP2040 has two PIO (Programmable I/O) blocks, each with 4 state machines and 32 instruction slots. This library loads a 27-instruction program that implements the full 1-Wire physical layer in hardware:

1. **Reset/Presence** — 480 µs LOW pulse, then samples the bus for a device presence response
2. **Write** — generates precise 1/60 µs LOW pulses for logical 1/0
3. **Read** — pulls LOW for 1 µs, releases, and samples the bus at exactly 14 µs

The PIO clock runs at 1 MHz (1 µs/tick), derived from the system clock via a configurable divider. All timing is cycle-accurate regardless of what the ARM cores are doing.

## Architecture

```
┌──────────────┐     ┌──────────────┐
│  DS18B20PIO  │────▶│  OneWirePIO  │
│  (Sensor)    │     │  (PHY Layer) │
│              │     │              │
│ • CRC check  │     │ • PIO mgmt   │
│ • Resolution │     │ • sendReset  │
│ • Temp scale │     │ • writeByte  │
│ • Diagnostics│     │ • readByte   │
└──────────────┘     └──────┬───────┘
                            │
                     ┌──────▼───────┐
                     │  RP2040 PIO  │
                     │  State Mach. │
                     │              │
                     │ 27-instr asm │
                     │ 1 µs/tick    │
                     └──────┬───────┘
                            │
                        GPIO pin
                            │
                       ┌────▼────┐
                       │ DS18B20 │
                       └─────────┘
```

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a pull request.

### Quick Guide

1. Fork this repository
2. Create a feature branch: `git checkout -b feature/my-improvement`
3. Commit your changes: `git commit -m "Add: description of change"`
4. Push to the branch: `git push origin feature/my-improvement`
5. Open a Pull Request

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.

## Acknowledgments

- Dallas Semiconductor / Maxim Integrated for the 1-Wire protocol specification
- Raspberry Pi Foundation for the RP2040 PIO architecture
- The Arduino-Pico community for the RP2040 Arduino core

## See Also

- [DHT22PIO_RP2040](https://github.com/angeloINTJ/DHT22PIO_RP2040) — PIO-accelerated DHT22 library by the same author
