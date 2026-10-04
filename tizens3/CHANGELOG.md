# Changelog

## v1.0.0 (2026-10-04)

First stable release.

- New: `ver` command; version now defined once (TIZENS3_VERSION in board.h)
  and shown in the boot banner

- New: software I2C (`i2c scan`, `i2c w`), SSD1306 OLED (`oled`), open-drain GPIO
- Fix: console could deadlock when printing from interrupt/panic context
  (semaphore taken with interrupts disabled); now an IRQ critical section
- Fix: SSD1306 framebuffer overflow if a size above 128x64 was requested
- Fix: I2C reconfigured the pin mux on every clock edge (slow, glitchy);
  pins are now set up once, open-drain with pull-up, with a bus lock and
  clock-stretch timeouts on reads
- Fix: shell commands accepted junk numbers ("gpio abc 1" drove GPIO0, the
  BOOT strap pin); all input is now strictly validated
- Fix: .bss clear used undefined pointer arithmetic
- Diagnostic trace/probe configs resynced; probe code only built when enabled

## v0.1 (2026-10-04)

First release, tested on the ESP32-S3 Super Mini.

- TizenRT 5.0 (commit 654daa8) running on the ESP32-S3 (PRO CPU)
- Console on native USB-C and UART0 at the same time
- 3 MB SmartFS flash filesystem at /mnt, auto-mounted
- `gpio` and `rgb` shell commands, GPIO C API
- CPU clock measured at boot
- Boot banner and `TizenS3>>` prompt
