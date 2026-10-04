/****************************************************************************
 * board/tizens3/include/board.h
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#ifndef __BOARD_TIZENS3_INCLUDE_BOARD_H
#define __BOARD_TIZENS3_INCLUDE_BOARD_H

/* Single source of truth for the TizenS3 release version */

#define TIZENS3_VERSION       "1.0.0"

/* 40 MHz crystal on every ESP32-S3 module. The CPU clock itself is read
 * from the ROM at runtime (esp32s3_cpu_freq_mhz()) because it is whatever
 * the 2nd stage bootloader configured.
 */

#define BOARD_XTAL_FREQUENCY  40000000
#define BOARD_CLOCK_FREQUENCY 80000000   /* nominal; not used for the tick */

/* Super Mini: RGB LED (WS2812) on GPIO48, BOOT button on GPIO0 */

#define BOARD_GPIO_RGB_LED    48
#define BOARD_GPIO_BOOT_BTN   0

#endif /* __BOARD_TIZENS3_INCLUDE_BOARD_H */
