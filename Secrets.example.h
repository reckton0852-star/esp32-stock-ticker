#pragma once

typedef struct {
  const char * ssid;
  const char * password;
} WifiCredential;

static const WifiCredential WIFI_CREDENTIALS[] = {
  {"your_home_wifi_ssid", "your_home_wifi_password"},
  {"your_office_wifi_ssid", "your_office_wifi_password"},
};

static const char * STOCK_PROXY_BASE_URL = "https://stock.your-domain.com";
static const char * DEFAULT_STOCK_SYMBOLS = "WDC,MU,AAPL,NVDA,AVGO,TSM";
static const char * DEFAULT_FX_SYMBOLS = "USD,EUR,GBP,CAD";
