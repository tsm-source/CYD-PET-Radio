"""Read several boards at once and prefix each line with the board name.

Usage: dual_monitor.py SECONDS NAME=PORT [NAME=PORT ...]
"""
import sys
import threading
import time

import serial

lock = threading.Lock()


def reader(name, port, stop_at):
    # Keep DTR/RTS low before opening so attaching does not reset the board.
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.5
    ser.dtr = False
    ser.rts = False
    ser.open()
    with ser:
        while time.time() < stop_at:
            line = ser.readline()
            if line:
                text = line.decode("utf-8", "replace").rstrip()
                with lock:
                    print(f"[{name:<8}] {text}", flush=True)


def main():
    duration = float(sys.argv[1])
    stop_at = time.time() + duration
    threads = []
    for spec in sys.argv[2:]:
        name, port = spec.split("=", 1)
        thread = threading.Thread(target=reader, args=(name, port, stop_at))
        thread.start()
        threads.append(thread)
    for thread in threads:
        thread.join()


if __name__ == "__main__":
    main()
