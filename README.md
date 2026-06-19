# ESP32-S3 LCD Stock Ticker

A small stock ticker project for the Waveshare `ESP32-S3-LCD-1.47B` board.

It displays US stock quotes on the 1.47 inch LCD in a compact landscape layout and switches symbols with the onboard buttons.

## Demo

![ESP32 stock ticker demo](images/demo.jpg)

## What This Project Does

- Shows one stock card at a time on the LCD
- Uses the left/right buttons to switch symbols
- Refreshes quotes in the background every 60 seconds
- Keeps the last successful price on screen while the next refresh is running
- Uses `red` for up and `green` for down
- Supports fallback WiFi credentials
- Uses a local proxy on your computer for more stable Finnhub access

## Current Symbols

- WDC
- MU
- AAPL
- NVDA
- AVGO
- TSM

## Hardware

- Waveshare ESP32-S3-LCD-1.47B
- 1.47 inch LCD
- 2 onboard buttons
- USB-C cable
- WiFi network

## Project Structure

- `LVGL_Arduino.ino`: Arduino entry file
- `Stock.cpp` / `Stock.h`: quote refresh and stock data logic
- `LVGL_Example.cpp`: screen layout and UI rendering
- `Secrets.example.h`: template for local WiFi and proxy settings
- `proxy/stock-proxy.js`: local Node.js proxy for Finnhub
- `proxy/start-stock-proxy.cmd`: one-click proxy launcher on Windows

## Why Use a Local Proxy

Direct HTTPS requests from the ESP32 were unstable in this project and could eventually hit memory/network issues during repeated refreshes.

The local proxy helps by:

- requesting Finnhub from your computer instead of the ESP32
- caching quote results
- giving the ESP32 a simpler local HTTP endpoint
- making refresh behavior more stable

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
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
```

If these fonts are disabled, the sketch may still compile, but the UI will fall back to smaller fonts.

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

This starts the local Node.js proxy.

Default health check:

```text
http://127.0.0.1:8787/health
```

### 6. Upload to the Board

Open:

[LVGL_Arduino.ino](C:/Users/reckt/Documents/ESP32-weixue-1.4inch/official-demo/Arduino/examples/Stock_Ticker/LVGL_Arduino/LVGL_Arduino.ino)

Then compile and upload from Arduino IDE.

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
  "ready": true,
  "error": ""
}
```

## Notes

- `Secrets.h` is ignored by Git and should stay local
- `proxy/proxy-secrets.json` is ignored by Git and should stay local
- if the proxy is not running, the board will keep the last successful quote on screen
- if a refresh fails, the screen should not clear the old price

## Troubleshooting

### Build cache corruption

If Arduino shows errors such as:

- `file truncated`
- `archive is not an object`

delete the Arduino build cache folder:

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

## Screenshot

You can place real device photos in the `images/` folder and reference them here for the GitHub page.

## License

Personal project for learning and experimentation.
