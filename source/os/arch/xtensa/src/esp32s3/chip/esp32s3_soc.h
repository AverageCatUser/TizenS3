/****************************************************************************
 * arch/xtensa/src/esp32s3/chip/esp32s3_soc.h
 *
 * Minimal ESP32-S3 register map for the TizenRT port. Values are taken from
 * ESP-IDF v4.4 components/soc/esp32s3/include/soc/ headers (Apache-2.0).
 ****************************************************************************/

#ifndef __ARCH_XTENSA_SRC_ESP32S3_CHIP_ESP32S3_SOC_H
#define __ARCH_XTENSA_SRC_ESP32S3_CHIP_ESP32S3_SOC_H

/* Peripheral base addresses */

#define DR_REG_UART_BASE              0x60000000
#define DR_REG_RTCCNTL_BASE           0x60008000
#define DR_REG_TIMERGROUP0_BASE       0x6001f000
#define DR_REG_TIMERGROUP1_BASE       0x60020000
#define DR_REG_USB_DEVICE_BASE        0x60038000   /* USB-Serial-JTAG */
#define DR_REG_INTERRUPT_CORE0_BASE   0x600c2000

/* Interrupt matrix: one map register per peripheral source */

#define INTERRUPT_CORE0_MAP_REGADDR(n) (DR_REG_INTERRUPT_CORE0_BASE + ((n) << 2))

/* RTC watchdog and super watchdog */

#define RTC_CNTL_WDTCONFIG0_REG       (DR_REG_RTCCNTL_BASE + 0x98)
#define RTC_CNTL_WDTWPROTECT_REG      (DR_REG_RTCCNTL_BASE + 0xb0)
#define RTC_CNTL_SWD_CONF_REG         (DR_REG_RTCCNTL_BASE + 0xb4)
#define RTC_CNTL_SWD_WPROTECT_REG     (DR_REG_RTCCNTL_BASE + 0xb8)
#define RTC_CNTL_WDT_EN               (1u << 31)
#define RTC_CNTL_WDT_FLASHBOOT_MOD_EN (1u << 12)
#define RTC_CNTL_SWD_AUTO_FEED_EN     (1u << 31)
#define RTC_CNTL_WDT_WKEY_VALUE       0x50d83aa1
#define RTC_CNTL_SWD_WKEY_VALUE       0x8f1d312a

/* Timer group main watchdogs */

#define TIMG_WDTCONFIG0_REG(i)        (DR_REG_TIMERGROUP0_BASE + (i) * 0x1000 + 0x48)
#define TIMG_WDTWPROTECT_REG(i)       (DR_REG_TIMERGROUP0_BASE + (i) * 0x1000 + 0x64)
#define TIMG_WDT_EN                   (1u << 31)
#define TIMG_WDT_FLASHBOOT_MOD_EN     (1u << 14)
#define TIMG_WDT_WKEY_VALUE           0x50d83aa1

/* UART0 (bootloader leaves it at 115200 8N1 on GPIO43 TX / GPIO44 RX) */

#define UART_FIFO_REG(i)              (DR_REG_UART_BASE + (i) * 0x10000 + 0x00)
#define UART_STATUS_REG(i)            (DR_REG_UART_BASE + (i) * 0x10000 + 0x1c)
#define UART_RXFIFO_CNT(v)            ((v) & 0x3ff)
#define UART_TXFIFO_CNT(v)            (((v) >> 16) & 0x3ff)
#define UART_TXFIFO_SIZE              128

/* USB-Serial-JTAG CDC endpoint (native USB-C port on S3 Super Mini) */

#define USB_SERIAL_JTAG_EP1_REG       (DR_REG_USB_DEVICE_BASE + 0x0)
#define USB_SERIAL_JTAG_EP1_CONF_REG  (DR_REG_USB_DEVICE_BASE + 0x4)
#define USB_SERIAL_JTAG_WR_DONE       (1u << 0)
#define USB_SERIAL_JTAG_IN_EP_DATA_FREE  (1u << 1)
#define USB_SERIAL_JTAG_OUT_EP_DATA_AVAIL (1u << 2)

/* Memory map used by the linker script and heap */

#define ESP32S3_IRAM_BASE             0x40378000   /* aliases DRAM 0x3fc88000 */
#define ESP32S3_IRAM_SIZE             0x00008000
#define ESP32S3_DRAM_BASE             0x3fc90000
#define ESP32S3_DRAM_END              0x3fce9000   /* below ROM-reserved area */

#endif /* __ARCH_XTENSA_SRC_ESP32S3_CHIP_ESP32S3_SOC_H */
