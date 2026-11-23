// FutureEarth climate service.
//
//   GET /future?lat=..&lon=..&year=2076&scenario=ssp245
//
// Returns one "FutureState": the projected climate at that place and year plus
// the 0..1 parameters the Unreal scene uses to dress the world. No dependencies;
// run with `node server.mjs` (Node 18+).

import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import {
  SCENARIOS, MIN_YEAR, MAX_YEAR, BASELINE_GLOBAL_WARMING,
  globalWarming, seaLevelRise, confidence,
} from './projections.mjs';
import { localPattern, hotDays, HOT_DAY_C } from './local.mjs';

const PORT = Number(process.env.PORT) || 8787;
const PUBLIC_DIR = path.join(path.dirname(fileURLToPath(import.meta.url)), 'public');
const clamp01 = (v) => Math.min(1, Math.max(0, v));
const round = (v, d = 2) => Math.round(v * 10 ** d) / 10 ** d;

export async function futureState(lat, lon, year, scenario) {
  const p = await localPattern(lat, lon);
  const gw = globalWarming(scenario, year);
  const localWarming = p.amplification * Math.max(0, gw - BASELINE_GLOBAL_WARMING);
  const precipPct = Math.max(-90, p.precip_pct_per_degc * Math.max(0, gw - BASELINE_GLOBAL_WARMING));
  const slr = seaLevelRise(scenario, year);
  const hotNow = hotDays(p, 0);
  const hotThen = hotDays(p, localWarming);

  // --- Visual drivers -------------------------------------------------------
  // These are art-direction heuristics, not published indices. They turn the
  // physical numbers above into 0..1 dials for materials, foliage and weather.
  const heat = clamp01(hotThen / 120);
  const drying = clamp01(-precipPct / 40 + localWarming / 12);
  const floodDepth = p.elevation_m == null ? null : Math.max(0, slr.median - p.elevation_m);

  return {
    location: { lat: p.lat, lon: p.lon, elevation_m: p.elevation_m },
    year,
    scenario: { id: scenario, ...SCENARIOS[scenario] },
    confidence: confidence(year),
    global: {
      warming_c_above_preindustrial: round(gw),
      sea_level_rise_m: { low: round(slr.low), median: round(slr.median), high: round(slr.high) },
    },
    local: {
      baseline: { ...p.baseline, hot_days_per_year: round(hotNow, 0) },
      warming_c_above_baseline: round(localWarming),
      mean_temp_c: round(p.baseline.mean_temp_c + localWarming, 1),
      precip_change_pct: round(precipPct, 0),
      precip_mm_per_year: round(p.baseline.precip_mm_per_year * (1 + precipPct / 100), 0),
      hot_days_per_year: round(hotThen, 0),
      hot_day_threshold_c: HOT_DAY_C,
      warming_amplification: p.amplification,
    },
    visual: {
      // Height of the sea surface above today's mean sea level. Place the water plane here.
      water_level_m: round(slr.median),
      water_level_high_m: round(slr.high),
      flood_depth_m: floodDepth == null ? null : round(floodDepth),
      inundated: floodDepth != null && floodDepth > 0,
      heat_stress: round(heat),
      aridity: round(drying),
      vegetation_dieback: round(clamp01(0.6 * drying + 0.4 * heat)),
      fire_risk: round(clamp01(0.5 * drying + 0.5 * heat)),
    },
    caveats: [
      'Sea level is the global mean; local land subsidence/uplift, tides and storm surge are not yet included.',
      'Elevation is from a 90 m terrain model and is too coarse to decide flooding street by street.',
      'Local values beyond 2050 are pattern-scaled from 1991-2050 model output, not directly simulated.',
    ],
    sources: [
      'IPCC AR6 WG1 (global temperature and sea level by SSP)',
      `Open-Meteo Climate API, CMIP6 HighResMIP downscaled to 10 km (${p.models.join(', ')})`,
      'Open-Meteo Elevation API (Copernicus DEM GLO-90)',
    ],
  };
}

function send(res, status, body, type = 'application/json') {
  res.writeHead(status, { 'content-type': type, 'access-control-allow-origin': '*', 'cache-control': 'no-store' });
  res.end(type === 'application/json' ? JSON.stringify(body) : body);
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://localhost');
  try {
    if (url.pathname === '/future') {
      const lat = Number(url.searchParams.get('lat'));
      const lon = Number(url.searchParams.get('lon'));
      const year = Math.round(Number(url.searchParams.get('year') ?? 2076));
      const scenario = url.searchParams.get('scenario') ?? 'ssp245';
      if (!url.searchParams.has('lat') || !url.searchParams.has('lon') || !(Math.abs(lat) <= 90) || !(Math.abs(lon) <= 180)) {
        return send(res, 400, { error: 'lat (-90..90) and lon (-180..180) are required.' });
      }
      if (!(year >= MIN_YEAR && year <= MAX_YEAR)) return send(res, 400, { error: `year must be ${MIN_YEAR}..${MAX_YEAR}.` });
      if (!SCENARIOS[scenario]) return send(res, 400, { error: `scenario must be one of ${Object.keys(SCENARIOS).join(', ')}.` });
      return send(res, 200, await futureState(lat, lon, year, scenario));
    }
    if (url.pathname === '/scenarios') return send(res, 200, { scenarios: SCENARIOS, min_year: MIN_YEAR, max_year: MAX_YEAR });
    if (url.pathname === '/' || url.pathname === '/index.html') {
      return send(res, 200, await readFile(path.join(PUBLIC_DIR, 'index.html')), 'text/html; charset=utf-8');
    }
    send(res, 404, { error: 'Not found' });
  } catch (err) {
    console.error(err);
    send(res, 502, { error: String(err.message ?? err) });
  }
});

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  server.listen(PORT, () => console.log(`FutureEarth climate service on http://localhost:${PORT}`));
}
