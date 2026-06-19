#pragma once

#include <Arduino.h>

typedef struct {
  const char * symbol;
  const char * name;
  float price;
  float change;
  float change_percent;
  bool ready;
  bool loading;
  bool profile_ready;
  char status[32];
  char updated_at[24];
  char industry[32];
  char country[20];
  char ipo[16];
  char market_cap[16];
  char shares_out[16];
  uint32_t last_fetch_ms;
} StockQuote;

extern const size_t STOCK_COUNT;

void Stock_Init(void);
void Stock_Next(void);
void Stock_Previous(void);
size_t Stock_CurrentIndex(void);
const StockQuote * Stock_CurrentQuote(void);
void Stock_RequestCurrent(void);
bool Stock_RequestCurrentIfStale(uint32_t min_interval_ms);
void Stock_ServiceAutoRefresh(uint32_t refresh_interval_ms, uint32_t retry_interval_ms);
