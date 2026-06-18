#include "LVGL_Example.h"
#include "Button_Driver.h"
#include "RGB_lamp.h"
#include "Stock.h"

lv_obj_t * tv = NULL;
lv_obj_t *Page_panel[50] = {0};
lv_obj_t *Simulated_panel1[100] = {0};
size_t Simulated_panel1_Size = 0;

static lv_timer_t * app_timer = NULL;
static const uint32_t STOCK_REFRESH_INTERVAL_MS = 60000;
static const uint32_t STOCK_RETRY_INTERVAL_MS = 15000;
static const uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
static uint32_t last_wifi_retry_ms = 0;

static lv_obj_t * symbol_label = NULL;
static lv_obj_t * company_label = NULL;
static lv_obj_t * price_label = NULL;
static lv_obj_t * change_box = NULL;
static lv_obj_t * change_label = NULL;
static lv_obj_t * market_label = NULL;
static lv_obj_t * updated_label = NULL;
static lv_obj_t * page_label = NULL;

static const lv_font_t * font_title(void)
{
#if LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t * font_price(void)
{
#if LV_FONT_MONTSERRAT_36
  return &lv_font_montserrat_36;
#elif LV_FONT_MONTSERRAT_32
  return &lv_font_montserrat_32;
#elif LV_FONT_MONTSERRAT_28
  return &lv_font_montserrat_28;
#elif LV_FONT_MONTSERRAT_24
  return &lv_font_montserrat_24;
#elif LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#else
  return font_title();
#endif
}

static const lv_font_t * font_change(void)
{
#if LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#else
  return font_title();
#endif
}

static void apply_rgb_for_quote(const StockQuote * quote)
{
  if(!quote || (quote->loading && !quote->ready)) {
    Set_Color(0, 56, 96);
    return;
  }

  if(quote->change > 0.001f) {
    Set_Color(96, 16, 0);
  } else if(quote->change < -0.001f) {
    Set_Color(0, 88, 32);
  } else {
    Set_Color(72, 56, 0);
  }
}

static lv_color_t change_color(const StockQuote * quote)
{
  if(!quote) {
    return lv_color_hex(0x475569);
  }
  if(quote->change > 0.001f) {
    return lv_color_hex(0xb91c1c);
  }
  if(quote->change < -0.001f) {
    return lv_color_hex(0x15803d);
  }
  return lv_color_hex(0x334155);
}

static void build_screen(void)
{
  lv_obj_t * screen = lv_scr_act();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x120f12), 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(0xf8fafc), 0);

  lv_obj_t * outer = lv_obj_create(screen);
  lv_obj_set_size(outer, 316, 164);
  lv_obj_align(outer, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(outer, 4, 0);
  lv_obj_set_style_bg_color(outer, lv_color_hex(0x161214), 0);
  lv_obj_set_style_border_width(outer, 0, 0);
  lv_obj_set_style_pad_all(outer, 6, 0);
  lv_obj_clear_flag(outer, LV_OBJ_FLAG_SCROLLABLE);

  symbol_label = lv_label_create(outer);
  lv_obj_set_style_text_font(symbol_label, font_title(), 0);
  lv_obj_set_style_text_color(symbol_label, lv_color_hex(0xfacc15), 0);
  lv_obj_align(symbol_label, LV_ALIGN_TOP_LEFT, 0, 0);

  company_label = lv_label_create(outer);
  lv_obj_set_style_text_color(company_label, lv_color_hex(0xcbd5e1), 0);
  lv_label_set_long_mode(company_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(company_label, 170);
  lv_obj_align(company_label, LV_ALIGN_TOP_LEFT, 70, 2);

  page_label = lv_label_create(outer);
  lv_obj_set_style_text_color(page_label, lv_color_hex(0x94a3b8), 0);
  lv_obj_align(page_label, LV_ALIGN_TOP_RIGHT, 0, 2);

  lv_obj_t * inner = lv_obj_create(outer);
  lv_obj_set_size(inner, 304, 116);
  lv_obj_align(inner, LV_ALIGN_TOP_MID, 0, 28);
  lv_obj_set_style_radius(inner, 4, 0);
  lv_obj_set_style_bg_color(inner, lv_color_hex(0xf8fafc), 0);
  lv_obj_set_style_border_width(inner, 0, 0);
  lv_obj_set_style_pad_all(inner, 7, 0);
  lv_obj_clear_flag(inner, LV_OBJ_FLAG_SCROLLABLE);

  price_label = lv_label_create(inner);
  lv_obj_set_style_text_color(price_label, lv_color_hex(0x0f172a), 0);
  lv_obj_set_style_text_font(price_label, font_price(), 0);
  lv_label_set_long_mode(price_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(price_label, 210);
  lv_obj_align(price_label, LV_ALIGN_LEFT_MID, 0, -12);

  change_box = lv_obj_create(inner);
  lv_obj_set_size(change_box, 82, 42);
  lv_obj_align(change_box, LV_ALIGN_RIGHT_MID, 0, -12);
  lv_obj_set_style_radius(change_box, 4, 0);
  lv_obj_set_style_border_width(change_box, 0, 0);
  lv_obj_clear_flag(change_box, LV_OBJ_FLAG_SCROLLABLE);

  change_label = lv_label_create(change_box);
  lv_obj_set_style_text_font(change_label, font_change(), 0);
  lv_obj_align(change_label, LV_ALIGN_CENTER, 0, 0);

  market_label = lv_label_create(inner);
  lv_obj_set_style_text_color(market_label, lv_color_hex(0x334155), 0);
  lv_obj_align(market_label, LV_ALIGN_BOTTOM_LEFT, 0, 1);

  updated_label = lv_label_create(inner);
  lv_obj_set_style_text_color(updated_label, lv_color_hex(0x334155), 0);
  lv_obj_align(updated_label, LV_ALIGN_BOTTOM_RIGHT, 0, 1);
}

static void update_screen(void)
{
  const StockQuote * quote = Stock_CurrentQuote();
  char buffer[64];

  if(!quote) {
    return;
  }

  lv_label_set_text(symbol_label, quote->symbol);
  lv_label_set_text(company_label, quote->name);
  snprintf(buffer, sizeof(buffer), "%u/%u", (unsigned)(Stock_CurrentIndex() + 1), (unsigned)STOCK_COUNT);
  lv_label_set_text(page_label, buffer);

  if(quote->ready) {
    snprintf(buffer, sizeof(buffer), "$%.2f", quote->price);
    lv_label_set_text(price_label, buffer);

    snprintf(buffer, sizeof(buffer), "%+.1f%%", quote->change_percent);
    lv_label_set_text(change_label, buffer);

    lv_obj_set_style_bg_color(change_box, change_color(quote), 0);
    lv_obj_set_style_text_color(change_label, lv_color_hex(0xffffff), 0);

    lv_label_set_text(market_label, quote->status);
    lv_label_set_text(updated_label, quote->updated_at);
  } else {
    lv_label_set_text(price_label, "--.--");
    lv_label_set_text(change_label, "--");
    lv_obj_set_style_bg_color(change_box, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_color(change_label, lv_color_hex(0xffffff), 0);
    lv_label_set_text(market_label, "NASDAQ - USD");
    lv_label_set_text(updated_label, "--:--");
  }

  apply_rgb_for_quote(quote);
}

static void handle_button_action(void)
{
  if(BOOT_KEY_State == Click) {
    Stock_Next();
    update_screen();
    Stock_RequestCurrentIfStale(STOCK_REFRESH_INTERVAL_MS);
  } else if(BOOT_KEY_State == DoubleClick) {
    Stock_RequestCurrent();
  } else if(BOOT_KEY_State == LongPressStart) {
    Stock_Previous();
    update_screen();
    Stock_RequestCurrentIfStale(STOCK_REFRESH_INTERVAL_MS);
  }

  BOOT_KEY_State = None;
}

void IRAM_ATTR example1_increase_lvgl_tick(lv_timer_t * t)
{
  (void)t;
  handle_button_action();

  bool wifi_ready = WIFI_Connection;
  if(!wifi_ready) {
    uint32_t now = millis();
    if(now - last_wifi_retry_ms > WIFI_RETRY_INTERVAL_MS) {
      last_wifi_retry_ms = now;
      wifi_ready = Wireless_EnsureConnected();
    }
  }

  if(wifi_ready) {
    Stock_ServiceAutoRefresh(STOCK_REFRESH_INTERVAL_MS, STOCK_RETRY_INTERVAL_MS);
  }

  update_screen();
}

void Lvgl_Example1(void)
{
  build_screen();
  update_screen();
  app_timer = lv_timer_create(example1_increase_lvgl_tick, 250, NULL);
}

void Lvgl_Example1_close(void)
{
  if(app_timer) {
    lv_timer_del(app_timer);
    app_timer = NULL;
  }
  lv_obj_clean(lv_scr_act());
}

void Backlight_adjustment_event_cb(lv_event_t * e)
{
  (void)e;
}

void LVGL_Backlight_adjustment(uint8_t Backlight)
{
  Set_Backlight(Backlight);
}
