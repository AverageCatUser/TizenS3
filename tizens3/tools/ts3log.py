"""ts3log.py - keep reconnecting to a reset-looping ESP32-S3 and log everything.

Usage:  python ts3log.py COM8
Stop:   Ctrl+C   (everything is also saved to ts3_log.txt next to this script)
"""
import sys
import time
import serial  # comes with esptool (pyserial)

port = sys.argv[1] if len(sys.argv) > 1 else "COM8"
log = open("ts3_log.txt", "a", encoding="utf-8", errors="replace")


def emit(text):
    sys.stdout.write(text)
    sys.stdout.flush()
    log.write(text)
    log.flush()


emit(f"\n--- ts3log started on {port} at {time.strftime('%H:%M:%S')} ---\n")
was_open = False
while True:
    try:
        # dsrdtr/rtscts off and DTR/RTS low: don't reset the S3 when opening
        s = serial.Serial()
        s.port = port
        s.baudrate = 115200
        s.timeout = 0.1
        s.dtr = False
        s.rts = False
        s.open()
        if not was_open:
            emit(f"\n[{time.strftime('%H:%M:%S')}] connected\n")
            was_open = True
        while True:
            data = s.read(256)
            if data:
                emit(data.decode("utf-8", errors="replace"))
    except KeyboardInterrupt:
        emit("\n--- stopped ---\n")
        break
    except Exception:
        if was_open:
            emit(f"\n[{time.strftime('%H:%M:%S')}] disconnected\n")
            was_open = False
        time.sleep(0.05)  # retry fast so we catch the next boot
