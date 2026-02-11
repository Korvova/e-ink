#include <Arduino.h>
#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"

static const int DISP_W = 1360;
static const int DISP_H = 480;
static const int BYTES_PER_LINE = DISP_W / 8; // 170
static uint8_t frameBuf[DISP_H * BYTES_PER_LINE];

static void initEpdPins() {
  pinMode(EPD_W21_CS, OUTPUT);
  pinMode(EPD_W21_CS2, OUTPUT);
  pinMode(EPD_W21_RST, OUTPUT);
  pinMode(EPD_W21_MOSI, OUTPUT);
  pinMode(EPD_W21_CLK, OUTPUT);
  pinMode(EPD_W21_BUSY, INPUT_PULLUP);

  digitalWrite(EPD_W21_CS, HIGH);
  digitalWrite(EPD_W21_CS2, HIGH);
  digitalWrite(EPD_W21_RST, HIGH);
  digitalWrite(EPD_W21_CLK, HIGH);
}

static void clearFrame() {
  memset(frameBuf, 0x00, sizeof(frameBuf));
}

static void setPixel(int x, int y, bool black) {
  if (x < 0 || x >= DISP_W || y < 0 || y >= DISP_H) return;
  int idx = y * BYTES_PER_LINE + (x / 8);
  uint8_t bit = (uint8_t)(0x80 >> (x & 7));
  if (black) frameBuf[idx] |= bit;
  else frameBuf[idx] &= (uint8_t)~bit;
}

static uint8_t glyphRow(char c, int row) {
  switch (c) {
    case 'A': { static const uint8_t g[8] = {0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00}; return g[row]; }
    case 'E': { static const uint8_t g[8] = {0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00}; return g[row]; }
    case 'I': { static const uint8_t g[8] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}; return g[row]; }
    case 'K': { static const uint8_t g[8] = {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00}; return g[row]; }
    case 'M': { static const uint8_t g[8] = {0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00}; return g[row]; }
    case 'N': { static const uint8_t g[8] = {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00}; return g[row]; }
    case 'P': { static const uint8_t g[8] = {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00}; return g[row]; }
    case 'R': { static const uint8_t g[8] = {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00}; return g[row]; }
    case 'S': { static const uint8_t g[8] = {0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C,0x00}; return g[row]; }
    case 'T': { static const uint8_t g[8] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}; return g[row]; }
    case 'V': { static const uint8_t g[8] = {0x42,0x42,0x42,0x24,0x24,0x18,0x18,0x00}; return g[row]; }
    case '1': { static const uint8_t g[8] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}; return g[row]; }
    case '2': { static const uint8_t g[8] = {0x3C,0x42,0x02,0x0C,0x30,0x40,0x7E,0x00}; return g[row]; }
    default: return 0x00;
  }
}

static void drawChar8x8(int x, int y, char c, int scale) {
  for (int row = 0; row < 8; row++) {
    uint8_t bits = glyphRow(c, row);
    for (int col = 0; col < 8; col++) {
      if (!(bits & (0x80 >> col))) continue;
      for (int sy = 0; sy < scale; sy++) {
        for (int sx = 0; sx < scale; sx++) {
          setPixel(x + col * scale + sx, y + row * scale + sy, true);
        }
      }
    }
  }
}

static void drawTextCentered(const char* text, int scale) {
  int len = (int)strlen(text);
  int cw = 8 * scale;
  int ch = 8 * scale;
  int gap = scale;
  int w = len * cw + (len - 1) * gap;
  int x = (DISP_W - w) / 2;
  int y = (DISP_H - ch) / 2;

  for (int i = 0; i < len; i++) {
    if (text[i] != ' ') {
      drawChar8x8(x, y, text[i], scale);
    }
    x += cw + gap;
  }
}

static void drawTestPattern() {
  for (int y = 0; y < DISP_H / 3; y++) {
    for (int x = 0; x < BYTES_PER_LINE; x++) {
      frameBuf[y * BYTES_PER_LINE + x] = 0xFF;
    }
  }

  for (int y = DISP_H / 3; y < 2 * DISP_H / 3; y++) {
    for (int x = 0; x < BYTES_PER_LINE; x++) {
      frameBuf[y * BYTES_PER_LINE + x] = (x & 1) ? 0x00 : 0xFF;
    }
  }

  for (int y = 2 * DISP_H / 3; y < DISP_H; y++) {
    for (int x = 0; x < BYTES_PER_LINE; x++) {
      frameBuf[y * BYTES_PER_LINE + x] = ((y / 8 + x) & 1) ? 0x55 : 0xAA;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("EPD serial test ready");
  Serial.printf("Pins: CS=%d CS2=%d RST=%d BUSY=%d CLK=%d MOSI=%d\n",
                EPD_W21_CS, EPD_W21_CS2, EPD_W21_RST, EPD_W21_BUSY, EPD_W21_CLK, EPD_W21_MOSI);

  initEpdPins();
  EPD_SetMono(true);

  Serial.println("Commands:");
  Serial.println(" 1 - white");
  Serial.println(" 2 - black");
  Serial.println(" 3 - test pattern");
  Serial.println(" 4 - PRIVET RMS EKRAN 2");
  Serial.println(" r - reset pulse");
  Serial.println(" b - busy state");
}

void loop() {
  if (!Serial.available()) {
    delay(10);
    return;
  }

  char c = Serial.read();
  if (c == '1') {
    Serial.println("[cmd] WHITE");
    EPD_Init();
    EPD_WhiteScreen_White();
    EPD_DeepSleep();
  } else if (c == '2') {
    Serial.println("[cmd] BLACK");
    EPD_Init();
    EPD_WhiteScreen_Black();
    EPD_DeepSleep();
  } else if (c == '3') {
    Serial.println("[cmd] TEST PATTERN");
    clearFrame();
    drawTestPattern();
    EPD_Init();
    EPD_WhiteScreen_ALL(frameBuf);
    EPD_DeepSleep();
  } else if (c == '4') {
    Serial.println("[cmd] PRIVET RMS EKRAN 2");
    clearFrame();
    drawTextCentered("PRIVET RMS EKRAN 2", 5);
    EPD_Init();
    EPD_WhiteScreen_ALL(frameBuf);
    EPD_DeepSleep();
  } else if (c == 'r') {
    Serial.println("[cmd] RESET pulse");
    EPD_W21_RST_0;
    delay(10);
    EPD_W21_RST_1;
  } else if (c == 'b') {
    Serial.printf("[cmd] BUSY=%d\n", isEPD_W21_BUSY);
  }
}
