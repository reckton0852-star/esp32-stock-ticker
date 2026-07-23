#include "Stock.h"
#include "Wireless.h"
#include "AppConfig.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>

static const uint32_t STOCK_HTTP_TIMEOUT_MS = 4000;
static const uint32_t STOCK_REQUEST_GAP_MS = 600;
static const size_t MAX_STOCK_COUNT = 8;
static StockQuote quotes[MAX_STOCK_COUNT];
static float quote_history[MAX_STOCK_COUNT][STOCK_TREND_POINTS];
static uint8_t quote_history_count[MAX_STOCK_COUNT];
static uint8_t quote_history_next[MAX_STOCK_COUNT];
const size_t STOCK_COUNT = MAX_STOCK_COUNT;
static const size_t NO_PENDING_PRIORITY = (size_t)-1;
static size_t stock_count = 0;

static const char * known_name(const char * symbol)
{
  if(strcmp(symbol, "WDC") == 0) return "W. Digital";
  if(strcmp(symbol, "MU") == 0) return "Micron";
  if(strcmp(symbol, "AAPL") == 0) return "Apple";
  if(strcmp(symbol, "NVDA") == 0) return "NVIDIA";
  if(strcmp(symbol, "AVGO") == 0) return "Broadcom";
  if(strcmp(symbol, "TSM") == 0) return "TSMC";
  return NULL;
}

static const char * known_fx_name(const char * symbol)
{
  if(strcmp(symbol, "USD") == 0) return "US Dollar";
  if(strcmp(symbol, "EUR") == 0) return "Euro";
  if(strcmp(symbol, "GBP") == 0) return "British Pound";
  if(strcmp(symbol, "CAD") == 0) return "Canadian Dollar";
  if(strcmp(symbol, "JPY") == 0) return "Japanese Yen";
  if(strcmp(symbol, "HKD") == 0) return "Hong Kong Dollar";
  return NULL;
}

static bool fx_mode(void)
{
  return AppConfig_Get()->display_mode == APP_DISPLAY_MODE_FX;
}

static size_t current_index = 0;
static size_t auto_refresh_index = 0;
static size_t pending_priority_index = NO_PENDING_PRIORITY;
static size_t in_flight_index = NO_PENDING_PRIORITY;
static uint32_t next_request_allowed_ms = 0;
static bool request_in_flight = false;
static TaskHandle_t stock_worker_handle = NULL;
static size_t pending_request_index = NO_PENDING_PRIORITY;

static bool queue_stock_request(size_t index);
static void finish_stock_request(StockQuote * quote);
static void StockWorkerTask(void * parameter);

static void append_quote_history(size_t index, float price)
{
  if(index >= stock_count || !(price > 0.01f)) {
    return;
  }

  quote_history[index][quote_history_next[index]] = price;
  quote_history_next[index] = (quote_history_next[index] + 1) % STOCK_TREND_POINTS;
  if(quote_history_count[index] < STOCK_TREND_POINTS) {
    quote_history_count[index]++;
  }
}

static void reset_quote(StockQuote * quote, const char * symbol)
{
  memset(quote, 0, sizeof(*quote));
  quote->symbol = strdup(symbol);
  const char * pretty_name = fx_mode() ? known_fx_name(symbol) : known_name(symbol);
  quote->name = pretty_name ? strdup(pretty_name) : quote->symbol;
  snprintf(quote->status, sizeof(quote->status), "Waiting");
  snprintf(quote->updated_at, sizeof(quote->updated_at), "--:--");
  snprintf(quote->industry, sizeof(quote->industry), "-");
  snprintf(quote->country, sizeof(quote->country), "-");
  snprintf(quote->ipo, sizeof(quote->ipo), "-");
  snprintf(quote->market_cap, sizeof(quote->market_cap), "-");
  snprintf(quote->shares_out, sizeof(quote->shares_out), "-");
}

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

static uint8_t extract_json_float_array(const String& payload, const char * key, float * values, size_t max_values)
{
  if(!values || max_values == 0) {
    return 0;
  }

  int start = payload.indexOf(key);
  if(start < 0) {
    return 0;
  }

  start += strlen(key);
  while(start < payload.length() && payload[start] != '[') {
    start++;
  }
  if(start >= payload.length()) {
    return 0;
  }
  start++;

  uint8_t count = 0;
  while(start < payload.length() && count < max_values) {
    while(start < payload.length() && (payload[start] == ' ' || payload[start] == '\t' || payload[start] == ',')) {
      start++;
    }
    if(start >= payload.length() || payload[start] == ']') {
      break;
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
      break;
    }

    values[count++] = payload.substring(start, end).toFloat();
    start = end;
  }

  return count;
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

static void perform_stock_request(size_t index)
{
  StockQuote * quote = &quotes[index];

  if(!WIFI_Connection) {
    if(!quote->ready) {
      snprintf(quote->status, sizeof(quote->status), "Wi-Fi offline");
    }
    finish_stock_request(quote);
    return;
  }

  char url[192];
  if(fx_mode()) {
    snprintf(url, sizeof(url),
      "%s/fx?base=%s",
      Stock_ProxyBaseUrl(), quote->symbol);
  } else {
    snprintf(url, sizeof(url),
      "%s/quote?symbol=%s",
      Stock_ProxyBaseUrl(), quote->symbol);
  }
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
      return;
    }
  }

  http.setConnectTimeout(STOCK_HTTP_TIMEOUT_MS);
  http.setTimeout(STOCK_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  http.addHeader("Connection", "close");
  http.setUserAgent("esp32-stock-ticker/1.0");

  printf("%s request start: %s via %s\r\n", fx_mode() ? "FX" : "Stock", quote->symbol, use_https ? "https" : "http");
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
  bool ok_open = extract_json_float(payload, "\"o\":", &quote->open);
  bool ok_high = extract_json_float(payload, "\"h\":", &quote->high);
  bool ok_low = extract_json_float(payload, "\"l\":", &quote->low);
  bool ok_previous_close = extract_json_float(payload, "\"pc\":", &quote->previous_close);
  bool ok_status = extract_json_string(payload, "\"status\":", status, sizeof(status));
  bool ok_updated_at = extract_json_string(payload, "\"updated_at\":", updated_at, sizeof(updated_at));
  bool ok_industry = extract_json_string(payload, "\"industry\":", industry, sizeof(industry));
  bool ok_country = extract_json_string(payload, "\"country\":", country, sizeof(country));
  bool ok_ipo = extract_json_string(payload, "\"ipo\":", ipo, sizeof(ipo));
  bool ok_market_cap = extract_json_string(payload, "\"market_cap\":", market_cap, sizeof(market_cap));
  bool ok_shares_out = extract_json_string(payload, "\"shares_out\":", shares_out, sizeof(shares_out));
  float daily_history[STOCK_DAILY_POINTS];
  uint8_t daily_history_count = extract_json_float_array(payload, "\"history\":", daily_history, STOCK_DAILY_POINTS);

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
    quote->trading_ready = ok_open || ok_high || ok_low || ok_previous_close;
    quote->ready = true;
    snprintf(quote->status, sizeof(quote->status), "%s", ok_status ? status : "USD");
    update_timestamp(quote);
    copy_or_default(quote->industry, sizeof(quote->industry), ok_industry, industry, "-");
    copy_or_default(quote->country, sizeof(quote->country), ok_country, country, "-");
    copy_or_default(quote->ipo, sizeof(quote->ipo), ok_ipo, ipo, "-");
    copy_or_default(quote->market_cap, sizeof(quote->market_cap), ok_market_cap, market_cap, "-");
    copy_or_default(quote->shares_out, sizeof(quote->shares_out), ok_shares_out, shares_out, "-");
    quote->profile_ready = ok_industry || ok_country || ok_ipo || ok_market_cap || ok_shares_out;
    quote->last_fetch_ms = millis();
    if(daily_history_count >= 2) {
      memcpy(quote->daily_history, daily_history, sizeof(float) * daily_history_count);
      quote->daily_history_count = daily_history_count;
    }
    append_quote_history(index, quote->price);
    printf("%s ok %s: %.4f %.4f %.2f%%\r\n",
      fx_mode() ? "FX" : "Stock",
      quote->symbol, quote->price, quote->change, quote->change_percent);
  } else {
    if(!quote->ready) {
      snprintf(quote->status, sizeof(quote->status), "Parse fail");
    }
    printf("Stock parse failed: %s\r\n", quote->symbol);
  }

  finish_stock_request(quote);
}

static void StockWorkerTask(void * parameter)
{
  (void)parameter;

  for(;;) {
    if(request_in_flight && in_flight_index < stock_count) {
      perform_stock_request(in_flight_index);
      continue;
    }

    uint32_t now = millis();
    if(!request_in_flight &&
       pending_request_index < stock_count &&
       (next_request_allowed_ms == 0 || (int32_t)(now - next_request_allowed_ms) >= 0)) {
      size_t index = pending_request_index;
      pending_request_index = NO_PENDING_PRIORITY;
      in_flight_index = index;
      request_in_flight = true;
      quotes[index].loading = true;
      quotes[index].last_attempt_ms = now;
      continue;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void finish_stock_request(StockQuote * quote)
{
  if(quote) {
    quote->loading = false;
  }
  request_in_flight = false;
  in_flight_index = NO_PENDING_PRIORITY;
  next_request_allowed_ms = millis() + STOCK_REQUEST_GAP_MS;
}

static bool queue_stock_request(size_t index)
{
  if(index >= stock_count) {
    return false;
  }

  uint32_t now = millis();
  if(pending_request_index == index || in_flight_index == index) {
    return false;
  }

  if(request_in_flight) {
    pending_request_index = index;
    return false;
  }

  if(next_request_allowed_ms != 0 && (int32_t)(now - next_request_allowed_ms) < 0) {
    pending_request_index = index;
    return false;
  }

  pending_request_index = index;
  return true;
}

void Stock_Init(void)
{
  current_index = 0;
  auto_refresh_index = 0;
  pending_priority_index = current_index;
  pending_request_index = NO_PENDING_PRIORITY;
  request_in_flight = false;
  in_flight_index = NO_PENDING_PRIORITY;
  next_request_allowed_ms = 0;

  const AppConfigData * config = AppConfig_Get();
  char symbols[APP_SYMBOLS_MAX];
  snprintf(symbols, sizeof(symbols), "%s", fx_mode() ? config->fx_symbols : config->stock_symbols);

  for(size_t i = 0; i < MAX_STOCK_COUNT; i++) {
    memset(&quotes[i], 0, sizeof(quotes[i]));
    memset(quote_history[i], 0, sizeof(quote_history[i]));
    quote_history_count[i] = 0;
    quote_history_next[i] = 0;
  }

  stock_count = 0;
  char * token = strtok(symbols, ",");
  while(token && stock_count < MAX_STOCK_COUNT) {
    while(*token == ' ' || *token == '\t') token++;
    size_t len = strlen(token);
    while(len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\t' || token[len - 1] == '\r' || token[len - 1] == '\n')) {
      token[--len] = '\0';
    }
    if(len > 0) {
      for(size_t j = 0; j < len; j++) {
        token[j] = (char)toupper((unsigned char)token[j]);
      }
      reset_quote(&quotes[stock_count], token);
      stock_count++;
    }
    token = strtok(NULL, ",");
  }

  if(stock_count == 0) {
    reset_quote(&quotes[0], fx_mode() ? "USD" : "WDC");
    stock_count = 1;
  }

  if(!stock_worker_handle) {
    xTaskCreatePinnedToCore(
      StockWorkerTask,
      "StockWorkerTask",
      16384,
      NULL,
      1,
      &stock_worker_handle,
      0
    );
  }
}

void Stock_Next(void)
{
  if(stock_count == 0) {
    return;
  }
  current_index = (current_index + 1) % stock_count;
}

void Stock_Previous(void)
{
  if(stock_count == 0) {
    return;
  }
  if(current_index == 0) {
    current_index = stock_count - 1;
  } else {
    current_index--;
  }
}

size_t Stock_CurrentIndex(void)
{
  return current_index;
}

size_t Stock_ActiveCount(void)
{
  return stock_count;
}

const StockQuote * Stock_CurrentQuote(void)
{
  return &quotes[current_index];
}

size_t Stock_GetCurrentTrend(float * values, size_t max_values)
{
  if(!values || max_values == 0 || current_index >= stock_count) {
    return 0;
  }

  size_t count = quote_history_count[current_index];
  if(count > max_values) {
    count = max_values;
  }

  uint8_t next = quote_history_next[current_index];
  uint8_t stored = quote_history_count[current_index];
  uint8_t start = stored < STOCK_TREND_POINTS ? 0 : next;

  for(size_t i = 0; i < count; i++) {
    values[i] = quote_history[current_index][(start + i) % STOCK_TREND_POINTS];
  }
  return count;
}

bool Stock_IsFxMode(void)
{
  return fx_mode();
}

void Stock_RequestCurrent(void)
{
  if(!queue_stock_request(current_index)) {
    pending_priority_index = current_index;
  }
}

bool Stock_RequestCurrentIfStale(uint32_t min_interval_ms)
{
  StockQuote * quote = &quotes[current_index];
  uint32_t now = millis();
  if(quote->ready && min_interval_ms > 0 && quote->last_fetch_ms != 0 && now - quote->last_fetch_ms < min_interval_ms) {
    return false;
  }

  if(request_in_flight) {
    pending_priority_index = current_index;
    return false;
  }

  return queue_stock_request(current_index);
}

static bool quote_needs_refresh(const StockQuote * quote, uint32_t now, uint32_t refresh_interval_ms, uint32_t retry_interval_ms)
{
  if(quote->last_attempt_ms != 0 && now - quote->last_attempt_ms < retry_interval_ms) {
    return false;
  }

  uint32_t interval = quote->ready ? refresh_interval_ms : retry_interval_ms;
  uint32_t anchor = quote->ready ? quote->last_fetch_ms : quote->last_attempt_ms;
  return anchor == 0 || now - anchor >= interval;
}

void Stock_ServiceAutoRefresh(uint32_t refresh_interval_ms, uint32_t retry_interval_ms)
{
  if(request_in_flight) {
    return;
  }

  uint32_t now = millis();
  if(pending_priority_index < stock_count) {
    size_t index = pending_priority_index;
    pending_priority_index = NO_PENDING_PRIORITY;
    if(queue_stock_request(index)) {
      printf("Stock priority refresh: %s\r\n", quotes[index].symbol);
      return;
    }
  }

  for(size_t step = 0; step < stock_count; step++) {
    size_t index = (auto_refresh_index + step) % stock_count;
    StockQuote * quote = &quotes[index];
    if(quote_needs_refresh(quote, now, refresh_interval_ms, retry_interval_ms)) {
      if(queue_stock_request(index)) {
        auto_refresh_index = (index + 1) % stock_count;
        printf("Stock auto refresh: %s\r\n", quote->symbol);
        return;
      }
    }
  }
}

const char * Stock_ProxyBaseUrl(void)
{
  return AppConfig_Get()->proxy_base_url;
}
