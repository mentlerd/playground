
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
void flip(bool dir, uint8_t x, uint8_t y) {
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
            flip((column >> (4 - offY)) & 1, x + offX, y + offY);
        }
    }
}

void hexString(uint8_t data[], uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        hex(i * 8 + 0, 0, data[i] >> 4);
        hex(i * 8 + 4, 0, data[i] & 15);
    }
}

volatile __xdata __at(0x8000) uint8_t kConfig = 0;

void main(void) {
    delay();
    delay();

    g_pixelPortDisable = false;

    while (true) {
        hexString(&kConfig, 1);

        for (uint8_t d = 0; d < 100; d++) {
            delay();
        }

        for (uint8_t dir = 0; dir < 2; dir++) {
            for (uint8_t y = 0; y < 16; y++) {
                for (uint8_t x = 0; x < 32; x++) {
                    flip(!dir, x, y);
                }
                delay();
            }

            delay();
            delay();
        }
    }
}
