#!/bin/bash
# Run on the Pi: pull, rebuild, restart the UI.
set -e
cd "$(dirname "$0")"
git pull --ff-only
make -s -j2                                # -s: the link line is 300 object paths
sudo systemctl stop lvglapp 2>/dev/null || true
sudo pkill -x app 2>/dev/null || true      # kill strays: they fight over /dev/fb0
sleep 1
sudo systemd-run --unit=lvglapp --collect "$PWD/app"
sleep 2                                    # let systemd actually start it
echo "running: $(pgrep -c -x app) instance(s)"
