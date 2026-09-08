#!/bin/bash
# Run on the Pi: pull, rebuild, (re)install the service, restart.
set -e
cd "$(dirname "$0")"
git pull --ff-only
make -s -j2                                # -s: the link line is 300 object paths

# Stop BEFORE touching the unit: systemd refuses to enable a real unit
# while a transient one of the same name is still loaded.
sudo systemctl stop lvglapp 2>/dev/null || true
sudo pkill -x app 2>/dev/null || true      # strays fight over /dev/fb0
sleep 1

if ! cmp -s lvglapp.service /etc/systemd/system/lvglapp.service; then
    sudo cp lvglapp.service /etc/systemd/system/lvglapp.service
    sudo systemctl daemon-reload
    sudo systemctl enable lvglapp
    echo "service unit updated + enabled at boot"
fi

sudo systemctl start lvglapp
sleep 2
echo "running: $(pgrep -c -x app) instance(s)  |  enabled: $(systemctl is-enabled lvglapp)"
