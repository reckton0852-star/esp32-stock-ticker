#include "SetupPortal.h"
#include "AppConfig.h"
#include "FirmwareVersion.h"
#include "OtaUpdater.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

static const byte DNS_PORT = 53;
static const char * AP_SSID = "Reckton-Stock-Setup";
static const char * AP_PASSWORD = "";

static DNSServer dns_server;
static WebServer web_server(80);
static bool portal_active = false;
static bool reboot_requested = false;
static bool ota_requested = false;
static bool routes_registered = false;
static String ap_ip = "192.168.4.1";

static String html_escape(const String& value)
{
  String s = value;
  s.replace("&", "&amp;");
  s.replace("\"", "&quot;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  return s;
}

static String form_page(const String& notice = "")
{
  const AppConfigData * config = AppConfig_Get();
  String html;
  html.reserve(6144);
  html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Stock Ticker Setup</title><style>";
  html += "body{font-family:Arial,sans-serif;background:#0b1320;color:#e6edf7;margin:0;padding:20px;}";
  html += ".wrap{max-width:680px;margin:0 auto;background:#111c2b;border:1px solid #243346;border-radius:12px;padding:20px;}";
  html += "h1{margin-top:0;font-size:24px;}h2{margin:26px 0 8px;font-size:18px;}label{display:block;margin:14px 0 6px;font-size:14px;color:#9fb0c7;}";
  html += "input,select{width:100%;box-sizing:border-box;padding:12px;border-radius:8px;border:1px solid #30445d;background:#0a111b;color:#f3f7fb;}";
  html += ".row{display:grid;grid-template-columns:1fr 1fr;gap:12px;}.notice{margin:0 0 16px;padding:12px;border-radius:8px;background:#17324d;color:#d9ecff;}";
  html += "button{margin-top:14px;width:100%;padding:13px;border:0;border-radius:8px;background:#2563eb;color:white;font-size:15px;font-weight:700;}";
  html += "button.secondary{background:#20334a;border:1px solid #36506d;}button:disabled{opacity:.5}.ota{margin-top:24px;padding-top:4px;border-top:1px solid #243346;}";
  html += ".meta{color:#9fb0c7;line-height:1.55;font-size:14px}.ok{color:#83d7a6}.warn{color:#ffd27d}small{display:block;margin-top:16px;color:#7f93ad;line-height:1.5;}</style></head><body><div class='wrap'>";
  html += "<h1>Stock Ticker Setup</h1>";
  if(notice.length() > 0) {
    html += "<div class='notice'>" + notice + "</div>";
  }
  html += "<form method='post' action='/save'>";
  for(size_t i = 0; i < APP_WIFI_SLOTS; i++) {
    html += "<div class='row'>";
    html += "<div><label>WiFi SSID " + String(i + 1) + "</label><input name='ssid" + String(i) + "' value='" + html_escape(config->wifi[i].ssid) + "'></div>";
    html += "<div><label>Password " + String(i + 1) + "</label><input name='pass" + String(i) + "' value='" + html_escape(config->wifi[i].password) + "'></div>";
    html += "</div>";
  }
  html += "<label>Proxy Base URL</label><input name='proxy' value='" + html_escape(config->proxy_base_url) + "'>";
  html += "<label>Display Mode</label><select name='mode'>";
  html += "<option value='0'";
  if(config->display_mode == APP_DISPLAY_MODE_STOCKS) html += " selected";
  html += ">Stocks</option>";
  html += "<option value='1'";
  if(config->display_mode == APP_DISPLAY_MODE_FX) html += " selected";
  html += ">FX to CNY</option>";
  html += "</select>";
  html += "<label>Stock Symbols (comma separated)</label><input name='symbols' value='" + html_escape(config->stock_symbols) + "'>";
  html += "<label>FX Symbols (base currencies, comma separated)</label><input name='fxsymbols' value='" + html_escape(config->fx_symbols) + "'>";
  html += "<div class='row'>";
  html += "<div><label>Refresh Seconds</label><input name='refresh' value='" + String(config->refresh_seconds) + "'></div>";
  html += "<div><label>Rotate Seconds</label><input name='rotate' value='" + String(config->rotate_seconds) + "'></div>";
  html += "</div>";
  html += "<div class='row'>";
  html += "<div><label>Brightness (10-100)</label><input name='bright' value='" + String(config->brightness) + "'></div>";
  html += "<div></div></div>";
  html += "<button type='submit'>Save and Reboot</button></form>";
  const OtaManifest * manifest = OtaUpdater_GetManifest();
  html += "<div class='ota'><h2>Firmware Update</h2>";
  html += "<div class='meta'>Current version: <b>v" + String(APP_FIRMWARE_VERSION) + "</b><br>";
  html += String("Internet: <b class='") + (WiFi.status() == WL_CONNECTED ? "ok'>connected" : "warn'>not connected") + "</b>";
  if(manifest->checked && manifest->ready) {
    html += "<br>Latest version: <b>" + html_escape(manifest->version) + "</b>";
    if(strlen(manifest->notes) > 0) {
      html += "<br>Notes: " + html_escape(manifest->notes);
    }
    html += manifest->update_available ? "<br><span class='ok'>A new version is ready.</span>" : "<br><span class='ok'>This device is up to date.</span>";
  } else if(manifest->checked) {
    html += "<br><span class='warn'>" + html_escape(manifest->error) + "</span>";
  }
  html += "</div><form method='post' action='/ota/check'><button class='secondary' type='submit'";
  if(WiFi.status() != WL_CONNECTED) html += " disabled";
  html += ">Check for Update</button></form>";
  if(manifest->checked && manifest->ready && manifest->update_available) {
    html += "<form method='post' action='/ota/install'><button type='submit'>Install v" + html_escape(manifest->version) + "</button></form>";
  }
  html += "<small>Keep power connected during installation. The device restarts automatically when the update is complete.</small></div>";
  html += "<small>Connect your phone to the ESP32 hotspot, open <b>http://192.168.4.1</b>, save, and wait for automatic reboot.</small>";
  html += "</div></body></html>";
  return html;
}

static uint16_t parse_u16_arg(const String& value, uint16_t fallback, uint16_t min_value, uint16_t max_value)
{
  long parsed = value.toInt();
  if(parsed < (long)min_value || parsed > (long)max_value) {
    return fallback;
  }
  return (uint16_t)parsed;
}

static uint8_t parse_u8_arg(const String& value, uint8_t fallback, uint8_t min_value, uint8_t max_value)
{
  long parsed = value.toInt();
  if(parsed < (long)min_value || parsed > (long)max_value) {
    return fallback;
  }
  return (uint8_t)parsed;
}

static void handle_root()
{
  web_server.send(200, "text/html; charset=utf-8", form_page());
}

static void handle_save()
{
  AppConfigData updated = *AppConfig_Get();

  for(size_t i = 0; i < APP_WIFI_SLOTS; i++) {
    String ssid_key = String("ssid") + String(i);
    String pass_key = String("pass") + String(i);
    snprintf(updated.wifi[i].ssid, sizeof(updated.wifi[i].ssid), "%s", web_server.arg(ssid_key).c_str());
    snprintf(updated.wifi[i].password, sizeof(updated.wifi[i].password), "%s", web_server.arg(pass_key).c_str());
  }

  snprintf(updated.proxy_base_url, sizeof(updated.proxy_base_url), "%s", web_server.arg("proxy").c_str());
  snprintf(updated.stock_symbols, sizeof(updated.stock_symbols), "%s", web_server.arg("symbols").c_str());
  snprintf(updated.fx_symbols, sizeof(updated.fx_symbols), "%s", web_server.arg("fxsymbols").c_str());
  updated.display_mode = parse_u8_arg(web_server.arg("mode"), updated.display_mode, APP_DISPLAY_MODE_STOCKS, APP_DISPLAY_MODE_FX);
  updated.refresh_seconds = parse_u16_arg(web_server.arg("refresh"), updated.refresh_seconds, 15, 600);
  updated.rotate_seconds = parse_u16_arg(web_server.arg("rotate"), updated.rotate_seconds, 5, 300);
  updated.brightness = parse_u8_arg(web_server.arg("bright"), updated.brightness, 10, 100);

  if(strlen(updated.wifi[0].ssid) == 0 || strlen(updated.proxy_base_url) == 0 ||
     strlen(updated.stock_symbols) == 0 || strlen(updated.fx_symbols) == 0) {
    web_server.send(200, "text/html; charset=utf-8", form_page("WiFi 1, proxy URL, stock symbols, and FX symbols cannot be empty."));
    return;
  }

  AppConfig_Save(&updated);
  reboot_requested = true;
  web_server.send(200, "text/html; charset=utf-8",
    "<!doctype html><html><head><meta charset='utf-8'><meta http-equiv='refresh' content='4;url=/'><meta name='viewport' content='width=device-width,initial-scale=1'></head>"
    "<body style='font-family:Arial;background:#0b1320;color:#e6edf7;padding:24px;'><h2>Saved</h2><p>The ESP32 will reboot in a few seconds.</p></body></html>");
}

static void handle_ota_check()
{
  bool ok = OtaUpdater_Check();
  String notice;
  if(ok) {
    const OtaManifest * manifest = OtaUpdater_GetManifest();
    notice = manifest->update_available ? "A new firmware version is available." : "The firmware is already up to date.";
  } else {
    notice = String("Update check failed: ") + OtaUpdater_LastError();
  }
  web_server.send(200, "text/html; charset=utf-8", form_page(html_escape(notice)));
}

static void handle_ota_install()
{
  const OtaManifest * manifest = OtaUpdater_GetManifest();
  if(!manifest->ready || !manifest->update_available || WiFi.status() != WL_CONNECTED) {
    web_server.send(409, "text/html; charset=utf-8", form_page("No online firmware update is ready to install."));
    return;
  }

  ota_requested = true;
  web_server.send(200, "text/html; charset=utf-8",
    "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'></head>"
    "<body style='font-family:Arial;background:#0b1320;color:#e6edf7;padding:24px;'><h2>Installing Update</h2>"
    "<p>Keep the device powered. Follow the progress on the ESP32 screen.</p></body></html>");
}

static void handle_not_found()
{
  web_server.sendHeader("Location", String("http://") + ap_ip + "/", true);
  web_server.send(302, "text/plain", "");
}

void SetupPortal_Begin(void)
{
  bool keep_station = WiFi.status() == WL_CONNECTED;
  if(!keep_station) {
    WiFi.disconnect(false, false);
    delay(100);
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  ap_ip = WiFi.softAPIP().toString();

  dns_server.start(DNS_PORT, "*", WiFi.softAPIP());
  if(!routes_registered) {
    web_server.on("/", HTTP_GET, handle_root);
    web_server.on("/save", HTTP_POST, handle_save);
    web_server.on("/ota/check", HTTP_POST, handle_ota_check);
    web_server.on("/ota/install", HTTP_POST, handle_ota_install);
    web_server.on("/generate_204", HTTP_GET, handle_root);
    web_server.on("/hotspot-detect.html", HTTP_GET, handle_root);
    web_server.on("/connecttest.txt", HTTP_GET, handle_root);
    web_server.on("/fwlink", HTTP_GET, handle_root);
    web_server.onNotFound(handle_not_found);
    routes_registered = true;
  }
  web_server.begin();

  portal_active = true;
  reboot_requested = false;
  ota_requested = false;
  printf("Setup portal started: SSID=%s IP=%s internet=%s\r\n", AP_SSID, ap_ip.c_str(), keep_station ? "yes" : "no");
}

void SetupPortal_Handle(void)
{
  if(!portal_active) {
    return;
  }
  dns_server.processNextRequest();
  web_server.handleClient();
}

bool SetupPortal_IsActive(void)
{
  return portal_active;
}

bool SetupPortal_ShouldReboot(void)
{
  return reboot_requested;
}

bool SetupPortal_ShouldStartOta(void)
{
  return ota_requested;
}

void SetupPortal_ClearOtaRequest(void)
{
  ota_requested = false;
}

const char * SetupPortal_ApSsid(void)
{
  return AP_SSID;
}

const char * SetupPortal_ApIp(void)
{
  return ap_ip.c_str();
}
