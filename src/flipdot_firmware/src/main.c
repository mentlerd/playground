
#ifndef __SDCC_STACK_AUTO
#error "Code is not reentrancy aware"
#endif

#include <8051.h>

#include <stdbool.h>
#include <stdint.h>

// TODO: I don't know why but SDCC links against an incorrect stdlib? Multibyte
//  support routines expect their arguments in variables which are never set.
//
// As a pretty stupid workaround I am including the source in my file to compile the
//  intrinsic with the correct flags.
#include "_mulint.c"

// ==========[ Startup/Watchdog ]==========

__sbit __at(0xB5) g_watchdogResetPin;

unsigned char __sdcc_external_startup(void) {
    // Disable interrupts
    IE = 0;

    // Give time for the hardware to initialize
    for (uint8_t i = 0; i < 100; i++) {
        for (uint8_t j = 0; j < 60; j++) {
            continue;
        }
    }

    // Disable watchdog until initialization is complete
    g_watchdogResetPin = true;
    return 0;
}

void watchdog(void) {
    g_watchdogResetPin = true;
    g_watchdogResetPin = false;
}

// ==========[ Pixel port ]==========

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

// ==========[ Image data ]==========
__data uint8_t g_imageW;
__data uint8_t g_imageH;

__data uint16_t g_imageStride;

__data uint8_t g_imageLimit;

__xdata uint8_t g_pixels[1000];

void pixel(uint8_t x, uint8_t y, bool target) {
    if (g_imageW <= x || g_imageH <= y) {
        return;
    }

    uint16_t pixelIndex = (x + y * g_imageW);
    uint16_t blockIndex = g_imageStride + pixelIndex / 8u;

    uint8_t block = g_pixels[blockIndex];
    uint8_t index = pixelIndex % 8u;

    bool current = (block >> index) & 1;
    if (current != target) {
        g_pixels[blockIndex] = block ^ (1 << index);
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

void hex_char(uint8_t x, uint8_t y, uint8_t value) {
    value = value & 15;

    for (uint8_t offX = 0; offX < 3; offX++) {
        uint8_t column = kSmallHexFont[value][offX];

        for (uint8_t offY = 0; offY < 5; offY++) {
            pixel(x + offX, y + offY, (column >> (4 - offY)) & 1);
        }
    }
}

void hex_str(uint8_t x, uint8_t y, uint8_t data[], uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        hex_char(x + i * 8 + 0, y, data[i] >> 4);
        hex_char(x + i * 8 + 4, y, data[i] & 15);
    }
}

void hex_u8(uint8_t x, uint8_t y, uint8_t value) {
    hex_str(x, y, &value, 1);
}

void hex_u16(uint8_t x, uint8_t y, uint16_t value) {
    hex_u8(x, y, value >> 8);
    hex_u8(x + 8, y, value & 0xFF);
}

void blit(uint8_t image, bool diff) {
    __xdata uint8_t* currentFront = &g_pixels[0];
    __xdata uint8_t* targetFront = &g_pixels[image * g_imageStride];

    uint8_t x = 0;
    uint8_t y = 0;

    while (true) {
        // Consume next block of pixels, determine which needs flipping
        uint8_t target = *(targetFront++);
        uint8_t mask;

        if (diff) {
            mask = target ^ *currentFront;
        } else {
            mask = 0xFF;
        }

        // Overwrite current state
        *(currentFront++) = target;

        // Perform flips
        for (uint8_t i = 0; i < 8; i++) {
            if (mask & 1) {
                pixelPortWritePixel(x, y, target & 1);
            }

            target >>= 1;
            mask >>= 1;

            if (++x == g_imageW) {
                x = 0;

                if (++y == g_imageH) {
                    return;
                }
            }
        }

        watchdog();
    }
}

// ==========[ Serial protocol ]==========

typedef enum {
    FS_WANT_FRAME_START,
    FS_WANT_ADDR,
    FS_WANT_TYPE,
    FS_WANT_DATA,
    FS_WANT_CLOSE,
    FS_WANT_CHECKSUM,
    FS_WANT_IGNORE,
} FramerState;

typedef enum {
    FE_NONE,
    FE_BAD_ADDR,
    FE_BAD_CLOSE,
    FE_BAD_CHECKSUM,
    FE_NO_UPLOAD,
} FramerError;

__data struct {
    uint16_t bytesReceived;

    __xdata uint8_t* front;
} g_upload;

__data struct {
    uint8_t totalBytes;
    uint8_t totalMessages;

    uint8_t addr;

    FramerState state;
    FramerError error;

    uint8_t stateBytesExpected;
    uint8_t stateBytesReceived;

    uint8_t checksum;
    uint8_t type;
    uint8_t data[16];
} g_framer;

void framer_complete(void) {
    g_framer.totalMessages++;

    if (g_framer.type == 0x80) {
        g_upload.bytesReceived += 16;
        g_upload.front += 16;

        if (g_upload.bytesReceived >= g_imageStride) {
            g_upload.front = 0;
        }
        return;
    }

    if (g_framer.data[0] != 0x10) {
        return;
    }

    switch (g_framer.data[1]) {
        case 0x02: { // Image upload start
            g_upload.bytesReceived = 0;
            g_upload.front = &g_pixels[g_imageStride * 2];
        } break;
    }
}

void framer_accept(uint8_t data) {
    g_framer.totalBytes++;
    g_framer.checksum += data;

    switch (g_framer.state) {
        case FS_WANT_FRAME_START: {
            g_framer.checksum = data;

            if (data != 0x78) {
                return;
            }

            g_framer.state = FS_WANT_ADDR;
        } break;

        case FS_WANT_ADDR: {
            if (g_framer.addr != data) {
                g_framer.error = FE_BAD_ADDR;

                g_framer.state = FS_WANT_IGNORE;
                g_framer.stateBytesExpected = 19;
                g_framer.stateBytesReceived = 0;
                return;
            }

            g_framer.state = FS_WANT_TYPE;
        } break;

        case FS_WANT_TYPE: {
            g_framer.type = data;

            if (g_framer.type == 0x80 && !g_upload.front) {
                g_framer.error = FE_NO_UPLOAD;

                g_framer.state = FS_WANT_IGNORE;
                g_framer.stateBytesExpected = 20;
                g_framer.stateBytesReceived = 0;
                return;
            }

            g_framer.state = FS_WANT_DATA;
            g_framer.stateBytesExpected = sizeof(g_framer.data);
            g_framer.stateBytesReceived = 0;
        } break;

        case FS_WANT_DATA: {
            if (g_framer.type == 0x80) {
                *(g_upload.front + g_framer.stateBytesReceived) = data;
            } else {
                g_framer.data[g_framer.stateBytesReceived] = data;
            }

            if (++g_framer.stateBytesReceived != g_framer.stateBytesExpected) {
                return;
            }

            g_framer.state = FS_WANT_CLOSE;
        } break;

        case FS_WANT_CLOSE: {
            if (data != 0x0) {
                g_framer.state = FE_BAD_CLOSE;

                g_framer.state = FS_WANT_IGNORE;
                g_framer.stateBytesExpected = 1;
                g_framer.stateBytesReceived = 0;
                return;
            }

            g_framer.state = FS_WANT_CHECKSUM;
        } break;

        case FS_WANT_CHECKSUM: {
            if (g_framer.checksum == 0) {
                g_framer.error = FE_NONE;

                framer_complete();
            } else {
                g_framer.error = FE_BAD_CHECKSUM;
            }

            g_framer.state = FS_WANT_FRAME_START;
        } break;

        case FS_WANT_IGNORE: {
            if (++g_framer.stateBytesReceived != g_framer.stateBytesExpected) {
                return;
            }

            g_framer.state = FS_WANT_FRAME_START;
        } break;
    }
}

// ==========[ Program ]==========

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
    framer_accept(SBUF);

    // Clear interrupt flag to continue receiving interrupts
    RI = false;
}

void main(void) {
    {
        // 01...... = 8 bit UART
        // ..0..... = Disable multiprocessor communication
        // ...1.... = Enable serial reception
        // ....0... = 9th bit to transmit
        SCON = 0b01010000;

        // 0...---- = Enable timer with TR1
        // .0..---- = Use system clock as source
        // ..10---- = 8-bit auto reload timer (reloaded from TH1)
        TMOD = 0b00100000;

        // (9600 baud rate generator), 11.0592 MHz crystal
        PCON = 0b00000000;
        TH1 = 0xFD;
    }

    {
        g_imageW = 32;
        g_imageH = 16;

        g_imageStride = (g_imageW * g_imageH + 7) / 8u;
        g_imageLimit = sizeof(g_pixels) / g_imageStride;

        g_framer.addr = 1;
    }

    // Finish startup
    {
        // Enable high priority serial interrupt
        ES = true;
        PS = 1;

        // Enable specified interrupts
        EA = true;

        // Start Timer 1
        TR1 = true;
    }

    g_serialReceiveDisable = false;
    g_pixelPortDisable = false;

    blit(0, false);

    while (true) {
        // Cycle through images
        for (uint8_t image = 1; image <= 2; image++) {

            // Update debug image with info
            hex_u8(0, 0, g_framer.totalBytes);
            hex_u8(8, 0, g_framer.totalMessages);

            hex_u8(0, 5, g_framer.state);
            hex_u8(8, 5, g_framer.error);

            hex_u8(0, 11, g_framer.type);
            hex_u8(8, 11, g_framer.data[1]);

            // Draw active image
            blit(image, true);

            for (uint8_t j = 0; j < 200; j++) {
                for (uint8_t k = 0; k < 200; k++) {
                    watchdog();
                }
            }
        }
    }
}
