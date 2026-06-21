# VPS Deploy Guide

This is the simplest cloud deployment path for the ESP32 stock proxy.

## Recommended Server

- 1 vCPU
- 2 GB RAM
- Ubuntu 22.04 or 24.04
- Public IPv4

This proxy is very small, so a low-end lightweight server is enough.

## Upload Files

Upload this folder to the server as:

```text
/opt/esp32-stock-proxy
```

Files needed:

- `stock-proxy.js`
- `proxy-secrets.json`
- `package.json`

## Install Node.js

On Ubuntu:

```bash
sudo apt update
sudo apt install -y nodejs npm
node -v
```

Node 18+ is recommended.

## Run Once for Testing

```bash
cd /opt/esp32-stock-proxy
node stock-proxy.js
```

If it starts correctly, test:

```bash
curl http://127.0.0.1:8787/health
curl "http://127.0.0.1:8787/quote?symbol=WDC"
```

## Run as a Service

Copy the service file:

```bash
sudo cp stock-proxy.service /etc/systemd/system/stock-proxy.service
sudo systemctl daemon-reload
sudo systemctl enable stock-proxy
sudo systemctl start stock-proxy
sudo systemctl status stock-proxy
```

## Firewall

Allow TCP port `8787`.

If using Ubuntu UFW:

```bash
sudo ufw allow 8787/tcp
```

If your cloud provider has a security group, also open `8787`.

## ESP32 Config

Set your proxy base URL in `Secrets.h`:

```cpp
static const char * STOCK_PROXY_BASE_URL = "http://YOUR_SERVER_PUBLIC_IP:8787";
```

Then re-upload the Arduino sketch.
