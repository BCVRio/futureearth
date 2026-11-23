// Global projection tables and interpolation.
//
// Scenarios are IPCC AR6 Shared Socioeconomic Pathways:
//   ssp126 = strong mitigation ("best case")
//   ssp245 = roughly current policies ("current path")
//   ssp585 = very high emissions ("worst case")
//
// PROVENANCE / TODO(verify): the anchor values below were transcribed from the
// IPCC AR6 WG1 report (Table SPM.1 for temperature, Table 9.9 / Ch. 9 for sea
// level, Ch. 4 for the 2300 extensions). They must be checked against the
// published tables before the installation is shown publicly. Entries marked
// `est` are NOT IPCC medians: AR6 only gives a range there (or nothing for
// that scenario) and the value is a midpoint/interpolation of ours.

export const SCENARIOS = {
  ssp126: { label: 'Best case', detail: 'Strong mitigation (SSP1-2.6)' },
  ssp245: { label: 'Current path', detail: 'Roughly current policies (SSP2-4.5)' },
  ssp585: { label: 'Worst case', detail: 'Very high emissions (SSP5-8.5)' },
};

// Global mean surface warming, degC above 1850-1900. [year, value]
const GLOBAL_WARMING = {
  ssp126: [[2020, 1.2], [2030, 1.5], [2050, 1.7], [2090, 1.8], [2300, 1.5 /* est: range 1.0-2.2 */]],
  ssp245: [[2020, 1.2], [2030, 1.5], [2050, 2.0], [2090, 2.7], [2300, 3.3 /* est: range 2.3-4.6 */]],
  ssp585: [[2020, 1.2], [2030, 1.6], [2050, 2.4], [2090, 4.4], [2300, 9.5 /* est: range 6.6-14.1 */]],
};

// Global mean sea level rise, metres above the 1995-2014 mean. [year, low, median, high]
const SEA_LEVEL = {
  ssp126: [[2020, 0.04, 0.05, 0.06], [2050, 0.16, 0.19, 0.25], [2100, 0.32, 0.44, 0.62], [2150, 0.46, 0.68, 0.99], [2300, 0.3, 1.0 /* est */, 3.1]],
  ssp245: [[2020, 0.04, 0.05, 0.06], [2050, 0.17, 0.20, 0.26], [2100, 0.44, 0.56, 0.76], [2150, 0.66, 0.92, 1.33], [2300, 0.8 /* est */, 2.0 /* est */, 4.5 /* est */]],
  ssp585: [[2020, 0.04, 0.05, 0.06], [2050, 0.20, 0.23, 0.29], [2100, 0.63, 0.77, 1.01], [2150, 0.98, 1.32, 1.88], [2300, 1.7, 3.7 /* est */, 6.8]],
};

// Global warming (above 1850-1900) during the 1991-2020 local baseline period.
export const BASELINE_GLOBAL_WARMING = 0.9;

export const MIN_YEAR = 2020;
export const MAX_YEAR = 2526;

function interp(table, year, col, { extrapolate }) {
  if (year <= table[0][0]) return table[0][col];
  for (let i = 1; i < table.length; i++) {
    const [y0, y1] = [table[i - 1][0], table[i][0]];
    if (year <= y1) {
      const t = (year - y0) / (y1 - y0);
      return table[i - 1][col] + t * (table[i][col] - table[i - 1][col]);
    }
  }
  const last = table[table.length - 1];
  if (!extrapolate) return last[col];
  const prev = table[table.length - 2];
  const rate = Math.max(0, (last[col] - prev[col]) / (last[0] - prev[0]));
  return last[col] + rate * (year - last[0]);
}

/** Global warming in degC above 1850-1900. Held constant after 2300. */
export function globalWarming(scenario, year) {
  return interp(GLOBAL_WARMING[scenario], year, 1, { extrapolate: false });
}

/** Global mean sea level rise in metres. Continues at the 2150-2300 rate after 2300. */
export function seaLevelRise(scenario, year) {
  const t = SEA_LEVEL[scenario];
  const opt = { extrapolate: true };
  return { low: interp(t, year, 1, opt), median: interp(t, year, 2, opt), high: interp(t, year, 3, opt) };
}

/** How much the numbers for a given year can be trusted. */
export function confidence(year) {
  if (year <= 2100) return { level: 'projected', note: 'Within the range of mainstream climate model projections.' };
  if (year <= 2300) return { level: 'low-confidence', note: 'Extended scenario; IPCC gives wide ranges with low confidence.' };
  return { level: 'extrapolated', note: 'Beyond any model run. Temperature held at 2300; sea level continued at the 2150-2300 rate.' };
}
