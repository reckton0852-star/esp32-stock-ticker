#include "OtaUpdater.h"

#include "FirmwareVersion.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static OtaManifest ota_manifest;
static String ota_last_error;

static void copy_text(char * destination, size_t destination_size, const String& value)
{
  if(destination_size == 0) {
    return;
  }
  snprintf(destination, destination_size, "%s", value.c_str());
}

static String json_string(const String& json, const char * key)
{
  String marker = String("\"") + key + "\"";
  int key_pos = json.indexOf(marker);
  if(key_pos < 0) {
    return "";
  }
  int colon = json.indexOf(':', key_pos + marker.length());
  int first_quote = json.indexOf('"', colon + 1);
  if(colon < 0 || first_quote < 0) {
    return "";
  }

  String result;
  bool escaped = false;
  for(int i = first_quote + 1; i < (int)json.length(); i++) {
    char c = json[i];
    if(escaped) {
      if(c == 'n' || c == 'r') {
        result += ' ';
      } else {
        result += c;
      }
      escaped = false;
    } else if(c == '\\') {
      escaped = true;
    } else if(c == '"') {
      break;
    } else {
      result += c;
    }
  }
  return result;
}

static bool json_bool(const String& json, const char * key, bool fallback)
{
  String marker = String("\"") + key + "\"";
  int key_pos = json.indexOf(marker);
  int colon = key_pos >= 0 ? json.indexOf(':', key_pos + marker.length()) : -1;
  if(colon < 0) {
    return fallback;
  }
  int value_pos = colon + 1;
  while(value_pos < (int)json.length() && isspace((unsigned char)json[value_pos])) {
    value_pos++;
  }
  if(json.startsWith("true", value_pos)) {
    return true;
  }
  if(json.startsWith("false", value_pos)) {
    return false;
  }
  return fallback;
}

static uint32_t json_u32(const String& json, const char * key)
{
  String marker = String("\"") + key + "\"";
  int key_pos = json.indexOf(marker);
  int colon = key_pos >= 0 ? json.indexOf(':', key_pos + marker.length()) : -1;
  if(colon < 0) {
    return 0;
  }
  return (uint32_t)strtoul(json.c_str() + colon + 1, NULL, 10);
}

static String normalized_version(const char * version)
{
  String value = version ? version : "";
  value.trim();
  if(value.startsWith("v") || value.startsWith("V")) {
    value.remove(0, 1);
  }
  return value;
}

static int compare_versions(const char * left, const char * right)
{
  String a = normalized_version(left);
  String b = normalized_version(right);
  int a_pos = 0;
  int b_pos = 0;

  for(uint8_t part = 0; part < 4; part++) {
    int a_dot = a.indexOf('.', a_pos);
    int b_dot = b.indexOf('.', b_pos);
    String a_part = a_dot < 0 ? a.substring(a_pos) : a.substring(a_pos, a_dot);
    String b_part = b_dot < 0 ? b.substring(b_pos) : b.substring(b_pos, b_dot);
    long a_value = a_part.toInt();
    long b_value = b_part.toInt();
    if(a_value != b_value) {
      return a_value > b_value ? 1 : -1;
    }
    if(a_dot < 0 && b_dot < 0) {
      return 0;
    }
    a_pos = a_dot < 0 ? a.length() : a_dot + 1;
    b_pos = b_dot < 0 ? b.length() : b_dot + 1;
  }
  return 0;
}

static bool valid_md5(const String& value)
{
  if(value.length() != 32) {
    return false;
  }
  for(size_t i = 0; i < value.length(); i++) {
    if(!isxdigit((unsigned char)value[i])) {
      return false;
    }
  }
  return true;
}

static String ota_url(const char * path)
{
  String base = APP_OTA_BASE_URL;
  while(base.endsWith("/")) {
    base.remove(base.length() - 1);
  }
  return base + path;
}

static bool get_text(const String& url, String * body, String * error)
{
  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(8000);
  http.setReuse(false);

  int code = -1;
  if(url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    if(!http.begin(client, url)) {
      *error = "Unable to start HTTPS request";
      return false;
    }
    code = http.GET();
    if(code == HTTP_CODE_OK) {
      *body = http.getString();
    }
  } else {
    WiFiClient client;
    if(!http.begin(client, url)) {
      *error = "Unable to start HTTP request";
      return false;
    }
    code = http.GET();
    if(code == HTTP_CODE_OK) {
      *body = http.getString();
    }
  }

  if(code != HTTP_CODE_OK) {
    if(code > 0) {
      *error = String("Manifest HTTP ") + code;
    } else {
      *error = http.errorToString(code);
    }
    http.end();
    return false;
  }
  http.end();
  return true;
}

void OtaUpdater_Init(void)
{
  memset(&ota_manifest, 0, sizeof(ota_manifest));
  ota_last_error = "";
}

bool OtaUpdater_Check(void)
{
  memset(&ota_manifest, 0, sizeof(ota_manifest));
  ota_manifest.checked = true;
  ota_last_error = "";

  if(WiFi.status() != WL_CONNECTED) {
    ota_last_error = "Device is not connected to the internet";
    copy_text(ota_manifest.error, sizeof(ota_manifest.error), ota_last_error);
    return false;
  }

  String body;
  String error;
  String url = ota_url("/firmware/manifest");
  printf("OTA manifest request: %s\r\n", url.c_str());
  if(!get_text(url, &body, &error)) {
    ota_last_error = error;
    copy_text(ota_manifest.error, sizeof(ota_manifest.error), error);
    return false;
  }

  ota_manifest.ready = json_bool(body, "ready", false);
  copy_text(ota_manifest.version, sizeof(ota_manifest.version), json_string(body, "version"));
  copy_text(ota_manifest.notes, sizeof(ota_manifest.notes), json_string(body, "notes"));
  copy_text(ota_manifest.md5, sizeof(ota_manifest.md5), json_string(body, "md5"));
  ota_manifest.size = json_u32(body, "size");

  if(!ota_manifest.ready) {
    String remote_error = json_string(body, "error");
    ota_last_error = remote_error.length() ? remote_error : "No OTA package has been published";
    copy_text(ota_manifest.error, sizeof(ota_manifest.error), ota_last_error);
    return false;
  }
  if(strlen(ota_manifest.version) == 0 || !valid_md5(ota_manifest.md5) || ota_manifest.size == 0) {
    ota_last_error = "Firmware manifest is incomplete";
    copy_text(ota_manifest.error, sizeof(ota_manifest.error), ota_last_error);
    ota_manifest.ready = false;
    return false;
  }

  ota_manifest.update_available = compare_versions(ota_manifest.version, APP_FIRMWARE_VERSION) > 0;
  printf("OTA manifest ok: current=%s latest=%s available=%s size=%lu\r\n",
         APP_FIRMWARE_VERSION,
         ota_manifest.version,
         ota_manifest.update_available ? "yes" : "no",
         (unsigned long)ota_manifest.size);
  return true;
}

bool OtaUpdater_Install(OtaProgressCallback callback)
{
  if(!ota_manifest.ready || !ota_manifest.update_available) {
    ota_last_error = "No newer firmware is ready to install";
    return false;
  }
  if(WiFi.status() != WL_CONNECTED) {
    ota_last_error = "WiFi disconnected before update";
    return false;
  }

  String url = ota_url("/firmware/download?version=") + ota_manifest.version;
  printf("OTA download start: %s\r\n", url.c_str());

  HTTPUpdate updater(25000);
  updater.rebootOnUpdate(false);
  updater.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  updater.setMD5sum(ota_manifest.md5);

  int last_percent = -1;
  updater.onStart([callback]() {
    if(callback) {
      callback(0, "Downloading firmware");
    }
  });
  updater.onProgress([callback, &last_percent](int current, int total) {
    uint8_t percent = total > 0 ? (uint8_t)((current * 100LL) / total) : 0;
    if(percent >= 100 || last_percent < 0 || percent >= last_percent + 2) {
      last_percent = percent;
      if(callback) {
        callback(percent, percent < 100 ? "Downloading firmware" : "Verifying firmware");
      }
    }
  });

  t_httpUpdate_return result;
  if(url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setInsecure();
    result = updater.update(client, url, APP_FIRMWARE_VERSION);
  } else {
    WiFiClient client;
    result = updater.update(client, url, APP_FIRMWARE_VERSION);
  }

  if(result == HTTP_UPDATE_OK) {
    ota_last_error = "";
    if(callback) {
      callback(100, "Update complete");
    }
    printf("OTA install complete\r\n");
    return true;
  }

  ota_last_error = updater.getLastErrorString();
  if(ota_last_error.length() == 0) {
    ota_last_error = result == HTTP_UPDATE_NO_UPDATES ? "Server reported no update" : "Firmware update failed";
  }
  printf("OTA install failed: %s\r\n", ota_last_error.c_str());
  return false;
}

const OtaManifest * OtaUpdater_GetManifest(void)
{
  return &ota_manifest;
}

const char * OtaUpdater_LastError(void)
{
  return ota_last_error.c_str();
}
