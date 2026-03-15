/**
 * @file onewire_protocol.pio.h
 * @brief Pre-assembled PIO program for the Dallas 1-Wire protocol.
 *
 * This file contains the binary representation of a PIO state machine program
 * that implements the complete 1-Wire physical layer: Reset/Presence detection,
 * bit-level Write, and bit-level Read — all with cycle-accurate timing.
 *
 * The program runs at 1 MHz (1 µs per instruction cycle) and uses a single
 * GPIO configured as open-drain via the SET pindirs mechanism.
 *
 * Protocol timing (per Dallas Semiconductor specifications):
 *   - Reset pulse:    480 µs LOW, then 70 µs wait, then sample presence
 *   - Write 0 slot:   60 µs LOW, then release
 *   - Write 1 slot:    1 µs LOW, then release (device samples at ~15 µs)
 *   - Read slot:        1 µs LOW, release, sample at ~14 µs
 *
 * Command word encoding (32-bit value written to TX FIFO):
 *   - Bit 0 = 1 → Reset command; bits 1-10 = tRSTL, 11-17 = tPDHIGH, 18+ = tPDLOW
 *   - Bit 0 = 0 → Write/Read; bits 1-8 = data byte (send 0xFF to read)
 *
 * @note This header is auto-generated from onewire_protocol.pio.
 *       Do not edit manually unless you understand RP2040 PIO assembly.
 *
 * @author Ângelo
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "hardware/pio.h"

/**
 * @brief Pre-assembled PIO instruction array for the 1-Wire protocol.
 *
 * 27 instructions total, fitting in a single PIO instruction memory block.
 * Organized in three logical sections:
 *   [0-2]   Command dispatcher (reset vs. write/read)
 *   [3-13]  Reset and presence detection routine
 *   [14-26] Byte write/read loop (8 bits, LSB first)
 */
static const uint16_t onewire_protocol_instructions[] = {

    // === COMMAND DISPATCHER ===
    0x80a0, //  0: pull   block              ; Wait for command word from CPU
    0x6021, //  1: out    x, 1               ; Extract bit 0 → X (command type)
    0x002e, //  2: jmp    !x, 14             ; X=0 → Write/Read, X=1 → Reset

    // === RESET AND PRESENCE DETECTION ===
    0x602a, //  3: out    x, 10              ; X = tRSTL (reset LOW duration)
    0xe081, //  4: set    pindirs, 1         ; Drive bus LOW
    0x0045, //  5: jmp    x--, 5             ; Wait tRSTL microseconds
    0x6027, //  6: out    x, 7               ; X = tPDHIGH (presence wait)
    0xe080, //  7: set    pindirs, 0         ; Release bus (input / pull-up)
    0x0048, //  8: jmp    x--, 8             ; Wait tPDHIGH microseconds
    0x4001, //  9: in     pins, 1            ; Sample bus → ISR (0 = presence)
    0x8020, // 10: push   block              ; Send presence result to CPU
    0x602a, // 11: out    x, 10              ; X = tPDLOW (remaining slot time)
    0x004c, // 12: jmp    x--, 12            ; Wait tPDLOW microseconds
    0x0000, // 13: jmp    0                  ; Return to idle (wait for next cmd)

    // === WRITE / READ BYTE LOOP (8 bits, LSB first) ===
    0xe047, // 14: set    y, 7               ; Y = bit counter (7 → 0)
    0xe081, // 15: set    pindirs, 1         ; Drive bus LOW (start of time slot)
    0x6021, // 16: out    x, 1               ; X = next data bit to send
    0x0036, // 17: jmp    !x, 22             ; X=0 → Write-0 path

    // --- Write-1 / Read path ---
    0xed80, // 18: set    pindirs, 0  [13]   ; Release bus after ~1 µs LOW
    0x5f01, // 19: in     pins, 1     [31]   ; Sample bus at ~14 µs (read window)
    0xbb42, // 20: nop                [27]   ; Pad slot to ~60 µs total
    0x0019, // 21: jmp    25                 ; Skip to end-of-loop

    // --- Write-0 path ---
    0xbf42, // 22: nop                [31]   ; Hold bus LOW for ~60 µs
    0x5b61, // 23: in     null, 1     [27]   ; Shift a 0-bit into ISR (alignment)
    0xe080, // 24: set    pindirs, 0         ; Release bus

    // --- End of bit loop ---
    0x028f, // 25: jmp    y--, 15     [2]    ; Next bit (with 2-cycle recovery)
    0x8020  // 26: push   block              ; All 8 bits done → send byte to CPU
};

/**
 * @brief PIO program descriptor for the RP2040 SDK.
 *
 * Used by pio_add_program() and pio_can_add_program() for dynamic
 * instruction memory management. origin = -1 allows the SDK to place
 * the program at any available offset.
 */
static const struct pio_program onewire_protocol_program = {
    .instructions = onewire_protocol_instructions,
    .length       = 27,
    .origin       = -1,
};
