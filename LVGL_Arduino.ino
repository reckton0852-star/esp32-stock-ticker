/**
 ******************************************************************************
 * @file     LVGL_Arduino.ino
 * @author   Yongqin Ou
 * @version  V1.0
 * @date     2024-10-30
 * @brief    Setup experiment for multiple modules
 * @license  MIT
 * @copyright Copyright (c) 2024, Waveshare
 ******************************************************************************
 * 
 * Experiment Objective: Learn how to set up and use multiple modules including SD card, display, LVGL, wireless, and RGB lamp.
 *
 * Hardware Resources and Pin Assignment: 
 * 1. SD Card Interface --> As configured in SD_Card.h.
 * 2. Display Interface --> As configured in Display_ST7789.h.
 * 3. Wireless Module Interface --> As configured in Wireless.h.
 * 4. RGB Lamp Interface --> As configured in RGB_lamp.h.
 *
 * Experiment Phenomenon:
 * 1. Runs various tests and initializations for different modules.
 * 2. Displays LVGL examples on the display.
 * 3. Continuously runs loops for timer, RGB lamp, and other tasks.
 * 
 * Notes:
 * None
 * 
 ******************************************************************************
 * 
 * Development Platform: ESP32
 * Support Forum: service.waveshare.com
 * Company Website: www.waveshare.com
 *
 ******************************************************************************
 */
#include "SD_Card.h"
#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "LVGL_Example.h"
#include "Wireless.h"
#include "Stock.h"
#include "AppConfig.h"
#include "SetupPortal.h"
#include "RGB_lamp.h"
#include "I2C_Driver.h"
#include "Gyro_QMI8658.h"
#include "Button_Driver.h"
#include "BAT_Driver.h"

static bool setup_mode = false;
static bool stock_ui_started = false;
static uint32_t wifi_offline_since_ms = 0;
static const uint32_t AUTO_SETUP_WIFI_TIMEOUT_MS = 45000;
static uint32_t setup_button_hold_since_ms = 0;
static bool setup_button_armed = true;
static uint32_t last_time_sync_attempt_ms = 0;
static const uint32_t TIME_SYNC_RETRY_MS = 30000;

static void show_boot_splash(uint32_t duration_ms)
{
  uint32_t started = millis();
  printf("Boot splash start\r\n");
  Lvgl_ShowBootSplash();
  for(uint8_t i = 0; i < 6; i++) {
    Timer_Loop();
    delay(5);
  }
  Set_Backlight(AppConfig_Get()->brightness);
  while(millis() - started < duration_ms) {
    Timer_Loop();
    delay(5);
  }
  printf("Boot splash end\r\n");
}

static void wifi_status_screen_callback(const char * line1, const char * line2, const char * line3)
{
  Lvgl_ShowWifiScanStatus(line1, line2, line3);
  for(uint8_t i = 0; i < 3; i++) {
    Timer_Loop();
    delay(5);
  }
}

void Driver_Loop(void *parameter)
{
  while(1)
  {
    BAT_Get_Volts();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

static void enter_setup_mode(void)
{
  if(setup_mode) {
    return;
  }

  printf("Entering setup mode\r\n");
  setup_mode = true;
  stock_ui_started = false;
  Lvgl_Example1_close();
  SetupPortal_Begin();
  Lvgl_ShowSetupMode(SetupPortal_ApSsid(), SetupPortal_ApIp());
}

static void check_setup_button_runtime(void)
{
  const uint32_t hold_required_ms = 1200;
  bool pressed = digitalRead(BOOT_KEY_PIN) == LOW;

  if(pressed) {
    if(setup_button_hold_since_ms == 0) {
      setup_button_hold_since_ms = millis();
    }

    if(setup_button_armed && millis() - setup_button_hold_since_ms >= hold_required_ms) {
      printf("Setup mode requested by BOOT button\r\n");
      setup_button_armed = false;
      enter_setup_mode();
    }
    return;
  }

  setup_button_hold_since_ms = 0;
  setup_button_armed = true;
}

void setup()
{
  printf("Stock Ticker boot\r\n");
  pinMode(BOOT_KEY_PIN, INPUT_PULLUP);

  AppConfig_Init();
  setup_mode = !AppConfig_HasSavedConfig();

  Flash_test();
  BAT_Init();
  Button_Init();
  I2C_Init();
  QMI8658_Init(); 
  Set_Color(0, 24, 40);
  SD_Init();         
  LCD_Backlight = 0;
  LCD_Init();
  Lvgl_Init();
  printf("LVGL init done\r\n");

  if(setup_mode) {
    printf("Setup mode selected\r\n");
    Set_Backlight(AppConfig_Get()->brightness);
    SetupPortal_Begin();
    Lvgl_ShowSetupMode(SetupPortal_ApSsid(), SetupPortal_ApIp());
    printf("Setup mode UI ready\r\n");
  } else {
    show_boot_splash(3000);
    printf("WiFi bootstrap start\r\n");
    bool wifi_ready = Wireless_ConnectSavedWithStatus(wifi_status_screen_callback);
    printf("WiFi bootstrap done: %s\r\n", wifi_ready ? "connected" : "failed");

    if(wifi_ready) {
      Wireless_SyncTimeNow();
      printf("Stock init start\r\n");
      Stock_Init();
      printf("Stock init done\r\n");
      Lvgl_Example1();
      printf("Stock UI ready\r\n");
      stock_ui_started = true;
    } else {
      setup_mode = true;
      SetupPortal_Begin();
      Lvgl_ShowSetupMode(SetupPortal_ApSsid(), SetupPortal_ApIp());
      printf("Setup mode UI ready after WiFi failure\r\n");
    }
  }
  // lv_demo_widgets();               
  // lv_demo_benchmark();          
  // lv_demo_keypad_encoder();     
  // lv_demo_music();  
  // lv_demo_stress();   

  xTaskCreatePinnedToCore(
    Driver_Loop,     
    "Other Driver task",   
    8192,                
    NULL,                 
    3,                    
    NULL,                
    0                    
  );
  printf("Driver task created\r\n");
}

void loop()
{
  if(!setup_mode) {
    check_setup_button_runtime();
  }

  if(SetupPortal_IsActive()) {
    SetupPortal_Handle();
    if(SetupPortal_ShouldReboot()) {
      delay(1200);
      ESP.restart();
    }
  } else if(stock_ui_started) {
    uint32_t now = millis();

    if(WIFI_Connection) {
      wifi_offline_since_ms = 0;
      if(!TIME_Synced && (last_time_sync_attempt_ms == 0 || now - last_time_sync_attempt_ms > TIME_SYNC_RETRY_MS)) {
        last_time_sync_attempt_ms = now;
        Wireless_SyncTimeNow();
      }
    } else {
      if(wifi_offline_since_ms == 0) {
        wifi_offline_since_ms = now;
      } else if(now - wifi_offline_since_ms > AUTO_SETUP_WIFI_TIMEOUT_MS) {
        enter_setup_mode();
      }
    }
  } else {
    wifi_offline_since_ms = 0;
  }
  Timer_Loop();
  delay(5);
}
