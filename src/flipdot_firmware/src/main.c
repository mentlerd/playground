
#include <8051.h>

#include <stdbool.h>
#include <stdint.h>

__sbit __at(0x90) p_dataIn;
__sbit __at(0x91) p_clock;
__sbit __at(0x92) p_strobe;
__sbit __at(0x93) p_disable;

/// Send 8 bits of information to the parallel port, MSB first
void p_feed(uint8_t value) {
    for (uint8_t i = 0; i < 8; i++) {
        // Send MSB first
        p_dataIn = (value & 0x80);

        // Data shifted in on rising edge
        p_clock = false;
        p_clock = true;

        // Shift data upwards
        value <<= 1;
    }
}

void delay(void) {
    for (uint8_t i = 0; i < 50; i++) {
        for (uint8_t j = 0; j < 100; j++) {
            // Wait
        }
    }
}

/// Send a pixel flip command to the distributor
void flip(bool dir, uint8_t x, uint8_t y) {
    // mode?
    p_feed(0xFE);

    // column bank
    p_feed(x >> 3);

    // column
    p_feed(x & 7);

    // direction + row
    p_feed((dir << 7) | (y & 15));

    p_strobe = true;
    p_strobe = false;
}

void main(void) {
    delay();
    delay();

    p_disable = false;

    while (true) {
        for (uint8_t y = 0; y < 16; y++) {
            flip(true, 0, y);
            delay();
        }

        for (uint8_t y = 0; y < 16; y++) {
            flip(false, 0, y);
            delay();
        }
    }
}
