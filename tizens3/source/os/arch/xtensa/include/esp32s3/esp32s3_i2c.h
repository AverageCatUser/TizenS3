/****************************************************************************
 * arch/xtensa/include/esp32s3/esp32s3_i2c.h
 *
 * TizenS3 software I2C master. Use from apps with:
 *   #include <arch/chip/esp32s3_i2c.h>
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#ifndef __ARCH_XTENSA_INCLUDE_ESP32S3_ESP32S3_I2C_H
#define __ARCH_XTENSA_INCLUDE_ESP32S3_ESP32S3_I2C_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set up the single software I2C bus. khz: 100 or 400 (0 defaults to 400). */
int s3_i2c_init(int scl_pin, int sda_pin, int khz);

/* 7-bit address. Return 0 on success (all bytes ACKed), <0 on error. */
int s3_i2c_write(uint8_t addr, const uint8_t *data, int len);
int s3_i2c_read(uint8_t addr, uint8_t *data, int len);

/* Return 0 if a device at addr ACKs, <0 otherwise. */
int s3_i2c_probe(uint8_t addr);

#ifdef __cplusplus
}
#endif

#endif
