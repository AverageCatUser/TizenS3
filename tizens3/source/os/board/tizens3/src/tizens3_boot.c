/****************************************************************************
 * board/tizens3/src/esp32s3_boot.c
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <syslog.h>

#include <tinyara/board.h>
#include <tinyara/fs/mtd.h>
#include <arch/board/board.h>

void up_puts(const char *str);
uint32_t esp32s3_cpu_freq_mhz(void);
void esp32s3_serial_start_rx(void);

#ifdef CONFIG_FLASH_PARTITION
#include "common.h"
#endif

#ifdef CONFIG_ESP32S3_FLASH
FAR struct mtd_dev_s *esp32s3_flash_initialize(void);
#endif

/* Called from __start before os_start(): nothing to do yet, the bootloader
 * already configured clocks and the flash cache.
 */

void esp32_board_initialize(void)
{
}

static void esp32s3_mount_flash(void)
{
#if defined(CONFIG_ESP32S3_FLASH) && defined(CONFIG_FLASH_PARTITION)
	FAR struct mtd_dev_s *mtd;
	partition_info_t partinfo;

	mtd = esp32s3_flash_initialize();
	if (mtd == NULL) {
		printf("ERROR: flash init failed\n");
		return;
	}

	if (configure_mtd_partitions(mtd, 0, &partinfo) != OK) {
		printf("ERROR: configure_mtd_partitions failed\n");
		return;
	}

#ifdef CONFIG_AUTOMOUNT
	automount_fs_partition(&partinfo);
#endif
#endif
}

/* Called by the OS after the kernel is up (CONFIG_BOARD_INITIALIZE) */

void board_initialize(void)
{
#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('F'); }
#endif
	/* Direct to console hardware: stdio is not wired up this early */
	up_puts("\n"
	       "  _____ _              ____ _____\n"
	       " |_   _(_)_______ _ _ / ___|___ /\n"
	       "   | | | |_  / -_) ' \\___ \\ |_ \\\n"
	       "   |_| |_/__\\___|_||_|___) |__) |\n"
	       "                     |____/____/\n"
	       "  TizenS3 v" TIZENS3_VERSION "  -  TizenRT on ESP32-S3 (CyberMeow)\n\n");

	{
		char line[64];
		snprintf(line, sizeof(line), "  ESP32-S3 @ %u MHz\n\n", (unsigned)esp32s3_cpu_freq_mhz());
		up_puts(line);
	}

#ifdef CONFIG_FS_PROCFS
	int ret = mount(NULL, "/proc", "procfs", 0, NULL);
	if (ret < 0) {
		syslog(LOG_ERR, "ERROR: Failed to mount procfs at /proc: %d\n", ret);
	}
#endif

#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('G'); }
#endif
	esp32s3_mount_flash();

#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('H'); }
#endif
	/* Console RX thread (scheduler is up by now) */
	esp32s3_serial_start_rx();
#ifdef CONFIG_ESP32S3_BOOT_TRACE
	{ extern void esp32s3_trace(char); esp32s3_trace('I'); }
#endif

}

#ifdef CONFIG_LIB_BOARDCTL
int board_app_initialize(void)
{
	return OK;
}
#endif
