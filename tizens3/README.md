# TizenS3 v1.0.0

**An unofficial port of Samsung TizenRT 5.0 to the ESP32-S3.**
Tested on real hardware: ESP32-S3 Super Mini (ESP32-S3FH4R2, 4 MB flash, 2 MB PSRAM).
Runs TizenRT's own kernel, which is derived from Apache NuttX.

> TizenS3 is not affiliated with, endorsed by, or supported by Samsung or the
> Linux Foundation. "Tizen" and "TizenRT" are trademarks of their respective
> owners and are used here only to describe what this project is based on.

```
  _____ _              ____ _____
 |_   _(_)_______ _ _ / ___|___ /
   | | | |_  / -_) ' \___ \ |_ \
   |_| |_/__\___|_||_|___) |__) |
                     |____/____/
  TizenS3 v1.0.0  -  TizenRT on ESP32-S3 (CyberMeow)

/dev/smart0p0 is mounted successfully @ /mnt
TizenS3>>
```

## Features

| Feature | Status |
|---|---|
| TizenRT 5.0 kernel + TASH shell (`TizenS3>>` prompt) | ✅ |
| Console over native USB-C (USB-Serial-JTAG) **and** UART0 (GPIO43/44) | ✅ |
| 3 MB persistent flash filesystem (SmartFS, wear levelling) at `/mnt` | ✅ |
| `gpio` command + C API (output, input, pull-up/down, blink) | ✅ |
| `rgb` command for the on-board WS2812 LED (GPIO48) | ✅ |
| Software I2C (`i2c scan`, `i2c w`) + C API | ✅ |
| SSD1306 OLED (`oled <text>`) + C API with built-in font | ✅ |
| CPU clock measured at boot (accurate `sleep`/`uptime`) | ✅ |
| Wi-Fi / Bluetooth | ❌ not yet |
| SPI / interrupts on GPIO | ❌ not yet |

## Flash it

### Easiest: in the browser (Spacehuhn ESPWebTool)

1. Download `flash/tizens3_full.bin` from this repo.
2. Open **https://esp.huhn.me** in Chrome or Edge.
3. Plug in the board, click **Connect**, pick its port.
   *If it won't connect: hold **BOOT**, tap **RST**, release **BOOT**, try again.*
4. Click **Erase** and wait.
5. In the first row set the address to **`0x0`** and choose `tizens3_full.bin`.
6. Click **Program**, wait, then tap **RST** on the board.
7. Open the serial console (any baud on USB-C). Press Enter to see `TizenS3>>`.

### Command line (esptool)

```
esptool --chip esp32s3 erase_flash
esptool --chip esp32s3 --baud 460800 write_flash 0x0 flash/tizens3_full.bin
```

## Commands

```
help                      list everything
ver                       TizenS3 version, base, CPU speed
free / ps / uptime        memory, tasks, time since boot
cat /proc/version         TizenRT version + commit hash
ls /mnt                   your persistent storage (survives reboots)
echo hi > /mnt/a.txt      write a file;  cat /mnt/a.txt  to read it
gpio <pin> 0|1            drive a pin
gpio <pin>                read a pin      (gpio <pin> up | down for pulls)
gpio <pin> blink 5        blink a pin
rgb 255 0 128             set the on-board RGB LED
rgb demo                  red, green, blue, white, off
rgb off
i2c scan                  list I2C devices (SCL=8 SDA=9)
i2c w 0x3c 0x00 0xaf      write bytes to a device
oled hello/world          text on an SSD1306 ('/' = new line)
```

`gpio` refuses pins that would break the board: 19/20 (USB), 22-32
(missing or flash/PSRAM), 43/44 (UART console).
Usable: 0-18, 21, 33-42, 45-48.

Don't use `format` or `corrupt` (they target LittleFS, not SmartFS).

## Build from source

TizenS3 is distributed as a patch against Samsung's TizenRT so the original
history and licensing stay intact.

1. Get Samsung TizenRT at commit **`654daa8`** (https://github.com/Samsung/TizenRT).
2. Apply `tizens3.patch`.
3. Toolchain: `xtensa-esp32s3-elf` GCC 8.4.0, Espressif release `esp-2021r2-patch5`.
4. Configure with `tizens3/tash`, build (`make pass1deps pass2deps`, then `make`),
   and package `build/output/bin/tinyara.elf` with `esptool elf2image` +
   `merge-bin` together with `flash/bootloader.bin` (0x0) and
   `flash/partitions.bin` (0x8000), app at 0x10000.

The `source/` folder contains every **new** file the port adds (same paths as in
the TizenRT tree) so you can browse the code here. `tizens3.patch` contains
those plus the small changes to existing TizenRT files.

## Flash layout (4 MB)

| Offset | Size | Contents |
|---|---|---|
| `0x0` | | 2nd-stage bootloader (ESP-IDF v4.4) |
| `0x8000` | 4 KB | Partition table (`flash/partitions_s3.csv`) |
| `0x10000` | 1 MB | TizenS3 firmware |
| `0x110000` | ~3 MB | `/mnt` SmartFS storage |

## Notable fixes found while porting

- **Cache/MMU never configured** after the bootloader: the S3 fetched rodata as
  code and crashed on the first flash instruction. Fixed by mirroring ESP-IDF's
  startup (`rom_config_*_cache_mode`, `Cache_Set_IDROM_MMU_Size`).
- **ROM reports the wrong CPU clock** (80 MHz while the core runs at 40 MHz):
  TizenS3 now measures it against the 16 MHz SYSTIMER.
- **USB console race**: polling USB-Serial-JTAG from the tick ISR corrupted the
  endpoint on hardware; RX now runs on the HP work queue with locking.
- Idle-stack overflow corrupting the system clock (raised to 4 KB).
- Upstream TizenRT ESP32 fixes: kernel heap placed on top of the exception
  vectors (all ESP32 defconfigs), a missing comma in `esp32_start.c`, and a
  `xchal_cp3store` typo in `xtensa_coproc.S`.

## Tools

`tools/ts3log.py`: auto-reconnecting serial logger. Useful if a build ever
reset-loops (`python ts3log.py COM8`).

## Credits

- **Samsung TizenRT**: kernel, shell, filesystem, libc (Apache-2.0)
- **Apache NuttX**: TizenRT's origin
- **Espressif ESP-IDF**: ESP32-S3 core headers, ROM symbol table,
  2nd-stage bootloader (Apache-2.0 / MIT for Xtensa config headers)
- Port: **AverageCatUser** / CyberMeow

## License

Apache License 2.0, see `LICENSE`. See `NOTICE` for attribution.
