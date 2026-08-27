/**
 * Minimal USB recovery image for ESP32-C3 (lolin_c3_mini).
 *
 * Contains NONE of the crash sources from 1.4.6–1.4.8:
 *   - no OpenTherm (no ISR, no bus)
 *   - no 1-Wire (no GPIO1 timing)
 *   - no WS2812 / RMT (no neopixelWrite)
 *   - no WiFiManager (no blocking portal)
 *
 * What it does:
 *   1. Clears the stuck unclean_boots NVS counter
 *   2. Connects to saved WiFi (SettingsStore keys in namespace "hcs")
 *      — falls back to an open AP "HCS-Recovery-XXXX" if it can't
 *   3. Serves a tiny page at http://<ip>/ with a .bin upload form
 *      to flash the full firmware back over LAN
 *   4. Prints an alive/heap line every 5 s over serial
 *
 * Flash:
 *   pio run -e lolin_c3_recovery -t upload        (from firmware/)
 *   pio device monitor -b 115200
 */
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

static WebServer server(80);
static String node_id;
static bool sta_ok = false;

static void handleRoot() {
  String html = F(
      "<!DOCTYPE html><html><head><meta charset=utf-8>"
      "<meta name=viewport content='width=device-width,initial-scale=1'>"
      "</head><body style='font-family:sans-serif;max-width:32rem;"
      "margin:2rem auto;background:#111;color:#eee;padding:1rem'>"
      "<h1>HCS Recovery</h1>"
      "<p>Recovery mode: OpenTherm / 1-Wire / status LED are all OFF.</p>"
      "<p>Node: <code>");
  html += node_id;
  html += F("</code><br>IP: <code>");
  html += (sta_ok ? WiFi.localIP() : WiFi.softAPIP()).toString();
  html += F(
      "</code></p>"
      "<p>Upload the full <code>firmware-lolin_c3_mini.bin</code> "
      "(from a HCS release) to restore normal firmware:</p>"
      "<form method=POST action=/update enctype=multipart/form-data>"
      "<input type=file name=firmware accept='.bin,.bin'>"
      "<button type=submit>Flash full firmware</button></form>"
      "<p style=color:#888>1.4.9-recovery &middot; settings in NVS are "
      "preserved</p></body></html>");
  server.send(200, "text/html", html);
}

static void handleUpdateDone() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain",
              (Update.hasError()) ? "FAIL — retry or use USB" : "OK — rebooting");
  delay(800);
  ESP.restart();
}

static void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("[rec] OTA start: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize)
      Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true))
      Serial.printf("[rec] OTA success: %u bytes\n", up.totalSize);
    else
      Update.printError(Serial);
  }
}

// Read STA creds exactly like SettingsStore does (namespace "hcs").
static bool trySavedWifi() {
  Preferences p;
  if (!p.begin("hcs", true)) return false;
  String ssid = p.getString("wifi_ssid", "");
  String pass = p.getString("wifi_pass", "");
  bool cfg = p.getBool("cfg", false);
  p.end();
  if (!cfg || !ssid.length()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(node_id.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[rec] connecting to '%s'", ssid.c_str());
  for (int i = 0; i < 40; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n[rec] WiFi up: %s (%d dBm)\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      return true;
    }
    delay(250);
    Serial.print('.');
  }
  Serial.println(F("\n[rec] saved WiFi did not connect"));
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // let USB-CDC enumerate before we print
  Serial.println();
  Serial.println(F("=== HCS RECOVERY 1.4.9 ==="));
  Serial.println(F("OT/1-Wire/WS2812 all disabled — this image cannot "
                   "hit the previous crash paths"));

  // 1) Clear the stuck crash counter so a later full image boots normally
  //    instead of staying in OT safe-mode forever.
  {
    Preferences pp;
    if (pp.begin("hcs", false)) {
      uint8_t before = pp.getUChar("unclean_boots", 0);
      pp.putUChar("unclean_boots", 0);
      pp.end();
      Serial.printf("[rec] unclean_boots: %u -> 0\n", before);
    } else {
      Serial.println(F("[rec] could not open NVS 'hcs'"));
    }
  }

  // 2) Identity
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char id[24];
  snprintf(id, sizeof(id), "hcs-%02x%02x%02x%02x%02x%02x", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  node_id = id;
  Serial.printf("[rec] node: %s\n", id);

  // 3) Network: saved STA, else fallback AP (never blocks >10 s)
  sta_ok = trySavedWifi();
  if (!sta_ok) {
    WiFi.mode(WIFI_AP);
    String ap = String("HCS-Recovery-") + String(mac[4], HEX) +
                String(mac[5], HEX);
    WiFi.softAP(ap.c_str(), "homeclimate");
    Serial.printf("[rec] AP '%s' pass 'homeclimate' ip %s\n", ap.c_str(),
                  WiFi.softAPIP().toString().c_str());
  }

  // 4) Tiny web server for LAN re-flash
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, []() {
    String j = "{\"version\":\"1.4.9-recovery\",\"recovery\":true,"
               "\"node_id\":\"";
    j += node_id;
    j += "\",\"ip\":\"";
    j += (sta_ok ? WiFi.localIP() : WiFi.softAPIP()).toString();
    j += "\",\"uptime\":";
    j += String(millis() / 1000UL);
    j += "}";
    server.send(200, "application/json", j);
  });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.begin();
  Serial.println(F("[rec] HTTP :80 up — open the IP above to re-flash"));
  Serial.println(F("[rec] staying alive; this image never reboots itself"));
}

void loop() {
  server.handleClient();
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    Serial.printf("[rec] alive up=%lus heap=%u\n", millis() / 1000UL,
                  (unsigned)ESP.getFreeHeap());
  }
  delay(2);
}
