/* oled - print text to an SSD1306 (default SCL=8 SDA=9 addr 0x3c 128x64)
 *   oled <text...>             clear and show text (wraps, newline via "/")
 *   oled -p <scl> <sda> <text> use other pins
 */
#include <tinyara/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arch/chip/esp32s3_ssd1306.h>

int oled_main(int argc, char *argv[])
{
	int scl = 8, sda = 9, first = 1;
	char line[128];

	if (argc >= 4 && !strcmp(argv[1], "-p")) {
		char *e1, *e2;
		scl = (int)strtol(argv[2], &e1, 0);
		sda = (int)strtol(argv[3], &e2, 0);
		if (*e1 || *e2) {
			printf("oled: pins must be numbers\n");
			return 1;
		}
		first = 4;
	}
	if (argc <= first) {
		printf("Usage: oled [-p scl sda] <text>   ('/' starts a new line)\n");
		return 1;
	}

	if (s3_ssd1306_init(scl, sda, 0, 128, 64) < 0) {
		printf("oled: no display at 0x3c (SCL=%d SDA=%d)\n", scl, sda);
		return 1;
	}

	line[0] = 0;
	for (int i = first; i < argc; i++) {
		if (i > first) strncat(line, " ", sizeof(line)-strlen(line)-1);
		strncat(line, argv[i], sizeof(line)-strlen(line)-1);
	}
	for (char *p = line; *p; p++) if (*p == '/') *p = '\n';

	s3_ssd1306_clear();
	s3_ssd1306_text(0, 0, line);
	if (s3_ssd1306_show() < 0) { printf("oled: write failed\n"); return 1; }
	printf("oled: ok\n");
	return 0;
}
