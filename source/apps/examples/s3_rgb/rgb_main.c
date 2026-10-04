/* rgb - drive the Super Mini's WS2812 RGB LED (GPIO48)
 *   rgb <r> <g> <b>   0-255 each, e.g. "rgb 255 0 128"
 *   rgb off
 *   rgb demo          cycle red/green/blue/white/off
 *   rgb <r> <g> <b> <pin>   use a WS2812 on another pin
 */
#include <tinyara/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arch/chip/esp32s3_gpio.h>

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

static int clamp(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

int rgb_main(int argc, char *argv[])
{
	int pin = S3_GPIO_RGB_LED;

	if (argc == 2 && !strcmp(argv[1], "off")) {
		s3_ws2812_write(pin, 0, 0, 0);
		printf("rgb: off\n");
		return 0;
	}

	if (argc == 2 && !strcmp(argv[1], "demo")) {
		static const unsigned char c[][3] = {
			{40, 0, 0}, {0, 40, 0}, {0, 0, 40}, {40, 40, 40}, {0, 0, 0}
		};
		static const char *name[] = { "red", "green", "blue", "white", "off" };
		for (int i = 0; i < 5; i++) {
			printf("rgb: %s\n", name[i]);
			s3_ws2812_write(pin, c[i][0], c[i][1], c[i][2]);
			sleep(1);
		}
		return 0;
	}

	if (argc == 4 || argc == 5) {
		int r, g, b;
		if (parse_num(argv[1], &r) < 0 || parse_num(argv[2], &g) < 0 ||
		    parse_num(argv[3], &b) < 0 || (argc == 5 && parse_num(argv[4], &pin) < 0)) {
			printf("rgb: values must be 0-255\n");
			return 1;
		}
		r = clamp(r); g = clamp(g); b = clamp(b);
		if (s3_ws2812_write(pin, r, g, b) < 0) {
			printf("rgb: pin %d is reserved or does not exist\n", pin);
			return 1;
		}
		printf("rgb: %d %d %d on gpio %d\n", r, g, b, pin);
		return 0;
	}

	printf("Usage: rgb <r> <g> <b> [pin] | rgb off | rgb demo\n");
	return 1;
}
