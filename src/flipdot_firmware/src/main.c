
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

// ==========[ Configuration ]==========

volatile __xdata __at(0x8000) uint8_t kConfigSwitches = 0;

typedef enum {
    CP_FGY = 0,
    CP_VMX = 1,
} Protocol;

typedef struct {
    Protocol protocol;
} Config;

void config_init(Config* this) { this->protocol = (kConfigSwitches & 1); }

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

// ==========[ Framebuffer ]==========

typedef struct {
    uint8_t pixels[(DISPLAY_W * DISPLAY_H + 7) / 8];
} Framebuffer;

void framebuffer_pixel(Framebuffer* this, uint8_t x, uint8_t y, bool target) {
    if (DISPLAY_W <= x || DISPLAY_H <= y) {
        return;
    }

    uint16_t pixelIndex = (x + y * ((uint8_t)DISPLAY_W));

    uint8_t block = this->pixels[pixelIndex / 8u];
    uint8_t index = pixelIndex % 8u;

    bool current = (block >> index) & 1;
    if (current != target) {
        this->pixels[pixelIndex / 8u] = block ^ (1 << index);
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
    uint8_t* currentFront = this->pixels;
    uint8_t* targetFront = buffer->pixels;

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

            if (++x == DISPLAY_W) {
                x = 0;

                if (++y == DISPLAY_H) {
                    return;
                }
            }
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

// ==========[ Program ]==========

__near Config g_config;

__xdata Framer g_framer;

__xdata Framebuffer g_current;
__xdata Framebuffer g_buffer;

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

void main(void) {
    config_init(&g_config);

    switch (g_config.protocol) {
        case CP_FGY: {
            // 11...... = 9 bit UART
            // ..0..... = Disable multiprocessor communication
            // ...1.... = Enable serial reception
            // ....0... = 9th bit to transmit
            SCON = 0b11010000;

            // 0...---- = Enable timer with TR1
            // .0..---- = Use system clock as source
            // ..10---- = 8-bit auto reload timer (reloaded from TH1)
            TMOD = 0b00100000;

            // (19200 baud rate generator), 11.0592 MHz crystal
            PCON = 0b10000000;
            TH1 = 0xFD;
        } break;

        case CP_VMX: {
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
        } break;
    }

    framer_init(&g_framer);

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
