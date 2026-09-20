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

POLL = 2.0          # state refresh; the bridge is on the LAN, this is cheap
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


def rooms():
    """[(group_id, name, on, brightness)] sorted by name - index is the address."""
    rs = api('room')['data']
    groups = {g['id']: g for g in api('grouped_light')['data']}
    out = []
    for r in rs:
        gid = next((s['rid'] for s in r['services'] if s['rtype'] == 'grouped_light'), None)
        g = groups.get(gid)
        if not g:
            continue
        out.append((gid, r['metadata']['name'],
                    1 if g.get('on', {}).get('on') else 0,
                    int(round(g.get('dimming', {}).get('brightness', 0)))))
    out.sort(key=lambda x: x[1].lower())
    return out


def write_state(rs):
    lines = ["updated=%d" % int(time.time())]
    for _, name, on, bri in rs:
        lines.append("room=%s|%d|%d" % (name.replace('|', ' '), on, bri))
    out = "\n".join(lines) + "\n"
    with open('hue.txt.tmp', 'w') as f:
        f.write(out)
    os.replace('hue.txt.tmp', 'hue.txt')     # atomic: never a half-written file
    return out


def apply(rs, line):
    """'set <idx> <on> <bri>' - absolute, because the UI already knows the state."""
    p = line.split()
    if len(p) != 4 or p[0] != 'set':
        return
    try:
        idx, on, bri = int(p[1]), int(p[2]), int(p[3])
    except ValueError:
        return
    if not 0 <= idx < len(rs):
        return
    body = {'on': {'on': bool(on)}}
    # Sending brightness 0 turns a bulb off rather than dimming it, and the
    # bridge rejects it outright on some firmwares.
    if on and bri > 0:
        body['dimming'] = {'brightness': max(1, min(100, bri))}
    api('grouped_light/%s' % rs[idx][0], 'PUT', body)


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
        print(write_state(rooms()).strip())
        return 0

    rs, last_poll = [], 0.0
    while True:
        now = time.monotonic()
        if now - last_poll >= POLL or not rs:
            last_poll = now
            try:
                rs = rooms()
                write_state(rs)
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
