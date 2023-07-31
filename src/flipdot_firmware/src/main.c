
#include <8051.h>

#include <stdbool.h>
#include <stdint.h>

__sbit __at(0x80) p_dataIn;
__sbit __at(0x81) p_clock;
__sbit __at(0x82) p_strobe;
__sbit __at(0x83) p_outputEnable;

/// Send 8 bits of information to the parallel port, MSB first
void p_feed(uint8_t value) {
    for (uint8_t i = 0; i < 8; i++) {
        // Send MSB first
        p_dataIn = (value & 0x80);

        // Data shifted in on rising edge
        p_clock = true;
        p_clock = false;

        // Shift data upwards
        value = value << 1;
    }
}

/// Send a pixel flip command to the distributor
void flip(bool dir, uint8_t x, uint8_t y) {
    p_feed(0);                     // mode?
    p_feed(x & 3);                 // column
    p_feed(x >> 3);                // column bank
    p_feed((dir << 7) | (y & 15)); // dir + row index

    p_strobe = true;

    p_clock = false;
    p_clock = true;

    p_strobe = false;
}

void main(void) {
    flip(true, 2, 3);

    while (true) {
    }
}
