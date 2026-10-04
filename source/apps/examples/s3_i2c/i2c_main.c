/* i2c - software I2C from the shell (default SCL=8 SDA=9, 400 kHz)
 *   i2c scan [scl sda]       probe all addresses, list what ACKs
 *   i2c w <addr> <b0> <b1>.. write bytes to a device (addr + bytes in hex)
 */
#include <tinyara/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arch/chip/esp32s3_i2c.h>

/* strict integer parse: whole string must be a number (dec, or 0x hex) */
static int parse_num(const char *s, int *out)
{
	char *end;
	long v;

	if (s == NULL || *s == '\0') {
		return -1;
	}
	v = strtol(s, &end, 0);
	if (*end != '\0') {
		return -1;
	}
	*out = (int)v;
	return 0;
}

int i2c_main(int argc, char *argv[])
{
	int scl = 8, sda = 9;

	if (argc >= 2 && !strcmp(argv[1], "scan")) {
		if (argc >= 4 && (parse_num(argv[2], &scl) < 0 || parse_num(argv[3], &sda) < 0)) {
			printf("i2c: pins must be numbers\n");
			return 1;
		}
		if (s3_i2c_init(scl, sda, 400) < 0) {
			printf("i2c: invalid pins (SCL=%d SDA=%d)\n", scl, sda);
			return 1;
		}
		printf("i2c: scanning SCL=%d SDA=%d\n", scl, sda);
		int found = 0;
		for (int a = 1; a < 0x78; a++) {
			if (s3_i2c_probe((unsigned char)a) == 0) {
				printf("  0x%02x\n", a);
				found++;
			}
		}
		printf("i2c: %d device%s found\n", found, found == 1 ? "" : "s");
		return 0;
	}

	if (argc >= 4 && !strcmp(argv[1], "w")) {
		unsigned char buf[16];
		int addr, v, n = 0;
		if (parse_num(argv[2], &addr) < 0 || addr < 1 || addr > 0x77) {
			printf("i2c: address must be 0x01-0x77\n");
			return 1;
		}
		for (int i = 3; i < argc && n < 16; i++) {
			if (parse_num(argv[i], &v) < 0 || v < 0 || v > 255) {
				printf("i2c: byte '%s' must be 0-255 / 0x00-0xff\n", argv[i]);
				return 1;
			}
			buf[n++] = (unsigned char)v;
		}
		if (s3_i2c_init(scl, sda, 400) < 0) {
			printf("i2c: invalid pins\n");
			return 1;
		}
		int ret = s3_i2c_write((unsigned char)addr, buf, n);
		printf("i2c: 0x%02x: %s (%d byte%s)\n", addr, ret == 0 ? "ACK" : "NACK", n, n == 1 ? "" : "s");
		return ret == 0 ? 0 : 1;
	}

	printf("Usage: i2c scan [scl sda] | i2c w <addr> <byte>...\n"
	       "  Default pins: SCL=8 SDA=9\n");
	return 1;
}
