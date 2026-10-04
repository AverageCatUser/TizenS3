/****************************************************************************
 * arch/xtensa/src/esp32s3/esp32s3_i2c.c
 *
 * Software (bit-banged) I2C master for TizenS3. Works on any two usable
 * GPIOs, open-drain with the internal pull-ups enabled, so an SSD1306 OLED
 * or any I2C sensor can be driven without touching the hardware I2C
 * peripheral. Standard (100 kHz) and fast (400 kHz) modes.
 *
 * Both pins are configured ONCE as open-drain outputs with the internal
 * pull-up: writing 0 pulls the line low, writing 1 releases it so the
 * pull-up (and the bus' external pull-ups) raise it. The input buffer stays
 * on, so the same pin can be read back. Clock stretching is honoured by
 * waiting for SCL to actually read high after releasing it.
 *
 * The internal pull-ups are weak (~45 kOhm): fine for short wires and most
 * OLED/sensor modules (which carry their own 4.7k-10k pull-ups), but add
 * external 4.7k pull-ups to 3.3V for long wires or 400 kHz reliability.
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#pragma GCC optimize ("O2")

#include <tinyara/config.h>

#include <stdint.h>
#include <errno.h>
#include <semaphore.h>

#include "xtensa.h"
#include "xtensa_attr.h"
#include <arch/chip/esp32s3_gpio.h>
#include <arch/chip/esp32s3_i2c.h>

extern uint32_t esp32s3_cpu_freq_mhz(void);

#define ccount() ({ uint32_t __c; __asm__ __volatile__("rsr %0, ccount" : "=a"(__c)); __c; })

struct i2c_bus_s {
	int scl;
	int sda;
	uint32_t half;      /* half bit period in CPU cycles */
	bool ready;
};

static struct i2c_bus_s g_bus = { .scl = -1, .sda = -1, .ready = false };
static sem_t g_i2c_lock = SEM_INITIALIZER(1);    /* one transfer at a time */

static void i2c_lock(void)   { while (sem_wait(&g_i2c_lock) != 0) ; }
static void i2c_unlock(void) { sem_post(&g_i2c_lock); }

static void i2c_delay(void)
{
	uint32_t start = ccount();
	while ((uint32_t)(ccount() - start) < g_bus.half) ;
}

/* open-drain line control: pins are configured once in s3_i2c_init(), so
 * each edge is a single register write (W1TS/W1TC), not a pad reconfigure.
 */
static inline void scl_hi(void) { s3_gpio_write(g_bus.scl, 1); }
static inline void scl_lo(void) { s3_gpio_write(g_bus.scl, 0); }
static inline void sda_hi(void) { s3_gpio_write(g_bus.sda, 1); }
static inline void sda_lo(void) { s3_gpio_write(g_bus.sda, 0); }
static inline int  sda_read(void) { return s3_gpio_read(g_bus.sda); }

static int scl_release_wait(void)
{
	int guard = 10000;

	scl_hi();
	while (s3_gpio_read(g_bus.scl) == 0) {       /* clock stretching */
		if (--guard == 0) {
			return -ETIMEDOUT;
		}
	}
	return 0;
}

int s3_i2c_init(int scl_pin, int sda_pin, int khz)
{
	uint32_t mhz;

	if (!s3_gpio_pin_ok(scl_pin) || !s3_gpio_pin_ok(sda_pin) || scl_pin == sda_pin) {
		return -EINVAL;
	}
	if (khz <= 0) {
		khz = 400;
	}

	mhz = esp32s3_cpu_freq_mhz();
	i2c_lock();
	g_bus.scl = scl_pin;
	g_bus.sda = sda_pin;
	g_bus.half = (mhz * 1000) / (2 * khz);       /* half period, in cycles */
	if (g_bus.half < 4) {
		g_bus.half = 4;
	}

	/* configure both pins once: open-drain + pull-up, released (high) */
	s3_gpio_write(sda_pin, 1);
	s3_gpio_write(scl_pin, 1);
	if (s3_gpio_config(sda_pin, S3_GPIO_OPEN_DRAIN) < 0 ||
	    s3_gpio_config(scl_pin, S3_GPIO_OPEN_DRAIN) < 0) {
		g_bus.ready = false;
		i2c_unlock();
		return -EINVAL;
	}
	sda_hi();
	scl_hi();
	g_bus.ready = true;
	i2c_unlock();
	return 0;
}

static void i2c_start(void)
{
	sda_hi();
	scl_hi();
	i2c_delay();
	sda_lo();                /* SDA falls while SCL high = START */
	i2c_delay();
	scl_lo();
	i2c_delay();
}

static void i2c_stop(void)
{
	sda_lo();
	i2c_delay();
	scl_release_wait();
	i2c_delay();
	sda_hi();                /* SDA rises while SCL high = STOP */
	i2c_delay();
}

/* write one byte, return 0 if ACKed, -EIO if NAK */
static int i2c_write_byte(uint8_t b)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (b & 0x80) {
			sda_hi();
		} else {
			sda_lo();
		}
		b <<= 1;
		i2c_delay();
		if (scl_release_wait() < 0) {
			return -ETIMEDOUT;
		}
		i2c_delay();
		scl_lo();
		i2c_delay();
	}

	/* 9th clock: read ACK */
	sda_hi();
	i2c_delay();
	if (scl_release_wait() < 0) {
		return -ETIMEDOUT;
	}
	i2c_delay();
	int ack = (sda_read() == 0);
	scl_lo();
	i2c_delay();
	return ack ? 0 : -EIO;
}

static int i2c_read_byte(int send_ack, uint8_t *out)
{
	uint8_t b = 0;
	int i;

	sda_hi();                /* release so the slave can drive */
	for (i = 0; i < 8; i++) {
		i2c_delay();
		if (scl_release_wait() < 0) {
			return -ETIMEDOUT;
		}
		b = (b << 1) | (sda_read() & 1);
		i2c_delay();
		scl_lo();
	}

	/* ACK/NAK */
	if (send_ack) {
		sda_lo();
	} else {
		sda_hi();
	}
	i2c_delay();
	if (scl_release_wait() < 0) {
		return -ETIMEDOUT;
	}
	i2c_delay();
	scl_lo();
	sda_hi();
	i2c_delay();
	*out = b;
	return 0;
}

int s3_i2c_write(uint8_t addr, const uint8_t *data, int len)
{
	int i, ret = 0;

	if (!g_bus.ready) {
		return -EPERM;
	}

	i2c_lock();
	i2c_start();
	ret = i2c_write_byte((uint8_t)(addr << 1));         /* write */
	for (i = 0; ret == 0 && i < len; i++) {
		ret = i2c_write_byte(data[i]);
	}
	i2c_stop();
	i2c_unlock();
	return ret;
}

int s3_i2c_read(uint8_t addr, uint8_t *data, int len)
{
	int i, ret;

	if (!g_bus.ready) {
		return -EPERM;
	}

	i2c_lock();
	i2c_start();
	ret = i2c_write_byte((uint8_t)((addr << 1) | 1));   /* read */
	if (ret == 0) {
		for (i = 0; ret == 0 && i < len; i++) {
			ret = i2c_read_byte(i < len - 1, &data[i]);  /* NAK on last */
		}
	}
	i2c_stop();
	i2c_unlock();
	return ret;
}

/* probe: START + address + STOP, 0 if a device ACKs */
int s3_i2c_probe(uint8_t addr)
{
	int ret;

	if (!g_bus.ready) {
		return -EPERM;
	}
	i2c_lock();
	i2c_start();
	ret = i2c_write_byte((uint8_t)(addr << 1));
	i2c_stop();
	i2c_unlock();
	return ret;
}
