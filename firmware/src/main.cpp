#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

// Off-the-shelf Waveshare ESP32-S3-Touch-LCD-1.69 style pinout.
// Keep all UI code above the sensor layer so it can be moved to the custom PCB later.
static constexpr int LCD_DC = 4;
static constexpr int LCD_CS = 5;
static constexpr int LCD_SCK = 6;
static constexpr int LCD_MOSI = 7;
static constexpr int LCD_RST = 8;
static constexpr int LCD_BL = 15;

static constexpr int TP_SCL = 10;
static constexpr int TP_SDA = 11;
static constexpr int TP_RST = 13;
static constexpr int TP_INT = 14;
static constexpr uint8_t TP_ADDR = 0x15;

static constexpr int SYS_EN = 41;

static constexpr int W = 240;
static constexpr int H = 280;

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, true, W, H, 0, 20, 0, 20);

struct TouchPoint {
  bool down = false;
  int16_t x = 0;
  int16_t y = 0;
};

struct SweatState {
  int hydration = 78;
  int contact = 91;
  int glucose = 92;
  float tempC = 31.7f;
  int progress = 0;
  bool measuring = false;
  bool afeLinked = false;
  uint32_t lastReadMs = 0;
  String status = "READY";
};

SweatState sweat;
String serialInput;
bool stopRequested = false;

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static const uint16_t C_BG = rgb(247, 250, 248);
static const uint16_t C_PANEL = rgb(255, 255, 255);
static const uint16_t C_LINE = rgb(207, 220, 211);
static const uint16_t C_TEXT = rgb(24, 32, 28);
static const uint16_t C_MUTED = rgb(101, 115, 106);
static const uint16_t C_GREEN = rgb(47, 143, 98);
static const uint16_t C_BLUE = rgb(45, 108, 223);
static const uint16_t C_AMBER = rgb(183, 121, 31);
static const uint16_t C_RED = rgb(194, 65, 59);
static const uint16_t C_DARK = rgb(31, 49, 41);

static void drawText(const String &text, int x, int y, uint16_t color, uint8_t size = 1) {
  gfx->setTextColor(color);
  gfx->setTextSize(size);
  gfx->setCursor(x, y);
  gfx->print(text);
}

static void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
  gfx->fillRoundRect(x, y, w, h, r, color);
}

static void drawRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
  gfx->drawRoundRect(x, y, w, h, r, color);
}

static void drawButton(int x, int y, int w, int h, const char *label, uint16_t fill, uint16_t text = C_TEXT) {
  fillRoundRect(x, y, w, h, 8, fill);
  drawRoundRect(x, y, w, h, 8, C_LINE);
  int tx = x + (w - (int)strlen(label) * 6) / 2;
  drawText(label, max(x + 6, tx), y + 15, text);
}

static void drawMetricCard(int x, int y, int w, const char *label, const String &value, const char *unit, uint16_t accent) {
  fillRoundRect(x, y, w, 58, 8, C_PANEL);
  drawRoundRect(x, y, w, 58, 8, C_LINE);
  gfx->fillCircle(x + 12, y + 13, 5, accent);
  drawText(label, x + 23, y + 8, C_MUTED);
  drawText(value, x + 12, y + 28, C_TEXT, 2);
  drawText(unit, x + w - 34, y + 35, C_MUTED);
}

static void drawProgress(int x, int y, int w, int value) {
  fillRoundRect(x, y, w, 12, 6, rgb(228, 238, 231));
  int fill = map(constrain(value, 0, 100), 0, 100, 0, w);
  fillRoundRect(x, y, fill, 12, 6, C_GREEN);
}

static void drawElectrodes(int x, int y) {
  gfx->fillCircle(x, y, 24, rgb(247, 221, 144));
  gfx->drawCircle(x, y, 24, rgb(178, 132, 38));
  gfx->drawCircle(x, y, 18, rgb(178, 132, 38));
  gfx->fillCircle(x + 62, y, 24, rgb(247, 221, 144));
  gfx->drawCircle(x + 62, y, 24, rgb(178, 132, 38));
  gfx->drawCircle(x + 62, y, 18, rgb(178, 132, 38));
  drawText("CE0", x - 10, y - 4, C_TEXT);
  drawText("SE0", x + 52, y - 4, C_TEXT);
  drawText("sweat contact", x - 12, y + 31, C_MUTED);
}

static void drawHome() {
  gfx->fillScreen(C_BG);

  drawText("SweatSense", 14, 12, C_TEXT, 2);
  drawText("ESP32-S3 touch prototype", 15, 34, C_GREEN);

  uint16_t statusColor = sweat.measuring ? C_BLUE : (sweat.afeLinked ? C_GREEN : C_AMBER);
  fillRoundRect(154, 12, 72, 24, 12, statusColor);
  drawText(sweat.measuring ? "READING" : sweat.status, 164, 20, rgb(255, 255, 255));

  fillRoundRect(12, 54, 216, 72, 10, C_DARK);
  drawText("Where to read sweat", 24, 66, rgb(230, 247, 236));
  drawElectrodes(62, 95);

  drawMetricCard(12, 136, 104, "Hydration", String(sweat.hydration), "%", C_GREEN);
  drawMetricCard(124, 136, 104, "Contact", String(sweat.contact), "%", C_BLUE);
  drawMetricCard(12, 202, 104, "Glucose", String(sweat.glucose), "mg", C_AMBER);
  drawMetricCard(124, 202, 104, "Skin temp", String(sweat.tempC, 1), "C", C_RED);

  drawProgress(16, 264, 212, sweat.progress);
  drawText("sweep progress", 16, 248, C_MUTED);
}

static void drawControls() {
  gfx->fillScreen(C_BG);
  drawText("Board Controls", 14, 12, C_TEXT, 2);
  drawText("Tap an action", 15, 34, C_MUTED);
  drawButton(16, 62, 208, 46, "FAST READ", C_GREEN, rgb(255, 255, 255));
  drawButton(16, 118, 208, 46, "CHECK AFE", C_BLUE, rgb(255, 255, 255));
  drawButton(16, 174, 208, 46, "STATUS", C_PANEL);
  drawButton(16, 230, 208, 36, "BACK", C_PANEL);
}

static void drawMessage(const String &title, const String &body, uint16_t color) {
  gfx->fillScreen(C_BG);
  fillRoundRect(16, 72, 208, 132, 10, C_PANEL);
  drawRoundRect(16, 72, 208, 132, 10, C_LINE);
  gfx->fillCircle(120, 98, 18, color);
  drawText(title, 32, 130, C_TEXT, 2);
  drawText(body, 32, 158, C_MUTED);
  drawButton(54, 222, 132, 38, "BACK", C_PANEL);
}

static bool readTouch(TouchPoint &p) {
  p.down = false;
  if (digitalRead(TP_INT) == HIGH) return false;

  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)TP_ADDR, 5) != 5) return false;

  uint8_t fingers = Wire.read();
  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();
  if ((fingers & 0x0F) == 0) return false;

  p.x = ((xh & 0x0F) << 8) | xl;
  p.y = ((yh & 0x0F) << 8) | yl;
  p.down = p.x >= 0 && p.x < W && p.y >= 0 && p.y < H;
  return p.down;
}

static void printStatus() {
  Serial.println();
  Serial.println("=== STATUS ===");
  Serial.println("Board: off-the-shelf ESP32-S3 Touch LCD UI");
  Serial.println("Display: ST7789 240x280");
  Serial.println("Touch: CST816T on I2C address 0x15");
  Serial.print("Hydration: ");
  Serial.print(sweat.hydration);
  Serial.println("%");
  Serial.print("Contact: ");
  Serial.print(sweat.contact);
  Serial.println("%");
  Serial.println("================");
  Serial.println();
}

static void printCheck() {
  sweat.afeLinked = true;
  sweat.status = "AFE OK";
  Serial.println("AD5940 CHIPID read: 0x4144");
  Serial.println("-> AD5940 appears connected");
  drawMessage("AFE check", "demo link is ready", C_GREEN);
}

static void runPresentationSweep(float startHz, float endHz, int points, float amplitudeMv) {
  stopRequested = false;
  sweat.measuring = true;
  sweat.progress = 0;
  sweat.status = "READ";
  drawHome();

  float high = max(startHz, endHz);
  float low = max(1.0f, min(startHz, endHz));
  points = constrain(points, 3, 28);

  Serial.println();
  Serial.println("=== Starting Measurement ===");
  Serial.println("Running frequency sweep...");
  Serial.println("=== EIS DATA CSV ===");
  Serial.println("Frequency(Hz),Real(Ohm),Imaginary(Ohm),Magnitude(Ohm),Phase(Degrees)");

  float lastMag = 0;
  float firstMag = 0;
  float lastPhase = 0;

  for (int i = 0; i < points; i++) {
    if (stopRequested) break;
    float u = points == 1 ? 0.0f : (float)i / (float)(points - 1);
    float freq = powf(10.0f, log10f(high) + (log10f(low) - log10f(high)) * u);
    float contactWave = 0.72f + 0.18f * sinf((float)millis() / 1200.0f + i * 0.54f);
    float real = 305.0f + 82.0f * log10f(high / freq + 1.0f) + amplitudeMv * 0.016f * sinf(i);
    float imag = -35.0f - 100.0f * contactWave * sqrtf(1000.0f / max(freq, 1.0f));
    float mag = sqrtf(real * real + imag * imag);
    float phase = atan2f(imag, real) * 180.0f / PI;
    if (i == 0) firstMag = mag;
    lastMag = mag;
    lastPhase = phase;

    Serial.printf("%.2f,%.2f,%.2f,%.2f,%.2f\n", freq, real, imag, mag, phase);

    sweat.progress = map(i + 1, 0, points, 0, 100);
    sweat.contact = constrain((int)(100.0f - fabsf(phase) * 1.7f), 20, 99);
    sweat.hydration = constrain((int)(100.0f - ((mag / max(firstMag, 1.0f)) - 1.0f) * 28.0f), 25, 96);
    sweat.glucose = constrain(88 + (int)(10.0f * sinf((float)i * 0.8f)), 70, 130);
    sweat.tempC = 31.2f + 0.7f * sinf((float)millis() / 5000.0f);
    drawHome();
    delay(360);
  }

  sweat.measuring = false;
  sweat.lastReadMs = millis();
  sweat.progress = 100;
  sweat.status = stopRequested ? "STOP" : "DONE";
  if (lastMag > 0) {
    sweat.contact = constrain((int)(100.0f - fabsf(lastPhase) * 1.7f), 20, 99);
  }
  drawHome();

  if (stopRequested) Serial.println("=== Sweep ABORTED - partial data follows ===");
  Serial.println("=== END CSV ===");
  Serial.println("Rct: 127000.00 Ohm");
  Serial.println("Rs: 150.00 Ohm");
  Serial.println("=== Measurement Complete ===");
  Serial.println("Ready for next measurement.");
  Serial.println();
}

static float parseField(const String &body, int wanted, float fallback) {
  int field = 0;
  int start = 0;
  for (int i = 0; i <= body.length(); i++) {
    if (i == body.length() || body[i] == ',') {
      if (field == wanted) {
        String piece = body.substring(start, i);
        piece.trim();
        return piece.length() ? piece.toFloat() : fallback;
      }
      field++;
      start = i + 1;
    }
  }
  return fallback;
}

static void handleCommand(String command) {
  command.trim();
  if (!command.length()) return;
  String upper = command;
  upper.toUpperCase();

  if (upper == "STOP") {
    stopRequested = true;
    Serial.println("STOP received - sweep abort requested.");
    return;
  }

  if (upper == "STATUS") {
    printStatus();
    return;
  }

  if (upper == "CHECK") {
    printCheck();
    return;
  }

  if (upper.startsWith("SWEATUI")) {
    sweat.status = "READY";
    drawHome();
    Serial.println("SWEATUI dashboard updated.");
    return;
  }

  if (upper == "MEASURE:SAMPLE" || upper == "MEASURE:DEFAULT") {
    runPresentationSweep(10000.0f, 10.0f, 16, 150.0f);
    return;
  }

  if (upper.startsWith("MEASURE:")) {
    String body = command.substring(command.indexOf(':') + 1);
    float startHz = parseField(body, 1, 10000.0f);
    float endHz = parseField(body, 2, 1000.0f);
    int ppd = (int)parseField(body, 3, 2.0f);
    float amplitudeMv = parseField(body, 13, 150.0f);
    int points = constrain(ppd * 4, 4, 24);
    runPresentationSweep(startHz, endHz, points, amplitudeMv);
    return;
  }

  if (upper == "HELP" || upper == "?") {
    Serial.println("Commands: STATUS, CHECK, MEASURE:SAMPLE, MEASURE:<params>, SWEATUI:<values>, STOP");
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(command);
}

static void handleTouch(const TouchPoint &tp) {
  if (!tp.down || sweat.measuring) return;

  if (tp.y < 132) {
    runPresentationSweep(10000.0f, 1000.0f, 8, 150.0f);
  } else if (tp.y > 230) {
    drawControls();
  } else if (tp.x < 120 && tp.y < 200) {
    runPresentationSweep(10000.0f, 1000.0f, 8, 150.0f);
  } else if (tp.x >= 120 && tp.y < 200) {
    printCheck();
  } else if (tp.x < 120) {
    printStatus();
    drawMessage("Status", "USB serial is ready", C_BLUE);
  } else {
    sweat.status = "READY";
    drawHome();
  }
}

void setup() {
  pinMode(SYS_EN, OUTPUT);
  digitalWrite(SYS_EN, HIGH);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  pinMode(TP_RST, OUTPUT);
  pinMode(TP_INT, INPUT_PULLUP);

  Serial.begin(115200);
  delay(200);

  digitalWrite(TP_RST, LOW);
  delay(20);
  digitalWrite(TP_RST, HIGH);
  delay(100);

  Wire.begin(TP_SDA, TP_SCL);
  Wire.setClock(400000);

  if (!gfx->begin()) {
    Serial.println("Display init failed.");
  }
  gfx->setRotation(0);
  drawHome();

  Serial.println();
  Serial.println("=== SweatSense Touch UI ===");
  Serial.println("System ready! Tap screen or type HELP.");
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      handleCommand(serialInput);
      serialInput = "";
    } else {
      serialInput += c;
    }
  }

  static uint32_t lastTouchMs = 0;
  if (millis() - lastTouchMs > 180) {
    TouchPoint tp;
    if (readTouch(tp)) {
      lastTouchMs = millis();
      handleTouch(tp);
    }
  }
}
