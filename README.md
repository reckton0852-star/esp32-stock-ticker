# ESP32-S3 LCD Stock Ticker

A compact US stock ticker for the Waveshare `ESP32-S3-LCD-1.47B` board.

It shows one stock at a time on the 1.47 inch LCD, supports button switching, dual-page viewing, automatic symbol rotation, and a local quote proxy for more stable updates.

## Demo

![ESP32 stock ticker demo](images/demo.jpg)

## Features

- Landscape stock card UI for the 1.47 inch LCD
- Dual-page mode:
  - Page 1: quote, change percent, market, update time
  - Page 2: company basics
- Single click: next symbol
- Long press: previous symbol
- Double click: switch between quote page and basics page
- Automatic background refresh every 60 seconds
- Automatic symbol rotation every 12 seconds
- Keeps the last successful price on screen while refreshing
- Up = red box, red LED
- Down = green box, green LED
- WiFi fallback between multiple saved networks
- Local Node.js proxy for stable Finnhub access

## Current Symbols

- WDC
- MU
- AAPL
- NVDA
- AVGO
- TSM

## Page 2 Basics

The second page currently shows:

- Industry
- Country
- Market Cap
- Shares Outstanding
- IPO Date

## Hardware

- Waveshare ESP32-S3-LCD-1.47B
- 1.47 inch LCD
- 2 onboard buttons
- USB-C cable
- WiFi network

## Project Structure

- `LVGL_Arduino.ino`: Arduino entry file
- `LVGL_Example.cpp`: screen layout, page switching, button behavior
- `Stock.cpp` / `Stock.h`: quote refresh and stock data model
- `Secrets.example.h`: template for local WiFi and proxy settings
- `proxy/stock-proxy.js`: local Node.js proxy for Finnhub
- `proxy/start-stock-proxy.cmd`: one-click proxy launcher on Windows

## Why Use a Local Proxy

Direct HTTPS requests from the ESP32 were not stable enough during repeated refreshes.

The local proxy helps by:

- requesting Finnhub from your computer instead of the ESP32
- caching quote and company profile data
- giving the ESP32 a simpler local HTTP endpoint
- reducing network and memory pressure on the board

## Setup

### 1. Arduino IDE

- Install Arduino IDE
- Install the ESP32 board package
- Select board: `ESP32S3 Dev Module`
- Select a large partition scheme such as `Huge APP`

### 2. LVGL Font Settings

In your Arduino LVGL library `lv_conf.h`, enable these fonts:

```cpp
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1
```

The project will automatically prefer larger fonts when they are enabled.

### 3. ESP32 Local Config

Copy:

```text
Secrets.example.h -> Secrets.h
```

Then edit `Secrets.h` and fill in:

- your WiFi credentials
- your proxy address, for example `http://192.168.31.118:8787`

### 4. Proxy Local Config

Copy:

```text
proxy/proxy-secrets.example.json -> proxy/proxy-secrets.json
```

Then edit `proxy/proxy-secrets.json` and fill in:

- your Finnhub API key
- host and port if needed
- refresh interval if needed

### 5. Start the Proxy

On Windows, double-click:

```text
proxy/start-stock-proxy.cmd
```

Default health check:

```text
http://127.0.0.1:8787/health
```

### 6. Upload to the Board

Open:

[LVGL_Arduino.ino](C:/Users/reckt/Documents/ESP32-weixue-1.4inch/official-demo/Arduino/examples/Stock_Ticker/LVGL_Arduino/LVGL_Arduino.ino)

Then compile and upload from Arduino IDE.

## Quick Start

1. Copy `Secrets.example.h` to `Secrets.h`
2. Fill in WiFi and proxy address in `Secrets.h`
3. Copy `proxy/proxy-secrets.example.json` to `proxy/proxy-secrets.json`
4. Fill in your Finnhub API key
5. Start `proxy/start-stock-proxy.cmd`
6. Open `LVGL_Arduino.ino` in Arduino IDE
7. Upload to the board

## Proxy API

The ESP32 reads data from:

```text
GET /quote?symbol=WDC
```

Example response:

```json
{
  "symbol": "WDC",
  "name": "W. Digital",
  "status": "NASDAQ - USD",
  "c": 61.23,
  "d": 0.74,
  "dp": 1.22,
  "updated_at": "21:34",
  "industry": "Technology",
  "country": "US",
  "ipo": "2015-05-06",
  "market_cap": "34.5B",
  "shares_out": "326M",
  "ready": true,
  "error": ""
}
```

## Notes

- `Secrets.h` is ignored by Git and should stay local
- `proxy/proxy-secrets.json` is ignored by Git and should stay local
- if the proxy is not running, the board keeps the last successful quote on screen
- if a refresh fails, the old price is not cleared

## Troubleshooting

### Build cache corruption

If Arduino shows errors such as:

- `file truncated`
- `archive is not an object`

delete:

```text
C:\Users\reckt\AppData\Local\arduino\sketches\
```

Then build again.

### Sketch too big

If you see `text section exceeds available space in board`, change the partition scheme to a larger app layout such as `Huge APP`.

### No stock data

Check these items:

- the proxy window is still running
- the ESP32 and your computer are on the same local network
- `STOCK_PROXY_BASE_URL` matches your computer IP and port
- your Finnhub API key is valid

### WiFi connected but no updates

Open Arduino Serial Monitor at `115200` and check:

- WiFi connection logs
- stock request logs
- HTTP return codes

## Known Issues

- Direct HTTPS access from ESP32 to public stock APIs can still be fragile on small devices
- If the proxy computer is turned off, the board can only show the last cached quote

## Roadmap

- add configurable symbols from a simpler file
- add more screen styles
- show market open/close status
- support additional data sources

## License

Personal project for learning and experimentation.
