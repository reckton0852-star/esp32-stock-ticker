#pragma once

static const char * FINNHUB_TOKEN = "your_finnhub_api_key";

typedef struct {
  const char * ssid;
  const char * password;
} WifiCredential;

static const WifiCredential WIFI_CREDENTIALS[] = {
  {"your_home_wifi_ssid", "your_home_wifi_password"},
  {"your_office_wifi_ssid", "your_office_wifi_password"},
};
