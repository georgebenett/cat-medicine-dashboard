#!/bin/bash
# Run the dashboard in the foreground for a look at it. Ctrl+C to quit.
#
# Two other things paint into /dev/fb0 and would otherwise sit on top of
# the UI:
#   - the lvglapp service, a second instance fighting for the framebuffer
#   - fbcon, the kernel's framebuffer console. `console=tty1` on the kernel
#     cmdline sends kernel messages there, which is how "Undervoltage
#     detected!" ends up across the design.
# Both are put back on the way out, whether you Ctrl+C, it crashes, or the
# ssh connection drops.
#
# This is for looking at the UI, not for running it: normal operation is
# the service, which deploy.sh installs and starts at boot.
set -e
cd "$(dirname "$0")"

sudo -v || { echo "needs sudo: the service and fbcon are both root-owned"; exit 1; }

# By name, not by number - vtcon0/vtcon1 are not a fixed assignment.
fbcon=$(grep -l "frame buffer device" /sys/class/vtconsole/*/name 2>/dev/null | head -1)
fbcon=${fbcon%/name}

was_active=$(systemctl is-active lvglapp 2>/dev/null || true)

restore() {
    if [ -n "$fbcon" ]; then
        echo 1 | sudo tee "$fbcon/bind" >/dev/null 2>&1 || true
    fi
    if [ "$was_active" = active ]; then
        sudo systemctl start lvglapp || true
        echo "service restarted"
    fi
}
trap restore EXIT

sudo systemctl stop lvglapp 2>/dev/null || true
sudo pkill -x app 2>/dev/null || true      # strays fight over /dev/fb0

if [ -n "$fbcon" ]; then
    echo 0 | sudo tee "$fbcon/bind" >/dev/null
else
    echo "warning: no framebuffer console found to unbind; console text may show through"
fi

echo "running - Ctrl+C to quit"
./app                                      # foreground, so Ctrl+C reaches it
