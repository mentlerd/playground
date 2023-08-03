
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
    pixelPortWriteByte((dir << 7) | (y & 15));

    g_pixelPortStrobe = true;
    g_pixelPortStrobe = false;

    watchdog();
}

void main(void) {
    delay();
    delay();

    g_pixelPortDisable = false;

    while (true) {
        for (uint8_t dir = 0; dir < 2; dir++) {
            for (uint8_t y = 0; y < 16; y++) {
                for (uint8_t x = 0; x < 32; x++) {
                    flip(dir, x, y);
                }
                delay();
            }

            delay();
            delay();
        }
    }
}
