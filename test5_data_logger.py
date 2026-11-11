#!/usr/bin/env python3
"""Đọc dữ liệu từ trạm nhận qua USB và ghi vào du_lieu.csv.

Cách dùng:  python3 ghi_du_lieu.py /dev/cu.usbserial-XXXX
Cần cài một lần:  python3 -m pip install pyserial
"""
import csv
import datetime
import glob
import os
import sys
import time

import serial  # gói pyserial

OUT = "du_lieu.csv"
FIELDS = ["mac_utc", "node", "seq", "node_utc", "adc_mv", "temp_c", "flags", "rst", "rssi"]


def open_port(port):
    while True:
        try:
            return serial.Serial(port, 115200, timeout=2)
        except serial.SerialException as e:
            print(f"Chưa mở được {port} ({e}). Thử lại sau 3 giây...")
            time.sleep(3)


def main():
    if len(sys.argv) < 2:
        print("Cách dùng: python3 ghi_du_lieu.py /dev/cu.usbserial-XXXX")
        print("Các cổng đang có:", glob.glob("/dev/cu.usbserial*") + glob.glob("/dev/cu.SLAB*"))
        return
    port = sys.argv[1]
    new_file = not os.path.exists(OUT)
    last_seq = {}
    with open(OUT, "a", newline="") as f:
        writer = csv.writer(f)
        if new_file:
            writer.writerow(FIELDS)
        ser = open_port(port)
        print(f"Đang nghe {port}, ghi vào {os.path.abspath(OUT)}. Bấm Ctrl+C để dừng.")
        while True:
            try:
                line = ser.readline().decode("utf-8", "replace").strip()
            except (serial.SerialException, OSError):
                print("Mất kết nối với trạm nhận, đang kết nối lại...")
                try:
                    ser.close()
                except Exception:
                    pass
                time.sleep(2)
                ser = open_port(port)
                continue
            if not line:
                continue
            if not line.startswith("DATA,"):
                print("trạm:", line)
                continue
            parts = line.split(",")[1:]
            if len(parts) != 8:
                continue
            now = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
            writer.writerow([now] + parts)
            f.flush()
            node, seq, flags = parts[0], int(parts[1]), int(parts[5])
            note = ""
            if node in last_seq and not flags & 1 and seq != last_seq[node] + 1:
                note = f"   <- mất {seq - last_seq[node] - 1} gói"
            if flags & 1:
                note = f"   <- nút vừa khởi động (rst={parts[6]})"
            last_seq[node] = seq
            print(f"{now}  N0{node}  seq {seq}  {parts[4]} °C  adc {parts[3]} mV  rssi {parts[7]}{note}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nĐã dừng.")
