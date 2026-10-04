/****************************************************************************
 * arch/xtensa/src/esp32s3/esp32s3_serial.c
 *
 * Console for the ESP32-S3 port. One TizenRT serial device that is backed
 * by BOTH:
 *
 *   - UART0 (GPIO43 TX / GPIO44 RX, 115200 8N1 as left by the bootloader),
 *     which is what a USB-UART bridge chip or QEMU sees, and
 *   - the S3's native USB-Serial-JTAG CDC port, which is what you get when
 *     you plug a USB-C cable straight into an ESP32-S3 Super Mini.
 *
 * Output is mirrored to both. Input is accepted from either. The driver is
 * fully polled: transmit drains synchronously and receive is pumped from
 * a high-priority work-queue job, so no interrupt-matrix routing is
 * required and the TizenRT upper-half (select/poll/read/write) works as
 * normal for TASH.
 *
 * If no USB host is reading, the 64-byte USB IN buffer fills up; we then
 * mark USB as stalled and drop USB output instead of hanging the system,
 * and resume automatically as soon as the host drains it.
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <tinyara/wqueue.h>

#include <tinyara/arch.h>
#include <tinyara/serial/serial.h>
#include <arch/irq.h>

#include "xtensa.h"
#include "chip/esp32s3_soc.h"

#ifdef USE_SERIALDRIVER

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_UART0_RXBUFSIZE
#define CONFIG_UART0_RXBUFSIZE 256
#endif
#ifndef CONFIG_UART0_TXBUFSIZE
#define CONFIG_UART0_TXBUFSIZE 256
#endif

#define UART_SPIN_LIMIT  200000   /* bounded wait for TX FIFO space */
#define USB_SPIN_LIMIT   20000    /* bounded wait for the USB IN buffer */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_rxenabled;
static bool g_usb_stalled;
static bool g_in_xmit;

static char g_rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_txbuffer[CONFIG_UART0_TXBUFSIZE];

/****************************************************************************
 * Low-level helpers
 ****************************************************************************/

static inline void uart0_putc(int ch)
{
	int spin = UART_SPIN_LIMIT;

	while (UART_TXFIFO_CNT(getreg32(UART_STATUS_REG(0))) >= UART_TXFIFO_SIZE - 1 && --spin > 0) ;
	putreg32((uint32_t)ch & 0xff, UART_FIFO_REG(0));
}

/* USB FIFO access is serialized with a short interrupts-off critical
 * section, NOT a semaphore: up_putc() runs with interrupts disabled and is
 * also used by assert/panic output and ISRs, where blocking on a semaphore
 * would deadlock. Single core, so masking interrupts is sufficient.
 */

static inline irqstate_t usb_lock(void)
{
	return irqsave();
}

static inline void usb_unlock(irqstate_t flags)
{
	irqrestore(flags);
}

static inline void usb_flush_locked(void)
{
	if (!g_usb_stalled) {
		putreg32(USB_SERIAL_JTAG_WR_DONE, USB_SERIAL_JTAG_EP1_CONF_REG);
	}
}

static void usb_flush(void)
{
	irqstate_t f = usb_lock();
	usb_flush_locked();
	usb_unlock(f);
}

static inline void usb_putc(int ch)
{
	int spin = g_usb_stalled ? 1 : USB_SPIN_LIMIT;

	if (!(getreg32(USB_SERIAL_JTAG_EP1_CONF_REG) & USB_SERIAL_JTAG_IN_EP_DATA_FREE)) {
		/* 64-byte packet is full: push it to the host before waiting */
		putreg32(USB_SERIAL_JTAG_WR_DONE, USB_SERIAL_JTAG_EP1_CONF_REG);
	}

	while (!(getreg32(USB_SERIAL_JTAG_EP1_CONF_REG) & USB_SERIAL_JTAG_IN_EP_DATA_FREE)) {
		if (--spin <= 0) {
			/* Nobody is reading the USB side: drop output, don't hang */
			g_usb_stalled = true;
			return;
		}
	}

	g_usb_stalled = false;
	putreg32((uint32_t)ch & 0xff, USB_SERIAL_JTAG_EP1_REG);
}

static inline bool usb_rxavail(void)
{
	return (getreg32(USB_SERIAL_JTAG_EP1_CONF_REG) & USB_SERIAL_JTAG_OUT_EP_DATA_AVAIL) != 0;
}

static inline bool uart0_rxavail(void)
{
	return UART_RXFIFO_CNT(getreg32(UART_STATUS_REG(0))) > 0;
}

static void console_putc(int ch)
{
	irqstate_t f;

	uart0_putc(ch);          /* UART0 FIFO is independent, no lock needed */
	f = usb_lock();
	usb_putc(ch);
	usb_unlock(f);
}

/****************************************************************************
 * Lower-half operations
 ****************************************************************************/

static int s3_setup(FAR struct uart_dev_s *dev)
{
	return OK;
}

static void s3_shutdown(FAR struct uart_dev_s *dev)
{
}

static int s3_attach(FAR struct uart_dev_s *dev)
{
	return OK;
}

static void s3_detach(FAR struct uart_dev_s *dev)
{
}

static int s3_ioctl(FAR struct uart_dev_s *dev, int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static int s3_receive(FAR struct uart_dev_s *dev, FAR unsigned int *status)
{
	*status = 0;

	if (uart0_rxavail()) {
		return getreg32(UART_FIFO_REG(0)) & 0xff;
	}

	if (usb_rxavail()) {
		int c;
		irqstate_t f = usb_lock();
		c = getreg32(USB_SERIAL_JTAG_EP1_REG) & 0xff;
		usb_unlock(f);
		return c;
	}

	return 0;
}

static void s3_rxint(FAR struct uart_dev_s *dev, bool enable)
{
	g_rxenabled = enable;
}

static bool s3_rxavailable(FAR struct uart_dev_s *dev)
{
	return uart0_rxavail() || usb_rxavail();
}

static void s3_send(FAR struct uart_dev_s *dev, int ch)
{
	console_putc(ch);
}

static void s3_txint(FAR struct uart_dev_s *dev, bool enable)
{
	/* No TX interrupt: when the upper half asks for one, drain the whole
	 * transmit buffer right now. uart_xmitchars() calls back here with
	 * enable == false once the buffer is empty.
	 */

	if (enable && !g_in_xmit) {
		g_in_xmit = true;
		uart_xmitchars(dev);
		g_in_xmit = false;
		usb_flush();
	}
}

static bool s3_txready(FAR struct uart_dev_s *dev)
{
	/* send() waits for FIFO space itself, so we are always "ready" */

	return true;
}

static bool s3_txempty(FAR struct uart_dev_s *dev)
{
	return UART_TXFIFO_CNT(getreg32(UART_STATUS_REG(0))) == 0;
}

static const struct uart_ops_s g_s3_ops = {
	.setup       = s3_setup,
	.shutdown    = s3_shutdown,
	.attach      = s3_attach,
	.detach      = s3_detach,
	.ioctl       = s3_ioctl,
	.receive     = s3_receive,
	.rxint       = s3_rxint,
	.rxavailable = s3_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
	.rxflowcontrol = NULL,
#endif
	.send        = s3_send,
	.txint       = s3_txint,
	.txready     = s3_txready,
	.txempty     = s3_txempty,
};

static uart_dev_t g_console_dev = {
	.isconsole = true,
	.recv = {
		.size   = CONFIG_UART0_RXBUFSIZE,
		.buffer = g_rxbuffer,
	},
	.xmit = {
		.size   = CONFIG_UART0_TXBUFSIZE,
		.buffer = g_txbuffer,
	},
	.ops  = &g_s3_ops,
	.priv = NULL,
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/* Console RX via the high-priority work queue. Polling the USB-Serial-JTAG
 * from the timer ISR raced with task-context writes and corrupted the USB
 * endpoint on real hardware (the emulator has no USB state so it was
 * invisible). A work-queue worker runs in normal task context, so the UART
 * upper-half can lock the device and wake readers safely, and it reschedules
 * itself every tick instead of blocking a dedicated thread.
 */

static struct work_s g_rx_work;

static void console_rx_worker(FAR void *arg)
{
	if (g_rxenabled && (uart0_rxavail() || usb_rxavail())) {
		uart_recvchars(&g_console_dev);
	}

	/* reschedule: ~every 2 ticks keeps typing responsive */
	(void)work_queue(HPWORK, &g_rx_work, console_rx_worker, NULL, 2);
}



void xtensa_early_serial_initialize(void)
{
	g_console_dev.isconsole = true;
	g_usb_stalled = false;
}

void xtensa_serial_initialize(void)
{
	(void)uart_register("/dev/console", &g_console_dev);
	(void)uart_register("/dev/ttyS0", &g_console_dev);
}

/* Start the console RX thread. Must run AFTER the scheduler is up, so it is
 * called from board bringup (board_initialize), not from up_initialize()
 * which runs before os_bringup() has the thread machinery ready.
 */

void esp32s3_serial_start_rx(void)
{
	(void)work_queue(HPWORK, &g_rx_work, console_rx_worker, NULL, 2);
}

int up_putc(int ch)
{
	irqstate_t flags = irqsave();

	if (ch == '\n') {
		console_putc('\r');
	}

	console_putc(ch);

	if (ch == '\n') {
		usb_flush();
	}

	irqrestore(flags);
	return ch;
}

void up_lowputc(char ch)
{
	(void)up_putc(ch);
}

#endif /* USE_SERIALDRIVER */
