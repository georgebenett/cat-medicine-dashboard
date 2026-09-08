#!/usr/bin/env python3
"""Fetch the forecast into weather.txt for the dashboard to read.

Deliberately not done inside the C app: that would mean linking libcurl and
making a blocking HTTP call from inside the LVGL loop, which stalls the
panel for as long as the network takes. A timer writes a file and the UI
reads it - the same shape as cat_cfg.txt.

Open-Meteo needs no API key. Location comes from lat/lon in cat_cfg.txt.
"""
import json, os, sys, time, urllib.request

os.chdir(os.path.dirname(os.path.abspath(__file__)))

def cfg(key, default):
    try:
        with open('cat_cfg.txt') as f:
            for line in f:
                k, _, v = line.strip().partition('=')
                if k == key and v:
                    return v
    except OSError:
        pass
    return default

lat, lon = cfg('lat', '55.6078'), cfg('lon', '12.9982')
url = ("https://api.open-meteo.com/v1/forecast"
       f"?latitude={lat}&longitude={lon}"
       "&current=temperature_2m,weather_code"
       "&daily=temperature_2m_max,temperature_2m_min"
       "&timezone=auto&forecast_days=1")

try:
    with urllib.request.urlopen(url, timeout=20) as r:
        d = json.load(r)
    cur, day = d['current'], d['daily']
    out = ("temp=%.0f\ncode=%d\nhi=%.0f\nlo=%.0f\nupdated=%d\n" % (
        cur['temperature_2m'], cur['weather_code'],
        day['temperature_2m_max'][0], day['temperature_2m_min'][0],
        int(time.time())))
except Exception as e:
    # Keep the stale file: yesterday's weather beats a blank panel, and the
    # UI greys it out once it is old. The timer retries.
    print("weather fetch failed: %s" % e, file=sys.stderr)
    sys.exit(0)

# Atomic, so the UI never reads a half-written file.
with open('weather.txt.tmp', 'w') as f:
    f.write(out)
os.replace('weather.txt.tmp', 'weather.txt')
print(out.strip().replace('\n', ' '))
