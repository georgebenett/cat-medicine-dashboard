#!/bin/bash
# Nudge wifi back up if the link is actually down.
#
# This is a safety net, not the primary mechanism: NetworkManager is already
# autoconnect=yes with autoconnect-retries=0 (forever), so it retries on its
# own. It also does NOT address this Pi rebooting on its own - that is
# undervoltage, see "Power" in README.md. Kept because a wedged association
# is a different failure from a brownout, and ssh access matters.
set -u

# By type, not by name: the SSID can change without this needing to know.
CONN=$(nmcli -t -f NAME,TYPE con show --active 2>/dev/null |
       awk -F: '$2 == "802-11-wireless" { print $1; exit }')
[ -n "$CONN" ] || CONN=$(nmcli -t -f NAME,TYPE con show 2>/dev/null |
       awk -F: '$2 == "802-11-wireless" { print $1; exit }')

link_ok() {
    local gw
    gw=$(ip route 2>/dev/null | awk '/^default/ { print $3; exit }')
    [ -n "$gw" ] && ping -c 2 -W 3 "$gw" >/dev/null 2>&1
}

# The gateway, not the internet: this is about the link, and the router being
# up is not our problem to solve.
if link_ok; then
    exit 0
fi

logger -t wifi-watchdog "link down, bouncing ${CONN:-wlan0}"
if [ -n "$CONN" ]; then
    nmcli con up "$CONN" >/dev/null 2>&1 || nmcli device connect wlan0 >/dev/null 2>&1
else
    nmcli device connect wlan0 >/dev/null 2>&1
fi
sleep 8

if link_ok; then
    logger -t wifi-watchdog "reconnected"
else
    logger -t wifi-watchdog "still down after bounce"
fi
