#include "Weather.h"
#include "Wireless.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static const char * WEATHER_URL =
  "https://api.open-meteo.com/v1/forecast"
  "?latitude=22.5431"
  "&longitude=114.0579"
  "&current=temperature_2m,weather_code"
  "&timezone=Asia%2FShanghai";

bool WEATHER_Loading = false;
bool WEATHER_Ready = false;
float WEATHER_TemperatureC = 0.0f;
char WEATHER_Description[32] = "Waiting";
char WEATHER_UpdatedAt[24] = "--:--";

static bool extract_json_float(const String& payload, const char * key, float * value, int search_from)
{
  int start = payload.indexOf(key, search_from);
  if(start < 0) {
    return false;
  }

  start += strlen(key);
  while(start < payload.length() && (payload[start] == ' ' || payload[start] == '\t')) {
    start++;
  }

  int end = start;
  while(end < payload.length()) {
    char c = payload[end];
    if((c >= '0' && c <= '9') || c == '.' || c == '-') {
      end++;
    } else {
      break;
    }
  }

  if(end <= start) {
    return false;
  }

  *value = payload.substring(start, end).toFloat();
  return true;
}

static bool extract_json_string(const String& payload, const char * key, char * out, size_t out_size, int search_from)
{
  int start = payload.indexOf(key, search_from);
  if(start < 0) {
    return false;
  }

  start += strlen(key);
  int end = payload.indexOf('"', start);
  if(end < 0) {
    return false;
  }

  String value = payload.substring(start, end);
  value.toCharArray(out, out_size);
  return true;
}

static const char * weather_code_to_text(int code)
{
  switch(code) {
    case 0: return "Clear";
    case 1: return "Mainly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55: return "Drizzle";
    case 61:
    case 63:
    case 65: return "Rain";
    case 71:
    case 73:
    case 75: return "Snow";
    case 80:
    case 81:
    case 82: return "Showers";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Storm hail";
    default: return "Unknown";
  }
}

static void WeatherTask(void * parameter)
{
  (void)parameter;
  WEATHER_Loading = true;

  if(!WIFI_Connection) {
    snprintf(WEATHER_Description, sizeof(WEATHER_Description), "Wi-Fi offline");
    WEATHER_Ready = false;
    WEATHER_Loading = false;
    vTaskDelete(NULL);
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5000);

  HTTPClient http;
  if(!http.begin(client, WEATHER_URL)) {
    snprintf(WEATHER_Description, sizeof(WEATHER_Description), "HTTP begin fail");
    WEATHER_Ready = false;
    WEATHER_Loading = false;
    vTaskDelete(NULL);
    return;
  }

  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  printf("Weather request start\r\n");
  int http_code = http.GET();
  printf("Weather HTTP code: %d\r\n", http_code);

  if(http_code != HTTP_CODE_OK) {
    snprintf(WEATHER_Description, sizeof(WEATHER_Description), "HTTP %d", http_code);
    WEATHER_Ready = false;
    http.end();
    WEATHER_Loading = false;
    vTaskDelete(NULL);
    return;
  }

  String payload = http.getString();
  http.end();
  printf("Weather payload head: %.160s\r\n", payload.c_str());

  float temperature = 0.0f;
  float weather_code = -1.0f;
  char weather_time[24] = {0};
  int current_pos = payload.indexOf("\"current\":{");
  if(current_pos < 0) {
    current_pos = 0;
  }

  bool ok_temp = extract_json_float(payload, "\"temperature_2m\":", &temperature, current_pos);
  bool ok_code = extract_json_float(payload, "\"weather_code\":", &weather_code, current_pos);
  bool ok_time = extract_json_string(payload, "\"time\":\"", weather_time, sizeof(weather_time), current_pos);

  if(ok_temp && ok_code && ok_time) {
    WEATHER_TemperatureC = temperature;
    snprintf(WEATHER_Description, sizeof(WEATHER_Description), "%s", weather_code_to_text((int)weather_code));

    const char * time_part = strchr(weather_time, 'T');
    if(time_part && strlen(time_part) >= 6) {
      snprintf(WEATHER_UpdatedAt, sizeof(WEATHER_UpdatedAt), "%c%c:%c%c",
        time_part[1], time_part[2], time_part[4], time_part[5]);
    } else {
      snprintf(WEATHER_UpdatedAt, sizeof(WEATHER_UpdatedAt), "--:--");
    }

    WEATHER_Ready = true;
    printf("Weather ok: %.1fC %s at %s\r\n", WEATHER_TemperatureC, WEATHER_Description, WEATHER_UpdatedAt);
  } else {
    snprintf(WEATHER_Description, sizeof(WEATHER_Description), "Parse fail");
    WEATHER_Ready = false;
    printf("Weather parse failed\r\n");
  }

  WEATHER_Loading = false;
  vTaskDelete(NULL);
}

void Weather_RequestUpdate(void)
{
  if(WEATHER_Loading) {
    return;
  }

  xTaskCreatePinnedToCore(
    WeatherTask,
    "WeatherTask",
    8192,
    NULL,
    1,
    NULL,
    0
  );
}
