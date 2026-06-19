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
static const uint32_t AUTO_SYMBOL_INTERVAL_MS = 12000;
static uint32_t last_wifi_retry_ms = 0;
static uint32_t last_auto_symbol_ms = 0;
static uint8_t current_view_mode = 0;

static void update_screen(void);

static lv_obj_t * symbol_label = NULL;
static lv_obj_t * company_label = NULL;
static lv_obj_t * page_label = NULL;
static lv_obj_t * content_panel = NULL;

static lv_obj_t * price_page = NULL;
static lv_obj_t * price_label = NULL;
static lv_obj_t * change_box = NULL;
static lv_obj_t * change_box_bg = NULL;
static lv_obj_t * change_label = NULL;
static lv_obj_t * market_label = NULL;
static lv_obj_t * updated_label = NULL;

static lv_obj_t * profile_page = NULL;
static lv_obj_t * basics_title_label = NULL;
static lv_obj_t * basics_updated_label = NULL;
static lv_obj_t * industry_value_label = NULL;
static lv_obj_t * country_value_label = NULL;
static lv_obj_t * market_cap_value_label = NULL;
static lv_obj_t * shares_out_value_label = NULL;
static lv_obj_t * ipo_value_label = NULL;

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
#if LV_FONT_MONTSERRAT_48
  return &lv_font_montserrat_48;
#elif LV_FONT_MONTSERRAT_40
  return &lv_font_montserrat_40;
#elif LV_FONT_MONTSERRAT_36
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
#if LV_FONT_MONTSERRAT_24
  return &lv_font_montserrat_24;
#elif LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#else
  return font_title();
#endif
}

static const lv_font_t * font_meta(void)
{
#if LV_FONT_MONTSERRAT_14
  return &lv_font_montserrat_14;
#elif LV_FONT_MONTSERRAT_12
  return &lv_font_montserrat_12;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t * font_meta_value(void)
{
#if LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#elif LV_FONT_MONTSERRAT_14
  return &lv_font_montserrat_14;
#else
  return LV_FONT_DEFAULT;
#endif
}

static void apply_rgb_for_quote(const StockQuote * quote)
{
  if(!quote || (quote->loading && !quote->ready)) {
    Set_Color(0, 56, 96);
    return;
  }

  if(quote->change > 0.001f) {
    Set_Color(180, 0, 0);
  } else if(quote->change < -0.001f) {
    Set_Color(0, 160, 0);
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
    return lv_color_hex(0x2626dc);
  }
  if(quote->change < -0.001f) {
    return lv_color_hex(0x16a34a);
  }
  return lv_color_hex(0x334155);
}

static void apply_change_box_color(const StockQuote * quote)
{
  lv_color_t color = change_color(quote);
  lv_obj_set_style_bg_color(change_box_bg, color, 0);
  lv_obj_set_style_bg_grad_color(change_box_bg, color, 0);
  lv_obj_set_style_bg_grad_dir(change_box_bg, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(change_box_bg, LV_OPA_COVER, 0);
}

static void mark_user_activity(void)
{
  uint32_t now = millis();
  last_auto_symbol_ms = now;
}

static void auto_advance_symbol_if_needed(void)
{
  uint32_t now = millis();
  if(now - last_auto_symbol_ms < AUTO_SYMBOL_INTERVAL_MS) {
    return;
  }

  last_auto_symbol_ms = now;
  Stock_Next();
  update_screen();
  Stock_RequestCurrentIfStale(STOCK_REFRESH_INTERVAL_MS);
}

static lv_obj_t * create_meta_pair(lv_obj_t * parent, const char * title, lv_coord_t x, lv_coord_t y, lv_obj_t ** value_label, lv_coord_t width)
{
  lv_obj_t * title_label = lv_label_create(parent);
  lv_obj_set_style_text_color(title_label, lv_color_hex(0x64748b), 0);
  lv_obj_set_style_text_font(title_label, font_meta(), 0);
  lv_label_set_text(title_label, title);
  lv_obj_set_pos(title_label, x, y);

  *value_label = lv_label_create(parent);
  lv_obj_set_style_text_color(*value_label, lv_color_hex(0x0f172a), 0);
  lv_obj_set_style_text_font(*value_label, font_meta_value(), 0);
  lv_label_set_long_mode(*value_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(*value_label, width);
  lv_obj_set_pos(*value_label, x, y + 16);
  return *value_label;
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

  content_panel = lv_obj_create(outer);
  lv_obj_set_size(content_panel, 304, 116);
  lv_obj_align(content_panel, LV_ALIGN_TOP_MID, 0, 28);
  lv_obj_set_style_radius(content_panel, 4, 0);
  lv_obj_set_style_bg_color(content_panel, lv_color_hex(0xf8fafc), 0);
  lv_obj_set_style_border_width(content_panel, 0, 0);
  lv_obj_set_style_pad_all(content_panel, 7, 0);
  lv_obj_clear_flag(content_panel, LV_OBJ_FLAG_SCROLLABLE);

  price_page = lv_obj_create(content_panel);
  lv_obj_set_size(price_page, 304, 116);
  lv_obj_align(price_page, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(price_page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(price_page, 0, 0);
  lv_obj_set_style_pad_all(price_page, 0, 0);
  lv_obj_clear_flag(price_page, LV_OBJ_FLAG_SCROLLABLE);

  price_label = lv_label_create(price_page);
  lv_obj_set_style_text_color(price_label, lv_color_hex(0x0f172a), 0);
  lv_obj_set_style_text_font(price_label, font_price(), 0);
  lv_label_set_long_mode(price_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(price_label, 216);
  lv_obj_align(price_label, LV_ALIGN_LEFT_MID, -2, -14);

  change_box = lv_obj_create(price_page);
  lv_obj_remove_style_all(change_box);
  lv_obj_set_size(change_box, 88, 46);
  lv_obj_align(change_box, LV_ALIGN_RIGHT_MID, -8, -12);
  lv_obj_set_style_bg_opa(change_box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(change_box, 0, 0);
  lv_obj_set_style_pad_all(change_box, 0, 0);
  lv_obj_set_style_shadow_width(change_box, 0, 0);
  lv_obj_clear_flag(change_box, LV_OBJ_FLAG_SCROLLABLE);

  change_box_bg = lv_obj_create(change_box);
  lv_obj_remove_style_all(change_box_bg);
  lv_obj_set_size(change_box_bg, 88, 46);
  lv_obj_align(change_box_bg, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(change_box_bg, 4, 0);
  lv_obj_set_style_border_width(change_box_bg, 0, 0);
  lv_obj_set_style_bg_opa(change_box_bg, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(change_box_bg, 0, 0);
  lv_obj_clear_flag(change_box_bg, LV_OBJ_FLAG_SCROLLABLE);

  change_label = lv_label_create(change_box);
  lv_obj_set_style_text_font(change_label, font_change(), 0);
  lv_obj_align(change_label, LV_ALIGN_CENTER, 0, 0);

  market_label = lv_label_create(price_page);
  lv_obj_set_style_text_color(market_label, lv_color_hex(0x334155), 0);
  lv_obj_align(market_label, LV_ALIGN_BOTTOM_LEFT, 0, 1);

  updated_label = lv_label_create(price_page);
  lv_obj_set_style_text_color(updated_label, lv_color_hex(0x334155), 0);
  lv_obj_align(updated_label, LV_ALIGN_BOTTOM_RIGHT, -6, -2);

  profile_page = lv_obj_create(content_panel);
  lv_obj_set_size(profile_page, 304, 116);
  lv_obj_align(profile_page, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(profile_page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(profile_page, 0, 0);
  lv_obj_set_style_pad_all(profile_page, 0, 0);
  lv_obj_clear_flag(profile_page, LV_OBJ_FLAG_SCROLLABLE);

  basics_title_label = lv_label_create(profile_page);
  lv_obj_set_style_text_color(basics_title_label, lv_color_hex(0x0f172a), 0);
  lv_obj_set_style_text_font(basics_title_label, font_title(), 0);
  lv_obj_align(basics_title_label, LV_ALIGN_TOP_LEFT, 0, 0);

  basics_updated_label = lv_label_create(profile_page);
  lv_obj_set_style_text_color(basics_updated_label, lv_color_hex(0x64748b), 0);
  lv_obj_set_style_text_font(basics_updated_label, font_meta(), 0);
  lv_obj_align(basics_updated_label, LV_ALIGN_TOP_RIGHT, 0, 1);

  create_meta_pair(profile_page, "Industry", 0, 24, &industry_value_label, 138);
  create_meta_pair(profile_page, "Country", 154, 24, &country_value_label, 130);
  create_meta_pair(profile_page, "Mkt Cap", 0, 62, &market_cap_value_label, 138);
  create_meta_pair(profile_page, "Shares", 100, 62, &shares_out_value_label, 86);
  create_meta_pair(profile_page, "IPO", 202, 62, &ipo_value_label, 82);
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
  snprintf(buffer, sizeof(buffer), "%u/%u P%u",
    (unsigned)(Stock_CurrentIndex() + 1),
    (unsigned)STOCK_COUNT,
    (unsigned)(current_view_mode + 1));
  lv_label_set_text(page_label, buffer);

  if(current_view_mode == 0) {
    lv_obj_clear_flag(price_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(profile_page, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(price_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(profile_page, LV_OBJ_FLAG_HIDDEN);
  }

  if(quote->ready) {
    snprintf(buffer, sizeof(buffer), "$%.2f", quote->price);
    lv_label_set_text(price_label, buffer);

    snprintf(buffer, sizeof(buffer), "%+.1f%%", quote->change_percent);
    lv_label_set_text(change_label, buffer);

    apply_change_box_color(quote);
    lv_obj_set_style_text_color(change_label, lv_color_hex(0xffffff), 0);

    lv_label_set_text(market_label, quote->status);
    lv_label_set_text(updated_label, quote->updated_at);
  } else {
    lv_label_set_text(price_label, "--.--");
    lv_label_set_text(change_label, "--");
    apply_change_box_color(NULL);
    lv_obj_set_style_text_color(change_label, lv_color_hex(0xffffff), 0);
    lv_label_set_text(market_label, "NASDAQ - USD");
    lv_label_set_text(updated_label, "--:--");
  }

  lv_label_set_text(basics_title_label, "Company Basics");
  if(quote->profile_ready) {
    lv_label_set_text(industry_value_label, quote->industry);
    lv_label_set_text(country_value_label, quote->country);
    lv_label_set_text(market_cap_value_label, quote->market_cap);
    lv_label_set_text(shares_out_value_label, quote->shares_out);
    lv_label_set_text(ipo_value_label, quote->ipo);
  } else {
    lv_label_set_text(industry_value_label, "-");
    lv_label_set_text(country_value_label, "-");
    lv_label_set_text(market_cap_value_label, "-");
    lv_label_set_text(shares_out_value_label, "-");
    lv_label_set_text(ipo_value_label, "-");
  }
  snprintf(buffer, sizeof(buffer), "Quote %s", quote->updated_at);
  lv_label_set_text(basics_updated_label, buffer);

  apply_rgb_for_quote(quote);
}

static void handle_button_action(void)
{
  if(BOOT_KEY_State == Click) {
    mark_user_activity();
    Stock_Next();
    update_screen();
    Stock_RequestCurrentIfStale(STOCK_REFRESH_INTERVAL_MS);
  } else if(BOOT_KEY_State == DoubleClick) {
    mark_user_activity();
    current_view_mode = (current_view_mode + 1) % 2;
    update_screen();
    Stock_RequestCurrent();
  } else if(BOOT_KEY_State == LongPressStart) {
    mark_user_activity();
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

  auto_advance_symbol_if_needed();
  update_screen();
}

void Lvgl_Example1(void)
{
  current_view_mode = 0;
  last_auto_symbol_ms = millis();
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
