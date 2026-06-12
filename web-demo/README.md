# SweatSense Board Demo

Interactive demo for the ESP32-S3 + AD5940/AD5941 sweat-sensing potentiostat.

## Run

The demo is currently served at:

```text
http://127.0.0.1:8087/
```

Use Chrome or Microsoft Edge for the real board connection. Click **Connect USB**, then choose the ESP32-S3 serial port. The board previously enumerated as:

```text
COM5, USB VID:PID 303A:1001
```

## Presentation Flow

1. Open the local URL.
2. Click **Demo mode** if you want immediate animated responses.
3. Click the board regions:
   - Gold electrode pads: run a fast sweat read.
   - AD5940 AFE: send `CHECK`.
   - ESP32-S3: send `STATUS`.
   - Display UI: send `SWEATUI:92,31.7,78,66`.
4. Use **Fast sweat read** for a short EIS sweep, or **Full EIS sweep** for the longer project sweep.

## If The Current Firmware Is Noisy

During testing, the connected board was printing repeated I2C errors:

```text
Wire.cpp:513 requestFrom(): i2cRead returned Error -1
```

That suggests the current firmware is polling a missing/unresponsive touch or display I2C part, and it may not answer serial commands promptly. The web app still has demo mode, and `firmware/sweatsense_serial_demo.ino` is included as an optional clean presentation firmware that responds to the same commands without requiring the AFE/display hardware.
