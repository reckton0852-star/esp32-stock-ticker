const symbols = [
  { symbol: "WDC", name: "W. Digital", status: "NASDAQ - USD" },
  { symbol: "MU", name: "Micron", status: "NASDAQ - USD" },
  { symbol: "AAPL", name: "Apple", status: "NASDAQ - USD" },
  { symbol: "NVDA", name: "NVIDIA", status: "NASDAQ - USD" },
  { symbol: "AVGO", name: "Broadcom", status: "NASDAQ - USD" },
  { symbol: "TSM", name: "TSMC", status: "NYSE - USD" },
];

const symbolMap = new Map(symbols.map((item) => [item.symbol, item]));

function resolveSymbol(symbol) {
  const known = symbolMap.get(symbol);
  if (known) {
    return known;
  }
  return { symbol, name: symbol, status: "US - USD" };
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

  return {
    symbol: item.symbol,
    name: item.name,
    status: item.status,
    c: Number(price.toFixed(2)),
    d: Number(change.toFixed(2)),
    dp: Number(changePercent.toFixed(2)),
    updated_at: nowTimeString(),
    industry: String(profile.finnhubIndustry || "-"),
    country: String(profile.country || "-"),
    ipo: String(profile.ipo || "-"),
    market_cap: formatMarketCapMillions(profile.marketCapitalization),
    shares_out: formatShareMillions(profile.shareOutstanding),
    ready: true,
    error: "",
  };
}

async function cachedQuote(item, request, env) {
  const cache = caches.default;
  const cacheKey = new Request(`https://cache.internal/quote?symbol=${encodeURIComponent(item.symbol)}`);
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

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (url.pathname === "/health") {
      return json({ ok: true, mode: "cloudflare-worker", symbols: symbols.length });
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
