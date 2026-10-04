/****************************************************************************
 * apps/examples/s3_ver/ver_main.c
 *
 * ver          - TizenS3 version and system summary
 * tizens3_main - application entry point run at boot (replaces the sample
 *                "hello" entry; the TASH shell starts on its own)
 *
 * Licensed under the Apache License, Version 2.0.
 ****************************************************************************/

#include <tinyara/config.h>
#include <tinyara/version.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/statfs.h>

#include <arch/board/board.h>

uint32_t esp32s3_cpu_freq_mhz(void);

static void print_memory(void)
{
	struct mallinfo mi;

#ifdef CONFIG_CAN_PASS_STRUCTS
	mi = mallinfo();
#else
	(void)mallinfo(&mi);
#endif
	printf("  Memory    %d KB free of %d KB\n", mi.fordblks / 1024, mi.arena / 1024);
}

static void print_storage(void)
{
	struct statfs fs;

	if (statfs("/mnt", &fs) == 0 && fs.f_blocks > 0) {
		unsigned long total = (unsigned long)(fs.f_blocks * fs.f_bsize / 1024);
		unsigned long avail = (unsigned long)(fs.f_bavail * fs.f_bsize / 1024);
		printf("  Storage   /mnt  %lu KB free of %lu KB\n", avail, total);
	} else {
		printf("  Storage   /mnt  not mounted\n");
	}
}

int ver_main(int argc, char *argv[])
{
	printf("TizenS3 %s\n", TIZENS3_VERSION);
	printf("  Base      TizenRT %s (commit %s)\n", CONFIG_VERSION_STRING, CONFIG_VERSION_BUILD);
	printf("  Platform  ESP32-S3 @ %u MHz\n", (unsigned)esp32s3_cpu_freq_mhz());
	print_memory();
	print_storage();
	printf("  Drivers   console  flash  gpio  ws2812  i2c  ssd1306\n");
	printf("  Built     %s\n", CONFIG_VERSION_BUILD_TIME);
	return 0;
}

/* Boot-time application entry. Intentionally quiet: the banner is printed
 * by the board and the TASH shell starts independently.
 */

int tizens3_main(int argc, char *argv[])
{
	return 0;
}
