// E-ink caption board: 2x GDEM1085T51 (1360x480) via DESPI-C1085 on ESP32-S3-ETH.
// TTF text rendering (OpenFontRender, PT Sans Latin+Cyrillic) + web UI over Ethernet (W5500).
#include <Arduino.h>
#include <SPI.h>
#include <ETH.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "OpenFontRender.h"
#include <Adafruit_NeoPixel.h>
#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"
#include "web_page.h"

// ---------------------------------------------------------------- display
static const int DISP_W = 1360;
static const int DISP_H = 480;
static const int BYTES_PER_LINE = DISP_W / 8; // 170
static uint8_t frameBuf[DISP_H * BYTES_PER_LINE];

// Pin sets from README: screen 1 / screen 2 (SDI+SCLK shared)
static const int PINSET[2][6] = { // CS, CS2, RST, BUSY, CLK, MOSI
  {37, 40, 38, 39, 36, 35},
  {41, 42, 45, 1, 36, 35},
};
static int curPinSet = -1;

static void applyPinSet(int n) {
  curPinSet = n;
  EPD_W21_CS = PINSET[n][0]; EPD_W21_CS2 = PINSET[n][1]; EPD_W21_RST = PINSET[n][2];
  EPD_W21_BUSY = PINSET[n][3]; EPD_W21_CLK = PINSET[n][4]; EPD_W21_MOSI = PINSET[n][5];
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

static void initAllEpdPins() {
  for (int n = 0; n < 2; n++) applyPinSet(n);
}

static void clearFrame() { memset(frameBuf, 0x00, sizeof(frameBuf)); }

static inline void setPixel(int x, int y, bool black) {
  if (x < 0 || x >= DISP_W || y < 0 || y >= DISP_H) return;
  int idx = y * BYTES_PER_LINE + (x / 8);
  uint8_t bit = (uint8_t)(0x80 >> (x & 7));
  if (black) frameBuf[idx] |= bit;
  else frameBuf[idx] &= (uint8_t)~bit;
}

static void drawTestPattern() {
  for (int y = 0; y < DISP_H / 3; y++)
    for (int x = 0; x < BYTES_PER_LINE; x++) frameBuf[y * BYTES_PER_LINE + x] = 0xFF;
  for (int y = DISP_H / 3; y < 2 * DISP_H / 3; y++)
    for (int x = 0; x < BYTES_PER_LINE; x++) frameBuf[y * BYTES_PER_LINE + x] = (x & 1) ? 0x00 : 0xFF;
  for (int y = 2 * DISP_H / 3; y < DISP_H; y++)
    for (int x = 0; x < BYTES_PER_LINE; x++) frameBuf[y * BYTES_PER_LINE + x] = ((y / 8 + x) & 1) ? 0x55 : 0xAA;
}

// ---------------------------------------------------------------- fonts
extern const uint8_t ptsans_start[] asm("_binary_data_ptsans_ttf_start");
extern const uint8_t ptsans_end[]   asm("_binary_data_ptsans_ttf_end");
extern const uint8_t ptsansb_start[] asm("_binary_data_ptsansb_ttf_start");
extern const uint8_t ptsansb_end[]   asm("_binary_data_ptsansb_ttf_end");
extern const uint8_t notosc_start[] asm("_binary_data_notosc_ttf_start");
extern const uint8_t notosc_end[]   asm("_binary_data_notosc_ttf_end");

static OpenFontRender ofrRegular;
static OpenFontRender ofrBold;
static OpenFontRender ofrCjk;      // Noto Sans SC (medium), Latin+Cyrillic+GB2312 level-1

// true if the UTF-8 string contains CJK characters (>= U+2E80)
static bool hasCjk(const String& t) {
  for (size_t i = 0; i < t.length(); i++) {
    uint8_t c = (uint8_t)t[i];
    if (c >= 0xE0) {                                   // 3-byte sequence -> U+0800..U+FFFF
      if (i + 2 < t.length()) {
        uint32_t cp = ((c & 0x0F) << 12) | (((uint8_t)t[i + 1] & 0x3F) << 6) | ((uint8_t)t[i + 2] & 0x3F);
        if (cp >= 0x2E80) return true;
      }
      i += 2;
    } else if (c >= 0xC0) i += 1;
  }
  return false;
}

// OpenFontRender blends fg (black) and bg (white) by glyph coverage into RGB565.
// The panel is 1-bit, so a pixel becomes black when coverage is above 50%.
static void ofrDrawPixel(int32_t x, int32_t y, uint16_t c) {
  uint8_t g = (c >> 5) & 0x3F; // green channel, 0..63
  if (g < 32) setPixel(x, y, true);
}

// Fully covered runs inside glyphs come through drawFastHLine, not drawPixel.
static void ofrDrawHLine(int32_t x, int32_t y, int32_t w, uint16_t c) {
  uint8_t g = (c >> 5) & 0x3F;
  if (g >= 32) return;
  for (int32_t i = 0; i < w; i++) setPixel(x + i, y, true);
}

static void setupFonts() {
  for (OpenFontRender* r : { &ofrRegular, &ofrBold, &ofrCjk }) {
    r->setDrawPixel(ofrDrawPixel);
    r->setDrawFastHLine(ofrDrawHLine);
    // draw2screen() uses these, not the fg/bg arguments of drawString(): black on white
    r->setFontColor((uint16_t)0x0000, (uint16_t)0xFFFF);
    r->setBackgroundFillMethod(BgFillMethod::None);
    r->setLineSpaceRatio(1.15);
    r->setDebugLevel(OFR_ERROR);
  }
  // glyphs are rasterised in a shared FreeRTOS task; CJK outlines need a bigger stack than the 20 KB default
  ofrCjk.setRenderTaskStackSize(40 * 1024);
  FT_Error e1 = ofrRegular.loadFont(ptsans_start, ptsans_end - ptsans_start);
  FT_Error e2 = ofrBold.loadFont(ptsansb_start, ptsansb_end - ptsansb_start);
  FT_Error e3 = ofrCjk.loadFont(notosc_start, notosc_end - notosc_start);
  Serial.printf("[font] regular=%d (%u bytes), bold=%d (%u bytes), cjk=%d (%u bytes)\n",
                (int)e1, (unsigned)(ptsans_end - ptsans_start), (int)e2, (unsigned)(ptsansb_end - ptsansb_start),
                (int)e3, (unsigned)(notosc_end - notosc_start));
}

// Split text into lines (\n), trim \r
static int splitLines(const String& text, String* out, int maxLines) {
  int n = 0, start = 0;
  while (n < maxLines) {
    int nl = text.indexOf('\n', start);
    String line = (nl < 0) ? text.substring(start) : text.substring(start, nl);
    line.replace("\r", "");
    out[n++] = line;
    if (nl < 0) break;
    start = nl + 1;
  }
  return n;
}

// Render text into frameBuf. size==0 -> auto-fit. Returns used font size.
static int renderText(const String& text, int size, bool bold) {
  const int MARGIN_X = 30, MARGIN_Y = 16;
  String lines[8];
  OpenFontRender* fonts[8];
  int n = splitLines(text, lines, 8);
  if (n == 0) n = 1;
  for (int i = 0; i < n; i++) fonts[i] = hasCjk(lines[i]) ? &ofrCjk : (bold ? &ofrBold : &ofrRegular);

  if (size <= 0) {
    for (size = 440; size > 16; size -= 8) {
      int lineH = (int)(size * 1.15);
      if (n * lineH > DISP_H - 2 * MARGIN_Y) continue;
      int maxW = 0;
      for (int i = 0; i < n; i++) {
        if (lines[i].length() == 0) continue;
        fonts[i]->setFontSize(size);
        int w = (int)fonts[i]->getTextWidth("%s", lines[i].c_str());
        if (w > maxW) maxW = w;
      }
      if (maxW <= DISP_W - 2 * MARGIN_X) break;
    }
  }
  int lineH = (int)(size * 1.15);
  int totalH = n * lineH;
  int y0 = (DISP_H - totalH) / 2;
  if (y0 < 0) y0 = 0;

  clearFrame();
  for (int i = 0; i < n; i++) {
    if (lines[i].length() == 0) continue;
    OpenFontRender& r = *fonts[i];
    r.setFontSize(size);
    r.setAlignment(Align::TopCenter);
    r.setCursor(DISP_W / 2, y0 + i * lineH);
    r.drawString(lines[i].c_str(), DISP_W / 2, y0 + i * lineH, 0x0000, 0xFFFF, Layout::Horizontal);
  }
  return size;
}

// ---------------------------------------------------------------- display job queue
enum JobKind : uint8_t { JOB_TEXT, JOB_WHITE, JOB_BLACK, JOB_PATTERN, JOB_IMAGE, JOB_DIAG };
static String diagResult[2] = {"", ""};
// Uploaded 1-bit picture (bit=1 -> black) goes straight into frameBuf (upload is refused while a job runs)
static volatile size_t imageLen = 0;
struct Job {
  JobKind kind;
  uint8_t screen;   // 0 or 1
  int size;
  bool bold;
  char text[512];
};
static QueueHandle_t jobQueue;
static volatile bool displayBusy = false;
static volatile uint8_t busyScreen = 0;
static Preferences prefs;
static String lastText[2];
static int lastSize[2] = {0, 0};
static bool lastBold[2] = {false, false};

static void pushToPanel(uint8_t screen) {
  applyPinSet(screen);
  EPD_Init();
  EPD_WhiteScreen_ALL(frameBuf);
  EPD_DeepSleep();
}

// Panel health check: reset + power on + white refresh while sampling BUSY.
// A live panel pulls BUSY low during power-on and for several seconds during refresh;
// a broken cable leaves BUSY at 1 (internal pull-up) the whole time.
static void runDiag(uint8_t screen) {
  applyPinSet(screen);
  String r = "screen " + String(screen + 1) + ": ";
  int busyIdle = digitalRead(EPD_W21_BUSY);
  // reset pulse and watch BUSY for 300 ms
  digitalWrite(EPD_W21_RST, LOW); delay(10); digitalWrite(EPD_W21_RST, HIGH);
  int lowAfterReset = 0;
  for (int i = 0; i < 30; i++) { if (digitalRead(EPD_W21_BUSY) == 0) lowAfterReset++; delay(10); }
  EPD_Init();                                   // includes power on (0x04)
  // white frame to both chips, then refresh
  EPD_W21_WriteCMD1(0x10); for (unsigned i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA1(0xFF);
  EPD_W21_WriteCMD1(0x13); for (unsigned i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA1(0xFF);
  EPD_W21_WriteCMD2(0x10); for (unsigned i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA2(0xFF);
  EPD_W21_WriteCMD2(0x13); for (unsigned i = 0; i < EPD_ARRAY; i++) EPD_W21_WriteDATA2(0xFF);
  EPD_W21_WriteCMD(0x12);
  unsigned long t0 = millis(), firstLow = 0, lastLow = 0; int lowSamples = 0;
  while (millis() - t0 < 20000) {
    if (digitalRead(EPD_W21_BUSY) == 0) {
      if (!firstLow) firstLow = millis() - t0;
      lastLow = millis() - t0; lowSamples++;
    }
    delay(10);
  }
  EPD_DeepSleep();
  r += "busy_idle=" + String(busyIdle) + " low_after_reset=" + String(lowAfterReset * 10) + "ms ";
  if (firstLow || lowSamples) r += "refresh: BUSY low from " + String(firstLow) + " ms to " + String(lastLow) + " ms (" + String(lowSamples * 10) + " ms low) -> PANEL OK";
  else r += "refresh: BUSY never went low in 20 s -> panel NOT responding (cable/BUSY line/power)";
  diagResult[screen] = r;
  Serial.println("[diag] " + r);
}

static void displayTask(void*) {
  Job job;
  for (;;) {
    if (xQueueReceive(jobQueue, &job, portMAX_DELAY) != pdTRUE) continue;
    displayBusy = true; busyScreen = job.screen;
    unsigned long t0 = millis();
    switch (job.kind) {
      case JOB_TEXT: {
        int used = renderText(String(job.text), job.size, job.bold);
        Serial.printf("[job] screen %d text (size %d%s): %s\n", job.screen + 1, used, job.bold ? " bold" : "", job.text);
        pushToPanel(job.screen);
        break;
      }
      case JOB_WHITE:
        applyPinSet(job.screen); EPD_Init(); EPD_WhiteScreen_White(); EPD_DeepSleep(); break;
      case JOB_BLACK:
        applyPinSet(job.screen); EPD_Init(); EPD_WhiteScreen_Black(); EPD_DeepSleep(); break;
      case JOB_PATTERN:
        clearFrame(); drawTestPattern(); pushToPanel(job.screen); break;
      case JOB_DIAG:
        runDiag(job.screen); break;
      case JOB_IMAGE:
        Serial.printf("[job] screen %d image\n", job.screen + 1);
        pushToPanel(job.screen); break;
    }
    Serial.printf("[job] done in %lu ms\n", millis() - t0);
    displayBusy = false;
  }
}

static bool submitText(uint8_t screen, const String& text, int size, bool bold) {
  Job job = {};
  job.kind = JOB_TEXT; job.screen = screen; job.size = size; job.bold = bold;
  strlcpy(job.text, text.c_str(), sizeof(job.text));
  lastText[screen] = text; lastSize[screen] = size; lastBold[screen] = bold;
  prefs.putString(screen == 0 ? "t0" : "t1", text);
  prefs.putInt(screen == 0 ? "s0" : "s1", size);
  prefs.putBool(screen == 0 ? "b0" : "b1", bold);
  return xQueueSend(jobQueue, &job, 0) == pdTRUE;
}

static bool submitSimple(JobKind kind, uint8_t screen) {
  Job job = {}; job.kind = kind; job.screen = screen;
  return xQueueSend(jobQueue, &job, 0) == pdTRUE;
}

// ---------------------------------------------------------------- Ethernet (W5500)
struct EthPins { const char* name; int sck, miso, mosi, cs, irq, rst; };
static const EthPins ETH_CANDIDATES[] = {
  {"YB-ESP32-S3-ETH",       12, 13, 11, 14, 18, 21},
  {"Waveshare ESP32-S3-ETH", 13, 12, 11, 14, 10, 9},
};
static const EthPins* ethPins = nullptr;
static bool ethStarted = false;
static bool ethGotIp = false;
static bool ethStaticApplied = false;
static unsigned long ethLinkUpAt = 0;
// Fallback when the segment has no DHCP server (lab network)
static const IPAddress STATIC_IP(192, 168, 1, 222), STATIC_GW(192, 168, 1, 1), STATIC_MASK(255, 255, 255, 0);
static const unsigned long DHCP_TIMEOUT_MS = 15000;

// Read W5500 VERSIONR (0x0039) - must be 0x04
static uint8_t w5500ReadVersion(const EthPins& p) {
  pinMode(p.cs, OUTPUT); digitalWrite(p.cs, HIGH);
  if (p.rst >= 0) { pinMode(p.rst, OUTPUT); digitalWrite(p.rst, LOW); delay(5); digitalWrite(p.rst, HIGH); delay(200); }
  SPI.begin(p.sck, p.miso, p.mosi, -1);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(p.cs, LOW);
  SPI.transfer(0x00); SPI.transfer(0x39); SPI.transfer(0x00);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(p.cs, HIGH);
  SPI.endTransaction();
  SPI.end();
  return v;
}

static void onNetEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:        Serial.println("[eth] started"); ETH.setHostname("eink"); break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.printf("[eth] link up, %d Mbps %s, MAC %s\n", ETH.linkSpeed(), ETH.fullDuplex() ? "full" : "half", ETH.macAddress().c_str());
      ethLinkUpAt = millis(); break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethGotIp = true;
      Serial.printf("[eth] IP: %s  mask %s  gw %s  (http://eink.local)\n", ETH.localIP().toString().c_str(),
                    ETH.subnetMask().toString().c_str(), ETH.gatewayIP().toString().c_str());
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED: Serial.println("[eth] link down"); ethGotIp = false; ethLinkUpAt = 0; break;
    case ARDUINO_EVENT_ETH_STOP:         Serial.println("[eth] stopped"); break;
    default: break;
  }
}

static void setupEthernet() {
  for (const EthPins& p : ETH_CANDIDATES) {
    uint8_t v = w5500ReadVersion(p);
    Serial.printf("[eth] probe %s: VERSIONR=0x%02X\n", p.name, v);
    if (v == 0x04) { ethPins = &p; break; }
  }
  if (!ethPins) {
    Serial.println("[eth] W5500 not found on known pin sets, Ethernet disabled");
    return;
  }
  Network.onEvent(onNetEvent);
  SPI.begin(ethPins->sck, ethPins->miso, ethPins->mosi, -1);
  // INT pin is required for RX (on YB board close the INT solder bridge)
  ethStarted = ETH.begin(ETH_PHY_W5500, 1, ethPins->cs, ethPins->irq, ethPins->rst, SPI, 20);
  Serial.printf("[eth] begin on %s -> %s\n", ethPins->name, ethStarted ? "ok" : "FAILED");
}

// ---------------------------------------------------------------- addressable COB RGB strip on GPIO47
// FOB/COB addressable RGB, 5 V, WS2812-compatible (800 kHz, GRB). GPIO47 = DATA.
static const int LED_PIN = 47;
static const int LED_MAX = 300;
static Adafruit_NeoPixel strip(LED_MAX, LED_PIN, NEO_GRB + NEO_KHZ800);
static bool ledOn = false;
static uint32_t ledColor = 0xFFFFFF;       // RRGGBB
static uint8_t ledBright = 128;            // 0..255
static int ledCount = 120;                 // pixels actually used by effects
enum LedEffect : uint8_t { FX_SOLID = 0, FX_WAVE, FX_LOAD, FX_CONVERGE, FX_RAINBOW, FX_BREATHE, FX_COMET, FX_COUNT };
static const char* FX_NAMES[FX_COUNT] = { "solid", "wave", "load", "converge", "rainbow", "breathe", "comet" };
static LedEffect ledEffect = FX_SOLID;
static uint8_t ledSpeed = 5;               // 1..10

static inline uint32_t scaleColor(uint32_t c, uint8_t v) {  // v: 0..255
  uint8_t r = ((c >> 16) & 0xFF) * v / 255, g = ((c >> 8) & 0xFF) * v / 255, b = (c & 0xFF) * v / 255;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void ledRenderSolid() { strip.fill(ledColor, 0, ledCount); }

// One animation frame
static void ledRenderEffect(uint32_t now) {
  float sp = ledSpeed / 5.0f;                          // 1.0 at speed 5
  float t = now * 0.001f * sp;
  int n = ledCount;
  strip.clear();
  switch (ledEffect) {
    case FX_WAVE: {                                    // running sine wave of brightness
      for (int i = 0; i < n; i++) {
        float v = 0.5f + 0.5f * sinf(i * (2 * PI / 20.0f) - t * 6.0f);
        strip.setPixelColor(i, scaleColor(ledColor, (uint8_t)(20 + 235 * v)));
      }
      break;
    }
    case FX_LOAD: {                                    // progress bar fills, holds, fades out
      float cycle = 3.0f, ph = fmodf(t, cycle) / cycle;   // 0..1
      int lit = ph < 0.8f ? (int)(n * ph / 0.8f) : n;
      uint8_t v = ph < 0.8f ? 255 : (uint8_t)(255 * (1.0f - (ph - 0.8f) / 0.2f));
      for (int i = 0; i < lit; i++) strip.setPixelColor(i, scaleColor(ledColor, v));
      break;
    }
    case FX_CONVERGE: {                                // two segments run from both ends to the centre, flash, restart
      float cycle = 2.5f, ph = fmodf(t, cycle) / cycle;
      int half = n / 2, len = max(2, n / 12);
      if (ph < 0.75f) {
        int pos = (int)(half * ph / 0.75f);
        for (int k = 0; k < len; k++) {
          uint8_t v = (uint8_t)(255 - k * (200 / len));
          int ia = pos - k, ib = n - 1 - pos + k;
          if (ia >= 0) strip.setPixelColor(ia, scaleColor(ledColor, v));
          if (ib < n) strip.setPixelColor(ib, scaleColor(ledColor, v));
        }
      } else {                                         // flash expanding from centre and fading
        float f = (ph - 0.75f) / 0.25f;
        int rad = (int)(half * f);
        uint8_t v = (uint8_t)(255 * (1.0f - f));
        for (int i = half - rad; i <= half + rad; i++) if (i >= 0 && i < n) strip.setPixelColor(i, scaleColor(ledColor, v));
      }
      break;
    }
    case FX_RAINBOW: {
      uint16_t base = (uint16_t)(fmodf(t * 8000.0f, 65536.0f));
      for (int i = 0; i < n; i++) strip.setPixelColor(i, strip.ColorHSV(base + (uint16_t)(i * 65536L / n)));
      break;
    }
    case FX_BREATHE: {
      float v = 0.5f + 0.5f * sinf(t * 2.0f);
      strip.fill(scaleColor(ledColor, (uint8_t)(10 + 245 * v)), 0, n);
      break;
    }
    case FX_COMET: {                                   // knight rider with tail
      float cycle = 2.0f, ph = fmodf(t, cycle) / cycle;
      float pos = ph < 0.5f ? (n - 1) * ph * 2 : (n - 1) * (1 - (ph - 0.5f) * 2);
      int tail = max(3, n / 8);
      for (int k = 0; k < tail; k++) {
        int i = (int)pos + (ph < 0.5f ? -k : k);
        if (i >= 0 && i < n) strip.setPixelColor(i, scaleColor(ledColor, (uint8_t)(255 - k * (230 / tail))));
      }
      break;
    }
    default: ledRenderSolid(); break;
  }
}

static void applyLed() {
  strip.setBrightness(ledOn ? ledBright : 0);
  strip.clear();
  if (ledOn) { if (ledEffect == FX_SOLID) ledRenderSolid(); else ledRenderEffect(millis()); }
  strip.show();
}

static void ledTick() {
  static uint32_t last = 0;
  if (!ledOn || ledEffect == FX_SOLID) return;
  uint32_t now = millis();
  if (now - last < 25) return;
  last = now;
  ledRenderEffect(now);
  strip.show();
}

static void setLed(bool on) {
  ledOn = on;
  prefs.putBool("led", on);
  applyLed();
  Serial.printf("[led] %s color=%06lX bright=%u effect=%s speed=%u count=%d\n", on ? "ON" : "OFF",
                (unsigned long)ledColor, ledBright, FX_NAMES[ledEffect], ledSpeed, ledCount);
}

static void setLedColor(uint32_t rgb, int bright) {
  ledColor = rgb & 0xFFFFFF;
  if (bright >= 0) ledBright = (uint8_t)constrain(bright, 0, 255);
  prefs.putUInt("ledc", ledColor);
  prefs.putUChar("ledb", ledBright);
  applyLed();
}

static void setLedEffect(const String& name, int speed, int count) {
  for (int i = 0; i < FX_COUNT; i++) if (name == FX_NAMES[i]) ledEffect = (LedEffect)i;
  if (speed > 0) ledSpeed = (uint8_t)constrain(speed, 1, 10);
  if (count > 0) ledCount = constrain(count, 1, LED_MAX);
  prefs.putUChar("ledfx", ledEffect);
  prefs.putUChar("ledsp", ledSpeed);
  prefs.putInt("ledn", ledCount);
  applyLed();
}

// ---------------------------------------------------------------- web
static WebServer server(80);

static String jsonEscape(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': break;
      case '\t': o += "\\t"; break;
      default: if ((uint8_t)c < 0x20) o += ' '; else o += c;
    }
  }
  return o;
}

static String statusJson() {
  String j = "{";
  j += "\"busy\":" + String(displayBusy ? "true" : "false");
  j += ",\"busyScreen\":" + String(busyScreen + 1);
  j += ",\"queued\":" + String((int)uxQueueMessagesWaiting(jobQueue));
  j += ",\"ip\":\"" + (ethStarted ? ETH.localIP().toString() : String("")) + "\"";
  j += ",\"board\":\"" + String(ethPins ? ethPins->name : "no ethernet") + "\"";
  j += ",\"link\":" + String(ethStarted && ETH.linkUp() ? "true" : "false") + ",\"mac\":\"" + (ethStarted ? ETH.macAddress() : String("")) + "\"";
  j += ",\"heap\":" + String(ESP.getFreeHeap());
  j += ",\"diag\":[\"" + jsonEscape(diagResult[0]) + "\",\"" + jsonEscape(diagResult[1]) + "\"]";
  j += ",\"led\":" + String(ledOn ? "true" : "false");
  char cbuf[8]; snprintf(cbuf, sizeof(cbuf), "%06lX", (unsigned long)ledColor);
  j += ",\"ledColor\":\"" + String(cbuf) + "\",\"ledBright\":" + String(ledBright);
  j += ",\"ledEffect\":\"" + String(FX_NAMES[ledEffect]) + "\",\"ledSpeed\":" + String(ledSpeed) + ",\"ledCount\":" + String(ledCount);
  j += ",\"screens\":[";
  for (int i = 0; i < 2; i++) {
    if (i) j += ",";
    j += "{\"text\":\"" + jsonEscape(lastText[i]) + "\",\"size\":" + String(lastSize[i]) +
         ",\"bold\":" + String(lastBold[i] ? "true" : "false") + "}";
  }
  j += "]}";
  return j;
}

static void handleRoot() {
  server.send(200, "text/html; charset=utf-8", WEB_PAGE);
}

static void handleStatus() {
  server.send(200, "application/json; charset=utf-8", statusJson());
}

static void handleShow() {
  int screen = server.arg("screen").toInt();      // 1 or 2
  if (screen < 1 || screen > 2) { server.send(400, "text/plain", "screen must be 1 or 2"); return; }
  String text = server.arg("text");
  int size = server.arg("size").toInt();
  bool bold = server.arg("bold") == "1" || server.arg("bold") == "true" || server.arg("bold") == "on";
  if (text.length() > 500) text = text.substring(0, 500);
  bool ok = submitText(screen - 1, text, size, bold);
  server.send(ok ? 202 : 503, "application/json; charset=utf-8", statusJson());
}

static void handleClear() {
  int screen = server.arg("screen").toInt();
  if (screen < 1 || screen > 2) { server.send(400, "text/plain", "screen must be 1 or 2"); return; }
  bool ok = submitSimple(JOB_WHITE, screen - 1);
  server.send(ok ? 202 : 503, "application/json; charset=utf-8", statusJson());
}

// Last rendered frame as a 1-bit BMP (what was actually sent to the panel)
static void handleFrameBmp() {
  // wait until no job is rendering/pushing so the picture is consistent (max 30 s)
  unsigned long t0 = millis();
  while ((displayBusy || uxQueueMessagesWaiting(jobQueue)) && millis() - t0 < 30000) delay(50);
  const int stride = ((BYTES_PER_LINE + 3) / 4) * 4; // 172
  const uint32_t dataSize = (uint32_t)stride * DISP_H;
  const uint32_t fileSize = 62 + dataSize;
  uint8_t hdr[62] = {0};
  hdr[0] = 'B'; hdr[1] = 'M';
  hdr[2] = fileSize; hdr[3] = fileSize >> 8; hdr[4] = fileSize >> 16; hdr[5] = fileSize >> 24;
  hdr[10] = 62;                       // pixel data offset
  hdr[14] = 40;                       // BITMAPINFOHEADER
  hdr[18] = DISP_W & 0xFF; hdr[19] = DISP_W >> 8;
  int32_t h = -DISP_H;                // negative = top-down rows
  hdr[22] = h & 0xFF; hdr[23] = (h >> 8) & 0xFF; hdr[24] = (h >> 16) & 0xFF; hdr[25] = (h >> 24) & 0xFF;
  hdr[26] = 1;                        // planes
  hdr[28] = 1;                        // bits per pixel
  hdr[34] = dataSize; hdr[35] = dataSize >> 8; hdr[36] = dataSize >> 16; hdr[37] = dataSize >> 24;
  hdr[46] = 2;                        // colors used
  // palette: index 0 = white, index 1 = black
  hdr[54] = 0xFF; hdr[55] = 0xFF; hdr[56] = 0xFF; hdr[57] = 0;
  hdr[58] = 0;    hdr[59] = 0;    hdr[60] = 0;    hdr[61] = 0;
  server.setContentLength(fileSize);
  server.send(200, "image/bmp", "");
  server.sendContent((const char*)hdr, sizeof(hdr));
  static uint8_t row[172];
  for (int y = 0; y < DISP_H; y++) {
    memcpy(row, &frameBuf[y * BYTES_PER_LINE], BYTES_PER_LINE);
    row[170] = 0; row[171] = 0;
    server.sendContent((const char*)row, stride);
  }
}

// /led?state=on|off|1|0|toggle&color=RRGGBB&bright=0..255  (state omitted -> keep/ toggle if nothing else given)
static void handleLed() {
  bool changed = false;
  if (server.hasArg("color") || server.hasArg("bright")) {
    uint32_t rgb = ledColor;
    if (server.hasArg("color")) { String c = server.arg("color"); c.replace("#", ""); rgb = strtoul(c.c_str(), nullptr, 16); }
    int b = server.hasArg("bright") ? server.arg("bright").toInt() : -1;
    setLedColor(rgb, b);
    changed = true;
  }
  if (server.hasArg("effect") || server.hasArg("speed") || server.hasArg("count")) {
    setLedEffect(server.arg("effect"), server.hasArg("speed") ? server.arg("speed").toInt() : 0,
                 server.hasArg("count") ? server.arg("count").toInt() : 0);
    changed = true;
  }
  String st = server.arg("state");
  st.toLowerCase();
  if (st == "on" || st == "1" || st == "true") setLed(true);
  else if (st == "off" || st == "0" || st == "false") setLed(false);
  else if (!changed) setLed(!ledOn);
  server.send(200, "application/json; charset=utf-8", statusJson());
}

static void handleImageUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    imageLen = 0;
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (displayBusy || uxQueueMessagesWaiting(jobQueue)) return;   // frameBuf in use -> drop, reported below
    size_t room = sizeof(frameBuf) - imageLen;
    size_t n = up.currentSize < room ? up.currentSize : room;
    memcpy(frameBuf + imageLen, up.buf, n);
    imageLen += n;
  } else if (up.status == UPLOAD_FILE_END) {
    Serial.printf("[image] received %u bytes\n", (unsigned)imageLen);
  }
}

static void handleImageDone() {
  int screen = server.arg("screen").toInt();
  if (screen < 1 || screen > 2) { server.send(400, "text/plain", "screen must be 1 or 2"); return; }
  if (imageLen != sizeof(frameBuf)) {
    server.send(imageLen == 0 ? 503 : 400, "text/plain",
                "expected " + String(sizeof(frameBuf)) + " bytes (1360x480, 1 bit, bit=1 black), got " + String((unsigned)imageLen) +
                (imageLen == 0 ? " (display busy, retry later)" : ""));
    return;
  }
  lastText[screen - 1] = "[картинка]";
  bool ok = submitSimple(JOB_IMAGE, screen - 1);
  server.send(ok ? 202 : 503, "application/json; charset=utf-8", statusJson());
}

// /diag?screen=1|2 -> queue a BUSY-line panel check; result appears in /status "diag"
static void handleDiag() {
  int screen = server.arg("screen").toInt();
  if (screen < 1 || screen > 2) { server.send(400, "text/plain", "screen must be 1 or 2"); return; }
  diagResult[screen - 1] = "running...";
  bool ok = submitSimple(JOB_DIAG, screen - 1);
  server.send(ok ? 202 : 503, "application/json; charset=utf-8", statusJson());
}

static void setupWeb() {
  server.on("/frame.bmp", HTTP_GET, handleFrameBmp);
  server.on("/diag", HTTP_GET, handleDiag);
  server.on("/diag", HTTP_POST, handleDiag);
  server.on("/image", HTTP_POST, handleImageDone, handleImageUpload);
  server.on("/led", HTTP_GET, handleLed);
  server.on("/led", HTTP_POST, handleLed);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/show", HTTP_POST, handleShow);
  server.on("/show", HTTP_GET, handleShow);   // convenience: /show?screen=1&text=...
  server.on("/clear", HTTP_POST, handleClear);
  server.on("/clear", HTTP_GET, handleClear);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  if (MDNS.begin("eink")) MDNS.addService("http", "tcp", 80);
}

// ---------------------------------------------------------------- serial commands
static void printHelp() {
  Serial.println("Commands (serial):");
  Serial.println(" 1 - white (current screen)   2 - black   3 - test pattern");
  Serial.println(" t<text>  - render text with TTF on current screen (UTF-8, \\n = new line)");
  Serial.println(" p - toggle current screen (1/2)    d - panel diag (BUSY)    l - strip on/off    i - info    h - help");
}

static void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  uint8_t scr = (uint8_t)curPinSet;
  if (c == '1') submitSimple(JOB_WHITE, scr);
  else if (c == '2') submitSimple(JOB_BLACK, scr);
  else if (c == '3') submitSimple(JOB_PATTERN, scr);
  else if (c == 't') {
    String text = Serial.readStringUntil('\n');
    text.trim();
    text.replace("\\n", "\n");
    if (text.length() == 0) text = "TEST";
    submitText(scr, text, 0, false);
  } else if (c == 'p') {
    curPinSet = curPinSet == 0 ? 1 : 0;
    Serial.printf("[cmd] current screen -> %d\n", curPinSet + 1);
  } else if (c == 'd') {
    submitSimple(JOB_DIAG, scr);
  } else if (c == 'l') {
    setLed(!ledOn);
  } else if (c == 'i') {
    Serial.println(statusJson());
  } else if (c == 'h' || c == '?') printHelp();
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nE-ink caption board");

  initAllEpdPins();
  curPinSet = (EPD_PIN_CS == 37) ? 0 : 1;
  EPD_SetMono(true);

  prefs.begin("eink", false);
  for (int i = 0; i < 2; i++) {
    lastText[i] = prefs.getString(i == 0 ? "t0" : "t1", "");
    lastSize[i] = prefs.getInt(i == 0 ? "s0" : "s1", 0);
    lastBold[i] = prefs.getBool(i == 0 ? "b0" : "b1", false);
  }

  strip.begin();
  ledColor = prefs.getUInt("ledc", 0xFFFFFF);
  ledBright = prefs.getUChar("ledb", 128);
  ledEffect = (LedEffect)min((int)prefs.getUChar("ledfx", FX_SOLID), (int)FX_COUNT - 1);
  ledSpeed = prefs.getUChar("ledsp", 5);
  ledCount = constrain(prefs.getInt("ledn", 120), 1, LED_MAX);
  setLed(true);   // always ON after boot (bench check); web button can still switch it

  setupFonts();

  jobQueue = xQueueCreate(4, sizeof(Job));
  xTaskCreatePinnedToCore(displayTask, "epd", 12288, nullptr, 3, nullptr, 0);   // above loop/web so HTTP traffic does not delay rendering

  setupEthernet();
  setupWeb();
  printHelp();
}

static void ethFallbackTick() {
  if (!ethStarted || ethGotIp || ethStaticApplied || ethLinkUpAt == 0) return;
  if (millis() - ethLinkUpAt < DHCP_TIMEOUT_MS) return;
  ethStaticApplied = true;
  Serial.printf("[eth] no DHCP lease in %lu s -> static %s\n", DHCP_TIMEOUT_MS / 1000, STATIC_IP.toString().c_str());
  ETH.config(STATIC_IP, STATIC_GW, STATIC_MASK, STATIC_GW);
}

void loop() {
  server.handleClient();
  handleSerial();
  ethFallbackTick();
  ledTick();
  delay(2);
}
