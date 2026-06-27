#include "Wireless.h"
#include "AppConfig.h"

static const char * NTP_SERVER_1 = "ntp.aliyun.com";
static const char * NTP_SERVER_2 = "pool.ntp.org";
static const long GMT_OFFSET_SECONDS = 8 * 3600;
static const int DAYLIGHT_OFFSET_SECONDS = 0;

bool WIFI_Connection = false;
uint8_t WIFI_NUM = 0;
uint8_t BLE_NUM = 0;
bool Scan_finish = false;
bool TIME_Synced = false;
char WIFI_IP[32] = "Offline";
static volatile bool wifi_connect_in_progress = false;

static void notify_status(WirelessStatusCallback callback, const char * line1, const char * line2, const char * line3)
{
  if(callback) {
    callback(line1, line2, line3);
  }
}

static void wifi_prepare_sta(void)
{
  WiFi.disconnect(true, true);
  delay(120);
  WiFi.mode(WIFI_MODE_NULL);
  delay(120);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  delay(120);
}

static bool wifi_connect_one(const AppWifiCredential * credential)
{
  printf("WiFi target SSID: %s\r\n", credential->ssid);

  if(WiFi.status() == WL_CONNECTED) {
    WIFI_Connection = true;
    snprintf(WIFI_IP, sizeof(WIFI_IP), "%s", WiFi.localIP().toString().c_str());
    printf("WiFi already connected, IP: %s\r\n", WIFI_IP);
    return true;
  }

  wifi_prepare_sta();
  WiFi.begin(credential->ssid, credential->password);
  printf("WiFi begin called\r\n");

  uint8_t retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 8) {
    printf("WiFi waiting... attempt=%u status=%d\r\n", retry + 1, WiFi.status());
    delay(500);
    retry++;
  }

  WIFI_Connection = WiFi.status() == WL_CONNECTED;
  if(WIFI_Connection) {
    snprintf(WIFI_IP, sizeof(WIFI_IP), "%s", WiFi.localIP().toString().c_str());
    printf("WiFi connected, IP: %s\r\n", WIFI_IP);
  } else {
    snprintf(WIFI_IP, sizeof(WIFI_IP), "Offline");
    printf("WiFi connect failed, final status=%d\r\n", WiFi.status());
  }
  return WIFI_Connection;
}

static bool wifi_connect_one_with_status(const AppWifiCredential * credential, WirelessStatusCallback callback)
{
  char line2[96];
  snprintf(line2, sizeof(line2), "Trying %s", credential->ssid);
  notify_status(callback, "WiFi Scan", line2, "Connecting...");

  if(WiFi.status() == WL_CONNECTED) {
    WIFI_Connection = true;
    snprintf(WIFI_IP, sizeof(WIFI_IP), "%s", WiFi.localIP().toString().c_str());
    snprintf(line2, sizeof(line2), "Connected %s", credential->ssid);
    notify_status(callback, "WiFi Scan", line2, WIFI_IP);
    printf("WiFi already connected, IP: %s\r\n", WIFI_IP);
    return true;
  }

  wifi_prepare_sta();
  WiFi.begin(credential->ssid, credential->password);
  printf("WiFi begin called\r\n");

  uint8_t retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 8) {
    char line3[96];
    printf("WiFi waiting... attempt=%u status=%d\r\n", retry + 1, WiFi.status());
    snprintf(line3, sizeof(line3), "Attempt %u  status %d", retry + 1, WiFi.status());
    notify_status(callback, "WiFi Scan", line2, line3);
    delay(500);
    retry++;
  }

  WIFI_Connection = WiFi.status() == WL_CONNECTED;
  if(WIFI_Connection) {
    snprintf(WIFI_IP, sizeof(WIFI_IP), "%s", WiFi.localIP().toString().c_str());
    snprintf(line2, sizeof(line2), "Connected %s", credential->ssid);
    notify_status(callback, "WiFi Ready", line2, WIFI_IP);
    printf("WiFi connected, IP: %s\r\n", WIFI_IP);
  } else {
    snprintf(WIFI_IP, sizeof(WIFI_IP), "Offline");
    char fail_line[96];
    snprintf(fail_line, sizeof(fail_line), "%s failed", credential->ssid);
    snprintf(line2, sizeof(line2), "status %d", WiFi.status());
    notify_status(callback, "WiFi Scan", fail_line, line2);
    printf("WiFi connect failed, final status=%d\r\n", WiFi.status());
  }
  return WIFI_Connection;
}

static bool wifi_connect(void)
{
  if(WiFi.status() == WL_CONNECTED) {
    WIFI_Connection = true;
    snprintf(WIFI_IP, sizeof(WIFI_IP), "%s", WiFi.localIP().toString().c_str());
    printf("WiFi already connected, IP: %s\r\n", WIFI_IP);
    return true;
  }

  const AppConfigData * config = AppConfig_Get();
  for(size_t i = 0; i < APP_WIFI_SLOTS; i++) {
    if(strlen(config->wifi[i].ssid) == 0) {
      continue;
    }

    if(wifi_connect_one(&config->wifi[i])) {
      return true;
    }
    printf("WiFi fallback next network\r\n");
  }

  WIFI_Connection = false;
  snprintf(WIFI_IP, sizeof(WIFI_IP), "Offline");
  return false;
}

bool Wireless_ConnectSavedWithStatus(WirelessStatusCallback callback)
{
  if(wifi_connect_in_progress) {
    notify_status(callback, "WiFi Scan", "Connection already running", "Please wait...");
    return false;
  }
  wifi_connect_in_progress = true;

  if(WiFi.status() == WL_CONNECTED) {
    WIFI_Connection = true;
    snprintf(WIFI_IP, sizeof(WIFI_IP), "%s", WiFi.localIP().toString().c_str());
    notify_status(callback, "WiFi Ready", "Already connected", WIFI_IP);
    wifi_connect_in_progress = false;
    return true;
  }

  wifi_prepare_sta();
  notify_status(callback, "WiFi Scan", "Scanning nearby networks...", "");
  int count = WiFi.scanNetworks(false, true);
  if(count < 0) {
    notify_status(callback, "WiFi Scan", "Scan failed", "Going to setup mode...");
    printf("WiFi scan failed: %d\r\n", count);
  } else {
    char line2[96];
    snprintf(line2, sizeof(line2), "Found %d network%s", count, count == 1 ? "" : "s");
    notify_status(callback, "WiFi Scan", line2, "Checking saved WiFi...");
    printf("WiFi scan found: %d\r\n", count);
    WiFi.scanDelete();
  }

  const AppConfigData * config = AppConfig_Get();
  for(size_t i = 0; i < APP_WIFI_SLOTS; i++) {
    if(strlen(config->wifi[i].ssid) == 0) {
      continue;
    }

    if(wifi_connect_one_with_status(&config->wifi[i], callback)) {
      wifi_connect_in_progress = false;
      return true;
    }
    notify_status(callback, "WiFi Scan", "Trying next saved WiFi...", "");
    printf("WiFi fallback next network\r\n");
  }

  WIFI_Connection = false;
  snprintf(WIFI_IP, sizeof(WIFI_IP), "Offline");
  notify_status(callback, "WiFi Scan", "No saved WiFi connected", "Entering setup mode...");
  wifi_connect_in_progress = false;
  return false;
}

int Wireless_ScanNearbyWiFi(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  int count = WiFi.scanNetworks(false, true);
  if(count < 0) {
    return 0;
  }
  WiFi.scanDelete();
  return count;
}

static void sync_time_if_needed(void)
{
  if(!WIFI_Connection) {
    TIME_Synced = false;
    printf("Time sync skipped, WiFi offline\r\n");
    return;
  }

  configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, NTP_SERVER_1, NTP_SERVER_2);

  struct tm timeinfo;
  TIME_Synced = getLocalTime(&timeinfo, 5000);
  printf("Time sync result: %s\r\n", TIME_Synced ? "ok" : "failed");
}

static int wifi_scan_number(void)
{
  if(!wifi_connect()) {
    return 0;
  }

  int count = WiFi.scanNetworks(false, true);
  if(count < 0) {
    printf("WiFi scan failed: %d\r\n", count);
    return 0;
  }

  printf("WiFi scan found: %d\r\n", count);
  WiFi.scanDelete();
  return count;
}

static int ble_scan_number(void)
{
  BLEDevice::init("ESP32");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);

  int count = 0;
#if ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API))
  BLEScanResults foundDevices = pBLEScan->start(3);
  count = foundDevices.getCount();
#else
  BLEScanResults *foundDevices = pBLEScan->start(3);
  count = foundDevices->getCount();
#endif

  pBLEScan->stop();
  pBLEScan->clearResults();
  BLEDevice::deinit(true);
  printf("BLE scan found: %d\r\n", count);
  return count;
}

void WirelessScanTask(void *parameter)
{
  (void)parameter;
  Scan_finish = false;
  printf("Wireless scan task started\r\n");

  wifi_connect();
  sync_time_if_needed();
  WIFI_NUM = 0;
  BLE_NUM = 0;

  Scan_finish = true;
  printf("Wireless scan task finished\r\n");
  vTaskDelete(NULL);
}

void Wireless_Test2()
{
  xTaskCreatePinnedToCore(
    WirelessScanTask,
    "WirelessScanTask",
    6144,
    NULL,
    2,
    NULL,
    0
  );
}

bool Wireless_EnsureConnected()
{
  if(wifi_connect_in_progress) {
    return false;
  }
  wifi_connect_in_progress = true;
  bool connected = wifi_connect();
  wifi_connect_in_progress = false;
  return connected;
}

void Wireless_SyncTimeNow(void)
{
  sync_time_if_needed();
}

bool Wireless_GetLocalTime(struct tm * timeinfo)
{
  if(!timeinfo) {
    return false;
  }
  return getLocalTime(timeinfo, 100);
}
