#include "AppConfig.h"
#include "Secrets.h"
#include <Preferences.h>

static Preferences preferences;
static AppConfigData app_config;

static void copy_string(char * dest, size_t dest_size, const char * src)
{
  if(!dest || dest_size == 0) {
    return;
  }
  snprintf(dest, dest_size, "%s", src ? src : "");
}

static void load_defaults(AppConfigData * config)
{
  memset(config, 0, sizeof(*config));

  for(size_t i = 0; i < APP_WIFI_SLOTS; i++) {
    if(i < (sizeof(WIFI_CREDENTIALS) / sizeof(WIFI_CREDENTIALS[0]))) {
      copy_string(config->wifi[i].ssid, sizeof(config->wifi[i].ssid), WIFI_CREDENTIALS[i].ssid);
      copy_string(config->wifi[i].password, sizeof(config->wifi[i].password), WIFI_CREDENTIALS[i].password);
    }
  }

  copy_string(config->proxy_base_url, sizeof(config->proxy_base_url), STOCK_PROXY_BASE_URL);
  copy_string(config->stock_symbols, sizeof(config->stock_symbols), DEFAULT_STOCK_SYMBOLS);
  config->brightness = APP_DEFAULT_BRIGHTNESS;
  config->refresh_seconds = APP_DEFAULT_REFRESH_SECONDS;
  config->rotate_seconds = APP_DEFAULT_ROTATE_SECONDS;
  config->has_saved_config = false;
}

void AppConfig_Init(void)
{
  load_defaults(&app_config);

  if(!preferences.begin("stockcfg", true)) {
    return;
  }

  bool saved = preferences.getBool("saved", false);
  if(saved) {
    for(size_t i = 0; i < APP_WIFI_SLOTS; i++) {
      char key_ssid[12];
      char key_pass[12];
      snprintf(key_ssid, sizeof(key_ssid), "ssid%u", (unsigned)i);
      snprintf(key_pass, sizeof(key_pass), "pass%u", (unsigned)i);
      copy_string(app_config.wifi[i].ssid, sizeof(app_config.wifi[i].ssid), preferences.getString(key_ssid, app_config.wifi[i].ssid).c_str());
      copy_string(app_config.wifi[i].password, sizeof(app_config.wifi[i].password), preferences.getString(key_pass, app_config.wifi[i].password).c_str());
    }

    copy_string(app_config.proxy_base_url, sizeof(app_config.proxy_base_url), preferences.getString("proxy", app_config.proxy_base_url).c_str());
    copy_string(app_config.stock_symbols, sizeof(app_config.stock_symbols), preferences.getString("symbols", app_config.stock_symbols).c_str());
    app_config.brightness = preferences.getUChar("bright", APP_DEFAULT_BRIGHTNESS);
    app_config.refresh_seconds = preferences.getUShort("refresh", APP_DEFAULT_REFRESH_SECONDS);
    app_config.rotate_seconds = preferences.getUShort("rotate", APP_DEFAULT_ROTATE_SECONDS);
    app_config.has_saved_config = true;
  }

  preferences.end();
}

const AppConfigData * AppConfig_Get(void)
{
  return &app_config;
}

bool AppConfig_Save(const AppConfigData * config)
{
  if(!config) {
    return false;
  }

  if(!preferences.begin("stockcfg", false)) {
    return false;
  }

  for(size_t i = 0; i < APP_WIFI_SLOTS; i++) {
    char key_ssid[12];
    char key_pass[12];
    snprintf(key_ssid, sizeof(key_ssid), "ssid%u", (unsigned)i);
    snprintf(key_pass, sizeof(key_pass), "pass%u", (unsigned)i);
    preferences.putString(key_ssid, config->wifi[i].ssid);
    preferences.putString(key_pass, config->wifi[i].password);
  }

  preferences.putString("proxy", config->proxy_base_url);
  preferences.putString("symbols", config->stock_symbols);
  preferences.putUChar("bright", config->brightness);
  preferences.putUShort("refresh", config->refresh_seconds);
  preferences.putUShort("rotate", config->rotate_seconds);
  preferences.putBool("saved", true);
  preferences.end();

  app_config = *config;
  app_config.has_saved_config = true;
  return true;
}

bool AppConfig_HasSavedConfig(void)
{
  return app_config.has_saved_config;
}

void AppConfig_ResetToDefaults(void)
{
  load_defaults(&app_config);
}
