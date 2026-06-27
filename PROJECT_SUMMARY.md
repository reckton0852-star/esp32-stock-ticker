# ESP32 Stock Ticker Project Summary

## 1. Project Goal

This project turns the Waveshare `ESP32-S3-LCD-1.47B` into a small standalone stock display.

Main goals:

- show US stock prices on a 1.47 inch LCD
- switch symbols with onboard buttons
- refresh stock data automatically
- survive WiFi changes without editing source code every time
- feel closer to a real product instead of a demo

---

## 2. Hardware Used

- Waveshare `ESP32-S3-LCD-1.47B`
- 1.47 inch LCD
- 2 onboard buttons
- onboard RGB LED
- WiFi connection

---

## 3. Core Functions

Current version supports:

- landscape stock card UI
- quote page + basics page
- automatic stock refresh
- automatic symbol rotation
- button control:
  - single click: next symbol
  - long press: previous symbol
  - double click: switch page
- WiFi bootstrap flow on startup
- setup mode hotspot
- browser-based configuration page

---

## 4. Current Boot Flow

After power-on, the device now works like this:

1. show boot splash for 3 seconds
2. show WiFi bootstrap page
3. scan and try saved WiFi credentials
4. if WiFi connects:
   - sync time
   - enter stock UI
5. if WiFi fails:
   - enter setup mode
   - create hotspot `Reckton-Stock-Setup`
   - open config page at `http://192.168.4.1`

This is one of the biggest improvements in `v2.0.0`.

---

## 5. Main File Structure

### Arduino entry

- `LVGL_Arduino.ino`

Responsible for:

- startup flow
- boot splash
- WiFi bootstrap
- setup mode entry
- background driver task

### UI layer

- `LVGL_Example.cpp`
- `LVGL_Example.h`

Responsible for:

- stock page layout
- basics page layout
- boot splash screen
- WiFi bootstrap status screen
- setup mode screen
- button event handling

### Stock data layer

- `Stock.cpp`
- `Stock.h`

Responsible for:

- stock symbol list
- quote model
- refresh queue
- HTTP requests to proxy
- current symbol state

### WiFi layer

- `Wireless.cpp`
- `Wireless.h`

Responsible for:

- connecting saved WiFi networks
- WiFi scanning
- time synchronization
- guarding against duplicate connection flows

### Config layer

- `AppConfig.cpp`
- `AppConfig.h`

Responsible for:

- persistent runtime settings
- WiFi credentials
- proxy URL
- stock symbols
- brightness
- refresh interval
- rotate interval

### Setup portal

- `SetupPortal.cpp`
- `SetupPortal.h`

Responsible for:

- hotspot mode
- local web configuration page
- save + reboot flow

### Display driver

- `Display_ST7789.cpp`
- `Display_ST7789.h`
- `LVGL_Driver.cpp`
- `LVGL_Driver.h`

Responsible for:

- LCD initialization
- LVGL display refresh
- backlight PWM

---

## 6. Proxy Architecture

The ESP32 does not request stock APIs directly in the final architecture.

Instead it uses a proxy:

- local Node.js proxy
- or Cloudflare Worker proxy

Why:

- direct HTTPS on ESP32 was unstable
- repeated requests caused memory/network issues
- proxy makes the board side simpler and more reliable

Current ESP32 request style:

```text
GET /quote?symbol=AAPL
```

Typical proxy return fields:

- symbol
- name
- status
- current price
- price change
- percent change
- update time
- company basics

---

## 7. Major Problems Solved During Development

### 7.1 Build issues

Problems seen:

- missing `lvgl.h`
- missing `SD_Card.h`
- sketch too big
- Arduino cache corruption (`file truncated`)

Typical fixes:

- install required libraries
- use larger partition scheme such as `Huge APP`
- clear Arduino build cache when needed

### 7.2 Screen orientation / display issues

Problems seen:

- portrait instead of landscape
- mirrored screen
- garbled startup display

Fix direction:

- adjust display init path
- avoid showing incomplete content before LVGL is ready
- control backlight timing more carefully

### 7.3 Boot loop / black screen / flicker

Problems seen:

- splash page showing repeatedly
- brief black screen then restart
- visible screen flicker

Actual causes included:

- task stack overflow
- unsafe LCD transfer buffer usage
- backlight PWM frequency too low

Important fixes:

- increased background task stack
- removed unsafe temporary LCD read buffer during writes
- raised PWM backlight frequency

### 7.4 WiFi connection conflicts

Problems seen:

- repeated WiFi attempts overlapping each other
- `sta is connecting, cannot set config`
- unstable fallback between saved SSIDs

Fix direction:

- add connection-in-progress guard
- use one WiFi connection flow at a time
- reset STA mode more cleanly between SSIDs

### 7.5 Network stack crash

Problem seen:

- TCP/IP core assertion crash

Root cause:

- multiple tasks touched networking at the same time

Fix direction:

- simplify network ownership
- remove duplicate network activity from background task

---

## 8. UI Design Decisions

The project gradually moved from a demo style to a product-like style.

Current UI direction:

- cold, clean terminal-like palette
- compact landscape layout
- strong contrast for price information
- startup flow that feels like a device, not a debug app

Design choices made:

- boot splash with `RECKTON / Stock Terminal`
- WiFi bootstrap page
- setup mode page
- stock page with large price
- color rule:
  - up = red
  - down = green

---

## 9. Setup Mode Design

Setup mode exists to reduce hardcoded configuration pain.

Hotspot:

- `Reckton-Stock-Setup`

Config page:

- `http://192.168.4.1`

User can configure:

- WiFi 1
- WiFi 2
- WiFi 3
- proxy base URL
- stock symbols
- brightness
- refresh seconds
- rotate seconds

This is one of the most important steps that makes the project reusable.

---

## 10. Current Stable Version

Current milestone release:

- `v2.0.0`

This version includes:

- stable boot flow
- setup portal
- WiFi bootstrap
- stock display stability fixes
- product-style startup experience

---

## 11. What Makes This Project Reusable Now

Compared with the earliest version, this project is now much easier to reuse because:

- WiFi does not need to be hardcoded every time
- stock symbols can be changed through setup page
- proxy URL can be changed without editing source
- failed WiFi can fall back to setup mode
- startup behavior is predictable

This means the project is now closer to a small product platform than a one-off experiment.

---

## 12. Good Topics for Future Learning

If you want to keep learning from this project, these are good next topics:

### Product / UX

- better setup page design
- stock list ordering UI
- add delete / add symbol controls
- improve page transitions

### Embedded / software

- safer memory ownership in `Stock.cpp`
- reduce heap fragmentation
- add watchdog-aware long task design
- reduce blocking delays in boot flow

### Connectivity

- reconnect behavior after WiFi drop
- offline cache and last-update aging
- stronger captive portal behavior

### Hardware

- 3D printable enclosure
- thermal optimization
- power saving tuning

---

## 13. Personal Study Notes

This project is a very good learning case because it touches several layers at once:

- Arduino / ESP32 basics
- LVGL UI
- WiFi and networking
- HTTP proxy architecture
- persistent local config
- small-device UX design
- debugging crashes on embedded devices

The biggest lesson from this project is:

> making the feature work is only the first step; making the device boot reliably, recover from failure, and be configurable is what starts turning it into a product.

---

## 14. Related Files

- Main program:
  - [LVGL_Arduino.ino](C:/Users/reckt/Documents/ESP32-weixue-1.4inch/official-demo/Arduino/examples/Stock_Ticker/LVGL_Arduino/LVGL_Arduino.ino)
- UI:
  - [LVGL_Example.cpp](C:/Users/reckt/Documents/ESP32-weixue-1.4inch/official-demo/Arduino/examples/Stock_Ticker/LVGL_Arduino/LVGL_Example.cpp)
- WiFi:
  - [Wireless.cpp](C:/Users/reckt/Documents/ESP32-weixue-1.4inch/official-demo/Arduino/examples/Stock_Ticker/LVGL_Arduino/Wireless.cpp)
- Stock logic:
  - [Stock.cpp](C:/Users/reckt/Documents/ESP32-weixue-1.4inch/official-demo/Arduino/examples/Stock_Ticker/LVGL_Arduino/Stock.cpp)
- Config system:
  - [AppConfig.cpp](C:/Users/reckt/Documents/ESP32-weixue-1.4inch/official-demo/Arduino/examples/Stock_Ticker/LVGL_Arduino/AppConfig.cpp)
- Setup portal:
  - [SetupPortal.cpp](C:/Users/reckt/Documents/ESP32-weixue-1.4inch/official-demo/Arduino/examples/Stock_Ticker/LVGL_Arduino/SetupPortal.cpp)

