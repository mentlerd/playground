
#ifndef __SDCC_STACK_AUTO
#error "Code is not reentrancy aware"
#endif

#ifndef DISPLAY_W
#define DISPLAY_W 16
#endif

#ifndef DISPLAY_H
#define DISPLAY_H 16
#endif

/// Due to hardware speed limitations, framebuffer data is stored in a
///  way to make it quick and efficient to blit it onto the pixel matrix.
///
/// This comes at the cost of pixel rendering operations being slightly less
///  efficient/easy to understand, but provides similar (or faster) refresh
///  rate to the original firmware.
///
/// Pixels are stored as tightly packed blocks of 8 in a contiguous array,
/// starting  from the top left corner, and progressing towards the bottom
/// right.
///
/// Each row contains enough blocks to cover the selected display resolution,
///  with additional paddig bits at the end of the last block if the display
///  width is not and exact multiple of blocks.
///
/// Note that these padding pixels are still sent through the pixel port, and
///  the matrix hardware is expected to disregard these commands.
#define BLOCKS_PER_ROW ((DISPLAY_W + 7) / 8)

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

/// Send a pixel flip command to the distributor
void pixelPortWritePixel(uint8_t x, uint8_t y, bool dir) {
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

typedef struct {
    uint8_t blocks[BLOCKS_PER_ROW * DISPLAY_H];
} Framebuffer;

void framebuffer_set_pixel(Framebuffer* this, uint8_t x, uint8_t y,
                           bool target) {
    uint16_t blockIndex = (x / 8) + (y * BLOCKS_PER_ROW);
    uint8_t pixel = x & 7;

    uint8_t block = this->blocks[blockIndex];

    if (block >> pixel != target) {
        this->blocks[blockIndex] = block ^ (1 << pixel);
    }
}

void framebuffer_blit(Framebuffer* this, const Framebuffer* current) {
    uint8_t* targetFront = this->blocks;
    uint8_t* currentFront = current->blocks;

    for (uint8_t y = 0; y < DISPLAY_H; y++) {
        uint8_t x = 0;

        // Process difference in blocks, this way we can early reject blocks
        //  of pixels which are actually in the target state anyway
        for (uint8_t block = 0; block < BLOCKS_PER_ROW; block++) {
            uint8_t target = *(targetFront++);
            uint8_t diff;

            if (current) {
                diff = target ^ *(currentFront)++;
            } else {
                diff = 0xFF;
            }

            for (uint8_t pixel = 0; pixel < 8; pixel++) {
                if (diff & 1) {
                    pixelPortWritePixel(x, y, target & 1);
                }

                target >>= 1;
                diff >>= 1;

                x++;
            }

            watchdog();
        }
    }
}

void delay(void) {
    for (uint8_t i = 0; i < 50; i++) {
        for (uint8_t j = 0; j < 100; j++) {
            watchdog();
        }
    }
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
            pixelPortWritePixel(x + offX, y + offY, (column >> (4 - offY)) & 1);
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

__xdata Framebuffer g_bufferA;
__xdata Framebuffer g_bufferB;

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

    {
        uint8_t* writeFront = g_bufferA.blocks;

        for (uint8_t y = 0; y < DISPLAY_H; y++) {
            for (uint8_t b = 0; b < BLOCKS_PER_ROW; b++) {
                *(writeFront++) = 0;
            }

            watchdog();
        }
    }
    {
        uint8_t* writeFront = g_bufferB.blocks;

        for (uint8_t y = 0; y < DISPLAY_H / 2; y++) {
            for (uint8_t b = 0; b < BLOCKS_PER_ROW; b++) {
                *(writeFront++) = 0b01010101;
            }
            for (uint8_t b = 0; b < BLOCKS_PER_ROW; b++) {
                *(writeFront++) = 0b10101010;
            }

            watchdog();
        }
    }

    g_serialReceiveDisable = false;
    g_pixelPortDisable = false;

    framebuffer_blit(&g_bufferA, 0);

    while (true) {
        framebuffer_blit(&g_bufferB, &g_bufferA);
        delay();

        framebuffer_blit(&g_bufferA, &g_bufferB);
        delay();
    }
}
