#!/bin/bash
# Fetch LVGL 8.3, generate lv_conf.h, build. Safe to re-run.
set -e
cd "$(dirname "$0")"
echo "### deps"
sudo apt-get install -y git >/dev/null 2>&1 || true
command -v git >/dev/null || { echo "git install FAILED"; exit 1; }

if [ ! -d lvgl ]; then
  echo "### cloning lvgl 8.3"
  git clone --depth 1 -b release/v8.3 https://github.com/lvgl/lvgl.git
fi

if [ ! -f lv_conf.h ]; then
  echo "### generating lv_conf.h"
  cp lvgl/lv_conf_template.h lv_conf.h
  # enable the file, and give the widgets demo room to allocate
  sed -i '0,/#if 0/s//#if 1/' lv_conf.h
  sed -i 's/#define LV_MEM_SIZE .*/#define LV_MEM_SIZE (1024U * 1024U)/' lv_conf.h
fi

echo "### lv_conf key settings"
grep -E "define (LV_COLOR_DEPTH|LV_MEM_SIZE|LV_COLOR_16_SWAP)" lv_conf.h

echo "### building (this takes a few minutes on a Pi 3A+)"
make -j2 2>&1 | tail -25
echo "### BUILD EXIT: ${PIPESTATUS[0]}"
ls -la app 2>/dev/null && echo "BINARY OK" || echo "NO BINARY"
