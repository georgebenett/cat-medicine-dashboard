#!/bin/bash
# Run on the Pi: pull, rebuild, (re)install the service, restart.
set -e
cd "$(dirname "$0")"
git pull --ff-only

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

if ! cmp -s lvglapp.service /etc/systemd/system/lvglapp.service; then
    sudo cp lvglapp.service /etc/systemd/system/lvglapp.service
    sudo systemctl daemon-reload
    sudo systemctl enable lvglapp
    echo "service unit updated + enabled at boot"
fi

sudo systemctl start lvglapp
sleep 2
echo "running: $(pgrep -c -x app) instance(s)  |  enabled: $(systemctl is-enabled lvglapp)"
