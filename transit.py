#!/usr/bin/env python3
"""Next departures Varnhem -> Scheeleparken, into transit.txt for the UI.

Skanetrafiken shut their own API down in 2021; this uses Trafiklab's
ResRobot v2.1 route planner, which aggregates it. Needs a free key from
trafiklab.se in cat_cfg.txt as transit_key.

Only fetches inside the commute window - roughly 36 calls a weekday
morning rather than 288 a day, which keeps it inside any free tier.

Writes six trips, not three: the UI drops any departure closer than
transit_lead minutes, and filtering here would go stale between runs.

  ./transit.py --lookup "Malmo Varnhem"   find a stop id
"""
import json, os, sys, time, urllib.parse, urllib.request
from datetime import datetime

os.chdir(os.path.dirname(os.path.abspath(__file__)))

def cfg(key, default=None):
    try:
        with open('cat_cfg.txt') as f:
            for line in f:
                k, _, v = line.strip().partition('=')
                if k == key and v:
                    return v
    except OSError:
        pass
    return default

KEY = cfg('transit_key')
if not KEY:
    print("no transit_key in cat_cfg.txt - see 'Transit' in README.md", file=sys.stderr)
    sys.exit(0)

def api(path, **params):
    params['accessId'] = KEY
    params['format'] = 'json'
    url = f"https://api.resrobot.se/v2.1/{path}?" + urllib.parse.urlencode(params)
    with urllib.request.urlopen(url, timeout=20) as r:
        return json.load(r)

if len(sys.argv) > 2 and sys.argv[1] == '--lookup':
    for s in api('location.name', input=sys.argv[2]).get('stopLocationOrCoordLocation', []):
        st = s.get('StopLocation') or {}
        if st:
            print(f"  {st.get('extId','?'):<12} {st.get('name','?')}")
    sys.exit(0)

ORIGIN, DEST = cfg('transit_from'), cfg('transit_to')
if not (ORIGIN and DEST):
    print("transit_from / transit_to not set in cat_cfg.txt", file=sys.stderr)
    sys.exit(0)

# Outside the window there is nothing to show, so do not spend a call on it.
def hhmm(s, dflt):
    try:
        h, m = s.split(':')
        return int(h) * 60 + int(m)
    except (AttributeError, ValueError):
        return dflt

now = datetime.now()
start = hhmm(cfg('transit_start'), 8 * 60)
end = hhmm(cfg('transit_end'), 9 * 60 + 30)
mins = now.hour * 60 + now.minute
inside = (start <= mins < end) if start < end else (mins >= start or mins < end)
if now.weekday() >= 5 or not inside:
    sys.exit(0)

try:
    d = api('trip', originId=ORIGIN, destId=DEST, numF=6)   # 6 is the API maximum; 7+ returns HTTP 400
except Exception as e:
    print("transit fetch failed: %s" % e, file=sys.stderr)
    sys.exit(0)          # keep the old file; the UI ages it out on its own

lines = []
for trip in d.get('Trip', [])[:6]:
    legs = [l for l in trip.get('LegList', {}).get('Leg', []) if l.get('type') != 'WALK']
    if not legs:
        continue
    dep, arr = legs[0]['Origin'], legs[-1]['Destination']
    fmt = '%Y-%m-%d %H:%M:%S'
    t0 = datetime.strptime(f"{dep['date']} {dep['time']}", fmt)
    t1 = datetime.strptime(f"{arr['date']} {arr['time']}", fmt)
    mins = int((t1 - t0).total_seconds() // 60)
    name = (legs[0].get('Product') or [{}])[0].get('name') if isinstance(legs[0].get('Product'), list) \
           else (legs[0].get('Product') or {}).get('name', '')
    lines.append("trip=%s|%s|%d|%d|%s" % (t0.strftime('%H:%M'), t1.strftime('%H:%M'),
                                          mins, len(legs) - 1, (name or '').strip()))

if not lines:
    sys.exit(0)

out = "updated=%d\n%s\n" % (int(time.time()), "\n".join(lines))
with open('transit.txt.tmp', 'w') as f:
    f.write(out)
os.replace('transit.txt.tmp', 'transit.txt')   # atomic: never a half-written file
print(out.strip().replace('\n', '  '))
