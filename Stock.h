#pragma once

#include <Arduino.h>

typedef struct {
  const char * symbol;
  const char * name;
  float price;
  float change;
  float change_percent;
  float open;
  float high;
  float low;
  float previous_close;
  bool ready;
  bool loading;
  bool profile_ready;
  bool trading_ready;
  char status[32];
  char updated_at[24];
  char industry[32];
  char country[20];
  char ipo[16];
  char market_cap[16];
  char shares_out[16];
  uint32_t last_fetch_ms;
  uint32_t last_attempt_ms;
  float daily_history[30];
  uint8_t daily_history_count;
} StockQuote;

extern const size_t STOCK_COUNT;
static const size_t STOCK_TREND_POINTS = 24;
static const size_t STOCK_DAILY_POINTS = 30;

void Stock_Init(void);
void Stock_Next(void);
void Stock_Previous(void);
size_t Stock_CurrentIndex(void);
size_t Stock_ActiveCount(void);
const StockQuote * Stock_CurrentQuote(void);
size_t Stock_GetCurrentTrend(float * values, size_t max_values);
bool Stock_IsFxMode(void);
void Stock_RequestCurrent(void);
bool Stock_RequestCurrentIfStale(uint32_t min_interval_ms);
void Stock_ServiceAutoRefresh(uint32_t refresh_interval_ms, uint32_t retry_interval_ms);
const char * Stock_ProxyBaseUrl(void);
