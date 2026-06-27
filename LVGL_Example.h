#pragma once

#include "LVGL_Driver.h"
#include "SD_Card.h"
#include "Wireless.h"
#include "Display_ST7789.h"
#include "Gyro_QMI8658.h"
#include "BAT_Driver.h"

#define EXAMPLE1_LVGL_TICK_PERIOD_MS  1000

extern lv_obj_t * tv;
extern lv_obj_t *Page_panel[50];
extern lv_obj_t *Simulated_panel1[100];
extern size_t Simulated_panel1_Size;

void Backlight_adjustment_event_cb(lv_event_t * e);

void Lvgl_Example1(void);
void Lvgl_Example1_close(void);
void Lvgl_ShowBootSplash(void);
void Lvgl_ShowWifiScanStatus(const char * line1, const char * line2, const char * line3);
void Lvgl_ShowSetupMode(const char * ap_ssid, const char * ap_ip);
void LVGL_Backlight_adjustment(uint8_t Backlight);
