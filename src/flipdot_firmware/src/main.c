
#include <8051.h>

#include <stdbool.h>
#include <stdint.h>

__sbit __at(0x90) p_dataIn;
__sbit __at(0x91) p_clock;
__sbit __at(0x92) p_strobe;
__sbit __at(0x93) p_outputEnable;

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

void delay(void) {
	for (uint8_t i = 0; i < 100; i++) {
		for (uint8_t j = 0; j < 100; j++) {
			continue;
		}
	}
}

void main(void) {
    while (true) {
		for (uint8_t state = 0; state < 2; state++) {
			for (uint8_t x = 0; x < 16; x++) {
				for (uint8_t y = 0; y < 16; y++) {
					flip(state, x, y);
					delay();
				}
			}

			delay();
		}
    }
}
