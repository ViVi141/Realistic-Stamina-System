"""
Temperature Model Analysis and Calibration Tool

Implements the same one-layer transient energy balance model we added to
`EnvironmentFactor.StepTemperature()` in Enfusion, but in Python for easy
analysis, plotting and parameter sweeps.

Usage examples:
  python temperature_model_analysis.py --lats 0 30 60 --days 172 355 --dt 300 --out results

Outputs per (lat, day): CSV file and optional PNG plot (requires matplotlib).

Dependencies: numpy, matplotlib (optional), pandas (optional but convenient)

"""
from __future__ import annotations
import math
import os
import argparse
from typing import List, Tuple

import numpy as np
import csv

try:
    import matplotlib.pyplot as plt
    HAS_MPL = True
except Exception:
    HAS_MPL = False

# Physical constants
S0 = 1361.0  # Solar constant W/m2
SIGMA = 5.670374419e-8  # Stefan-Boltzmann
RHO_AIR = 1.225  # kg/m3
CP_AIR = 1004.0  # J/(kg K)

# Default model parameters (in sync with Enfusion defaults)
DEFAULTS = {
    'albedo': 0.2,
    'aod': 0.14,  # aerosol optical depth (for tau)
    'mixing_height': 1000.0,  # m
    'emissivity': 0.98,
    'initial_T': 15.0,  # °C
    'cloud_factor': 0.2,
    'surface_wetness': 0.0,
}


def day_of_year(year: int, month: int, day: int) -> int:
    mdays = [31,28,31,30,31,30,31,31,30,31,30,31]
    is_leap = (year % 4 == 0 and year % 100 != 0) or (year % 400 == 0)
    if is_leap:
        mdays[1] = 29
    n = sum(mdays[:month-1]) + day
    return n


def solar_declination(n: int) -> float:
    # radians
    return math.radians(23.44) * math.sin(2.0 * math.pi * (284 + n) / 365.0)


def solar_cos_zenith(lat_deg: float, n: int, local_hour: float) -> float:
    lat = math.radians(lat_deg)
    decl = solar_declination(n)
    hour_angle = math.radians(15.0 * (local_hour - 12.0))
    cos_theta = math.sin(lat) * math.sin(decl) + math.cos(lat) * math.cos(decl) * math.cos(hour_angle)
    return max(cos_theta, 0.0)


def air_mass(cos_theta: float) -> float:
    if cos_theta <= 0.0:
        return 1e6
    theta_deg = math.degrees(math.acos(cos_theta))
    m = 1.0 / (cos_theta + 0.50572 * ((96.07995 - theta_deg) ** -1.6364))
    return m


def clear_sky_transmittance(m: float, aod: float) -> float:
    tau = math.exp(-aod * m)
    return max(0.0, min(1.0, tau))


def infer_cloud_factor(rain_intensity: float, surface_wetness: float, weather_state: str | None = None) -> float:
    cloud = max(0.0, rain_intensity)
    cloud = max(cloud, surface_wetness * 0.8)
    if weather_state:
        s = weather_state.lower()
        if 'storm' in s or 'heavy' in s:
            cloud = max(cloud, 0.95)
        elif 'rain' in s or 'shower' in s or 'overcast' in s or 'cloud' in s:
            cloud = max(cloud, 0.6)
        elif 'partly' in s:
            cloud = max(cloud, 0.25)
    return max(0.0, min(1.0, cloud))


class TemperatureSimulator:
    def __init__(self, params: dict = None):
        self.p = DEFAULTS.copy()
        if params:
            self.p.update(params)

    def simulate_day(self, lat: float, lon: float, year: int, month: int, day: int, dt: float = 300.0,
                     initial_T: float | None = None, rain_intensity: float = 0.0,
                     weather_state: str | None = None, timezone: float = 0.0,
                     elevation: float = 0.0, wind: float = 0.0, cloud_override: float | None = None) -> Tuple[np.ndarray, np.ndarray, dict]:
        n = day_of_year(year, month, day)
        times = np.arange(0.0, 24.0, dt / 3600.0)  # hours
        T = np.zeros_like(times)
        SW = np.zeros_like(times)
        LWd = np.zeros_like(times)
        NET = np.zeros_like(times)
        cloudf = np.zeros_like(times)

        T[0] = initial_T if initial_T is not None else self.p['initial_T']

        for i in range(1, len(times)):
            tod_mid = 0.5 * (times[i] + times[i-1])

            # apply longitude/timezone correction to obtain local solar time
            local_hour = tod_mid + lon / 15.0 - timezone
            # normalize to [0,24)
            local_hour = (local_hour + 24.0) % 24.0

            cos_theta = solar_cos_zenith(lat, n, local_hour)
            I0 = S0 * (1.0 + 0.033 * math.cos(2 * math.pi * n / 365.0)) * cos_theta

            m = air_mass(cos_theta)
            tau = clear_sky_transmittance(m, self.p['aod'])

            # cloud factor: allow forcing via cloud_override or deduced from inputs
            if cloud_override is not None:
                cloud = max(0.0, min(1.0, cloud_override))
            else:
                cloud = infer_cloud_factor(rain_intensity, self.p['surface_wetness'], weather_state)

            cloud_block = 0.7 * cloud

            sw_down = I0 * tau * (1.0 - cloud_block)

            # elevation effects: lapse rate adjustment for atmospheric temperature
            lapse_rate = 6.5  # °C per 1000m
            elev_adjust = lapse_rate * (elevation / 1000.0)

            T_atm = T[i-1] - elev_adjust + 2.0
            eps_atm = 0.78 + 0.14 * cloud
            lw_down = eps_atm * SIGMA * ((T_atm + 273.15) ** 4)
            lw_up = self.p['emissivity'] * SIGMA * ((T[i-1] + 273.15) ** 4)

            net_rad = (1.0 - self.p['albedo']) * sw_down + lw_down - lw_up

            LE = 200.0 * self.p['surface_wetness']

            # wind coupling increases mixing height, thus reducing dT magnitude
            wind_factor = 1.0 + (wind / 10.0)
            mixing_height_eff = max(10.0, self.p['mixing_height'] * wind_factor)

            # air density decreases with elevation (approx scale height 8434 m)
            rho_elev = RHO_AIR * math.exp(-elevation / 8434.0)

            Q_net = net_rad - LE
            dT = (Q_net * (dt)) / (rho_elev * CP_AIR * mixing_height_eff)

            newT = T[i-1] + dT
            # clamp
            newT = max(-80.0, min(60.0, newT))

            T[i] = newT
            SW[i] = sw_down
            LWd[i] = lw_down
            NET[i] = Q_net
            cloudf[i] = cloud

        meta = {'lat': lat, 'date': f'{year}-{month:02d}-{day:02d}', 'params': self.p}
        return times, {'T': T, 'SW': SW, 'LWd': LWd, 'NET': NET, 'cloud': cloudf}, meta


def save_csv(out_path: str, times: np.ndarray, series: dict):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        header = ['hour', 'T_C', 'SW_down_Wm2', 'LW_down_Wm2', 'NetRadiation_Wm2', 'cloud']
        writer.writerow(header)
        for i, t in enumerate(times):
            row = [t, series['T'][i], series['SW'][i], series['LWd'][i], series['NET'][i], series['cloud'][i]]
            writer.writerow(row)


def plot_series(times: np.ndarray, series: dict, out_png: str, title: str = ''):
    if not HAS_MPL:
        return
    plt.figure(figsize=(8, 4))
    plt.plot(times, series['T'], label='T (C)')
    plt.plot(times, series['SW'] / 50.0 - 10.0, label='SW_down scaled')
    plt.xlabel('Hour')
    plt.title(title)
    plt.legend()
    plt.grid(True)
    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    plt.savefig(out_png, dpi=150)
    plt.close()


def main():
    parser = argparse.ArgumentParser(description='Temperature model analysis')
    parser.add_argument('--lats', type=float, nargs='+', default=[0.0, 30.0, 60.0])
    parser.add_argument('--lons', type=float, nargs='+', default=[0.0], help='longitudes in degrees (used for solar time correction)')
    parser.add_argument('--days', type=str, nargs='+', default=['172', '355'])
    parser.add_argument('--year', type=int, default=2024)
    parser.add_argument('--dt', type=float, default=300.0, help='time step in seconds')
    parser.add_argument('--out', type=str, default='tools/temperature_results')
    parser.add_argument('--plot', action='store_true')
    parser.add_argument('--surface-wetness', type=float, default=0.0)
    parser.add_argument('--cloud', type=float, default=None, help='force cloud factor (0..1)')
    parser.add_argument('--wind', type=float, nargs='+', default=[0.0], help='wind speeds in m/s')
    parser.add_argument('--elevation', type=float, nargs='+', default=[0.0], help='elevation in meters')
    parser.add_argument('--timezone', type=float, default=0.0, help='timezone offset in hours (e.g. UTC+1 => 1.0)')
    parser.add_argument('--weather-state', type=str, default=None, help='optional weather state name to infer cloud factor')
    parser.add_argument('--initial-T', type=float, default=None, help='initial surface temperature override (°C)')

    args = parser.parse_args()

    sim = TemperatureSimulator({'surface_wetness': args.surface_wetness})

    for lat in args.lats:
        for lon in args.lons:
            for elev in args.elevation:
                for wind in args.wind:
                    for daystr in args.days:
                        # allow day as int or month-day string like 06-21
                        if '-' in daystr:
                            m, d = [int(x) for x in daystr.split('-')]
                            month, day = m, d
                        else:
                            # treat as day-of-year
                            day_of_year_val = int(daystr)
                            # convert day_of_year to month/day approximately using non-leap 2021
                            # simple search
                            month = 1
                            mdays = [31,28,31,30,31,30,31,31,30,31,30,31]
                            doy = day_of_year_val
                            for mi in range(12):
                                if doy > mdays[mi]:
                                    doy -= mdays[mi]
                                    month += 1
                                else:
                                    day = doy
                                    break

                        times, series, meta = sim.simulate_day(lat, lon, args.year, month, day, dt=args.dt,
                                                               initial_T=args.initial_T if args.initial_T is not None else sim.p['initial_T'],
                                                               rain_intensity=0.0,
                                                               weather_state=args.weather_state,
                                                               timezone=args.timezone,
                                                               elevation=elev,
                                                               wind=wind,
                                                               cloud_override=args.cloud)

                        tag = f'lat{int(lat)}_lon{int(lon)}_e{int(elev)}_w{int(wind)}_d{month:02d}{day:02d}'
                        csv_path = os.path.join(args.out, f'{tag}.csv')
                        save_csv(csv_path, times, series)
                        print(f'Saved {csv_path}')

                        if args.plot and HAS_MPL:
                            png_path = os.path.join(args.out, f'{tag}.png')
                            title = f'Lat {lat} Lon {lon} Elev {elev} Wind {wind} Day {month:02d}-{day:02d}'
                            plot_series(times, series, png_path, title)
                            print(f'Saved {png_path}')


if __name__ == '__main__':
    main()
