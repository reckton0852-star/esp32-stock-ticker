#include "Wireless.h"
#include "Secrets.h"

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

static bool wifi_connect_one(const WifiCredential * credential)
{
  printf("WiFi target SSID: %s\r\n", credential->ssid);

  if(WiFi.status() == WL_CONNECTED) {
    WIFI_Connection = true;
    snprintf(WIFI_IP, sizeof(WIFI_IP), "%s", WiFi.localIP().toString().c_str());
    printf("WiFi already connected, IP: %s\r\n", WIFI_IP);
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(credential->ssid, credential->password);
  printf("WiFi begin called\r\n");

  uint8_t retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 16) {
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

static bool wifi_connect(void)
{
  if(WiFi.status() == WL_CONNECTED) {
    WIFI_Connection = true;
    snprintf(WIFI_IP, sizeof(WIFI_IP), "%s", WiFi.localIP().toString().c_str());
    printf("WiFi already connected, IP: %s\r\n", WIFI_IP);
    return true;
  }

  const size_t wifi_count = sizeof(WIFI_CREDENTIALS) / sizeof(WIFI_CREDENTIALS[0]);
  for(size_t i = 0; i < wifi_count; i++) {
    if(wifi_connect_one(&WIFI_CREDENTIALS[i])) {
      return true;
    }
    printf("WiFi fallback next network\r\n");
  }

  WIFI_Connection = false;
  snprintf(WIFI_IP, sizeof(WIFI_IP), "Offline");
  return false;
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
  return wifi_connect();
}

bool Wireless_GetLocalTime(struct tm * timeinfo)
{
  if(!timeinfo) {
    return false;
  }
  return getLocalTime(timeinfo, 100);
}
