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

os.chdir(os.path.dirname(os.path.abspath(__file__)))

POLL = 2.0          # on/off + brightness; the bridge is on the LAN, this is cheap
STRUCT_EVERY = 30.0 # rooms, bulb membership, mesh health - all near-static
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
                m = ((lights.get(s['rid']) or {}).get('color_temperature') or {}).get('mirek')
                if m:
                    mireks.append(m)
        out.append({'gid': gid, 'name': r['metadata']['name'],
                    'reachable': 1 if reach else 0,
                    # 0 means the room has no tunable-white bulbs at all.
                    'mirek': int(sum(mireks) / len(mireks)) if mireks else 0,
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
    lines = ["updated=%d" % int(time.time())]
    for r in rs:
        lines.append("room=%s|%d|%d|%d|%d" % (r['name'].replace('|', ' '),
                                              r['on'], r['bri'],
                                              r['reachable'], r['mirek']))
    out = "\n".join(lines) + "\n"
    with open('hue.txt.tmp', 'w') as f:
        f.write(out)
    os.replace('hue.txt.tmp', 'hue.txt')     # atomic: never a half-written file
    return out


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
        mirek = max(153, min(500, mirek))
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


def main():
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

    rs, last_poll, last_struct = [], 0.0, 0.0
    while True:
        now = time.monotonic()

        if not rs or now - last_struct >= STRUCT_EVERY:
            # Set the timer even on failure, or a bridge that is down turns
            # this loop into a retry storm.
            last_struct = now
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

        time.sleep(CMD_POLL)


if __name__ == '__main__':
    sys.exit(main())
