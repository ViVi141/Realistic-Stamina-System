# Temperature Model Update

Date: 2026-02-07

Status: Development version, not officially released.

## Summary
We upgraded the stamina system temperature calculation from a simple day-night curve to a physics-based model. The new model explicitly accounts for geographic and physical variables, producing more natural temperature behavior.

## What Changed
- Replaced the fixed cosine day-night curve with a physics-based energy balance and stepped updates.
- Clearer geographic and astronomical influence: latitude, solar elevation, sunrise/sunset, and seasonal variation affect peak temperature and day-night amplitude.
- Stronger weather and surface effects: cloud/rain reduce shortwave radiation, surface moisture increases latent heat loss, and wind raises effective mixing height to cool faster.
- Smoother temperature transitions with more realistic warming and cooling pacing.

## Player-facing: What affects temperature (summary) 🌤️❄️🌧️
These are the factors you will notice in-game and that affect surface temperature and stamina recovery:

- **Sun / Time (time of day, latitude, season)**: Midday is hottest, nights are cold; equatorial maps show small diurnal swing, polar maps have extreme day/night differences.
- **Clouds / Rain**: Clouds reduce daytime warming and reduce nighttime cooling; rain makes surfaces wet and increases latent heat loss (harder to warm up).
- **Surface wetness / mud**: Wet surfaces increase latent heat loss and affect movement (slower, slip risk) and stamina costs.
- **Wind speed**: Strong wind increases cooling and raises the mixing height, leading to faster temperature equilibration.
- **Surface type (albedo/emissivity)**: Snow/bright surfaces reflect more sunlight; dark surfaces absorb more heat.
- **Mixing height / approximate elevation**: Affects thermal inertia; wind and terrain affect how quickly temperature responds.
- **Aerosols / visibility**: High aerosol load reduces incoming shortwave, cooling daytime temperatures.
- **Moon phase / night illumination**: Moonlight has a very small effect (full moon nights slightly brighter/warmer), not a primary driver.
- **Indoors / cover**: Being under roof/shelter reduces direct radiative effects and rain exposure.
- **Server/admin overrides**: Admins can force temperatures or set explicit lat/long/timezone which override automatic estimation.

### How to observe & react (player tips) 💡
- Avoid prolonged exposure at midday; take shelter or slow down to reduce overheating and stamina drain. At night stay indoors or use warm equipment to maintain recovery rates.
- After rain, surfaces stay wet; seek shelter to avoid increased latent cooling and wet weight.
- Use terrain and shelter to mitigate wind chill; mountain maps may have stronger day/night swings.

### Admin & ops tips 🔧
- For maps with inconsistent day/night, admins can set latitude/longitude or timezone via `SetCurrentLatitude/SetCurrentLongitude/SetTimeZoneOffset` to ensure consistent solar timing.
- Logs to inspect: `[RealisticSystem][TempStep]` (temperature step summary), `[RealisticSystem][LocationEstimate]` (estimated lat/lon & confidence), `[RealisticSystem][WeatherDebug]` (weather overrides and engine state).
- If confidence is low (log shows Conf < 0.5), consider manually setting map coordinates or timezone to remove ambiguity.

## Notes
- The new model requires a short "equilibrium" period to stabilize after startup or major weather changes.
- Server-enforced temperature/weather settings take precedence over the model.

## Feedback
When reporting feedback, please include map, latitude (if known), time of day, weather, and how it felt. This helps us tune the model further.

## Factors and Formulas

The following matches the C code logic and variable names for direct comparison.

### 1) Inputs and state
- Weather/time manager: `m_pCachedWeatherManager`
- Location/time: `GetCurrentLatitude()`, `GetTimeOfTheDay()`, `GetDate()`
- Timezone: `m_bUseEngineTimezone`, `m_fTimeZoneOffsetHours`
- Temperature source: `m_bUseEngineTemperature`, `m_bUseEngineWeather`
- Physical parameters: `m_fSolarConstant`, `m_fAerosolOpticalDepth`, `m_fCloudBlockingCoeff`, `m_fAlbedo`, `m_fSurfaceEmissivity`, `m_fLECoef`, `m_fTemperatureMixingHeight`
- Environment values: `m_fCachedSurfaceWetness`, `m_fCachedRainIntensity`, `m_fCachedWindSpeed`
- Cached temps: `m_fCachedSurfaceTemperature`, `m_fCachedTemperature`
- Constants: `STEFAN_BOLTZMANN`, `M_E`, `rho = 1.225`, `Cp = 1004`

### 2) Temperature source selection
- If engine temperature & weather are enabled, the system uses engine-provided min/max daily temperatures as the baseline and steps the surface temperature at fixed intervals (`m_fTempUpdateInterval`, default 5 s).
- If engine temperature is not used, an equilibrium temperature is computed from physical balance (net radiation ≈ 0) and then updated with the same time-stepping.
- In both cases the current near-surface temperature is stored in `m_fCachedSurfaceTemperature` and exposed as the system temperature.

### 3) StepTemperature flow
Overview: The surface temperature is updated in time steps dt (default 5 s) with the following conceptual steps:
- Compute local solar geometry (year-day n, local hour) and the solar zenith to decide shortwave availability.
- Estimate shortwave (SW_down) using top-of-atmosphere irradiance scaled by cosθ and clear-sky transmittance τ; estimate longwave down and up fluxes using empirical atmospheric emissivity and surface emissivity.
- Net radiation Q_rad = (1 - albedo) * SW_down + LW_down - LW_up. Subtract latent heat loss LE (proportional to surface wetness).
- Account for wind influence on mixing-height H_eff; compute temperature change dT = Q_net * dt / (ρ * Cp * H_eff).
- Update and clamp surface temperature to a safe range [-80°C, +60°C]. Key formulas include I0 = S*(1+0.033 cos(2π n/365))*cosθ, τ ≈ exp(-AOD * m), and LE ≈ m_fLECoef * surfaceWetness.

### 4) AirMass and SolarCosZenith
- Solar declination: δ ≈ 23.44° × sin(2π (284 + n) / 365).
- Solar zenith cosine cosθ is computed from latitude, declination and local hour using standard solar geometry; cosθ ≤ 0 indicates night.
- Air mass (Kasten & Young) is an empirical function of zenith angle used to scale atmospheric optical thickness at low sun angles.

### 5) InferCloudFactor
- The cloud factor is inferred from rain intensity, surface wetness and weather state tags (range 0..1). High rain/storm states force the factor near 1; partly/cloudy states yield lower values. This factor adjusts shortwave blocking and empirical atmospheric emissivity.

### 6) CalculateTemperatureFromAPI
- When using engine temperature, the system uses engine-provided daily min/max and interpolates through the day using a cosine-shaped diurnal curve (peak typically near 14:00). If min and max are nearly equal, the system falls back to the physical equilibrium solver.

### 7) CalculateEquilibriumTemperatureFromPhysics
- The equilibrium temperature is found numerically (e.g., bisection) by solving for T such that net radiation at the surface is approximately zero. Inputs are year-day, time, latitude and cloud factor. If the solver cannot bracket a root, the model falls back to a simplified diurnal curve.

### 8) NetRadiationAtSurface
- Components: top-of-atmosphere irradiance scaled by seasonal factor and cosθ; air mass scaled clear-sky transmittance; cloud blocking; empirical atmospheric emissivity for LW_down; surface emissivity for LW_up; latent heat loss LE proportional to surface wetness.
- Net radiation = (1 - albedo) * SW_down + LW_down - LW_up - LE, used as the heat source/sink in the equilibrium solver and time-stepping.
