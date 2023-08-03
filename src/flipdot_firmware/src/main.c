
#ifndef __SDCC_STACK_AUTO
#error "Code is not reentrancy aware"
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

    uint8_t prev = 0;
    uint8_t curr = 0;

    while (true) {
        for (uint8_t i = 0; i < 32; i++) {
            curr = g_recv;

            if (prev != curr) {
                prev = curr;

                hexString(&prev, 1);
            }

            flip(i > 15, i & 15, 10);

            delay();
            delay();
        }
    }
}
