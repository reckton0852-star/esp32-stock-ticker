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

const fxItems = [
  { symbol: "USD", name: "US Dollar", status: "USD/CNY" },
  { symbol: "EUR", name: "Euro", status: "EUR/CNY" },
  { symbol: "GBP", name: "British Pound", status: "GBP/CNY" },
  { symbol: "CAD", name: "Canadian Dollar", status: "CAD/CNY" },
  { symbol: "JPY", name: "Japanese Yen", status: "JPY/CNY" },
  { symbol: "HKD", name: "Hong Kong Dollar", status: "HKD/CNY" },
];

function resolveSymbol(symbol) {
  for (const item of symbols) {
    if (item.symbol === symbol) {
      return item;
    }
  }
  return { symbol, name: symbol, status: "US - USD" };
}

function resolveFx(symbol) {
  for (const item of fxItems) {
    if (item.symbol === symbol) {
      return item;
    }
  }
  return { symbol, name: symbol, status: `${symbol}/CNY` };
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
      o: 0,
      h: 0,
      l: 0,
      pc: 0,
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

function addDays(dateText, days) {
  const date = new Date(`${dateText}T00:00:00Z`);
  date.setUTCDate(date.getUTCDate() + days);
  return date.toISOString().slice(0, 10);
}

function unixSecondsForDaysAgo(daysAgo) {
  const date = new Date();
  date.setUTCDate(date.getUTCDate() - daysAgo);
  date.setUTCHours(0, 0, 0, 0);
  return Math.floor(date.getTime() / 1000);
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

function lastValues(values, maxCount) {
  const cleaned = values
    .map((value) => Number(value))
    .filter((value) => value > 0.0001);
  return cleaned.slice(Math.max(0, cleaned.length - maxCount));
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
    o: Number(Number(data.o || 0).toFixed(2)),
    h: Number(Number(data.h || 0).toFixed(2)),
    l: Number(Number(data.l || 0).toFixed(2)),
    pc: Number(previousClose.toFixed(2)),
    updated_at: nowTimeString(),
  };
}

async function fetchStockHistory(item) {
  const to = Math.floor(Date.now() / 1000);
  const from = unixSecondsForDaysAgo(45);
  const url = `https://finnhub.io/api/v1/stock/candle?symbol=${encodeURIComponent(item.symbol)}&resolution=D&from=${from}&to=${to}&token=${encodeURIComponent(config.finnhubToken)}`;
  const data = await fetchJson(url);
  if (data.s === "ok" && Array.isArray(data.c)) {
    return lastValues(data.c, 30).map((value) => Number(value.toFixed(2)));
  }
  return [];
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

async function fetchFx(item) {
  const url = `https://api.frankfurter.dev/v1/latest?base=${encodeURIComponent(item.symbol)}&symbols=CNY`;
  const data = await fetchJson(url);
  const price = Number(data?.rates?.CNY || 0);

  if (!(price > 0.0001)) {
    throw new Error("No valid FX rate");
  }

  let previousClose = price;
  let previousDate = "";
  for (let i = 1; i <= 7; i++) {
    const previousDateCandidate = addDays(data.date, -i);
    const previousUrl = `https://api.frankfurter.dev/v1/${previousDateCandidate}?base=${encodeURIComponent(item.symbol)}&symbols=CNY`;
    try {
      const previousData = await fetchJson(previousUrl);
      const previousRate = Number(previousData?.rates?.CNY || 0);
      if (previousData.date !== data.date && previousRate > 0.0001) {
        previousClose = previousRate;
        previousDate = previousData.date;
        break;
      }
    } catch (_) {
      previousClose = price;
    }
  }

  const change = price - previousClose;
  const changePercent = previousClose > 0.0001 ? (change * 100) / previousClose : 0;
  let history = [];
  try {
    const startDate = addDays(data.date, -45);
    const historyUrl = `https://api.frankfurter.dev/v1/${startDate}..${data.date}?base=${encodeURIComponent(item.symbol)}&symbols=CNY`;
    const historyData = await fetchJson(historyUrl);
    const rows = Object.entries(historyData?.rates || {})
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([, rates]) => Number(rates?.CNY || 0));
    history = lastValues(rows, 30).map((value) => Number(value.toFixed(4)));
  } catch (_) {
    history = [];
  }

  return {
    symbol: item.symbol,
    name: item.name,
    status: item.status,
    c: Number(price.toFixed(4)),
    d: Number(change.toFixed(4)),
    dp: Number(changePercent.toFixed(2)),
    o: Number(price.toFixed(4)),
    h: Number(price.toFixed(4)),
    l: Number(price.toFixed(4)),
    pc: Number(previousClose.toFixed(4)),
    updated_at: nowTimeString(),
    industry: "Currency",
    country: "CNY",
    ipo: String(data.date || "-"),
    market_cap: previousDate || "Daily",
    shares_out: "Frankfurter",
    history,
    ready: true,
    error: "",
  };
}

async function refreshSymbol(item) {
  const current = cache.get(item.symbol);
  const quoteData = await fetchQuote(item);
  let history = current.history || [];
  try {
    history = await fetchStockHistory(item);
  } catch (error) {
    console.log(`[WARN] ${item.symbol} history: ${error.message || error}`);
  }

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
    history,
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
    return json(res, 200, { ok: true, refreshRunning, count: symbols.length, fx: fxItems.length });
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
        o: 0,
        h: 0,
        l: 0,
        pc: 0,
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

  if (url.pathname === "/fx") {
    const symbol = (url.searchParams.get("base") || "").toUpperCase();
    if (!symbol) {
      return json(res, 404, { error: "Unknown FX base" });
    }
    const item = resolveFx(symbol);
    fetchFx(item)
      .then((payload) => json(res, 200, payload))
      .catch((error) => json(res, 500, {
        symbol: item.symbol,
        name: item.name,
        status: item.status,
        c: 0,
        d: 0,
        dp: 0,
        o: 0,
        h: 0,
        l: 0,
        pc: 0,
        updated_at: "--:--",
        industry: "-",
        country: "-",
        ipo: "-",
        market_cap: "-",
        shares_out: "-",
        ready: false,
        error: String(error.message || error),
      }));
    return;
  }

  if (url.pathname === "/fxs") {
    Promise.all(fxItems.map(async (item) => {
      try {
        return await fetchFx(item);
      } catch (error) {
        return {
          symbol: item.symbol,
          name: item.name,
          status: item.status,
          c: 0,
          d: 0,
          dp: 0,
          o: 0,
          h: 0,
          l: 0,
          pc: 0,
          updated_at: "--:--",
          industry: "-",
          country: "-",
          ipo: "-",
          market_cap: "-",
          shares_out: "-",
          ready: false,
          error: String(error.message || error),
        };
      }
    })).then((results) => json(res, 200, results));
    return;
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
