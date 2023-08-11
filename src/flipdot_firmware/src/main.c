
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
#define BLOCKS_PER_ROW ((uint8_t)((DISPLAY_W + 7) / 8))

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

void framebuffer_pixel(Framebuffer* this, uint8_t x, uint8_t y, bool target) {
    uint16_t blockIndex = (x / 8) + (y * BLOCKS_PER_ROW);
    uint8_t pixel = x & 7;

    // TODO: 16 bit arithmetic is really weird/broken for some reason
    {
        blockIndex = 0;
        blockIndex = x >> 3;

        for (uint8_t i = 0; i < y; i++) {
            blockIndex += BLOCKS_PER_ROW;
        }
    }

    uint8_t block = this->blocks[blockIndex];

    bool current = (block >> pixel) & 1;
    if (current != target) {
        this->blocks[blockIndex] = block ^ (1 << pixel);
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

void framebuffer_hex(Framebuffer* this, uint8_t x, uint8_t y, uint8_t value) {
    value = value & 15;

    for (uint8_t offX = 0; offX < 3; offX++) {
        uint8_t column = kSmallHexFont[value][offX];

        for (uint8_t offY = 0; offY < 5; offY++) {
            framebuffer_pixel(this, x + offX, y + offY, (column >> (4 - offY)) & 1);
        }
    }
}

void framebuffer_hex_string(Framebuffer* this, uint8_t x, uint8_t y, uint8_t data[], uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        framebuffer_hex(this, x + i * 8 + 0, y, data[i] >> 4);
        framebuffer_hex(this, x + i * 8 + 4, y, data[i] & 15);
    }
}

void framebuffer_blit(Framebuffer* this, const Framebuffer* buffer, bool diff) {
    uint8_t* currentFront = this->blocks;
    uint8_t* targetFront = buffer->blocks;

    for (uint8_t y = 0; y < DISPLAY_H; y++) {
        uint8_t x = 0;

        // Process difference in blocks, this way we can early reject blocks
        //  of pixels which are actually in the target state anyway
        for (uint8_t block = 0; block < BLOCKS_PER_ROW; block++) {
            uint8_t target = *(targetFront++);
            uint8_t mask;

            if (diff) {
                mask = target ^ *currentFront;
            } else {
                mask = 0xFF;
            }

            *(currentFront++) = target;

            for (uint8_t pixel = 0; pixel < 8; pixel++) {
                if (mask & 1) {
                    pixelPortWritePixel(x, y, target & 1);
                }

                target >>= 1;
                mask >>= 1;

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

typedef enum {
    MT_COMMAND = 0,
    MT_DMA = 1,
} MessageType;

typedef struct {
    uint8_t type;
    uint8_t payload[16];
} Message;

typedef enum {
    FS_WANT_FRAME_START = 0,
    FS_WANT_ADDR = 1,
    FS_WANT_TYPE = 2,
    FS_WANT_PAYLOAD = 3,
    FS_WANT_PAYLOAD_CLOSE = 4,
    FS_WANT_CHECKSUM = 5,
    FS_WANT_IGNORE = 6,
} FramerState;

typedef enum {
    ERR_NONE = 0,
    ERR_ADDR = 1,
    ERR_PAYLOAD_TERM = 2,
    ERR_CHECKSUM = 3,
} FramerError;

typedef struct {
    uint8_t numBytesReceived;

    FramerState state;
    FramerError error;

    uint8_t stateBytesRemain;
    uint8_t checksum;

    uint8_t messageWriteFront;
    uint8_t messageAvailableFront;

    uint8_t messageReadFront;

    Message messages[4];
} Framer;

__xdata Framer g_framer;

void framer_init(Framer* this) {
    this->state = FS_WANT_FRAME_START;

    this->messageWriteFront = 1;
    this->messageAvailableFront = 0;
    this->messageReadFront = 0;
}

void framer_accept(Framer* this, uint8_t data) {
    this->numBytesReceived++;

    Message* msg = &this->messages[this->messageWriteFront];

    switch (this->state) {
        case FS_WANT_FRAME_START: {
            if (data != 0x78) {
                return;
            }

            this->checksum = data;

            this->state = FS_WANT_ADDR;
            this->error = ERR_NONE;
        } break;

        case FS_WANT_ADDR: {
            this->checksum += data;

            if (data != 1) {
                this->state = FS_WANT_IGNORE;
                this->stateBytesRemain = 19;

                this->error = ERR_ADDR;
                return;
            }

            this->state = FS_WANT_TYPE;
        } break;

        case FS_WANT_TYPE: {
            this->checksum += data;

            msg->type = data;

            this->state = FS_WANT_PAYLOAD;
            this->stateBytesRemain = sizeof(msg->payload);
        } break;

        case FS_WANT_PAYLOAD: {
            this->checksum += data;

            msg->payload[sizeof(msg->payload) - this->stateBytesRemain - 1] = data;

            if (--this->stateBytesRemain != 0) {
                return;
            }

            this->state = FS_WANT_PAYLOAD_CLOSE;
        } break;

        case FS_WANT_PAYLOAD_CLOSE: {
            this->checksum += data;

            if (data != 0x0) {
                this->state = FS_WANT_IGNORE;
                this->stateBytesRemain = 1;

                this->error = ERR_PAYLOAD_TERM;
                return;
            }

            this->state = FS_WANT_CHECKSUM;
        } break;

        case FS_WANT_CHECKSUM: {
            this->checksum += data;
            this->state = FS_WANT_FRAME_START;

            if (this->checksum != 0) {
                this->error = ERR_CHECKSUM;
                return;
            }

            uint8_t nextFront = this->messageWriteFront + 1;

            // TODO: Arithmetic is broken
            if (nextFront == sizeof(this->messages) - 1) {
                nextFront = 0;
            }

            this->messageWriteFront = nextFront;
        } break;

        case FS_WANT_IGNORE: {
            if (--this->stateBytesRemain != 0) {
                return;
            }

            this->state = FS_WANT_FRAME_START;
        } break;
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
    framer_accept(&g_framer, SBUF);

    // Clear interrupt flag to continue receiving interrupts
    RI = false;
}

volatile __xdata __at(0x1FF8 | (1 << 13)) uint8_t g_clockData[7];

__xdata Framebuffer g_current;
__xdata Framebuffer g_buffer;

void main(void) {
    delay();
    delay();

    // Serial mode setup
    {
        // 01...... = 8 bit UART
        // ..0..... = Disable multiprocessor communication
        // ...1.... = Enable serial reception
        // ....0... = 9th bit to transmit
        SCON = 0b01010000;

        // Enable interrupt
        ES = true;

        // High priority interrupt
        PS = 1;
    }

    // Timer 1 setup (9600 baud rate generator), 11.0592 MHz crystal
    {
        // 0...---- = Enable timer with TR1
        // .0..---- = Use system clock as source
        // ..10---- = 8-bit auto reload timer (reloaded from TH1)
        TMOD = 0b00100000;

        // Required reset value
        TH1 = 0xFD;
    }

    framer_init(&g_framer);

    // Finish startup
    {
        // Enable specified interrupts
        EA = true;

        // Start Timer 1
        TR1 = true;
    }

    g_serialReceiveDisable = false;
    g_pixelPortDisable = false;

    framebuffer_blit(&g_current, &g_buffer, false);

    while (true) {
        for (uint8_t i = 0; i < 16; i++) {
            framebuffer_hex_string(&g_buffer, 0, 0, &g_framer.numBytesReceived, 1);
            framebuffer_hex_string(&g_buffer, 10, 0, &g_framer.state, 1);
            watchdog();

            framebuffer_hex_string(&g_buffer, 20, 0, &g_framer.error, 1);
            framebuffer_hex_string(&g_buffer, 30, 0, &g_framer.messageWriteFront, 1);

            framebuffer_blit(&g_current, &g_buffer, true);
            delay();
        }
    }
}
