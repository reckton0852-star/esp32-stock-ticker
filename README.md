# ESP32-S3 LCD Stock Ticker

Stock ticker project for the Waveshare ESP32-S3-LCD-1.47B board.

## Demo

![ESP32 stock ticker demo](images/demo.jpg)

## Features

- Landscape stock quote UI for the 1.47 inch LCD
- Button switching between symbols
- Background refresh every 60 seconds
- Keeps the last successful price on screen while refreshing
- Red for up, green for down
- WiFi fallback between home and office networks
- Local stock proxy support for stable quote updates

## Symbols

- WDC
- MU
- AAPL
- NVDA
- AVGO
- TSM

## Setup

1. Copy `Secrets.example.h` to `Secrets.h`.
2. Fill in your WiFi credentials and local proxy address.
3. Copy `proxy/proxy-secrets.example.json` to `proxy/proxy-secrets.json`.
4. Fill in your Finnhub API key in `proxy/proxy-secrets.json`.
5. Start the local proxy with `proxy/start-stock-proxy.cmd`.
6. Open `LVGL_Arduino.ino` in Arduino IDE.
7. Select board `ESP32S3 Dev Module`.
8. Use a large app partition scheme, such as `Huge APP`.
9. Upload to the board.

## LVGL Font Note

For the larger price and percent fonts, enable these in your Arduino LVGL
library `lv_conf.h`:

```cpp
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
```

If these fonts are not enabled, the project still compiles, but the UI uses
smaller fallback fonts.

## Secrets

`Secrets.h` is ignored by Git and should not be uploaded.

Use `Secrets.example.h` as the template for other devices or machines.

`proxy/proxy-secrets.json` is also ignored by Git and should stay local.
