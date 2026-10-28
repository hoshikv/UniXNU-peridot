/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "GoodixBerlin.h"
#include <stdio.h>
#include <string.h>

static uint16_t
le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t
le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool
checksum_ok(const uint8_t *data, size_t length)
{
    uint32_t sum = 0;
    bool blank = true;

    if (length < 2) {
        return false;
    }
    for (size_t i = 0; i < length - 2; i++) {
        sum += data[i];
        blank &= data[i] == 0 || data[i] == 0xff;
    }
    return !blank && (uint16_t)sum == le16(data + length - 2);
}

bool
goodix_berlin_read(struct goodix_berlin_chip *chip, uint32_t address, uint8_t *data, size_t length)
{
    uint8_t tx[8 + GOODIX_BERLIN_MAX_READ];
    uint8_t rx[8 + GOODIX_BERLIN_MAX_READ];

    if (!chip || !chip->bus || !chip->bus->transfer || !data || length > GOODIX_BERLIN_MAX_READ) {
        return false;
    }
    tx[0] = 0xf1;
    tx[1] = (uint8_t)(address >> 24);
    tx[2] = (uint8_t)(address >> 16);
    tx[3] = (uint8_t)(address >> 8);
    tx[4] = (uint8_t)address;
    tx[5] = tx[6] = tx[7] = 0xff;
    memset(tx + 8, 0, length);
    if (!chip->bus->transfer(chip->bus->context, tx, rx, 8 + length)) {
        return false;
    }
    memcpy(data, rx + 8, length);
    return true;
}

bool
goodix_berlin_write(struct goodix_berlin_chip *chip, uint32_t address, const uint8_t *data, size_t length)
{
    uint8_t tx[5 + GOODIX_BERLIN_MAX_READ];

    if (!chip || !chip->bus || !chip->bus->transfer || (!data && length) || length > GOODIX_BERLIN_MAX_READ) {
        return false;
    }
    tx[0] = 0xf0;
    tx[1] = (uint8_t)(address >> 24);
    tx[2] = (uint8_t)(address >> 16);
    tx[3] = (uint8_t)(address >> 8);
    tx[4] = (uint8_t)address;
    if (length) {
        memcpy(tx + 5, data, length);
    }
    return chip->bus->transfer(chip->bus->context, tx, NULL, 5 + length);
}

bool
goodix_berlin_confirm(struct goodix_berlin_chip *chip)
{
    static const uint8_t pattern[8] = { 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa };
    uint8_t answer[sizeof(pattern)];

    for (int attempt = 0; attempt < 3; attempt++) {
        if (goodix_berlin_write(chip, 0x10000, pattern, sizeof(pattern)) &&
            goodix_berlin_read(chip, 0x10000, answer, sizeof(answer)) &&
            memcmp(answer, pattern, sizeof(pattern)) == 0) {
            return true;
        }
        if (chip->bus && chip->bus->delay_ms) {
            chip->bus->delay_ms(chip->bus->context, 5);
        }
    }
    return false;
}

bool
goodix_berlin_probe(struct goodix_berlin_chip *chip, bool verbose)
{
    uint8_t info[GOODIX_BERLIN_MAX_READ];
    size_t length, offset;

    if (!chip || !goodix_berlin_read(chip, GOODIX_BERLIN_FW_VERSION, chip->version, sizeof(chip->version))) {
        return false;
    }
    if (!checksum_ok(chip->version, sizeof(chip->version))) {
        if (verbose) {
            printf("goodix: no valid firmware version: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                chip->version[0], chip->version[1], chip->version[2], chip->version[3],
                chip->version[4], chip->version[5], chip->version[6], chip->version[7]);
        }
        return false;
    }
    printf("goodix: chip \"%.6s\", firmware \"%.8s\" %02x.%02x.%02x.%02x, sensor %u\n",
        (const char *)chip->version, (const char *)chip->version + 10, chip->version[18], chip->version[19],
        chip->version[20], chip->version[21], chip->version[23]);

    if (!goodix_berlin_read(chip, GOODIX_BERLIN_IC_INFO, info, 2)) {
        return false;
    }
    length = le16(info);
    if (length < 40 || length >= GOODIX_BERLIN_MAX_READ ||
        !goodix_berlin_read(chip, GOODIX_BERLIN_IC_INFO, info, length) ||
        !checksum_ok(info, length)) {
        printf("goodix: bad chip description (length %zu)\n", length);
        return false;
    }
    offset = 2 + 16 + 10 + 4;
    for (int i = 0; i < 5; i++) {
        if (offset >= length) {
            return false;
        }
        offset += 1 + 2u * info[offset];
    }
    if (offset + 48 + 2 > length) {
        printf("goodix: chip description too short (%zu, lists end at %zu)\n", length, offset);
        return false;
    }
    chip->touch_address = le32(info + offset + 44);
    chip->command_address = le32(info + offset);
    if (offset + 122 + 20 + 2 <= length) {
        uint16_t x = le16(info + offset + 122 + 16), y = le16(info + offset + 122 + 18);

        if (x && y) {
            chip->max_x = x;
            chip->max_y = y;
        }
    }
    printf("goodix: touch report at %#x, coordinates up to %u x %u\n",
        chip->touch_address, chip->max_x, chip->max_y);
    return chip->touch_address != 0;
}

bool
goodix_berlin_command(struct goodix_berlin_chip *chip, uint8_t command, uint8_t data)
{
    uint8_t buffer[16], answer[16];
    uint32_t sum;

    if (!chip || !chip->command_address) {
        return false;
    }
    memset(buffer, 0, sizeof(buffer));
    buffer[2] = 5;
    buffer[3] = command;
    buffer[4] = data;
    sum = buffer[2] + buffer[3] + buffer[4];
    buffer[5] = (uint8_t)sum;
    buffer[6] = (uint8_t)(sum >> 8);
    for (int attempt = 0; attempt < 6; attempt++) {
        if (!goodix_berlin_write(chip, chip->command_address, buffer, sizeof(buffer))) {
            return false;
        }
        for (int look = 0; look < 6; look++) {
            if (!goodix_berlin_read(chip, chip->command_address, answer, sizeof(answer))) {
                return false;
            }
            if (answer[1] == 0x80) {
                return true;
            }
            if (chip->bus->delay_ms) {
                chip->bus->delay_ms(chip->bus->context, 1);
            }
            if (answer[1] != 0x02 && answer[1] != 0) {
                break;
            }
        }
    }
    return false;
}

enum goodix_berlin_poll
goodix_berlin_poll(struct goodix_berlin_chip *chip, struct goodix_berlin_contact *contacts, unsigned *count)
{
    uint8_t report[8 + 8 * GOODIX_BERLIN_MAX_TOUCHES + 2];
    const uint8_t zero = 0;
    const size_t first = 8 + 8 * 2 + 2;
    unsigned touches;

    if (!chip || !contacts || !count) {
        return GOODIX_BERLIN_POLL_ERROR;
    }
    *count = 0;
    if (!goodix_berlin_read(chip, chip->touch_address, report, first) || report[0] == 0) {
        return GOODIX_BERLIN_POLL_IDLE;
    }
    touches = report[2] & 0xf;
    if ((report[0] & GOODIX_BERLIN_STATUS_TOUCH) && touches > 2 && touches <= GOODIX_BERLIN_MAX_TOUCHES &&
        !goodix_berlin_read(chip, chip->touch_address + 8, report + 8, 8 * touches + 2)) {
        return GOODIX_BERLIN_POLL_ERROR;
    }
    if (!goodix_berlin_write(chip, chip->touch_address, &zero, 1)) {
        return GOODIX_BERLIN_POLL_ERROR;
    }
    if (!checksum_ok(report, 8)) {
        return GOODIX_BERLIN_POLL_ERROR;
    }
    if (report[0] & GOODIX_BERLIN_STATUS_REQUEST) {
        return (report[2] & 0xf) == GOODIX_BERLIN_REQUEST_RESET || report[2] == GOODIX_BERLIN_REQUEST_RESET
            ? GOODIX_BERLIN_POLL_RESET : GOODIX_BERLIN_POLL_IDLE;
    }
    if (!(report[0] & GOODIX_BERLIN_STATUS_TOUCH)) {
        return GOODIX_BERLIN_POLL_IDLE;
    }
    if (touches > GOODIX_BERLIN_MAX_TOUCHES || (touches && !checksum_ok(report + 8, 8 * touches + 2))) {
        return GOODIX_BERLIN_POLL_ERROR;
    }
    for (unsigned i = 0; i < touches; i++) {
        const uint8_t *touch = report + 8 + 8 * i;
        unsigned id = touch[0] >> 4, kind = touch[0] & 0xf;

        if (id >= GOODIX_BERLIN_MAX_TOUCHES || kind == 1 || kind == 3) {
            continue;
        }
        contacts[*count].id = (uint8_t)id;
        contacts[*count].x = le16(touch + 2);
        contacts[*count].y = le16(touch + 4);
        (*count)++;
    }
    return GOODIX_BERLIN_POLL_TOUCH;
}
