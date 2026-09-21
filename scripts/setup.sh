#!/bin/bash
# First build on a fresh clone: fetch LVGL, build. Safe to re-run.
#
# lv_conf.h is tracked, so there is nothing to generate - the old version
# had a branch to create it from the LVGL template that could never run,
# and apt-get'd git using a hardcoded sudo password, on a machine that
# had just used git to clone this.
set -e
cd "$(dirname "$0")/.."   # scripts/ -> project root

if [ ! -d lvgl ]; then
    echo "### cloning lvgl 8.3 (125MB, not vendored)"
    git clone --depth 1 -b release/v8.3 https://github.com/lvgl/lvgl.git
fi

echo "### building (a few minutes on a Pi 3A+)"
make -j2
echo "### done: $(ls -lh app | awk '{print $5}')"
