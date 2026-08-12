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

const firmwareAssetName = "esp32-stock-ticker.bin";
const defaultFirmwareRepository = "reckton0852-star/esp32-stock-ticker";

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

function parseRequestedItems(rawValue, resolver, fallbackItems) {
  if (!rawValue) {
    return fallbackItems;
  }

  const seen = new Set();
  const items = [];
  for (const rawSymbol of rawValue.split(",")) {
    const symbol = rawSymbol.trim().toUpperCase();
    if (!/^[A-Z0-9._-]{1,12}$/.test(symbol) || seen.has(symbol)) {
      continue;
    }
    seen.add(symbol);
    items.push(resolver(symbol));
    if (items.length >= 8) {
      break;
    }
  }
  return items.length > 0 ? items : fallbackItems;
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

function timeString(timestampMs = Date.now()) {
  return new Intl.DateTimeFormat("en-GB", {
    timeZone: "Asia/Shanghai",
    hour: "2-digit",
    minute: "2-digit",
    hourCycle: "h23",
  }).format(new Date(timestampMs));
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

function finnhubApiBase(env) {
  return String(env.FINNHUB_BASE_URL || "https://finnhub.io/api/v1").replace(/\/$/, "");
}

async function loadLatestFirmware(env) {
  const repository = env.GITHUB_REPOSITORY || defaultFirmwareRepository;
  const manifestUrl = env.FIRMWARE_MANIFEST_URL ||
    `https://raw.githubusercontent.com/${repository}/main/firmware/manifest.json`;
  const cache = caches.default;
  const cacheKey = new Request(`https://cache.internal/firmware-release-v2?url=${encodeURIComponent(manifestUrl)}`);
  const cached = await cache.match(cacheKey);
  if (cached) {
    return cached.json();
  }

  let payload;
  try {
    const manifestResponse = await fetch(manifestUrl, {
      headers: { "User-Agent": "eyuxia-stock-ota-worker/1.0" },
    });
    if (!manifestResponse.ok) {
      payload = {
        ready: false,
        version: "",
        error: "No OTA package has been published",
      };
    } else {
      const manifest = await manifestResponse.json();
      if (!manifest.ready) {
        payload = {
          ready: false,
          version: String(manifest.version || ""),
          error: String(manifest.error || "No OTA package has been published"),
        };
      } else {
        const assetUrl = new URL(String(manifest.asset_url || ""));
        const allowedHosts = new Set([
          "github.com",
          "objects.githubusercontent.com",
          "github-releases.githubusercontent.com",
        ]);
        const md5 = String(manifest.md5 || "").toLowerCase();
        const size = Number(manifest.size || 0);
        if (!allowedHosts.has(assetUrl.hostname) ||
            !/^v?\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?$/.test(String(manifest.version || "")) ||
            !/^[a-f0-9]{32}$/.test(md5) || !(size > 0)) {
          throw new Error("Firmware manifest is invalid");
        }
        payload = {
          ready: true,
          version: String(manifest.version),
          notes: String(manifest.notes || "Firmware update").slice(0, 120),
          md5,
          size,
          asset_url: assetUrl.toString(),
          published_at: String(manifest.published_at || ""),
        };
      }
    }
  } catch (error) {
    payload = {
      ready: false,
      version: "",
      error: String(error.message || error),
    };
  }

  const response = json(payload, 200, { "cache-control": "public, max-age=300" });
  await cache.put(cacheKey, response.clone());
  return payload;
}

function publicFirmwareManifest(release, origin) {
  if (!release.ready) {
    return {
      ready: false,
      version: release.version || "",
      error: release.error || "No OTA firmware is available",
    };
  }
  return {
    ready: true,
    version: release.version,
    notes: release.notes,
    md5: release.md5,
    size: release.size,
    published_at: release.published_at,
    download_url: `${origin}/firmware/download?version=${encodeURIComponent(release.version)}`,
  };
}

function quoteCacheKey(symbol) {
  return new Request(`https://cache.internal/quote-v6?symbol=${encodeURIComponent(symbol)}`);
}

function quoteBatchCacheKey(items) {
  const symbols = items.map((item) => item.symbol).join(",");
  return new Request(`https://cache.internal/quote-batch-v5?symbols=${encodeURIComponent(symbols)}`);
}

function withQuoteAge(payload, source) {
  const quoteAtMs = Number(payload?.market_at_ms || payload?.fetched_at_ms || Date.now());
  return {
    ...payload,
    source,
    age_seconds: Math.max(0, Math.floor((Date.now() - quoteAtMs) / 1000)),
  };
}

function lastGoodQuoteCacheKey(symbol) {
  return new Request(`https://cache.internal/quote-last-good-v2?symbol=${encodeURIComponent(symbol)}`);
}

function quoteRetryCacheKey(symbol) {
  return new Request(`https://cache.internal/quote-retry-v1?symbol=${encodeURIComponent(symbol)}`);
}

function profileCacheKey(symbol) {
  return new Request(`https://cache.internal/profile-v1?symbol=${encodeURIComponent(symbol)}`);
}

function historyCacheKey(symbol) {
  return new Request(`https://cache.internal/history-v1?symbol=${encodeURIComponent(symbol)}`);
}

async function readCachedJson(cacheKey) {
  const cached = await caches.default.match(cacheKey);
  return cached ? cached.json() : null;
}

async function writeCachedJson(cacheKey, payload, ttl) {
  const response = json(payload, 200, { "cache-control": `public, max-age=${ttl}` });
  await caches.default.put(cacheKey, response);
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function fetchProfilePayload(item, env) {
  const token = env.FINNHUB_TOKEN;
  const profileUrl = `${finnhubApiBase(env)}/stock/profile2?symbol=${encodeURIComponent(item.symbol)}&token=${encodeURIComponent(token)}`;
  const profile = await fetchJson(profileUrl);
  return {
    industry: String(profile.finnhubIndustry || "-"),
    country: String(profile.country || "-"),
    ipo: String(profile.ipo || "-"),
    market_cap: formatMarketCapMillions(profile.marketCapitalization),
    shares_out: formatShareMillions(profile.shareOutstanding),
  };
}

async function fetchHistoryPayload(item, env) {
  const token = env.FINNHUB_TOKEN;
  const to = Math.floor(Date.now() / 1000);
  const from = unixSecondsForDaysAgo(45);
  const candleUrl = `${finnhubApiBase(env)}/stock/candle?symbol=${encodeURIComponent(item.symbol)}&resolution=D&from=${from}&to=${to}&token=${encodeURIComponent(token)}`;
  const candleData = await fetchJson(candleUrl);
  const values = candleData.s === "ok" && Array.isArray(candleData.c)
    ? lastValues(candleData.c, 30).map((value) => Number(value.toFixed(2)))
    : [];
  return { values };
}

async function warmOneSupplementalItem(items, env) {
  const profileTtl = Number(env.PROFILE_CACHE_TTL_SECONDS || 86400);
  const historyTtl = Number(env.HISTORY_CACHE_TTL_SECONDS || 21600);
  const retryTtl = Number(env.SUPPLEMENT_RETRY_TTL_SECONDS || 900);

  for (const item of items) {
    const key = profileCacheKey(item.symbol);
    if (await readCachedJson(key)) continue;
    try {
      await writeCachedJson(key, await fetchProfilePayload(item, env), profileTtl);
    } catch (error) {
      await writeCachedJson(key, { unavailable: true }, retryTtl);
      console.warn(`Profile warm failed ${item.symbol}: ${String(error.message || error)}`);
    }
    break;
  }

  for (const item of items) {
    const key = historyCacheKey(item.symbol);
    if (await readCachedJson(key)) continue;
    try {
      await writeCachedJson(key, await fetchHistoryPayload(item, env), historyTtl);
    } catch (error) {
      await writeCachedJson(key, { unavailable: true, values: [] }, retryTtl);
      console.warn(`History warm failed ${item.symbol}: ${String(error.message || error)}`);
    }
    break;
  }
}

function firstPositive(values) {
  if (!Array.isArray(values)) return 0;
  for (const value of values) {
    const number = Number(value || 0);
    if (number > 0.01) return number;
  }
  return 0;
}

function yahooQuoteFromMeta(meta, quote = {}) {
  const price = Number(meta.regularMarketPrice || 0);
  const previousClose = Number(meta.chartPreviousClose || meta.previousClose || 0);
  const marketAtMs = Number(meta.regularMarketTime || 0) * 1000;

  if (!(price > 0.01) || !(previousClose > 0.01)) {
    throw new Error("Yahoo returned no valid price");
  }
  if (!(marketAtMs > 1577836800000) || marketAtMs > Date.now() + 300000) {
    throw new Error("Yahoo returned no valid market timestamp");
  }

  return {
    price,
    previousClose,
    open: Number(meta.regularMarketOpen || firstPositive(quote.open) || firstPositive(quote.close) || 0),
    high: Number(meta.regularMarketDayHigh || 0),
    low: Number(meta.regularMarketDayLow || 0),
    marketAtMs,
    provider: "YAHOO",
  };
}

async function fetchYahooQuotes(items) {
  const symbols = items.map((item) => item.symbol).join(",");
  const url = `https://query1.finance.yahoo.com/v7/finance/spark?symbols=${encodeURIComponent(symbols)}` +
    "&range=1d&interval=1m";
  const data = await fetchJson(url);
  const result = new Map();
  for (const row of data?.spark?.result || []) {
    const response = row?.response?.[0];
    try {
      result.set(String(row.symbol || "").toUpperCase(), yahooQuoteFromMeta(
        response?.meta || {},
        response?.indicators?.quote?.[0] || {},
      ));
    } catch (_) {
      // A missing symbol falls back to Finnhub without failing the whole batch.
    }
  }
  if (result.size === 0) throw new Error("Yahoo returned no batch quotes");
  return result;
}

async function fetchYahooQuote(item) {
  const quotes = await fetchYahooQuotes([item]);
  const quote = quotes.get(item.symbol);
  if (!quote) throw new Error("Yahoo returned no quote for symbol");
  return quote;
}

async function fetchFinnhubQuote(item, env) {
  const token = env.FINNHUB_TOKEN;
  if (!token) {
    throw new Error("Missing FINNHUB_TOKEN secret");
  }

  const quoteUrl = `${finnhubApiBase(env)}/quote?symbol=${encodeURIComponent(item.symbol)}&token=${encodeURIComponent(token)}`;
  const quoteData = await fetchJson(quoteUrl);

  const price = Number(quoteData.c || 0);
  const previousClose = Number(quoteData.pc || 0);
  const marketAtMs = Number(quoteData.t || 0) * 1000;

  if (!(price > 0.01)) {
    throw new Error("No valid price");
  }
  if (!(marketAtMs > 1577836800000) || marketAtMs > Date.now() + 300000) {
    throw new Error("No valid market timestamp");
  }

  return {
    price,
    previousClose,
    open: Number(quoteData.o || 0),
    high: Number(quoteData.h || 0),
    low: Number(quoteData.l || 0),
    marketAtMs,
    provider: "FINNHUB",
  };
}

async function buildQuotePayload(item, env, loadPrimaryQuotes = null) {
  let marketQuote;
  let primaryError = "";
  try {
    if (loadPrimaryQuotes) {
      const quotes = await loadPrimaryQuotes();
      marketQuote = quotes.get(item.symbol);
      if (!marketQuote) throw new Error("Yahoo batch missing symbol");
    } else {
      marketQuote = await fetchYahooQuote(item);
    }
  } catch (error) {
    primaryError = String(error.message || error);
    marketQuote = await fetchFinnhubQuote(item, env);
  }

  const price = marketQuote.price;
  const previousClose = marketQuote.previousClose;
  const change = price - previousClose;
  const changePercent = previousClose > 0.01 ? (change * 100) / previousClose : 0;
  const marketAtMs = marketQuote.marketAtMs;

  const [cachedProfile, cachedHistory] = await Promise.all([
    readCachedJson(profileCacheKey(item.symbol)),
    readCachedJson(historyCacheKey(item.symbol)),
  ]);
  const profile = cachedProfile && !cachedProfile.unavailable ? cachedProfile : {};
  const history = cachedHistory && Array.isArray(cachedHistory.values)
    ? cachedHistory.values
    : [];

  return {
    symbol: item.symbol,
    name: item.name,
    status: item.status,
    c: Number(price.toFixed(2)),
    d: Number(change.toFixed(2)),
    dp: Number(changePercent.toFixed(2)),
    o: Number(marketQuote.open.toFixed(2)),
    h: Number(marketQuote.high.toFixed(2)),
    l: Number(marketQuote.low.toFixed(2)),
    pc: Number(previousClose.toFixed(2)),
    fetched_at_ms: Date.now(),
    market_at_ms: marketAtMs,
    updated_at: timeString(marketAtMs),
    provider: marketQuote.provider,
    industry: String(profile.industry || "-"),
    country: String(profile.country || "-"),
    ipo: String(profile.ipo || "-"),
    market_cap: String(profile.market_cap || "-"),
    shares_out: String(profile.shares_out || "-"),
    history,
    ready: true,
    error: primaryError ? `Primary fallback: ${primaryError}` : "",
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
    fetched_at_ms: Date.now(),
    updated_at: timeString(),
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

async function fetchAndCacheQuote(item, env, loadPrimaryQuotes = null) {
  const cache = caches.default;
  const cacheKey = quoteCacheKey(item.symbol);
  try {
    const payload = await buildQuotePayload(item, env, loadPrimaryQuotes);
    const ttl = Number(env.CACHE_TTL_SECONDS || 55);
    const response = json(payload, 200, { "cache-control": `public, max-age=${ttl}` });
    await cache.put(cacheKey, response.clone());
    await writeCachedJson(
      lastGoodQuoteCacheKey(item.symbol),
      payload,
      Number(env.STALE_QUOTE_TTL_SECONDS || 86400),
    );
    return { payload, source: "LIVE" };
  } catch (error) {
    const errorText = String(error.message || error);
    await writeCachedJson(
      quoteRetryCacheKey(item.symbol),
      { error: errorText },
      Number(env.QUOTE_RETRY_TTL_SECONDS || 20),
    );
    const lastGood = await readCachedJson(lastGoodQuoteCacheKey(item.symbol));
    if (lastGood && Number(lastGood.c || 0) > 0.01) {
      return {
        payload: {
          ...lastGood,
          stale: true,
          error: `Using last good quote: ${errorText}`,
        },
        source: "STALE",
      };
    }
    throw error;
  }
}

async function cachedQuote(item, env, loadPrimaryQuotes = null) {
  const cache = caches.default;
  const cached = await cache.match(quoteCacheKey(item.symbol));
  if (cached) {
    return json(withQuoteAge(await cached.json(), "CACHE"));
  }

  const retryState = await readCachedJson(quoteRetryCacheKey(item.symbol));
  if (retryState) {
    const lastGood = await readCachedJson(lastGoodQuoteCacheKey(item.symbol));
    if (lastGood && Number(lastGood.c || 0) > 0.01) {
      return json({
        ...withQuoteAge({
          ...lastGood,
          stale: true,
          error: `Using last good quote: ${String(retryState.error || "upstream cooldown")}`,
        }, "STALE"),
      }, 200, { "cache-control": "public, max-age=15" });
    }
    throw new Error(String(retryState.error || "Upstream cooldown"));
  }

  const result = await fetchAndCacheQuote(item, env, loadPrimaryQuotes);
  return json(withQuoteAge(result.payload, result.source));
}

async function cachedFx(item, request, env) {
  const cache = caches.default;
  const cacheKey = new Request(`https://cache.internal/fx-history-v1?base=${encodeURIComponent(item.symbol)}`);
  const cached = await cache.match(cacheKey);
  if (cached) {
    return json(withQuoteAge(await cached.json(), "CACHE"));
  }

  const payload = await buildFxPayload(item);
  const ttl = Number(env.FX_CACHE_TTL_SECONDS || 3600);
  const response = json(payload, 200, { "cache-control": `public, max-age=${ttl}` });
  await cache.put(cacheKey, response.clone());
  return json(withQuoteAge(payload, "LIVE"));
}

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);

    if (url.pathname === "/health") {
      return json({ ok: true, mode: "cloudflare-worker", symbols: symbols.length, fx: fxItems.length });
    }

    if (url.pathname === "/firmware/manifest") {
      const release = await loadLatestFirmware(env);
      return json(publicFirmwareManifest(release, url.origin));
    }

    if (url.pathname === "/firmware/download") {
      const release = await loadLatestFirmware(env);
      const requestedVersion = url.searchParams.get("version") || "";
      if (!release.ready || requestedVersion !== release.version) {
        return json({ error: "Requested OTA package is not available" }, 404);
      }

      const firmwareResponse = await fetch(release.asset_url, {
        headers: { "User-Agent": "eyuxia-stock-ota-worker/1.0" },
      });
      if (!firmwareResponse.ok || !firmwareResponse.body) {
        return json({ error: `Firmware asset HTTP ${firmwareResponse.status}` }, 502);
      }

      return new Response(firmwareResponse.body, {
        status: 200,
        headers: {
          "content-type": "application/octet-stream",
          "content-length": String(release.size),
          "content-disposition": `attachment; filename="${firmwareAssetName}"`,
          "cache-control": "public, max-age=3600, immutable",
          "x-MD5": release.md5,
        },
      });
    }

    if (url.pathname === "/quote") {
      const symbol = (url.searchParams.get("symbol") || "").toUpperCase();
      if (!symbol) {
        return json({ error: "Unknown symbol" }, 404);
      }
      const item = resolveSymbol(symbol);
      try {
        const response = await cachedQuote(item, env);
        if (ctx) {
          ctx.waitUntil(warmOneSupplementalItem([item], env));
        }
        return response;
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
      const requestedItems = parseRequestedItems(url.searchParams.get("bases"), resolveFx, fxItems);
      const results = await Promise.all(requestedItems.map(async (item) => {
        try {
          const response = await cachedFx(item, request, env);
          return await response.json();
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
      }));
      return json(results);
    }

    if (url.pathname === "/quotes") {
      const requestedItems = parseRequestedItems(url.searchParams.get("symbols"), resolveSymbol, symbols);
      const batchCache = caches.default;
      const batchCacheKey = quoteBatchCacheKey(requestedItems);
      let cachedBatch = null;
      try {
        cachedBatch = await batchCache.match(batchCacheKey);
      } catch (error) {
        console.warn(`Batch cache read failed: ${String(error.message || error)}`);
      }
      if (cachedBatch) {
        const cachedResults = await cachedBatch.json();
        return json(cachedResults.map((result) => ({
          ...withQuoteAge(result, result.stale ? "STALE" : "CACHE"),
        })));
      }

      const results = [];
      const requestGapMs = Number(env.QUOTE_REQUEST_GAP_MS || 350);
      let primaryQuotesPromise = null;
      const loadPrimaryQuotes = () => {
        if (!primaryQuotesPromise) {
          primaryQuotesPromise = fetchYahooQuotes(requestedItems);
        }
        return primaryQuotesPromise;
      };
      for (let index = 0; index < requestedItems.length; index++) {
        const item = requestedItems[index];
        try {
          const response = await cachedQuote(item, env, loadPrimaryQuotes);
          const result = await response.json();
          results.push(result);
          if (index + 1 < requestedItems.length && requestGapMs > 0 && result.source === "LIVE") {
            await sleep(requestGapMs);
          }
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
      if (results.every((result) => result.ready && !result.stale)) {
        const batchTtl = Number(env.BATCH_CACHE_TTL_SECONDS || 12);
        try {
          await batchCache.put(
            batchCacheKey,
            json(results, 200, { "cache-control": `public, max-age=${batchTtl}` }),
          );
        } catch (error) {
          console.warn(`Batch cache write failed: ${String(error.message || error)}`);
        }
      }
      if (ctx && results.every((result) => result.ready)) {
        ctx.waitUntil(warmOneSupplementalItem(requestedItems, env));
      }
      return json(results);
    }

    return json({ error: "Not found" }, 404);
  },
};
