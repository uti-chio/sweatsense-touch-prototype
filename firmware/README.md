# Waveshare Touch UI Prototype

This is the touchscreen-first firmware for the off-the-shelf ESP32-S3 touch board.
It is meant to prove the user interface now, then transfer the UI/sensor contract to
the custom potentiostat PCB after the hardware bugs are fixed.

## Target Board

Likely target: Waveshare `ESP32-S3-Touch-LCD-1.69` or a pin-compatible clone.

Detected from the connected board:

- ESP32-S3
- 8 MB PSRAM
- 16 MB flash
- USB VID:PID `303A:1001`
- Serial port `COM5`

The firmware uses the off-the-shelf board pinout:

| Function | Pins |
| --- | --- |
| LCD ST7789 240x280 | DC 4, CS 5, CLK 6, DIN 7, RST 8, BL 15 |
| Touch CST816T | SCL 10, SDA 11, RST 13, INT 14 |
| Power hold | SYS_EN 41 |

## What It Shows

- Sweat contact area with CE0/SE0 electrode pads
- Hydration index, skin contact, glucose placeholder, skin temperature
- Touch actions for fast read, AFE check, status, and display refresh
- USB serial commands compatible with the web demo:
  - `STATUS`
  - `CHECK`
  - `MEASURE:SAMPLE`
  - `MEASURE:0,start,end,points,...,amplitude`
  - `SWEATUI:...`
  - `STOP`

The measurement values are presentation/demo values on this off-the-shelf board.
When the custom PCB is ready, replace `runPresentationSweep()` with the real AD5940
sensor call while keeping the UI screens and serial command contract.

## Build And Flash

From this folder:

```powershell
C:\Users\zoeyz\AppData\Local\Programs\Python\Python314\python.exe -m pip install --user platformio
C:\Users\zoeyz\AppData\Local\Programs\Python\Python314\python.exe -m platformio run -t upload --upload-port COM5
```

Then open serial monitor:

```powershell
C:\Users\zoeyz\AppData\Local\Programs\Python\Python314\python.exe -m platformio device monitor -p COM5 -b 115200
```

## Transfer Plan

1. Keep this app as the stable HMI.
2. Move board-specific pins into one adapter file.
3. On the custom PCB, swap display/touch pins and enable the AD5940 measurement function.
4. Keep the same serial/UI commands so the web demo and touchscreen demo continue to behave the same way.
