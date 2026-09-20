#!/bin/bash
# Run on the Pi: pull, rebuild, (re)install the service, restart.
set -e
cd "$(dirname "$0")"
# Not fatal. This Pi's wifi drops, and being unable to restart the panel
# because github is unreachable is worse than deploying what is already
# checked out - so say plainly which commit is going on, and carry on.
if ! git pull --ff-only; then
    echo
    echo "WARNING: could not reach the remote. Deploying what is checked out:"
    git log --oneline -1
    echo
fi

# Installing the unit and restarting need root. Ask for it up front: over a
# non-interactive ssh there is no tty to type into, and the `|| true` on the
# stop below swallows that, so the deploy would "succeed" having done nothing.
sudo -v || { echo "deploy needs sudo - run this from a terminal on the Pi"; exit 1; }

# Stop BEFORE building, for two reasons: the linker cannot write `app` while
# that file is being executed (ETXTBSY), and strays fight over /dev/fb0.
# Also before touching the unit: systemd refuses to enable a real unit while
# a transient one of the same name is still loaded.
sudo systemctl stop lvglapp 2>/dev/null || true
sudo pkill -x app 2>/dev/null || true
sleep 1

make -s -j2                                # -s: the link line is 300 object paths

changed=0
for u in lvglapp.service cat-backup.service cat-backup.timer \
         cat-weather.service cat-weather.timer \
         cat-wifi.service cat-wifi.timer cat-dim.service \
         cat-transit.service cat-transit.timer cat-hue.service; do
    if ! cmp -s "$u" "/etc/systemd/system/$u"; then
        sudo cp "$u" "/etc/systemd/system/$u"
        # Verify: a brownout mid-copy left cat-dim.service zero length once,
        # and systemd reads a zero-length unit as *masked* - so it silently
        # never ran rather than failing loudly.
        if ! cmp -s "$u" "/etc/systemd/system/$u"; then
            echo "ERROR: $u did not install correctly" >&2
            exit 1
        fi
        echo "unit updated: $u"
        changed=1
    fi
done
if [ "$changed" = 1 ]; then
    sudo systemctl daemon-reload
    # cat-backup.service is oneshot and triggered by its timer, so only the
    # dashboard and the timer are enabled at boot.
    sudo systemctl enable lvglapp cat-backup.timer cat-weather.timer cat-wifi.timer \
        cat-dim.service cat-transit.timer cat-hue.service
    sudo systemctl start cat-backup.timer cat-weather.timer cat-wifi.timer cat-transit.timer
    # A daemon, not a timer: restart so a changed hue.py actually takes effect.
    sudo systemctl restart cat-hue.service
fi

sudo systemctl start lvglapp
sleep 2
echo "running: $(pgrep -c -x app) instance(s)  |  enabled: $(systemctl is-enabled lvglapp)"
