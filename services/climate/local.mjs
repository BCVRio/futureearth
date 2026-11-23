// Local climate pattern for a point on Earth, from downscaled CMIP6 data
// (Open-Meteo Climate API, 10 km, 1950-2050) plus terrain elevation.
//
// The models only run to 2050, so we derive a *pattern*: how much this place
// warms / dries per degree of global warming. Any year and scenario is then
// obtained by scaling the pattern with the global curve (standard "pattern
// scaling"). The result is cached on disk; it never changes for a location.

import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const CACHE_DIR = path.join(path.dirname(fileURLToPath(import.meta.url)), '.cache');
const MODELS = ['EC_Earth3P_HR', 'MRI_AGCM3_2_S', 'MPI_ESM1_2_XR'];
const BASE = ['1991-01-01', '2020-12-31'];
const FUTURE = ['2031-01-01', '2050-12-31'];
// Global warming between the centres of the two periods (~2005 -> ~2040) under
// the high-emissions forcing these HighResMIP runs use.
const GLOBAL_DELTA_BETWEEN_PERIODS = 1.1;
export const HOT_DAY_C = 35;

const memory = new Map();

// The free Open-Meteo tier allows only about one new location per minute for
// 50 years of daily data (and is non-commercial). Set OPEN_METEO_API_KEY to use
// the paid endpoint for the public installation.
const API_KEY = process.env.OPEN_METEO_API_KEY;

async function getJson(url, attempt = 0) {
  const target = API_KEY ? url.replace('https://', 'https://customer-') + `&apikey=${API_KEY}` : url;
  const res = await fetch(target, { signal: AbortSignal.timeout(60_000) });
  if (res.status === 429 && attempt < 2) {
    await new Promise((r) => setTimeout(r, 62_000));
    return getJson(url, attempt + 1);
  }
  if (!res.ok) throw new Error(`${new URL(url).hostname} responded ${res.status}: ${(await res.text()).slice(0, 200)}`);
  return res.json();
}

async function fetchPeriod(lat, lon, [start, end]) {
  const url = 'https://climate-api.open-meteo.com/v1/climate'
    + `?latitude=${lat}&longitude=${lon}&start_date=${start}&end_date=${end}`
    + `&models=${MODELS.join(',')}&daily=temperature_2m_mean,temperature_2m_max,precipitation_sum`;
  const { daily } = await getJson(url);
  const years = (new Date(end) - new Date(start)) / (365.25 * 864e5);
  const perModel = MODELS.map((m) => {
    const tmean = daily[`temperature_2m_mean_${m}`];
    const tmax = daily[`temperature_2m_max_${m}`];
    const precip = daily[`precipitation_sum_${m}`];
    if (!tmean || !tmax || !precip) return null;
    const clean = (a) => a.filter((v) => v != null);
    const avg = (a) => a.reduce((s, v) => s + v, 0) / a.length;
    return {
      tmean: avg(clean(tmean)),
      precipPerYear: clean(precip).reduce((s, v) => s + v, 0) / years,
      tmaxDaily: clean(tmax),
    };
  }).filter(Boolean);
  if (!perModel.length) throw new Error('Climate API returned no usable model data for this location.');
  const mean = (f) => perModel.reduce((s, m) => s + f(m), 0) / perModel.length;
  return { tmean: mean((m) => m.tmean), precipPerYear: mean((m) => m.precipPerYear), perModel, years };
}

async function build(lat, lon) {
  const [base, future, elev] = await Promise.all([
    fetchPeriod(lat, lon, BASE),
    fetchPeriod(lat, lon, FUTURE),
    getJson(`https://api.open-meteo.com/v1/elevation?latitude=${lat}&longitude=${lon}`),
  ]);

  // Histogram of baseline daily max temperature (1 degC bins, -60..70), averaged
  // over models, in days per year. Lets us count hot days for any warming level
  // by shifting the distribution, without keeping the raw daily series.
  const hist = new Array(131).fill(0);
  for (const m of base.perModel) {
    for (const t of m.tmaxDaily) hist[Math.min(130, Math.max(0, Math.round(t) + 60))] += 1 / (base.years * base.perModel.length);
  }

  return {
    lat, lon,
    elevation_m: elev.elevation?.[0] ?? null,
    baseline: {
      period: '1991-2020',
      mean_temp_c: round(base.tmean, 2),
      precip_mm_per_year: round(base.precipPerYear, 0),
    },
    // Local degC per degC of global warming.
    amplification: round((future.tmean - base.tmean) / GLOBAL_DELTA_BETWEEN_PERIODS, 2),
    // Percent change in annual precipitation per degC of global warming.
    precip_pct_per_degc: round(((future.precipPerYear / base.precipPerYear) - 1) * 100 / GLOBAL_DELTA_BETWEEN_PERIODS, 1),
    tmax_hist_days_per_year: hist.map((v) => round(v, 3)),
    models: MODELS,
  };
}

/** Days per year with max temperature >= HOT_DAY_C after `localWarming` degC of warming. */
export function hotDays(pattern, localWarming) {
  let days = 0;
  pattern.tmax_hist_days_per_year.forEach((n, i) => {
    if (i - 60 + localWarming >= HOT_DAY_C) days += n;
  });
  return days;
}

/** Cached local pattern. Coordinates are snapped to 0.05 deg (~5 km), finer than the model grid. */
export async function localPattern(lat, lon) {
  const snap = (v) => (Math.round(v * 20) / 20).toFixed(2);
  const key = `${snap(lat)}_${snap(lon)}`;
  if (memory.has(key)) return memory.get(key);
  const file = path.join(CACHE_DIR, `${key}.json`);
  const promise = (async () => {
    try { return JSON.parse(await readFile(file, 'utf8')); } catch { /* not cached yet */ }
    const pattern = await build(Number(snap(lat)), Number(snap(lon)));
    await mkdir(CACHE_DIR, { recursive: true });
    await writeFile(file, JSON.stringify(pattern));
    return pattern;
  })();
  memory.set(key, promise);
  promise.catch(() => memory.delete(key));
  return promise;
}

function round(v, d) { const f = 10 ** d; return Math.round(v * f) / f; }
