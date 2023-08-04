
#ifndef __SDCC_STACK_AUTO
#error "Code is not reentrancy aware"
#endif

#ifndef DISPLAY_W
#define DISPLAY_W 16
#endif

#ifndef DISPLAY_H
#define DISPLAY_H 16
#endif

#include <8051.h>

#include <stdbool.h>
#include <stdint.h>

__sbit __at(0xB5) g_watchdogResetPin;

void watchdog(void) {
    g_watchdogResetPin = true;
    g_watchdogResetPin = false;
}

__sbit __at(0x90) g_pixelPortDataIn;
__sbit __at(0x91) g_pixelPortClock;
__sbit __at(0x92) g_pixelPortStrobe;
__sbit __at(0x93) g_pixelPortDisable;

__sbit __at(0x94) g_serialReceiveDisable;

/// Send 8 bits of information to the parallel port, MSB first
void pixelPortWriteByte(uint8_t value) {
    for (uint8_t i = 0; i < 8; i++) {
        // Send MSB first
        g_pixelPortDataIn = (value & 0x80);

        // Data shifted in on rising edge
        g_pixelPortClock = false;
        g_pixelPortClock = true;

        // Shift data upwards
        value <<= 1;
    }
}

void delay(void) {
    for (uint8_t i = 0; i < 5; i++) {
        for (uint8_t j = 0; j < 100; j++) {
            watchdog();
        }
    }
}

/// Send a pixel flip command to the distributor
void pixel(uint8_t x, uint8_t y, bool dir) {
    // mode?
    pixelPortWriteByte(0xFE);

    // column bank
    pixelPortWriteByte(x >> 4);

    // column
    pixelPortWriteByte(x & 15);

    // direction + row
    pixelPortWriteByte((~dir << 7) | (y & 15));

    g_pixelPortStrobe = true;
    g_pixelPortStrobe = false;

    watchdog();
}

const uint8_t kSmallHexFont[16][3] = {
    {0b11111, 0b10001, 0b11111}, // 0
    {0b00000, 0b11111, 0b00000}, // 1
    {0b10111, 0b10101, 0b11101}, // 2
    {0b10101, 0b10101, 0b11111}, // 3
    {0b11100, 0b00100, 0b11111}, // 4
    {0b11101, 0b10101, 0b10111}, // 5
    {0b11111, 0b10101, 0b10111}, // 6
    {0b10000, 0b10000, 0b11111}, // 7
    {0b11111, 0b10101, 0b11111}, // 8
    {0b11100, 0b10100, 0b11111}, // 9
    {0b01111, 0b10100, 0b01111}, // A
    {0b11111, 0b10101, 0b01010}, // B
    {0b01110, 0b10001, 0b01010}, // C
    {0b11111, 0b10001, 0b01110}, // D
    {0b11111, 0b10101, 0b10001}, // E
    {0b11111, 0b10100, 0b10000}, // F
};

void hex(uint8_t x, uint8_t y, uint8_t value) {
    value = value & 15;

    for (uint8_t offX = 0; offX < 3; offX++) {
        uint8_t column = kSmallHexFont[value][offX];

        for (uint8_t offY = 0; offY < 5; offY++) {
            pixel(x + offX, y + offY, (column >> (4 - offY)) & 1);
        }
    }
}

void hexString(uint8_t x, uint8_t y, uint8_t data[], uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        hex(x + i * 8 + 0, y, data[i] >> 4);
        hex(x + i * 8 + 4, y, data[i] & 15);
    }
}

volatile __xdata __at(0x8000) uint8_t kConfig = 0;

volatile uint8_t g_recv = 0;

void interrupt_external_0(void) __interrupt(0) {
    // Disabled
}

void interrupt_timer_0(void) __interrupt(1) {
    // Disabled
}

void interrupt_external_1(void) __interrupt(2) {
    // Disabled
}

void interrupt_timer_1(void) __interrupt(3) {
    // Disabled
}

void interrupt_serial(void) __interrupt(4) {
    g_recv = SBUF;

    // Clear interrupt flag to continue receiving interrupts
    RI = false;
}

volatile __xdata __at(0x1FF8 | (1 << 13)) uint8_t g_clockData[7];

void main(void) {
    delay();
    delay();

    // Serial mode setup
    {
        // 11...... = 9 bit UART
        // ..0..... = Disable multiprocessor communication
        // ...1.... = Enable serial reception
        // ....0... = 9th bit to transmit
        SCON = 0b11010000;

        // Enable interrupt
        ES = true;

        // High priority interrupt
        PS = 1;
    }

    // Timer 1 setup (19200 baud rate generator), 11.0592 MHz crystal
    {
        // 0...---- = Enable timer with TR1
        // .0..---- = Use system clock as source
        // ..10---- = 8-bit auto reload timer (reloaded from TH1)
        TMOD = 0b00100000;

        // Required reset value
        TH1 = 0xFD;

        // 1....... = Enable baud rate doubling
        PCON = 0b10000000;
    }

    // Finish startup
    {
        // Enable specified interrupts
        EA = true;

        // Start Timer 1
        TR1 = true;
    }

    g_serialReceiveDisable = false;
    g_pixelPortDisable = false;

    for (uint8_t x = 0; x < DISPLAY_W; x++) {
        for (uint8_t y = 0; y < DISPLAY_H; y++) {
            pixel(x, y, false);
        }
    }

    pixel(9, 2, true);
    pixel(9, 4, true);
    pixel(19, 2, true);
    pixel(19, 4, true);

    if (kConfig & 1) {
        // 1....... = Write
        // .0...... = Read
        // ..1..... = Stop
        // ...01000 = Calibration
        g_clockData[0] = 0b10101000;

        for (uint8_t i = 1; i < 8; i++) {
            g_clockData[i] = 0;
        }
    }

    uint8_t prev = 0;
    uint8_t curr;

    while (true) {
        // 0100..... = Read
        g_clockData[0] = 0b01001000;

        curr = g_clockData[1];

        if (prev != curr) {
            prev = curr;
        } else {
            // 000..... = Update
            g_clockData[0] = 0b00001000;

            for (uint8_t i = 0; i < 10; i++) {
                delay();
            }
            continue;
        }

        // HH:MM:SS
        hexString(1, 1, &g_clockData[3], 1);
        hexString(11, 1, &g_clockData[2], 1);
        hexString(21, 1, &g_clockData[1], 1);

        // 000..... = Update
        g_clockData[0] = 0b00001000;
    }
}
