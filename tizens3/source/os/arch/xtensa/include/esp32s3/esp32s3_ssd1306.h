/****************************************************************************
 * arch/xtensa/include/esp32s3/esp32s3_ssd1306.h
 *
 * SSD1306 OLED over TizenS3 software I2C. Use from apps with:
 *   #include <arch/chip/esp32s3_ssd1306.h>
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#ifndef __ARCH_XTENSA_INCLUDE_ESP32S3_ESP32S3_SSD1306_H
#define __ARCH_XTENSA_INCLUDE_ESP32S3_ESP32S3_SSD1306_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* addr 0 -> default 0x3c; width/height 0 -> 128x64 */
int  s3_ssd1306_init(int scl, int sda, uint8_t addr, int width, int height);
void s3_ssd1306_clear(void);
void s3_ssd1306_pixel(int x, int y, int on);
void s3_ssd1306_char(int x, int y, char ch);
void s3_ssd1306_text(int x, int y, const char *s);
int  s3_ssd1306_show(void);          /* push framebuffer to the panel */

#ifdef __cplusplus
}
#endif

#endif
