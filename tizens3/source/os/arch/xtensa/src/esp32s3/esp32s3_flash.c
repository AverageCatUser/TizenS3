/****************************************************************************
 * arch/xtensa/src/esp32s3/esp32s3_flash.c
 *
 * MTD driver for the ESP32-S3's own SPI flash (the chip TizenRT boots from).
 *
 * Flash access goes through the ROM's esp_rom_spiflash_* routines on the
 * SPI1 controller. While SPI1 is talking to the flash, the instruction and
 * data caches (which fetch our code and rodata through SPI0 from the SAME
 * chip) must be suspended, so:
 *
 *   - interrupts are masked first (the ISRs live in flash),
 *   - the cache suspend / ROM call / cache resume sequence runs from IRAM,
 *   - data always goes through a bounce buffer in internal DRAM, so the ROM
 *     never touches a flash-mapped or unaligned buffer.
 *
 * Only a window of the flash is exposed (CONFIG_ESP32S3_FLASH_FS_OFFSET /
 * _SIZE), so nothing the filesystem does can reach the bootloader,
 * partition table or the running firmware.
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <debug.h>

#include <tinyara/arch.h>
#include <tinyara/fs/ioctl.h>
#include <tinyara/fs/mtd.h>
#include <arch/irq.h>

#include "xtensa.h"
#include "xtensa_attr.h"

#ifdef CONFIG_ESP32S3_FLASH

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define FLASH_SECTOR_SIZE   4096          /* erase unit */
#define FLASH_PAGE_SIZE     256           /* program unit / MTD block */
#define FLASH_PAGE_SHIFT    8
#define BOUNCE_SIZE         FLASH_PAGE_SIZE

#define FS_BASE   CONFIG_ESP32S3_FLASH_FS_OFFSET
#define FS_SIZE   CONFIG_ESP32S3_FLASH_FS_SIZE

#if (FS_BASE % FLASH_SECTOR_SIZE) != 0 || (FS_SIZE % FLASH_SECTOR_SIZE) != 0
#error "Flash FS window must be 4 KB aligned"
#endif

#define OP_READ   0
#define OP_WRITE  1
#define OP_ERASE  2

/****************************************************************************
 * ROM functions (addresses from esp32s3_rom.ld)
 ****************************************************************************/

extern int esp_rom_spiflash_read(uint32_t src_addr, uint32_t *dest, int32_t len);
extern int esp_rom_spiflash_write(uint32_t dest_addr, const uint32_t *src, int32_t len);
extern int esp_rom_spiflash_erase_sector(uint32_t sector_num);
extern uint32_t rom_Cache_Suspend_ICache(void);
extern void Cache_Resume_ICache(uint32_t autoload);
extern uint32_t rom_Cache_Suspend_DCache(void);
extern void Cache_Resume_DCache(uint32_t autoload);

/* The S3 ROM's Cache_Suspend_* return before the cache is actually idle
 * (ESP-IDF: ESP_ROM_HAS_CACHE_SUSPEND_WAITI_BUG). Like IDF's patch, poll
 * EXTMEM_CACHE_STATE_REG until the cache reports idle (state == 1).
 */

#define EXTMEM_CACHE_STATE_REG  0x600c4130
#define ICACHE_STATE(v)         ((v) & 0xfff)
#define DCACHE_STATE(v)         (((v) >> 12) & 0xfff)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t g_bounce[BOUNCE_SIZE / 4] __attribute__((aligned(4)));
static sem_t g_flash_lock;

static struct mtd_dev_s g_s3_mtd;

/****************************************************************************
 * IRAM: everything between cache suspend and resume lives here. No calls to
 * flash code, no switch tables, no string literals.
 ****************************************************************************/

static int IRAM_ATTR __attribute__((noinline)) s3_flash_op(int op, uint32_t addr, uint32_t len)
{
	uint32_t istate;
	uint32_t dstate;
	int ret;

	istate = rom_Cache_Suspend_ICache();
	while (ICACHE_STATE(*(volatile uint32_t *)EXTMEM_CACHE_STATE_REG) != 1) ;
	dstate = rom_Cache_Suspend_DCache();
	while (DCACHE_STATE(*(volatile uint32_t *)EXTMEM_CACHE_STATE_REG) != 1) ;

	if (op == OP_READ) {
		ret = esp_rom_spiflash_read(addr, g_bounce, (int32_t)len);
	} else if (op == OP_WRITE) {
		ret = esp_rom_spiflash_write(addr, g_bounce, (int32_t)len);
	} else {
		ret = esp_rom_spiflash_erase_sector(addr / FLASH_SECTOR_SIZE);
	}

	Cache_Resume_DCache(dstate);
	Cache_Resume_ICache(istate);

	return ret;
}

/****************************************************************************
 * Helpers (normal flash-resident code, cache enabled)
 ****************************************************************************/

static int s3_flash_locked_op(int op, uint32_t addr, uint32_t len)
{
	irqstate_t flags;
	int ret;

	flags = irqsave();
	ret = s3_flash_op(op, addr, len);
	irqrestore(flags);

	return ret == 0 ? OK : -EIO;
}

static void s3_lock(void)
{
	while (sem_wait(&g_flash_lock) != OK) {
		DEBUGASSERT(get_errno() == EINTR);
	}
}

static void s3_unlock(void)
{
	sem_post(&g_flash_lock);
}

/* Read arbitrary range [off, off+n) of the FS window into buf */

static int s3_read_bytes(uint32_t off, uint8_t *buf, size_t n)
{
	while (n > 0) {
		uint32_t abs = FS_BASE + off;
		uint32_t aligned = abs & ~3u;
		uint32_t lead = abs - aligned;
		uint32_t chunk = BOUNCE_SIZE - lead;
		uint32_t rdlen;
		int ret;

		if (chunk > n) {
			chunk = n;
		}

		rdlen = (lead + chunk + 3) & ~3u;
		ret = s3_flash_locked_op(OP_READ, aligned, rdlen);
		if (ret < 0) {
			return ret;
		}

		memcpy(buf, (uint8_t *)g_bounce + lead, chunk);
		buf += chunk;
		off += chunk;
		n -= chunk;
	}

	return OK;
}

/* Program arbitrary range. NOR flash can only clear bits, so padding bytes
 * outside the requested range are written as 0xff (leaves them unchanged).
 * A program operation must not cross a 256-byte page boundary.
 */

static int s3_write_bytes(uint32_t off, const uint8_t *buf, size_t n)
{
	while (n > 0) {
		uint32_t abs = FS_BASE + off;
		uint32_t aligned = abs & ~3u;
		uint32_t lead = abs - aligned;
		uint32_t page_left = FLASH_PAGE_SIZE - (aligned & (FLASH_PAGE_SIZE - 1));
		uint32_t chunk = page_left - lead;
		uint32_t wrlen;
		int ret;

		if (chunk > n) {
			chunk = n;
		}

		wrlen = (lead + chunk + 3) & ~3u;
		memset(g_bounce, 0xff, wrlen);
		memcpy((uint8_t *)g_bounce + lead, buf, chunk);

		ret = s3_flash_locked_op(OP_WRITE, aligned, wrlen);
		if (ret < 0) {
			return ret;
		}

		buf += chunk;
		off += chunk;
		n -= chunk;
	}

	return OK;
}

/****************************************************************************
 * MTD operations
 ****************************************************************************/

static int s3_erase(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks)
{
	size_t i;
	int ret = OK;

	if ((startblock + nblocks) * FLASH_SECTOR_SIZE > FS_SIZE) {
		return -EINVAL;
	}

	s3_lock();
	for (i = 0; i < nblocks; i++) {
		ret = s3_flash_locked_op(OP_ERASE, FS_BASE + (startblock + i) * FLASH_SECTOR_SIZE, 0);
		if (ret < 0) {
			break;
		}
	}
	s3_unlock();

	return ret < 0 ? ret : (int)nblocks;
}

static ssize_t s3_bread(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks, FAR uint8_t *buffer)
{
	int ret;

	if (((startblock + nblocks) << FLASH_PAGE_SHIFT) > FS_SIZE) {
		return -EINVAL;
	}

	s3_lock();
	ret = s3_read_bytes(startblock << FLASH_PAGE_SHIFT, buffer, nblocks << FLASH_PAGE_SHIFT);
	s3_unlock();

	return ret < 0 ? ret : (ssize_t)nblocks;
}

static ssize_t s3_bwrite(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks, FAR const uint8_t *buffer)
{
	int ret;

	if (((startblock + nblocks) << FLASH_PAGE_SHIFT) > FS_SIZE) {
		return -EINVAL;
	}

	s3_lock();
	ret = s3_write_bytes(startblock << FLASH_PAGE_SHIFT, buffer, nblocks << FLASH_PAGE_SHIFT);
	s3_unlock();

	return ret < 0 ? ret : (ssize_t)nblocks;
}

static ssize_t s3_read(FAR struct mtd_dev_s *dev, off_t offset, size_t nbytes, FAR uint8_t *buffer)
{
	int ret;

	if (offset + nbytes > FS_SIZE) {
		return -EINVAL;
	}

	s3_lock();
	ret = s3_read_bytes(offset, buffer, nbytes);
	s3_unlock();

	return ret < 0 ? ret : (ssize_t)nbytes;
}

#ifdef CONFIG_MTD_BYTE_WRITE
static ssize_t s3_write(FAR struct mtd_dev_s *dev, off_t offset, size_t nbytes, FAR const uint8_t *buffer)
{
	int ret;

	if (offset + nbytes > FS_SIZE) {
		return -EINVAL;
	}

	s3_lock();
	ret = s3_write_bytes(offset, buffer, nbytes);
	s3_unlock();

	return ret < 0 ? ret : (ssize_t)nbytes;
}
#endif

static int s3_ioctl(FAR struct mtd_dev_s *dev, int cmd, unsigned long arg)
{
	int ret = -ENOTTY;

	switch (cmd) {
	case MTDIOC_GEOMETRY: {
		FAR struct mtd_geometry_s *geo = (FAR struct mtd_geometry_s *)arg;
		if (geo) {
			geo->blocksize = FLASH_PAGE_SIZE;
			geo->erasesize = FLASH_SECTOR_SIZE;
			geo->neraseblocks = FS_SIZE / FLASH_SECTOR_SIZE;
			strncpy(geo->model, "esp32s3-flash", sizeof(geo->model) - 1);
			geo->model[sizeof(geo->model) - 1] = '\0';
			ret = OK;
		} else {
			ret = -EINVAL;
		}
		break;
	}

	case MTDIOC_BULKERASE:
		ret = s3_erase(dev, 0, FS_SIZE / FLASH_SECTOR_SIZE);
		ret = ret < 0 ? ret : OK;
		break;

	case MTDIOC_ERASESTATE: {
		FAR uint8_t *state = (FAR uint8_t *)arg;
		if (state) {
			*state = 0xff;
			ret = OK;
		} else {
			ret = -EINVAL;
		}
		break;
	}

	case MTDIOC_XIPBASE:
	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct mtd_dev_s *esp32s3_flash_initialize(void)
{
	static bool initialized;

	if (!initialized) {
		sem_init(&g_flash_lock, 0, 1);

		memset(&g_s3_mtd, 0, sizeof(g_s3_mtd));
		g_s3_mtd.erase  = s3_erase;
		g_s3_mtd.bread  = s3_bread;
		g_s3_mtd.bwrite = s3_bwrite;
		g_s3_mtd.read   = s3_read;
#ifdef CONFIG_MTD_BYTE_WRITE
		g_s3_mtd.write  = s3_write;
#endif
		g_s3_mtd.ioctl  = s3_ioctl;
#ifdef CONFIG_MTD_REGISTRATION
		g_s3_mtd.name   = "esp32s3_flash";
#endif
		initialized = true;
	}

	return &g_s3_mtd;
}

#endif /* CONFIG_ESP32S3_FLASH */
