// E-ink caption board: 2x GDEM1085T51 (1360x480) via DESPI-C1085 on ESP32-S3-ETH.
// TTF text rendering (OpenFontRender, PT Sans Latin+Cyrillic) + web UI over Ethernet (W5500).
#include <Arduino.h>
#include <SPI.h>
#include <ETH.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "OpenFontRender.h"
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

static OpenFontRender ofrRegular;
static OpenFontRender ofrBold;

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
  for (OpenFontRender* r : { &ofrRegular, &ofrBold }) {
    r->setDrawPixel(ofrDrawPixel);
    r->setDrawFastHLine(ofrDrawHLine);
    // draw2screen() uses these, not the fg/bg arguments of drawString(): black on white
    r->setFontColor((uint16_t)0x0000, (uint16_t)0xFFFF);
    r->setBackgroundFillMethod(BgFillMethod::None);
    r->setLineSpaceRatio(1.15);
  }
  FT_Error e1 = ofrRegular.loadFont(ptsans_start, ptsans_end - ptsans_start);
  FT_Error e2 = ofrBold.loadFont(ptsansb_start, ptsansb_end - ptsansb_start);
  Serial.printf("[font] regular=%d (%u bytes), bold=%d (%u bytes)\n",
                (int)e1, (unsigned)(ptsans_end - ptsans_start), (int)e2, (unsigned)(ptsansb_end - ptsansb_start));
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
  OpenFontRender& r = bold ? ofrBold : ofrRegular;
  const int MARGIN_X = 30, MARGIN_Y = 16;
  String lines[8];
  int n = splitLines(text, lines, 8);
  if (n == 0) n = 1;

  if (size <= 0) {
    for (size = 440; size > 16; size -= 8) {
      r.setFontSize(size);
      int lineH = (int)(size * 1.15);
      if (n * lineH > DISP_H - 2 * MARGIN_Y) continue;
      int maxW = 0;
      for (int i = 0; i < n; i++) {
        if (lines[i].length() == 0) continue;
        int w = (int)r.getTextWidth("%s", lines[i].c_str());
        if (w > maxW) maxW = w;
      }
      if (maxW <= DISP_W - 2 * MARGIN_X) break;
    }
  }
  r.setFontSize(size);
  int lineH = (int)(size * 1.15);
  int totalH = n * lineH;
  int y0 = (DISP_H - totalH) / 2;
  if (y0 < 0) y0 = 0;

  clearFrame();
  r.setAlignment(Align::TopCenter);
  for (int i = 0; i < n; i++) {
    if (lines[i].length() == 0) continue;
    r.setCursor(DISP_W / 2, y0 + i * lineH);
    r.drawString(lines[i].c_str(), DISP_W / 2, y0 + i * lineH, 0x0000, 0xFFFF, Layout::Horizontal);
  }
  return size;
}

// ---------------------------------------------------------------- display job queue
enum JobKind : uint8_t { JOB_TEXT, JOB_WHITE, JOB_BLACK, JOB_PATTERN };
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

static void setupWeb() {
  server.on("/frame.bmp", HTTP_GET, handleFrameBmp);
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
  Serial.println(" p - toggle current screen (1/2)    i - network info    h - help");
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

  setupFonts();

  jobQueue = xQueueCreate(4, sizeof(Job));
  xTaskCreatePinnedToCore(displayTask, "epd", 12288, nullptr, 1, nullptr, 1);

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
  delay(2);
}
