#pragma once

#include <Arduino.h>

static const size_t APP_WIFI_SLOTS = 3;
static const size_t APP_SSID_MAX = 33;
static const size_t APP_PASSWORD_MAX = 65;
static const size_t APP_PROXY_URL_MAX = 96;
static const size_t APP_SYMBOLS_MAX = 96;
static const size_t APP_FX_SYMBOLS_MAX = 96;
static const uint8_t APP_DEFAULT_BRIGHTNESS = 60;
static const uint16_t APP_DEFAULT_REFRESH_SECONDS = 120;
static const uint16_t APP_DEFAULT_ROTATE_SECONDS = 12;
static const bool APP_ENABLE_RGB_LED = false;
static const bool APP_ENABLE_IMU = false;
static const uint8_t APP_DISPLAY_MODE_STOCKS = 0;
static const uint8_t APP_DISPLAY_MODE_FX = 1;

typedef struct {
  char ssid[APP_SSID_MAX];
  char password[APP_PASSWORD_MAX];
} AppWifiCredential;

typedef struct {
  AppWifiCredential wifi[APP_WIFI_SLOTS];
  char proxy_base_url[APP_PROXY_URL_MAX];
  char stock_symbols[APP_SYMBOLS_MAX];
  char fx_symbols[APP_FX_SYMBOLS_MAX];
  uint8_t display_mode;
  uint8_t brightness;
  uint16_t refresh_seconds;
  uint16_t rotate_seconds;
  bool has_saved_config;
} AppConfigData;

void AppConfig_Init(void);
const AppConfigData * AppConfig_Get(void);
bool AppConfig_Save(const AppConfigData * config);
bool AppConfig_HasSavedConfig(void);
void AppConfig_ResetToDefaults(void);
