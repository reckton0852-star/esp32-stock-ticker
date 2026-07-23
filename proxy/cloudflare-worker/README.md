# Cloudflare Worker Proxy

This folder contains a cloud relay version of the stock proxy.

## What It Does

- receives requests from the ESP32
- fetches quote and company profile data from Finnhub
- fetches daily FX reference rates from Frankfurter
- returns the same JSON shape used by the local proxy
- caches each symbol briefly at the edge to reduce repeated upstream calls

## Endpoints

- `/health`: service status
- `/quote?symbol=AAPL`: one stock quote
- `/quotes`: configured stock quote list
- `/fx?base=USD`: one FX rate against CNY
- `/fxs`: configured FX rate list

## Files

- `worker.js`: Worker source
- `wrangler.example.toml`: example Wrangler config

## Deploy Steps

1. Install Wrangler
2. Log in to Cloudflare
3. Copy `wrangler.example.toml` to `wrangler.toml`
4. Set your Finnhub token as a Worker secret
5. Deploy the Worker
6. Optional but recommended: bind a Cloudflare custom domain such as `stock.your-domain.com`
7. Copy the final HTTPS base URL into `Secrets.h` as `STOCK_PROXY_BASE_URL`

## Example Commands

```powershell
npm install -g wrangler
wrangler login
Copy-Item wrangler.example.toml wrangler.toml
wrangler secret put FINNHUB_TOKEN
wrangler deploy
```

After deploy, your base URL will look similar to:

```text
https://your-worker-name.your-subdomain.workers.dev
```

If you bind a custom domain in `Workers & Pages -> your worker -> Domains`, your final URL can look like:

```text
https://stock.your-domain.com
```

Then set:

```cpp
static const char * STOCK_PROXY_BASE_URL = "https://stock.your-domain.com";
```
