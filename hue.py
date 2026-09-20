#!/usr/bin/env python3
"""Philips Hue room control, local only - no cloud, no Google account.

The bridge speaks a documented REST API (CLIP v2) on the LAN, so the whole
Google Home detour is unnecessary: press the bridge button once, keep the
application key, and every light paired to it - Hue-branded or not - is
reachable in about 20ms.

Unlike weather.py and transit.py this is a daemon, not a timer job: a light
switch that reacts in five minutes is not a light switch. It runs two ways
at once:

  hue.txt   state, rewritten every POLL seconds       (UI reads)
  hue.cmd   commands, one per line, consumed at once  (UI writes)

The UI moves its own toggle immediately and lets the next poll confirm it,
so a press feels instant even though the bridge round-trip happens here.
Rooms are addressed by index, not UUID, to keep 36-char identifiers out of
ui.c - the order is sorted by name, so it is stable across restarts.

  ./hue.py --pair     press the bridge button first, prints the key
  ./hue.py --once     one poll, print state, exit
"""
import json, os, ssl, sys, time, urllib.request
from datetime import datetime, timedelta

os.chdir(os.path.dirname(os.path.abspath(__file__)))

POLL = 2.0          # on/off + brightness; the bridge is on the LAN, this is cheap
STRUCT_EVERY = 30.0 # rooms, bulb membership, mesh health - all near-static
AUTO_EVERY = 60.0   # the daylight curve; it moves far slower than a minute
CMD_POLL = 0.2      # how fast a press reaches the bulb

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

BRIDGE = cfg('hue_bridge')
KEY = cfg('hue_key')

# The bridge serves a self-signed certificate tied to its bridge id, so the
# usual chain check cannot pass. It is a device on our own LAN reached by
# IP, and the application key is the actual authentication.
CTX = ssl.create_default_context()
CTX.check_hostname = False
CTX.verify_mode = ssl.CERT_NONE


def api(path, method='GET', body=None):
    req = urllib.request.Request(
        f"https://{BRIDGE}/clip/v2/resource/{path}", method=method,
        data=json.dumps(body).encode() if body else None,
        headers={'hue-application-key': KEY, 'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=8, context=CTX) as r:
        return json.load(r)


def pair():
    """One-time: POST within 30s of pressing the round button."""
    req = urllib.request.Request(
        f"https://{BRIDGE}/api", method='POST',
        data=json.dumps({'devicetype': 'cat-dashboard#raspi'}).encode(),
        headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=8, context=CTX) as r:
        d = json.load(r)[0]
    if 'error' in d:
        print("pairing failed: %s" % d['error']['description'], file=sys.stderr)
        print("press the round button on the bridge, then run this again", file=sys.stderr)
        return 1
    print("hue_key=%s" % d['success']['username'])
    print("\nadd that line to cat_cfg.txt (it is gitignored)")
    return 0


def structure():
    """[{gid,name,reachable,mirek}] sorted by name - index is the address.

    Rooms, bulb membership and mesh connectivity barely ever change, so
    this runs on its own slow clock while the on/off state is polled.
    """
    rs = api('room')['data']
    devices = {d['id']: d for d in api('device')['data']}
    lights = {l['id']: l for l in api('light')['data']}
    conn = {z.get('owner', {}).get('rid'): z.get('status')
            for z in api('zigbee_connectivity')['data']}

    out = []
    for r in rs:
        gid = next((s['rid'] for s in r['services'] if s['rtype'] == 'grouped_light'), None)
        if not gid:
            continue
        mireks, reach = [], False
        lo, hi = 153, 500
        for c in r['children']:
            dev = devices.get(c['rid'])
            if not dev:
                continue
            # One bulb off the mesh in a four-spot room still leaves
            # something worth controlling; only call the room unreachable
            # when nothing in it answers.
            if conn.get(dev['id']) == 'connected':
                reach = True
            for s in dev['services']:
                if s['rtype'] != 'light':
                    continue
                ct = (lights.get(s['rid']) or {}).get('color_temperature') or {}
                if ct.get('mirek'):
                    mireks.append(ct['mirek'])
                # Bulbs disagree about how cool they go - the ambiance spots
                # stop at 454 (2200K), not the 500 the API allows. Keep the
                # narrowest range so a preset never asks for a white the
                # room cannot make.
                sch = ct.get('mirek_schema') or {}
                if sch.get('mirek_minimum'):
                    lo = max(lo, sch['mirek_minimum'])
                if sch.get('mirek_maximum'):
                    hi = min(hi, sch['mirek_maximum'])
        out.append({'gid': gid, 'name': r['metadata']['name'],
                    'reachable': 1 if reach else 0,
                    # 0 means the room has no tunable-white bulbs at all.
                    'mirek': int(sum(mireks) / len(mireks)) if mireks else 0,
                    'lo': lo, 'hi': hi,
                    'on': 0, 'bri': 0})
    out.sort(key=lambda x: x['name'].lower())
    return out


def read_state(rs):
    groups = {g['id']: g for g in api('grouped_light')['data']}
    for r in rs:
        g = groups.get(r['gid'], {})
        r['on'] = 1 if g.get('on', {}).get('on') else 0
        r['bri'] = int(round(g.get('dimming', {}).get('brightness', 0)))
    return rs


def write_state(rs):
    lines = ["updated=%d" % int(time.time()),
             "auto=%d|%s|%s" % (AUTO_STATUS['on'], AUTO_STATUS['why'],
                                AUTO_STATUS['room'].replace('|', ' '))]
    for r in rs:
        lines.append("room=%s|%d|%d|%d|%d" % (r['name'].replace('|', ' '),
                                              r['on'], r['bri'],
                                              r['reachable'], r['mirek']))
    out = "\n".join(lines) + "\n"
    with open('hue.txt.tmp', 'w') as f:
        f.write(out)
    os.replace('hue.txt.tmp', 'hue.txt')     # atomic: never a half-written file
    return out


# --- daylight automation ---------------------------------------------
#
# Keeps one room lit across the day: daylight-white when the window opens,
# warming to candle-ish by the time it closes. The warm half is anchored to
# the real sunset rather than a fixed hour, because in Malmo that moves from
# 16:37 in December to 21:55 in June - a clock-based curve would be warming
# the hallway at lunchtime in winter and still cold at bedtime in summer.
#
# Interpolation is in mireks, not kelvin, on purpose: mireks are the
# perceptually even unit, so a straight line between two of them looks like
# a straight fade. The same line in kelvin crawls at the warm end and races
# at the cool one.


def hhmm(s, dflt):
    try:
        h, m = s.split(':')
        return int(h) * 60 + int(m)
    except (AttributeError, ValueError):
        return dflt


def auto_cfg():
    return {
        'on':    int(cfg('hue_auto', '0')),
        'room':  cfg('hue_auto_room', 'Hallway'),
        'from':  hhmm(cfg('hue_auto_from', '07:00'), 7 * 60),
        'to':    hhmm(cfg('hue_auto_to', '22:00'), 22 * 60),
        'day':   int(cfg('hue_auto_day', '80')),
        'night': int(cfg('hue_auto_night', '35')),
        'ramp':  int(cfg('hue_auto_ramp', '30')),
        'cool':  int(cfg('hue_auto_cool', '200')),    # 5000K, window opens
        'warm':  int(cfg('hue_auto_warm', '370')),    # 2700K, at sunset
        'late':  int(cfg('hue_auto_late', '454')),    # 2200K, window closes
        # Hours of daylight above which the room lights itself through the
        # window and this is just burning power. 16h skips roughly May to
        # early August in Malmo; 0 disables the opt-out.
        'skip':  float(cfg('hue_auto_skip_daylen', '16')),
    }


def solar():
    """(sunrise, sunset) in minutes past midnight, from weather.txt.

    Age does not matter much - sunset shifts about two minutes a day, so a
    file from last week is still close enough to steer a light by. That is
    why a stale weather.txt is used rather than treated as missing.
    """
    out = {}
    try:
        with open('weather.txt') as f:
            for line in f:
                k, _, v = line.strip().partition('=')
                if k in ('sunrise', 'sunset') and v:
                    out[k] = hhmm(v, None)
    except OSError:
        pass
    return out.get('sunrise'), out.get('sunset')


def lerp(a, b, t):
    return a + (b - a) * max(0.0, min(1.0, t))


def auto_target(mins, sunrise, sunset, a):
    """(brightness, mirek) for this minute, or None to leave the room alone.

    None means "not our business right now" - outside the window, or a
    summer day long enough that the automation opts out. It deliberately
    does not mean "off": switching off is an edge the caller fires once
    when the window closes, so a lamp turned on by hand at 3am is not
    fought a minute later.
    """
    if not (a['from'] <= mins < a['to']):
        return None
    if a['skip'] > 0 and sunrise is not None and sunset is not None \
       and (sunset - sunrise) >= a['skip'] * 60:
        return None

    # Keep the anchor strictly inside the window, so a midsummer sunset
    # past closing time still leaves a sane two-part curve.
    ss = sunset if sunset is not None else (a['from'] + a['to']) // 2
    ss = max(a['from'] + 1, min(a['to'] - 1, ss))

    if mins < ss:
        mirek = lerp(a['cool'], a['warm'], (mins - a['from']) / float(ss - a['from']))
    else:
        mirek = lerp(a['warm'], a['late'], (mins - ss) / float(a['to'] - ss))

    if a['ramp'] > 0 and mins < a['from'] + a['ramp']:
        bri = lerp(1, a['day'], (mins - a['from']) / float(a['ramp']))
    elif mins < ss:
        bri = a['day']
    else:
        bri = lerp(a['day'], a['night'], (mins - ss) / float(a['to'] - ss))

    return int(round(bri)), int(round(mirek))


def apply(rs, line):
    """'set <idx> <on> <bri> [mirek]' - absolute: the UI knows the state.

    mirek 0, or a room with no tunable-white bulbs, leaves the temperature
    alone - the bridge rejects color_temperature on a fixed-white group.
    """
    p = line.split()
    if len(p) not in (4, 5) or p[0] != 'set':
        return
    try:
        idx, on, bri = int(p[1]), int(p[2]), int(p[3])
        mirek = int(p[4]) if len(p) == 5 else 0
    except ValueError:
        return
    if not 0 <= idx < len(rs):
        return
    body = {'on': {'on': bool(on)}}
    # Sending brightness 0 turns a bulb off rather than dimming it, and the
    # bridge rejects it outright on some firmwares.
    if on and bri > 0:
        body['dimming'] = {'brightness': max(1, min(100, bri))}
    if on and mirek and rs[idx]['mirek']:
        mirek = max(rs[idx]['lo'], min(rs[idx]['hi'], mirek))
        body['color_temperature'] = {'mirek': mirek}
    else:
        mirek = 0
    api('grouped_light/%s' % rs[idx]['gid'], 'PUT', body)
    # A grouped_light reports color_temperature as an empty object, so the
    # 2s poll cannot read the temperature back - only the 30s structure
    # pass can. Without this the UI would snap the warmth slider back to
    # the old value a second after the finger left it.
    if mirek:
        rs[idx]['mirek'] = mirek


# What the automation is doing right now, for the panel to show. "on" is
# the setting; "why" is the truth - enabled and idle through a summer week
# would otherwise look identical to broken.
AUTO_STATUS = {'on': 0, 'why': 'off', 'room': ''}


def touches_auto_room(rs, line, a):
    p = line.split()
    if len(p) < 2 or p[0] != 'set':
        return False
    try:
        idx = int(p[1])
    except ValueError:
        return False
    return 0 <= idx < len(rs) and rs[idx]['name'].lower() == a['room'].lower()


def auto_run(rs, a, state):
    """Steer the configured room. Returns True if it sent anything."""
    AUTO_STATUS['on'] = 1 if a['on'] else 0
    AUTO_STATUS['room'] = a['room']
    if not a['on']:
        AUTO_STATUS['why'] = 'off'
        return False
    idx = next((i for i, r in enumerate(rs)
                if r['name'].lower() == a['room'].lower()), -1)
    if idx < 0 or not rs[idx]['reachable']:
        AUTO_STATUS['why'] = 'unreachable'
        return False

    now = datetime.now()
    mins = now.hour * 60 + now.minute

    # Touching the room by hand hands it back to you for the rest of the
    # day. Coming back two minutes later to re-impose a curve is the thing
    # that makes people rip these automations out.
    if state['resume'] and now < state['resume']:
        AUTO_STATUS['why'] = 'manual'
        return False

    sunrise, sunset = solar()
    want = auto_target(mins, sunrise, sunset, a)
    inside = want is not None
    if inside:
        AUTO_STATUS['why'] = 'active'
    elif not (a['from'] <= mins < a['to']):
        AUTO_STATUS['why'] = 'closed'
    else:
        AUTO_STATUS['why'] = 'summer'

    # Closing time is an edge, fired once, so that the rest of the night
    # the light is yours.
    if state['inside'] and not inside and mins >= a['to']:
        api('grouped_light/%s' % rs[idx]['gid'], 'PUT', {'on': {'on': False}})
        state['inside'] = False
        print("auto: window closed, %s off" % rs[idx]['name'], flush=True)
        return True
    state['inside'] = inside
    if not inside:
        return False

    bri, mirek = want
    # Only speak when it matters: a degree of mirek or a percent of
    # brightness per minute would be hundreds of calls a day for a change
    # nobody can see.
    if rs[idx]['on'] and abs(rs[idx]['bri'] - bri) < 3 \
       and abs(rs[idx]['mirek'] - mirek) < 8:
        return False

    body = {'on': {'on': True}, 'dimming': {'brightness': max(1, min(100, bri))}}
    if rs[idx]['mirek']:
        body['color_temperature'] = {'mirek': max(rs[idx]['lo'], min(rs[idx]['hi'], mirek))}
    api('grouped_light/%s' % rs[idx]['gid'], 'PUT', body)
    rs[idx]['mirek'] = mirek          # same reason as in apply()
    print("auto: %s -> %d%% %dK" % (rs[idx]['name'], bri, 1000000 // mirek), flush=True)
    return True


def selftest():
    """Curve checks against real Malmo solar times."""
    a = auto_cfg()
    a.update({'on': 1, 'from': 7 * 60, 'to': 22 * 60, 'day': 80, 'night': 35,
              'ramp': 30, 'cool': 200, 'warm': 370, 'late': 454, 'skip': 16})

    win_r, win_s = 9 * 60 + 34, 16 * 60 + 37     # 21 Dec
    sum_r, sum_s = 4 * 60 + 24, 21 * 60 + 55     # 21 Jun
    eq_r,  eq_s  = 6 * 60 + 49, 19 * 60 + 12     # 20 Sep

    # Outside the window nothing is imposed, at either end.
    assert auto_target(6 * 60, eq_r, eq_s, a) is None
    assert auto_target(22 * 60, eq_r, eq_s, a) is None
    assert auto_target(3 * 60, eq_r, eq_s, a) is None

    # A 17h31m midsummer day opts out; a 7h03m midwinter one does not.
    assert auto_target(12 * 60, sum_r, sum_s, a) is None
    assert auto_target(12 * 60, win_r, win_s, a) is not None

    # Opening: dim and cool, because the ramp has barely started.
    bri, mirek = auto_target(7 * 60 + 1, eq_r, eq_s, a)
    assert bri < 10 and mirek <= 205, (bri, mirek)

    # Ramp is done by its end, and brightness holds through the day.
    assert auto_target(7 * 60 + 30, eq_r, eq_s, a)[0] == 80
    assert auto_target(13 * 60, eq_r, eq_s, a)[0] == 80

    # Sunset is the warm anchor, whenever it happens to fall.
    for r, s in ((win_r, win_s), (eq_r, eq_s)):
        assert abs(auto_target(s, r, s, a)[1] - 370) <= 1, (s, auto_target(s, r, s, a))

    # Closing: dimmest and warmest of the day.
    bri, mirek = auto_target(21 * 60 + 59, eq_r, eq_s, a)
    assert 35 <= bri <= 36 and 450 <= mirek <= 454, (bri, mirek)

    # Monotonic warming across the whole window - no going cold again.
    prev = 0
    for m in range(7 * 60, 22 * 60):
        mk = auto_target(m, win_r, win_s, a)[1]
        assert mk >= prev - 1, (m, mk, prev)
        prev = mk

    # Winter warms hours earlier than equinox, which is the entire point.
    assert auto_target(17 * 60, win_r, win_s, a)[1] > \
           auto_target(17 * 60, eq_r, eq_s, a)[1]

    print("selftest ok")
    return 0


def main():
    if '--selftest' in sys.argv:
        return selftest()

    if not BRIDGE:
        print("no hue_bridge in cat_cfg.txt - see 'Lights' in README.md", file=sys.stderr)
        return 0

    if '--pair' in sys.argv:       # the one mode that runs without a key
        return pair()

    if not KEY:
        print("no hue_key in cat_cfg.txt - run ./hue.py --pair", file=sys.stderr)
        return 0

    if '--once' in sys.argv:
        print(write_state(read_state(structure())).strip())
        return 0

    rs, last_poll, last_struct, last_auto = [], 0.0, 0.0, 0.0
    a = auto_cfg()
    auto_state = {'inside': False, 'resume': None}
    while True:
        now = time.monotonic()

        if not rs or now - last_struct >= STRUCT_EVERY:
            # Set the timer even on failure, or a bridge that is down turns
            # this loop into a retry storm.
            last_struct = now
            a = auto_cfg()          # so cat_cfg.txt edits land without a restart
            try:
                rs = structure()
                last_poll = 0.0
            except Exception as e:
                print("hue structure failed: %s" % e, file=sys.stderr, flush=True)

        if rs and now - last_poll >= POLL:
            last_poll = now
            try:
                write_state(read_state(rs))
            except Exception as e:
                # Bridge rebooting or wifi dropped: keep the old file and
                # retry. The UI ages it out on its own.
                print("hue poll failed: %s" % e, file=sys.stderr, flush=True)

        # Commands are read between polls so a press is not stuck behind one.
        if rs:
            try:
                with open('hue.cmd') as f:
                    cmds = f.read().splitlines()
                os.remove('hue.cmd')
            except OSError:
                cmds = []
            for c in cmds:
                try:
                    apply(rs, c)
                except Exception as e:
                    print("hue cmd %r failed: %s" % (c, e), file=sys.stderr, flush=True)
            if cmds:
                last_poll = 0.0        # reflect the change on the next loop
                # Anything arriving here came from a finger on the panel -
                # the automation talks to the bridge directly. So a command
                # is the signal to back off until the window next opens.
                if a['on'] and any(touches_auto_room(rs, c, a) for c in cmds):
                    n = datetime.now()
                    nxt = n.replace(hour=a['from'] // 60, minute=a['from'] % 60,
                                    second=0, microsecond=0)
                    if nxt <= n:
                        nxt += timedelta(days=1)
                    auto_state['resume'] = nxt
                    print("auto: manual override, resuming %s" % nxt.strftime('%H:%M %d %b'),
                          flush=True)

        if rs and a['on'] and now - last_auto >= AUTO_EVERY:
            last_auto = now
            try:
                auto_run(rs, a, auto_state)
            except Exception as e:
                print("auto failed: %s" % e, file=sys.stderr, flush=True)

        time.sleep(CMD_POLL)


if __name__ == '__main__':
    sys.exit(main())
