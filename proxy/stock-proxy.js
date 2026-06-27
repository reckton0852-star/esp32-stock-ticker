const fs = require("fs");
const http = require("http");
const path = require("path");

const symbols = [
  { symbol: "WDC", name: "W. Digital", status: "NASDAQ - USD" },
  { symbol: "MU", name: "Micron", status: "NASDAQ - USD" },
  { symbol: "AAPL", name: "Apple", status: "NASDAQ - USD" },
  { symbol: "NVDA", name: "NVIDIA", status: "NASDAQ - USD" },
  { symbol: "AVGO", name: "Broadcom", status: "NASDAQ - USD" },
  { symbol: "TSM", name: "TSMC", status: "NYSE - USD" },
];

function resolveSymbol(symbol) {
  for (const item of symbols) {
    if (item.symbol === symbol) {
      return item;
    }
  }
  return { symbol, name: symbol, status: "US - USD" };
}

const configPath = path.join(__dirname, "proxy-secrets.json");
if (!fs.existsSync(configPath)) {
  throw new Error("Missing proxy-secrets.json. Copy proxy-secrets.example.json first.");
}

const config = JSON.parse(fs.readFileSync(configPath, "utf8"));
const port = config.port || 8787;
const host = config.host || "0.0.0.0";
const refreshMs = config.refreshMs || 60000;

const cache = new Map(
  symbols.map((item) => [
    item.symbol,
    {
      symbol: item.symbol,
      name: item.name,
      status: item.status,
      c: 0,
      d: 0,
      dp: 0,
      updated_at: "--:--",
      industry: "-",
      country: "-",
      ipo: "-",
      market_cap: "-",
      shares_out: "-",
      ready: false,
      error: "",
    },
  ])
);

let refreshRunning = false;

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function nowTimeString() {
  const now = new Date();
  const hh = String(now.getHours()).padStart(2, "0");
  const mm = String(now.getMinutes()).padStart(2, "0");
  return `${hh}:${mm}`;
}

function formatMarketCapMillions(value) {
  const num = Number(value || 0);
  if (!(num > 0)) {
    return "-";
  }

  if (num >= 1000000) {
    return `${(num / 1000000).toFixed(2)}T`;
  }

  if (num >= 1000) {
    return `${(num / 1000).toFixed(1)}B`;
  }

  return `${num.toFixed(0)}M`;
}

function formatShareMillions(value) {
  const num = Number(value || 0);
  if (!(num > 0)) {
    return "-";
  }

  if (num >= 1000) {
    return `${(num / 1000).toFixed(2)}B`;
  }

  return `${num.toFixed(0)}M`;
}

async function fetchJson(url) {
  const response = await fetch(url, {
    headers: { "User-Agent": "esp32-stock-proxy/1.0" },
  });

  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }

  return response.json();
}

async function fetchQuote(item) {
  const url = `https://finnhub.io/api/v1/quote?symbol=${encodeURIComponent(item.symbol)}&token=${encodeURIComponent(config.finnhubToken)}`;
  const data = await fetchJson(url);
  const price = Number(data.c || 0);
  let change = Number(data.d || 0);
  let changePercent = Number(data.dp || 0);
  const previousClose = Number(data.pc || 0);

  if (!(price > 0.01)) {
    throw new Error("No valid price");
  }

  if (Math.abs(change) < 0.0001 && previousClose > 0.01) {
    change = price - previousClose;
  }

  if (Math.abs(changePercent) < 0.0001 && previousClose > 0.01) {
    changePercent = (change * 100) / previousClose;
  }

  return {
    c: Number(price.toFixed(2)),
    d: Number(change.toFixed(2)),
    dp: Number(changePercent.toFixed(2)),
    updated_at: nowTimeString(),
  };
}

async function fetchProfile(item) {
  const url = `https://finnhub.io/api/v1/stock/profile2?symbol=${encodeURIComponent(item.symbol)}&token=${encodeURIComponent(config.finnhubToken)}`;
  const data = await fetchJson(url);

  return {
    industry: String(data.finnhubIndustry || "-"),
    country: String(data.country || "-"),
    ipo: String(data.ipo || "-"),
    market_cap: formatMarketCapMillions(data.marketCapitalization),
    shares_out: formatShareMillions(data.shareOutstanding),
  };
}

async function refreshSymbol(item) {
  const current = cache.get(item.symbol);
  const quoteData = await fetchQuote(item);

  let profileData = {
    industry: current.industry || "-",
    country: current.country || "-",
    ipo: current.ipo || "-",
    market_cap: current.market_cap || "-",
    shares_out: current.shares_out || "-",
  };

  try {
    profileData = await fetchProfile(item);
  } catch (error) {
    console.log(`[WARN] ${item.symbol} profile: ${error.message || error}`);
  }

  cache.set(item.symbol, {
    ...current,
    symbol: item.symbol,
    name: item.name,
    status: item.status,
    ...quoteData,
    ...profileData,
    ready: true,
    error: "",
  });
}

async function refreshAllQuotes() {
  if (refreshRunning) {
    return;
  }

  refreshRunning = true;
  try {
    for (const item of symbols) {
      try {
        await refreshSymbol(item);
        console.log(`[OK] ${item.symbol}`, cache.get(item.symbol));
      } catch (error) {
        const current = cache.get(item.symbol);
        cache.set(item.symbol, {
          ...current,
          error: String(error.message || error),
        });
        console.log(`[ERR] ${item.symbol}: ${error.message || error}`);
      }
      await wait(300);
    }
  } finally {
    refreshRunning = false;
  }
}

function json(res, statusCode, payload) {
  res.writeHead(statusCode, { "Content-Type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(payload));
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);

  if (url.pathname === "/health") {
    return json(res, 200, { ok: true, refreshRunning, count: symbols.length });
  }

  if (url.pathname === "/quotes") {
    return json(res, 200, Array.from(cache.values()));
  }

  if (url.pathname === "/quote") {
    const symbol = (url.searchParams.get("symbol") || "").toUpperCase();
    if (!symbol) {
      return json(res, 404, { error: "Unknown symbol" });
    }
    if (!cache.has(symbol)) {
      const item = resolveSymbol(symbol);
      cache.set(symbol, {
        symbol: item.symbol,
        name: item.name,
        status: item.status,
        c: 0,
        d: 0,
        dp: 0,
        updated_at: "--:--",
        industry: "-",
        country: "-",
        ipo: "-",
        market_cap: "-",
        shares_out: "-",
        ready: false,
        error: "",
      });
      refreshSymbol(item).catch((error) => {
        console.log(`[ERR] ${item.symbol}: ${error.message || error}`);
      });
    }
    return json(res, 200, cache.get(symbol));
  }

  return json(res, 404, { error: "Not found" });
});

server.listen(port, host, async () => {
  console.log(`Stock proxy listening on http://${host}:${port}`);
  await refreshAllQuotes();
  setInterval(() => {
    refreshAllQuotes().catch((error) => {
      console.error("Refresh loop error:", error);
    });
  }, refreshMs);
});
