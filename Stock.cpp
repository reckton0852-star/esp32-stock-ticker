#include "Stock.h"
#include "Wireless.h"
#include "Secrets.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

static const uint32_t STOCK_HTTP_TIMEOUT_MS = 4000;
static const uint32_t STOCK_REQUEST_WATCHDOG_MS = 12000;
static const uint32_t STOCK_REQUEST_GAP_MS = 2000;

static StockQuote quotes[] = {
  {"WDC", "W. Digital", 0.0f, 0.0f, 0.0f, false, false, false, "Waiting", "--:--", "-", "-", "-", "-", "-", 0},
  {"MU", "Micron", 0.0f, 0.0f, 0.0f, false, false, false, "Waiting", "--:--", "-", "-", "-", "-", "-", 0},
  {"AAPL", "Apple", 0.0f, 0.0f, 0.0f, false, false, false, "Waiting", "--:--", "-", "-", "-", "-", "-", 0},
  {"NVDA", "NVIDIA", 0.0f, 0.0f, 0.0f, false, false, false, "Waiting", "--:--", "-", "-", "-", "-", "-", 0},
  {"AVGO", "Broadcom", 0.0f, 0.0f, 0.0f, false, false, false, "Waiting", "--:--", "-", "-", "-", "-", "-", 0},
  {"TSM", "TSMC", 0.0f, 0.0f, 0.0f, false, false, false, "Waiting", "--:--", "-", "-", "-", "-", "-", 0},
};

const size_t STOCK_COUNT = sizeof(quotes) / sizeof(quotes[0]);
static const size_t NO_PENDING_PRIORITY = (size_t)-1;

static size_t current_index = 0;
static size_t auto_refresh_index = 0;
static size_t pending_priority_index = NO_PENDING_PRIORITY;
static size_t in_flight_index = NO_PENDING_PRIORITY;
static uint32_t request_started_ms = 0;
static uint32_t next_request_allowed_ms = 0;
static bool request_in_flight = false;
static TaskHandle_t stock_task_handle = NULL;

static bool start_stock_task(size_t index);
static void finish_stock_request(StockQuote * quote);

static const char * http_error_text(int code)
{
  switch(code) {
    case HTTPC_ERROR_CONNECTION_REFUSED: return "connection refused";
    case HTTPC_ERROR_SEND_HEADER_FAILED: return "send header failed";
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED: return "send payload failed";
    case HTTPC_ERROR_NOT_CONNECTED: return "not connected";
    case HTTPC_ERROR_CONNECTION_LOST: return "connection lost";
    case HTTPC_ERROR_NO_STREAM: return "no stream";
    case HTTPC_ERROR_NO_HTTP_SERVER: return "no http server";
    case HTTPC_ERROR_TOO_LESS_RAM: return "too less ram";
    case HTTPC_ERROR_ENCODING: return "encoding";
    case HTTPC_ERROR_STREAM_WRITE: return "stream write";
    case HTTPC_ERROR_READ_TIMEOUT: return "read timeout";
    default: return "unknown";
  }
}

static bool extract_json_float(const String& payload, const char * key, float * value)
{
  int start = payload.indexOf(key);
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

static bool extract_json_string(const String& payload, const char * key, char * value, size_t value_size)
{
  int start = payload.indexOf(key);
  if(start < 0) {
    return false;
  }

  start += strlen(key);
  if(start >= payload.length() || payload[start] != '"') {
    return false;
  }
  start++;

  int end = start;
  while(end < payload.length()) {
    if(payload[end] == '"' && (end == start || payload[end - 1] != '\\')) {
      break;
    }
    end++;
  }

  if(end <= start) {
    return false;
  }

  String parsed = payload.substring(start, end);
  parsed.replace("\\\"", "\"");
  parsed.toCharArray(value, value_size);
  return true;
}

static void copy_or_default(char * dest, size_t dest_size, bool ok, const char * value, const char * fallback)
{
  snprintf(dest, dest_size, "%s", (ok && value && value[0] != '\0') ? value : fallback);
}

static void update_timestamp(StockQuote * quote)
{
  struct tm timeinfo;
  if(Wireless_GetLocalTime(&timeinfo)) {
    snprintf(quote->updated_at, sizeof(quote->updated_at), "%02d:%02d",
      timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    snprintf(quote->updated_at, sizeof(quote->updated_at), "--:--");
  }
}

static void StockTask(void * parameter)
{
  size_t index = (size_t)(uintptr_t)parameter;
  StockQuote * quote = &quotes[index];

  if(!WIFI_Connection) {
    if(!quote->ready) {
      snprintf(quote->status, sizeof(quote->status), "Wi-Fi offline");
    }
    finish_stock_request(quote);
    vTaskDelete(NULL);
    return;
  }

  char url[192];
  snprintf(url, sizeof(url),
    "%s/quote?symbol=%s",
    STOCK_PROXY_BASE_URL, quote->symbol);
  printf("Stock request URL: %s\r\n", url);

  HTTPClient http;
  bool use_https = strncmp(url, "https://", 8) == 0;
  WiFiClient client;
  WiFiClientSecure secure_client;

  if(use_https) {
    secure_client.setInsecure();
    secure_client.setTimeout(STOCK_HTTP_TIMEOUT_MS);
    secure_client.setHandshakeTimeout(12);
    if(!http.begin(secure_client, url)) {
      if(!quote->ready) {
        snprintf(quote->status, sizeof(quote->status), "HTTPS begin fail");
      }
      char ssl_error[128] = {0};
      secure_client.lastError(ssl_error, sizeof(ssl_error));
      printf("HTTPS begin error %s: %s\r\n", quote->symbol, ssl_error);
      secure_client.stop();
      finish_stock_request(quote);
      vTaskDelete(NULL);
      return;
    }
  } else {
    client.setTimeout(STOCK_HTTP_TIMEOUT_MS);
    if(!http.begin(client, url)) {
      if(!quote->ready) {
        snprintf(quote->status, sizeof(quote->status), "HTTP begin fail");
      }
      client.stop();
      finish_stock_request(quote);
      vTaskDelete(NULL);
      return;
    }
  }

  http.setConnectTimeout(STOCK_HTTP_TIMEOUT_MS);
  http.setTimeout(STOCK_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  http.addHeader("Connection", "close");
  http.setUserAgent("esp32-stock-ticker/1.0");

  printf("Stock request start: %s via %s\r\n", quote->symbol, use_https ? "https" : "http");
  int http_code = http.GET();
  printf("Stock HTTP code %s: %d\r\n", quote->symbol, http_code);
  if(http_code < 0) {
    printf("Stock HTTP error %s: %s\r\n", quote->symbol, http_error_text(http_code));
    if(use_https) {
      char ssl_error[128] = {0};
      secure_client.lastError(ssl_error, sizeof(ssl_error));
      printf("Stock HTTPS detail %s: %s\r\n", quote->symbol, ssl_error);
    }
  }
  if(http_code != HTTP_CODE_OK) {
    if(!quote->ready) {
      snprintf(quote->status, sizeof(quote->status), "HTTP %d", http_code);
    }
    http.end();
    if(use_https) {
      secure_client.stop();
    } else {
      client.stop();
    }
    finish_stock_request(quote);
    vTaskDelete(NULL);
    return;
  }

  String payload = http.getString();
  http.end();
  if(use_https) {
    secure_client.stop();
  } else {
    client.stop();
  }

  float price = 0.0f;
  float change = 0.0f;
  float change_percent = 0.0f;
  char status[32] = {0};
  char updated_at[24] = {0};
  char industry[32] = {0};
  char country[20] = {0};
  char ipo[16] = {0};
  char market_cap[16] = {0};
  char shares_out[16] = {0};

  bool ok_price = extract_json_float(payload, "\"c\":", &price);
  bool ok_change = extract_json_float(payload, "\"d\":", &change);
  bool ok_dp = extract_json_float(payload, "\"dp\":", &change_percent);
  bool ok_status = extract_json_string(payload, "\"status\":", status, sizeof(status));
  bool ok_updated_at = extract_json_string(payload, "\"updated_at\":", updated_at, sizeof(updated_at));
  bool ok_industry = extract_json_string(payload, "\"industry\":", industry, sizeof(industry));
  bool ok_country = extract_json_string(payload, "\"country\":", country, sizeof(country));
  bool ok_ipo = extract_json_string(payload, "\"ipo\":", ipo, sizeof(ipo));
  bool ok_market_cap = extract_json_string(payload, "\"market_cap\":", market_cap, sizeof(market_cap));
  bool ok_shares_out = extract_json_string(payload, "\"shares_out\":", shares_out, sizeof(shares_out));

  if(ok_price && price > 0.01f) {
    if(!ok_change) {
      change = 0.0f;
    }
    if(!ok_dp) {
      change_percent = 0.0f;
    }

    quote->price = price;
    quote->change = change;
    quote->change_percent = change_percent;
    quote->ready = true;
    snprintf(quote->status, sizeof(quote->status), "%s", ok_status ? status : "USD");
    snprintf(quote->updated_at, sizeof(quote->updated_at), "%s", ok_updated_at ? updated_at : "--:--");
    copy_or_default(quote->industry, sizeof(quote->industry), ok_industry, industry, "-");
    copy_or_default(quote->country, sizeof(quote->country), ok_country, country, "-");
    copy_or_default(quote->ipo, sizeof(quote->ipo), ok_ipo, ipo, "-");
    copy_or_default(quote->market_cap, sizeof(quote->market_cap), ok_market_cap, market_cap, "-");
    copy_or_default(quote->shares_out, sizeof(quote->shares_out), ok_shares_out, shares_out, "-");
    quote->profile_ready = ok_industry || ok_country || ok_ipo || ok_market_cap || ok_shares_out;
    printf("Stock ok %s: %.2f %.2f %.2f%%\r\n",
      quote->symbol, quote->price, quote->change, quote->change_percent);
  } else {
    if(!quote->ready) {
      snprintf(quote->status, sizeof(quote->status), "Parse fail");
    }
    printf("Stock parse failed: %s\r\n", quote->symbol);
  }

  finish_stock_request(quote);
  vTaskDelete(NULL);
}

static void finish_stock_request(StockQuote * quote)
{
  if(quote) {
    quote->loading = false;
  }
  request_in_flight = false;
  in_flight_index = NO_PENDING_PRIORITY;
  request_started_ms = 0;
  next_request_allowed_ms = millis() + STOCK_REQUEST_GAP_MS;
  stock_task_handle = NULL;
}

static void Stock_ServiceWatchdog(void)
{
  if(!request_in_flight || request_started_ms == 0) {
    return;
  }

  uint32_t now = millis();
  if(now - request_started_ms <= STOCK_REQUEST_WATCHDOG_MS) {
    return;
  }

  printf("Stock request watchdog timeout\r\n");
  if(stock_task_handle) {
    vTaskDelete(stock_task_handle);
  }

  if(in_flight_index < STOCK_COUNT) {
    quotes[in_flight_index].loading = false;
  }

  stock_task_handle = NULL;
  request_in_flight = false;
  in_flight_index = NO_PENDING_PRIORITY;
  request_started_ms = 0;
  next_request_allowed_ms = millis() + STOCK_REQUEST_GAP_MS;
  WIFI_Connection = false;
  WiFi.disconnect(false);
}

static bool start_stock_task(size_t index)
{
  if(request_in_flight) {
    return false;
  }

  uint32_t now = millis();
  if(next_request_allowed_ms != 0 && (int32_t)(now - next_request_allowed_ms) < 0) {
    return false;
  }

  StockQuote * quote = &quotes[index];
  quote->loading = true;
  quote->last_fetch_ms = millis();
  in_flight_index = index;
  request_started_ms = millis();
  request_in_flight = true;

  BaseType_t created = xTaskCreatePinnedToCore(
    StockTask,
    "StockTask",
    16384,
    (void *)(uintptr_t)index,
    1,
    &stock_task_handle,
    0
  );

  if(created != pdPASS) {
    snprintf(quote->status, sizeof(quote->status), "Task fail");
    quote->loading = false;
    request_in_flight = false;
    in_flight_index = NO_PENDING_PRIORITY;
    request_started_ms = 0;
    stock_task_handle = NULL;
    return false;
  }

  return true;
}

void Stock_Init(void)
{
  current_index = 0;
  auto_refresh_index = 0;
  pending_priority_index = current_index;
}

void Stock_Next(void)
{
  current_index = (current_index + 1) % STOCK_COUNT;
}

void Stock_Previous(void)
{
  if(current_index == 0) {
    current_index = STOCK_COUNT - 1;
  } else {
    current_index--;
  }
}

size_t Stock_CurrentIndex(void)
{
  return current_index;
}

const StockQuote * Stock_CurrentQuote(void)
{
  return &quotes[current_index];
}

void Stock_RequestCurrent(void)
{
  if(!start_stock_task(current_index)) {
    pending_priority_index = current_index;
  }
}

bool Stock_RequestCurrentIfStale(uint32_t min_interval_ms)
{
  if(request_in_flight) {
    pending_priority_index = current_index;
    return false;
  }

  StockQuote * quote = &quotes[current_index];
  uint32_t now = millis();
  if(quote->ready && min_interval_ms > 0 && quote->last_fetch_ms != 0 && now - quote->last_fetch_ms < min_interval_ms) {
    return false;
  }

  return start_stock_task(current_index);
}

static bool quote_needs_refresh(const StockQuote * quote, uint32_t now, uint32_t refresh_interval_ms, uint32_t retry_interval_ms)
{
  uint32_t interval = quote->ready ? refresh_interval_ms : retry_interval_ms;
  return quote->last_fetch_ms == 0 || now - quote->last_fetch_ms >= interval;
}

void Stock_ServiceAutoRefresh(uint32_t refresh_interval_ms, uint32_t retry_interval_ms)
{
  Stock_ServiceWatchdog();

  if(request_in_flight) {
    return;
  }

  uint32_t now = millis();
  if(pending_priority_index < STOCK_COUNT) {
    size_t index = pending_priority_index;
    pending_priority_index = NO_PENDING_PRIORITY;
    if(start_stock_task(index)) {
      printf("Stock priority refresh: %s\r\n", quotes[index].symbol);
      return;
    }
  }

  for(size_t step = 0; step < STOCK_COUNT; step++) {
    size_t index = (auto_refresh_index + step) % STOCK_COUNT;
    StockQuote * quote = &quotes[index];
    if(quote_needs_refresh(quote, now, refresh_interval_ms, retry_interval_ms)) {
      if(start_stock_task(index)) {
        auto_refresh_index = (index + 1) % STOCK_COUNT;
        printf("Stock auto refresh: %s\r\n", quote->symbol);
        return;
      }
    }
  }
}
