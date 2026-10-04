/****************************************************************************
 * arch/xtensa/src/esp32s3/esp32s3_gpio.c
 *
 * Minimal GPIO for TizenS3 (ESP32-S3): configure, write, read, plus a
 * WS2812 ("NeoPixel") writer for the Super Mini's on-board RGB LED.
 *
 * Register layout from ESP-IDF v4.4 soc/esp32s3 gpio_reg.h / io_mux_reg.h.
 *
 * Pins that would break the board are refused:
 *   19, 20       USB D-/D+  (selecting GPIO disables the USB pad -> console dies)
 *   22-25        do not exist on the S3
 *   26-32        in-package flash / PSRAM SPI
 *   43, 44       UART0 console (TX/RX)
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

/* Bit-exact WS2812 timing needs real optimization regardless of build level */
#pragma GCC optimize ("O2")

#include <tinyara/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <tinyara/arch.h>
#include <arch/irq.h>

#include "xtensa.h"
#include "xtensa_attr.h"
#include <arch/chip/esp32s3_gpio.h>

/* GPIO matrix */

#define GPIO_BASE              0x60004000
#define GPIO_OUT_W1TS_REG      (GPIO_BASE + 0x08)
#define GPIO_OUT_W1TC_REG      (GPIO_BASE + 0x0c)
#define GPIO_OUT1_W1TS_REG     (GPIO_BASE + 0x14)
#define GPIO_OUT1_W1TC_REG     (GPIO_BASE + 0x18)
#define GPIO_ENABLE_W1TS_REG   (GPIO_BASE + 0x24)
#define GPIO_ENABLE_W1TC_REG   (GPIO_BASE + 0x28)
#define GPIO_ENABLE1_W1TS_REG  (GPIO_BASE + 0x30)
#define GPIO_ENABLE1_W1TC_REG  (GPIO_BASE + 0x34)
#define GPIO_IN_REG            (GPIO_BASE + 0x3c)
#define GPIO_IN1_REG           (GPIO_BASE + 0x40)
#define GPIO_FUNC_OUT_SEL(n)   (GPIO_BASE + 0x554 + 4 * (n))
#define GPIO_PIN_REG(n)        (GPIO_BASE + 0x74 + 4 * (n))
#define GPIO_PIN_PAD_DRIVER    (1u << 2)   /* 1 = open-drain */
#define SIG_GPIO_OUT_IDX       256          /* plain GPIO output, OEN from ENABLE reg */

/* IO_MUX: one register per pad at base + 4 + 4*n */

#define IO_MUX_BASE            0x60009000
#define IO_MUX_REG(n)          (IO_MUX_BASE + 0x04 + 4 * (n))
#define FUN_PD                 (1u << 7)
#define FUN_PU                 (1u << 8)
#define FUN_IE                 (1u << 9)
#define FUN_DRV_S              10
#define FUN_DRV_M              (3u << FUN_DRV_S)
#define MCU_SEL_S              12
#define MCU_SEL_M              (7u << MCU_SEL_S)
#define PIN_FUNC_GPIO          1

#define REG32(a)               (*(volatile uint32_t *)(a))

bool s3_gpio_pin_ok(int pin)
{
	if (pin < 0 || pin > 48) {
		return false;
	}
	if (pin == 19 || pin == 20) {          /* USB */
		return false;
	}
	if (pin >= 22 && pin <= 32) {          /* missing + flash/PSRAM */
		return false;
	}
	if (pin == 43 || pin == 44) {          /* UART0 console */
		return false;
	}
	return true;
}

int s3_gpio_config(int pin, int mode)
{
	uint32_t mux;
	irqstate_t flags;

	if (!s3_gpio_pin_ok(pin)) {
		return -EINVAL;
	}

	flags = irqsave();

	mux = REG32(IO_MUX_REG(pin));
	mux &= ~(MCU_SEL_M | FUN_PU | FUN_PD | FUN_DRV_M);
	mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | FUN_IE | (2u << FUN_DRV_S);
	if (mode == S3_GPIO_INPUT_PULLUP || mode == S3_GPIO_OPEN_DRAIN) {
		mux |= FUN_PU;
	} else if (mode == S3_GPIO_INPUT_PULLDOWN) {
		mux |= FUN_PD;
	}
	REG32(IO_MUX_REG(pin)) = mux;

	if (mode == S3_GPIO_OUTPUT || mode == S3_GPIO_OPEN_DRAIN) {
		REG32(GPIO_FUNC_OUT_SEL(pin)) = SIG_GPIO_OUT_IDX;

		/* open-drain also keeps the input buffer on so the line can be read */
		if (mode == S3_GPIO_OPEN_DRAIN) {
			REG32(GPIO_PIN_REG(pin)) |= GPIO_PIN_PAD_DRIVER;
		} else {
			REG32(GPIO_PIN_REG(pin)) &= ~GPIO_PIN_PAD_DRIVER;
		}

		if (pin < 32) {
			REG32(GPIO_ENABLE_W1TS_REG) = 1u << pin;
		} else {
			REG32(GPIO_ENABLE1_W1TS_REG) = 1u << (pin - 32);
		}
	} else {
		if (pin < 32) {
			REG32(GPIO_ENABLE_W1TC_REG) = 1u << pin;
		} else {
			REG32(GPIO_ENABLE1_W1TC_REG) = 1u << (pin - 32);
		}
	}

	irqrestore(flags);
	return OK;
}

void s3_gpio_write(int pin, int value)
{
	if (!s3_gpio_pin_ok(pin)) {
		return;
	}
	if (pin < 32) {
		REG32(value ? GPIO_OUT_W1TS_REG : GPIO_OUT_W1TC_REG) = 1u << pin;
	} else {
		REG32(value ? GPIO_OUT1_W1TS_REG : GPIO_OUT1_W1TC_REG) = 1u << (pin - 32);
	}
}

int s3_gpio_read(int pin)
{
	if (!s3_gpio_pin_ok(pin)) {
		return -EINVAL;
	}
	if (pin < 32) {
		return (REG32(GPIO_IN_REG) >> pin) & 1;
	}
	return (REG32(GPIO_IN1_REG) >> (pin - 32)) & 1;
}

/****************************************************************************
 * WS2812 / NeoPixel
 *
 * 800 kHz, GRB order, MSB first:
 *   "0" = high 0.40 us, low 0.85 us
 *   "1" = high 0.80 us, low 0.45 us
 *   latch = low > 280 us (covers WS2812B-V5 / SK6812 too)
 *
 * Timed against the CPU cycle counter with absolute deadlines so register
 * write latency is absorbed. Runs from IRAM with interrupts off so a cache
 * miss or the tick interrupt can't stretch a bit.
 ****************************************************************************/

extern uint32_t esp32s3_cpu_freq_mhz(void);

/* Read the CPU cycle counter: a macro, so the poll loops are rsr/sub/branch
 * with no call overhead (a call here would also risk window spills mid-bit).
 */
#define ccount() ({ uint32_t __c; __asm__ __volatile__("rsr %0, ccount" : "=a"(__c)); __c; })

static void IRAM_ATTR __attribute__((noinline))
ws2812_send(volatile uint32_t *set, volatile uint32_t *clr, uint32_t mask,
            const uint8_t *buf, int len,
            uint32_t t0h, uint32_t t1h, uint32_t period)
{
	uint32_t start = ccount();

	for (int i = 0; i < len; i++) {
		uint8_t b = buf[i];
		for (int bit = 7; bit >= 0; bit--) {
			uint32_t high = (b & (1u << bit)) ? t1h : t0h;

			while ((uint32_t)(ccount() - start) < period) ;
			start = ccount();
			*set = mask;
			while ((uint32_t)(ccount() - start) < high) ;
			*clr = mask;
		}
	}
	while ((uint32_t)(ccount() - start) < period) ;
	*clr = mask;
}

static void s3_delay_us(uint32_t us)
{
	uint32_t start = ccount();
	uint32_t cycles = us * esp32s3_cpu_freq_mhz();

	while ((uint32_t)(ccount() - start) < cycles) ;
}

int s3_ws2812_write(int pin, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t grb[3];
	uint32_t mhz;
	volatile uint32_t *set;
	volatile uint32_t *clr;
	uint32_t mask;
	irqstate_t flags;
	int ret;

	ret = s3_gpio_config(pin, S3_GPIO_OUTPUT);
	if (ret < 0) {
		return ret;
	}

	grb[0] = g;
	grb[1] = r;
	grb[2] = b;

	mhz = esp32s3_cpu_freq_mhz();
	if (pin < 32) {
		set = (volatile uint32_t *)GPIO_OUT_W1TS_REG;
		clr = (volatile uint32_t *)GPIO_OUT_W1TC_REG;
		mask = 1u << pin;
	} else {
		set = (volatile uint32_t *)GPIO_OUT1_W1TS_REG;
		clr = (volatile uint32_t *)GPIO_OUT1_W1TC_REG;
		mask = 1u << (pin - 32);
	}

	/* latch any previous frame */

	*clr = mask;
	s3_delay_us(300);

	/* High-time targets compensate for ~6 cycles of poll-loop + register
	 * write overhead, which is a big fraction of a bit at 40-80 MHz.
	 * WS2812 decodes a bit by its HIGH time only (0: <~0.55 us,
	 * 1: >~0.65 us); the low time may stretch, so the period is generous.
	 *   actual T0H ~0.35-0.40 us, actual T1H ~0.80 us, period ~1.4 us
	 */

	{
		int t0h = (int)(mhz * 350) / 1000 - 6;
		int t1h = (int)(mhz * 800) / 1000 - 6;
		uint32_t period = (mhz * 1400) / 1000;

		if (t0h < 2) {
			t0h = 2;
		}

		flags = irqsave();
		ws2812_send(set, clr, mask, grb, 3, (uint32_t)t0h, (uint32_t)t1h, period);
		irqrestore(flags);
	}

	s3_delay_us(300);                   /* latch */
	return OK;
}
