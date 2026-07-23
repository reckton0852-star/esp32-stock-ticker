const symbols = [
  { symbol: "WDC", name: "W. Digital", status: "NASDAQ - USD" },
  { symbol: "MU", name: "Micron", status: "NASDAQ - USD" },
  { symbol: "AAPL", name: "Apple", status: "NASDAQ - USD" },
  { symbol: "NVDA", name: "NVIDIA", status: "NASDAQ - USD" },
  { symbol: "AVGO", name: "Broadcom", status: "NASDAQ - USD" },
  { symbol: "TSM", name: "TSMC", status: "NYSE - USD" },
];

const symbolMap = new Map(symbols.map((item) => [item.symbol, item]));

const fxItems = [
  { symbol: "USD", name: "US Dollar", status: "USD/CNY" },
  { symbol: "EUR", name: "Euro", status: "EUR/CNY" },
  { symbol: "GBP", name: "British Pound", status: "GBP/CNY" },
  { symbol: "CAD", name: "Canadian Dollar", status: "CAD/CNY" },
  { symbol: "JPY", name: "Japanese Yen", status: "JPY/CNY" },
  { symbol: "HKD", name: "Hong Kong Dollar", status: "HKD/CNY" },
];

const fxMap = new Map(fxItems.map((item) => [item.symbol, item]));

function resolveSymbol(symbol) {
  const known = symbolMap.get(symbol);
  if (known) {
    return known;
  }
  return { symbol, name: symbol, status: "US - USD" };
}

function resolveFx(symbol) {
  const known = fxMap.get(symbol);
  if (known) {
    return known;
  }
  return { symbol, name: symbol, status: `${symbol}/CNY` };
}

function json(data, status = 200, extraHeaders = {}) {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": "no-store",
      ...extraHeaders,
    },
  });
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
  if (!(num > 0)) return "-";
  if (num >= 1000000) return `${(num / 1000000).toFixed(2)}T`;
  if (num >= 1000) return `${(num / 1000).toFixed(1)}B`;
  return `${num.toFixed(0)}M`;
}

function formatShareMillions(value) {
  const num = Number(value || 0);
  if (!(num > 0)) return "-";
  if (num >= 1000) return `${(num / 1000).toFixed(2)}B`;
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
    headers: { "User-Agent": "esp32-stock-proxy-worker/1.0" },
  });
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return response.json();
}

async function buildQuotePayload(item, env) {
  const token = env.FINNHUB_TOKEN;
  if (!token) {
    throw new Error("Missing FINNHUB_TOKEN secret");
  }

  const quoteUrl = `https://finnhub.io/api/v1/quote?symbol=${encodeURIComponent(item.symbol)}&token=${encodeURIComponent(token)}`;
  const quoteData = await fetchJson(quoteUrl);

  const price = Number(quoteData.c || 0);
  let change = Number(quoteData.d || 0);
  let changePercent = Number(quoteData.dp || 0);
  const previousClose = Number(quoteData.pc || 0);

  if (!(price > 0.01)) {
    throw new Error("No valid price");
  }

  if (Math.abs(change) < 0.0001 && previousClose > 0.01) {
    change = price - previousClose;
  }
  if (Math.abs(changePercent) < 0.0001 && previousClose > 0.01) {
    changePercent = (change * 100) / previousClose;
  }

  let profile = {};
  try {
    const profileUrl = `https://finnhub.io/api/v1/stock/profile2?symbol=${encodeURIComponent(item.symbol)}&token=${encodeURIComponent(token)}`;
    profile = await fetchJson(profileUrl);
  } catch (_) {
    profile = {};
  }

  let history = [];
  try {
    const to = Math.floor(Date.now() / 1000);
    const from = unixSecondsForDaysAgo(45);
    const candleUrl = `https://finnhub.io/api/v1/stock/candle?symbol=${encodeURIComponent(item.symbol)}&resolution=D&from=${from}&to=${to}&token=${encodeURIComponent(token)}`;
    const candleData = await fetchJson(candleUrl);
    if (candleData.s === "ok" && Array.isArray(candleData.c)) {
      history = lastValues(candleData.c, 30).map((value) => Number(value.toFixed(2)));
    }
  } catch (_) {
    history = [];
  }

  return {
    symbol: item.symbol,
    name: item.name,
    status: item.status,
    c: Number(price.toFixed(2)),
    d: Number(change.toFixed(2)),
    dp: Number(changePercent.toFixed(2)),
    o: Number(Number(quoteData.o || 0).toFixed(2)),
    h: Number(Number(quoteData.h || 0).toFixed(2)),
    l: Number(Number(quoteData.l || 0).toFixed(2)),
    pc: Number(previousClose.toFixed(2)),
    updated_at: nowTimeString(),
    industry: String(profile.finnhubIndustry || "-"),
    country: String(profile.country || "-"),
    ipo: String(profile.ipo || "-"),
    market_cap: formatMarketCapMillions(profile.marketCapitalization),
    shares_out: formatShareMillions(profile.shareOutstanding),
    history,
    ready: true,
    error: "",
  };
}

async function buildFxHistory(item, latestDate) {
  const startDate = addDays(latestDate, -45);
  const url = `https://api.frankfurter.dev/v1/${startDate}..${latestDate}?base=${encodeURIComponent(item.symbol)}&symbols=CNY`;
  const data = await fetchJson(url);
  const rows = Object.entries(data?.rates || {})
    .sort(([a], [b]) => a.localeCompare(b))
    .map(([, rates]) => Number(rates?.CNY || 0));
  return lastValues(rows, 30).map((value) => Number(value.toFixed(4)));
}

async function buildFxPayload(item) {
  const latestUrl = `https://api.frankfurter.dev/v1/latest?base=${encodeURIComponent(item.symbol)}&symbols=CNY`;
  const latestData = await fetchJson(latestUrl);
  const price = Number(latestData?.rates?.CNY || 0);

  if (!(price > 0.0001)) {
    throw new Error("No valid FX rate");
  }

  let previousClose = price;
  let previousDate = "";
  for (let i = 1; i <= 7; i++) {
    const date = addDays(latestData.date, -i);
    const previousUrl = `https://api.frankfurter.dev/v1/${date}?base=${encodeURIComponent(item.symbol)}&symbols=CNY`;
    try {
      const previousData = await fetchJson(previousUrl);
      const previousRate = Number(previousData?.rates?.CNY || 0);
      if (previousData.date !== latestData.date && previousRate > 0.0001) {
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
    history = await buildFxHistory(item, latestData.date);
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
    ipo: String(latestData.date || "-"),
    market_cap: previousDate || "Daily",
    shares_out: "Frankfurter",
    history,
    ready: true,
    error: "",
  };
}

async function cachedQuote(item, request, env) {
  const cache = caches.default;
  const cacheKey = new Request(`https://cache.internal/quote-history-v1?symbol=${encodeURIComponent(item.symbol)}`);
  const cached = await cache.match(cacheKey);
  if (cached) {
    return cached;
  }

  const payload = await buildQuotePayload(item, env);
  const ttl = Number(env.CACHE_TTL_SECONDS || 55);
  const response = json(payload, 200, { "cache-control": `public, max-age=${ttl}` });
  await cache.put(cacheKey, response.clone());
  return response;
}

async function cachedFx(item, request, env) {
  const cache = caches.default;
  const cacheKey = new Request(`https://cache.internal/fx-history-v1?base=${encodeURIComponent(item.symbol)}`);
  const cached = await cache.match(cacheKey);
  if (cached) {
    return cached;
  }

  const payload = await buildFxPayload(item);
  const ttl = Number(env.FX_CACHE_TTL_SECONDS || 3600);
  const response = json(payload, 200, { "cache-control": `public, max-age=${ttl}` });
  await cache.put(cacheKey, response.clone());
  return response;
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (url.pathname === "/health") {
      return json({ ok: true, mode: "cloudflare-worker", symbols: symbols.length, fx: fxItems.length });
    }

    if (url.pathname === "/quote") {
      const symbol = (url.searchParams.get("symbol") || "").toUpperCase();
      if (!symbol) {
        return json({ error: "Unknown symbol" }, 404);
      }
      const item = resolveSymbol(symbol);
      try {
        return await cachedQuote(item, request, env);
      } catch (error) {
        return json({
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
        }, 500);
      }
    }

    if (url.pathname === "/fx") {
      const symbol = (url.searchParams.get("base") || "").toUpperCase();
      if (!symbol) {
        return json({ error: "Unknown FX base" }, 404);
      }
      const item = resolveFx(symbol);
      try {
        return await cachedFx(item, request, env);
      } catch (error) {
        return json({
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
        }, 500);
      }
    }

    if (url.pathname === "/fxs") {
      const results = [];
      for (const item of fxItems) {
        try {
          const response = await cachedFx(item, request, env);
          results.push(await response.json());
        } catch (error) {
          results.push({
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
          });
        }
      }
      return json(results);
    }

    if (url.pathname === "/quotes") {
      const results = [];
      for (const item of symbols) {
        try {
          const response = await cachedQuote(item, request, env);
          results.push(await response.json());
        } catch (error) {
          results.push({
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
          });
        }
      }
      return json(results);
    }

    return json({ error: "Not found" }, 404);
  },
};
