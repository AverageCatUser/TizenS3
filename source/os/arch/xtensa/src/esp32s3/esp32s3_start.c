/****************************************************************************
 * arch/xtensa/src/esp32s3/esp32s3_start.c
 *
 * ESP32-S3 entry point for TizenRT. The ESP-IDF 2nd stage bootloader has
 * already configured clocks, flash cache/MMU and loaded our IRAM/DRAM
 * segments; it jumps to __start on the PRO CPU with the APP CPU held in
 * reset. We switch to our own idle stack, kill the watchdogs the ROM and
 * bootloader left running, clear .bss and hand over to os_start().
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdint.h>
#include <string.h>

#include <tinyara/init.h>
#include <tinyara/arch.h>
#include <arch/irq.h>

#include "xtensa.h"
#include "xtensa_attr.h"

#include "chip/esp32s3_soc.h"
#include "esp32_start.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

uint32_t g_idlestack[IDLETHREAD_STACKWORDS]
__attribute__((aligned(16), section(".noinit")));

const uint32_t g_idle_topstack = (uint32_t)(g_idlestack + IDLETHREAD_STACKWORDS);

volatile uint32_t cpstate = 0;
volatile struct xtensa_cpstate_s g_cpstate = { 0 };

/* ROM function (address supplied by esp32s3.rom.ld) */

extern uint32_t ets_get_cpu_frequency(void);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void IRAM_ATTR esp32s3_disable_wdts(void)
{
	uint32_t regval;
	int i;

	/* RTC watchdog: the bootloader arms it (~9 s) to catch a hung app */

	putreg32(RTC_CNTL_WDT_WKEY_VALUE, RTC_CNTL_WDTWPROTECT_REG);
	putreg32(0, RTC_CNTL_WDTCONFIG0_REG);
	putreg32(0, RTC_CNTL_WDTWPROTECT_REG);

	/* Super watchdog: cannot be disabled, but can be set to auto-feed */

	putreg32(RTC_CNTL_SWD_WKEY_VALUE, RTC_CNTL_SWD_WPROTECT_REG);
	regval = getreg32(RTC_CNTL_SWD_CONF_REG);
	putreg32(regval | RTC_CNTL_SWD_AUTO_FEED_EN, RTC_CNTL_SWD_CONF_REG);
	putreg32(0, RTC_CNTL_SWD_WPROTECT_REG);

	/* Timer group 0/1 main watchdogs (ROM enables TG0 in flash-boot mode) */

	for (i = 0; i < 2; i++) {
		putreg32(TIMG_WDT_WKEY_VALUE, TIMG_WDTWPROTECT_REG(i));
		regval = getreg32(TIMG_WDTCONFIG0_REG(i));
		regval &= ~(TIMG_WDT_EN | TIMG_WDT_FLASHBOOT_MOD_EN);
		putreg32(regval, TIMG_WDTCONFIG0_REG(i));
		putreg32(0, TIMG_WDTWPROTECT_REG(i));
	}
}

/* Everything after the stack switch lives in its own non-inlined function
 * so the compiler never carries a frame across the stack pointer change.
 */

static void IRAM_ATTR __attribute__((noinline, noreturn)) esp32s3_start(void)
{
	esp32s3_disable_wdts();
#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('B'); }
#endif


	/* Clear .bss (the bootloader does not) */

	memset(&_sbss, 0, (size_t)((uintptr_t)&_ebss - (uintptr_t)&_sbss));
#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('C'); }
#endif


#ifdef CONFIG_STACK_COLORATION
	{
		/* The idle stack is in use right now; colour only below our frame */
		register uint32_t *ptr = g_idlestack;
		register int i;

		for (i = 0; i < IDLETHREAD_STACKWORDS - 256; i++) {
			*ptr++ = STACK_COLOR;
		}
	}
#endif

#if XTENSA_CP_ALLSET != 0
	/* Initial co-processor state (FPU + PIE/TIE on the S3) */

	g_cpstate.cpasa = (uint32_t *)&cpstate;
	g_cpstate.cpenable = xtensa_get_cpenable();
	g_cpstate.cpstored = g_cpstate.cpenable;
	xtensa_coproc_enable((struct xtensa_cpstate_s *)&g_cpstate, XTENSA_CP_ALLSET);
#endif

#ifdef USE_EARLYSERIALINIT
	xtensa_early_serial_initialize();
#endif

#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('D'); }
#endif
	esp32_board_initialize();

#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('E'); }
#endif
	/* Bring up TizenRT */

	os_start();

	for (;;) ;
}


/* ROM cache routines (addresses from esp32s3_rom.ld) */

extern void rom_config_instruction_cache_mode(uint32_t size, uint8_t ways, uint8_t line);
extern void rom_config_data_cache_mode(uint32_t size, uint8_t ways, uint8_t line);
extern uint32_t rom_Cache_Suspend_DCache(void);
extern void Cache_Resume_DCache(uint32_t autoload);
extern uint32_t Cache_Set_IDROM_MMU_Size(uint32_t irom_size, uint32_t drom_size);
extern uint32_t _srodata;

#define S3_CACHE_STATE_REG   0x600c4130
#define S3_DCACHE_STATE(v)   (((v) >> 12) & 0xfff)
#define S3_DROM_LOW          0x3c000000
#define S3_MMU_PAGE          0x10000
#define S3_DROM_MMU_MAX_END  0x400

/* Finish the flash cache setup the 2nd-stage bootloader leaves in a
 * "preliminary" state. This mirrors ESP-IDF's call_start_cpu0() on the S3
 * and MUST run from IRAM before any code is executed from flash:
 *
 *  - configure I-cache (16 KB, 8-way, 32 B lines) and D-cache (32 KB,
 *    8-way, 32 B lines), same as IDF defaults;
 *  - tell the cache which MMU entries are code (IROM) and which are rodata
 *    (DROM). Without this the S3 can resolve IROM pages through the DROM
 *    part of the shared MMU table, so the CPU executes rodata bytes and
 *    faults (IllegalInstruction) on the first flash function. The emulator
 *    does not model this split, which is why it only failed on hardware.
 */

static void IRAM_ATTR esp32s3_cache_init(void)
{
	uint32_t rodata_start;
	uint32_t irom_size;

	rom_config_instruction_cache_mode(0x4000, 8, 32);

	(void)rom_Cache_Suspend_DCache();
	while (S3_DCACHE_STATE(*(volatile uint32_t *)S3_CACHE_STATE_REG) != 1) ;
	rom_config_data_cache_mode(0x8000, 8, 32);
	Cache_Resume_DCache(0);

	/* IROM occupies the MMU entries below the (64 KB aligned) start of
	 * rodata; the rest of the table up to the end belongs to DROM.
	 */

	rodata_start = (uint32_t)&_srodata & ~(S3_MMU_PAGE - 1);
	irom_size = ((rodata_start - S3_DROM_LOW) / S3_MMU_PAGE) * sizeof(uint32_t);
	Cache_Set_IDROM_MMU_Size(irom_size, S3_DROM_MMU_MAX_END - irom_size);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/* CPU clock in MHz, MEASURED against the 16 MHz SYSTIMER on first use.
 *
 * Trusting ets_get_cpu_frequency() was wrong on real hardware: the ROM's
 * idea of the clock did not match the actual core clock, which made every
 * cycle-counted delay (system tick, WS2812 pulses) the wrong length. The
 * SYSTIMER always counts at 16 MHz from the crystal, so counting CPU cycles
 * across a fixed SYSTIMER interval gives the real frequency.
 */

#define S3_SYSTIMER_CONF        0x60023000
#define S3_SYSTIMER_UNIT0_OP    0x60023004
#define S3_SYSTIMER_UNIT0_HI    0x60023040
#define S3_SYSTIMER_UNIT0_LO    0x60023044
#define S3_PERIP_CLK_EN0        0x600c0018
#define S3_PERIP_RST_EN0        0x600c0020
#define S3_SYSTIMER_BIT         (1u << 29)

static uint32_t g_cpu_mhz;
static uint32_t g_rom_mhz;

static inline uint32_t s3_ccount(void)
{
	uint32_t c;
	__asm__ __volatile__("rsr %0, ccount" : "=a"(c));
	return c;
}

static int s3_systimer_read(uint32_t *lo)
{
	int guard = 10000;

	putreg32(1u << 30, S3_SYSTIMER_UNIT0_OP);            /* UPDATE */
	while (!(getreg32(S3_SYSTIMER_UNIT0_OP) & (1u << 29))) {  /* VALUE_VALID */
		if (--guard == 0) {
			return -1;
		}
	}
	*lo = getreg32(S3_SYSTIMER_UNIT0_LO);
	return 0;
}

static uint32_t s3_snap_mhz(uint32_t mhz)
{
	static const uint32_t known[] = { 20, 40, 80, 160, 240 };
	for (int i = 0; i < 5; i++) {
		uint32_t k = known[i];
		if (mhz * 10 >= k * 9 && mhz * 10 <= k * 11) {   /* within 10% */
			return k;
		}
	}
	return mhz;
}

uint32_t esp32s3_cpu_freq_mhz(void)
{
	uint32_t t0, t1, c0, c1;
	int iter;

	if (g_cpu_mhz) {
		return g_cpu_mhz;
	}

	g_rom_mhz = ets_get_cpu_frequency();

	/* make sure SYSTIMER is clocked, out of reset and counting */

	putreg32(getreg32(S3_PERIP_CLK_EN0) | S3_SYSTIMER_BIT, S3_PERIP_CLK_EN0);
	putreg32(getreg32(S3_PERIP_RST_EN0) & ~S3_SYSTIMER_BIT, S3_PERIP_RST_EN0);
	putreg32(getreg32(S3_SYSTIMER_CONF) | (1u << 31) | (1u << 30), S3_SYSTIMER_CONF);

	if (s3_systimer_read(&t0) == 0) {
		c0 = s3_ccount();
		for (iter = 0; iter < 5000000; iter++) {
			if (s3_systimer_read(&t1) != 0) {
				break;
			}
			if ((uint32_t)(t1 - t0) >= 32000) {             /* 2 ms at 16 MHz */
				c1 = s3_ccount();
				g_cpu_mhz = s3_snap_mhz(((c1 - c0) + 1000) / 2000);
				break;
			}
		}
	}

	if (g_cpu_mhz < 10 || g_cpu_mhz > 260) {
		/* SYSTIMER unusable: fall back to the ROM's value */
		g_cpu_mhz = (g_rom_mhz >= 10 && g_rom_mhz <= 240) ? g_rom_mhz : 80;
	}

	return g_cpu_mhz;
}

uint32_t esp32s3_rom_freq_mhz(void)
{
	(void)esp32s3_cpu_freq_mhz();
	return g_rom_mhz;
}

void IRAM_ATTR __start(void)
{
#ifdef CONFIG_ESP32S3_USB_PROBE
	/* Diagnostic: first instruction, IRAM/DRAM only, zero flash access */
	{ extern void esp32s3_usbprobe(void); esp32s3_usbprobe(); }
#endif

#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('A'); }
#endif
	/* Finish flash cache/MMU setup BEFORE the first call into flash */

	esp32s3_cache_init();

	/* Mask interrupts left over from the ROM/bootloader */

	up_irq_disable();

	/* Point the exception vectors at our IRAM copy */

	__asm__ __volatile__("wsr %0, vecbase\n" "rsync\n" : : "r"(&_init_start));

	/* Switch to the known idle thread stack */

	__asm__ __volatile__("mov sp, %0\n" : : "r"(g_idlestack + IDLETHREAD_STACKWORDS));

	esp32s3_start();
}
