# SweatSense Touch Prototype

Touchscreen-first prototype for a wearable sweat-sensing potentiostat project.

This repository separates the project into two parts:

- `firmware/` - ESP32-S3 touchscreen firmware for the off-the-shelf touch board.
- `web-demo/` - browser-based USB serial demo for presenting board responses from a laptop.

## Current Hardware Target

The current development board is an off-the-shelf ESP32-S3 touch display board, likely a Waveshare `ESP32-S3-Touch-LCD-1.69`.
Detected board characteristics:

- ESP32-S3
- 8 MB PSRAM
- 16 MB flash
- USB Serial/JTAG, `VID:PID 303A:1001`
- Tested on `COM5`

The firmware targets this display/touch pinout:

| Peripheral | Details |
| --- | --- |
| LCD | ST7789, 240x280, SPI |
| LCD pins | DC 4, CS 5, CLK 6, DIN 7, RST 8, BL 15 |
| Touch | CST816T capacitive touch |
| Touch pins | SCL 10, SDA 11, RST 13, INT 14 |

## Why This Exists

The custom potentiostat PCB is still being debugged. This repo lets us finish the user interface on a known-good off-the-shelf touchscreen board first. When the custom PCB is ready, the UI and serial command contract can transfer over, while the board-specific pin and sensor code changes underneath.

## Firmware

From `firmware/`:

```powershell
C:\Users\zoeyz\AppData\Local\Programs\Python\Python314\python.exe -m platformio run
C:\Users\zoeyz\AppData\Local\Programs\Python\Python314\python.exe -m platformio run -t upload --upload-port COM5
```

Serial commands:

- `STATUS`
- `CHECK`
- `MEASURE:SAMPLE`
- `MEASURE:0,start,end,points,...,amplitude`
- `SWEATUI:...`
- `STOP`

## Web Demo

Serve `web-demo/` from localhost and open in Chrome or Edge:

```powershell
cd web-demo
C:\Users\zoeyz\AppData\Local\Programs\Python\Python314\python.exe -m http.server 8087 --bind 127.0.0.1
```

Then open:

```text
http://127.0.0.1:8087/
```

Use **Connect USB** to select the ESP32-S3 serial port, or **Demo mode** for a presentation without live hardware.

## Transfer Plan For Custom PCB

1. Keep the touchscreen UI and serial commands.
2. Move board-specific pins into a hardware adapter.
3. Replace the presentation sweep with real AD5940/AD5941 measurements.
4. Retest the same command set from the web demo and touchscreen UI.
