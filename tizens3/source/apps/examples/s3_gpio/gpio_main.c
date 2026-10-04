/* gpio - TizenS3 pin control from the shell
 *   gpio <pin> <0|1>     drive a pin low/high (configures it as output)
 *   gpio <pin>           read a pin (configures it as input, no pull)
 *   gpio <pin> up|down   read a pin with internal pull-up / pull-down
 *   gpio <pin> blink [n] toggle a pin n times (default 10), 250 ms each
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

static int usage(void)
{
	printf("Usage: gpio <pin> [0 | 1 | up | down | blink [count]]\n"
	       "  Pins: 0-18, 21, 33-42, 45-48\n");
	return 1;
}

int gpio_main(int argc, char *argv[])
{
	int pin;

	if (argc < 2) {
		return usage();
	}
	if (parse_num(argv[1], &pin) < 0) {
		printf("gpio: invalid pin '%s'\n", argv[1]);
		return usage();
	}
	if (!s3_gpio_pin_ok(pin)) {
		printf("gpio: pin %d is reserved (USB, flash or UART) or does not exist\n", pin);
		return usage();
	}

	if (argc == 2 || !strcmp(argv[2], "up") || !strcmp(argv[2], "down")) {
		int mode = S3_GPIO_INPUT;
		if (argc > 2) {
			mode = !strcmp(argv[2], "up") ? S3_GPIO_INPUT_PULLUP : S3_GPIO_INPUT_PULLDOWN;
		}
		s3_gpio_config(pin, mode);
		usleep(1000);
		printf("gpio %d: %s\n", pin, s3_gpio_read(pin) ? "high" : "low");
		return 0;
	}

	if (!strcmp(argv[2], "blink")) {
		int n = 10;
		if (argc > 3 && (parse_num(argv[3], &n) < 0 || n < 1 || n > 10000)) {
			printf("gpio: count must be 1-10000\n");
			return 1;
		}
		s3_gpio_config(pin, S3_GPIO_OUTPUT);
		for (int i = 0; i < n; i++) {
			s3_gpio_write(pin, 1);
			usleep(250000);
			s3_gpio_write(pin, 0);
			usleep(250000);
		}
		printf("gpio %d: blinked %d times\n", pin, n);
		return 0;
	}

	if (!strcmp(argv[2], "0") || !strcmp(argv[2], "1")) {
		int v = argv[2][0] - '0';
		s3_gpio_config(pin, S3_GPIO_OUTPUT);
		s3_gpio_write(pin, v);
		printf("gpio %d: %s\n", pin, v ? "high" : "low");
		return 0;
	}

	return usage();
}
