/****************************************************************************
 * arch/xtensa/include/esp32s3/esp32s3_gpio.h
 *
 * TizenS3 GPIO API. Use from apps with: #include <arch/chip/esp32s3_gpio.h>
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#ifndef __ARCH_XTENSA_INCLUDE_ESP32S3_ESP32S3_GPIO_H
#define __ARCH_XTENSA_INCLUDE_ESP32S3_ESP32S3_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#define S3_GPIO_INPUT           0
#define S3_GPIO_OUTPUT          1
#define S3_GPIO_INPUT_PULLUP    2
#define S3_GPIO_INPUT_PULLDOWN  3
#define S3_GPIO_OPEN_DRAIN      4

#define S3_GPIO_RGB_LED         48   /* WS2812 on the ESP32-S3 Super Mini */

#ifdef __cplusplus
extern "C" {
#endif

bool s3_gpio_pin_ok(int pin);                 /* false for USB/flash/UART pins */
int  s3_gpio_config(int pin, int mode);       /* 0 or -EINVAL */
void s3_gpio_write(int pin, int value);
int  s3_gpio_read(int pin);                   /* 0/1 or -EINVAL */
int  s3_ws2812_write(int pin, uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
