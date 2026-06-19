#pragma once

typedef struct {
  const char * ssid;
  const char * password;
} WifiCredential;

static const WifiCredential WIFI_CREDENTIALS[] = {
  {"your_home_wifi_ssid", "your_home_wifi_password"},
  {"your_office_wifi_ssid", "your_office_wifi_password"},
};

static const char * STOCK_PROXY_BASE_URL = "http://your-computer-ip:8787";
