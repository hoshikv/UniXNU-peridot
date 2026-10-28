/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef PERIDOT_GOODIX_BERLIN_H
#define PERIDOT_GOODIX_BERLIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GOODIX_BERLIN_MAX_TOUCHES 10
#define GOODIX_BERLIN_MAX_READ    1024

#define GOODIX_BERLIN_FW_VERSION  0x10014
#define GOODIX_BERLIN_IC_INFO     0x10070
#define GOODIX_BERLIN_STATUS_TOUCH   0x80
#define GOODIX_BERLIN_STATUS_REQUEST 0x40
#define GOODIX_BERLIN_REQUEST_RESET  3

struct goodix_berlin_bus {
    void *context;
    bool (*transfer)(void *context, const uint8_t *tx, uint8_t *rx, size_t length);
    void (*delay_ms)(void *context, unsigned milliseconds);
    void (*reset)(void *context);
};

struct goodix_berlin_contact {
    uint8_t id;
    uint16_t x;
    uint16_t y;
};

enum goodix_berlin_poll {
    GOODIX_BERLIN_POLL_IDLE,
    GOODIX_BERLIN_POLL_TOUCH,
    GOODIX_BERLIN_POLL_RESET,
    GOODIX_BERLIN_POLL_ERROR,
};

struct goodix_berlin_chip {
    const struct goodix_berlin_bus *bus;
    uint32_t touch_address;
    uint32_t command_address;
    uint32_t max_x;
    uint32_t max_y;
    uint8_t version[28];
};

bool goodix_berlin_read(struct goodix_berlin_chip *chip, uint32_t address, uint8_t *data, size_t length);
bool goodix_berlin_write(struct goodix_berlin_chip *chip, uint32_t address, const uint8_t *data, size_t length);
bool goodix_berlin_confirm(struct goodix_berlin_chip *chip);
bool goodix_berlin_probe(struct goodix_berlin_chip *chip, bool verbose);
bool goodix_berlin_command(struct goodix_berlin_chip *chip, uint8_t command, uint8_t data);
enum goodix_berlin_poll goodix_berlin_poll(struct goodix_berlin_chip *chip,
    struct goodix_berlin_contact *contacts, unsigned *count);

#endif
