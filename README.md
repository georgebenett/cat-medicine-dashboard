# lvgl_demo

EEZ Studio LVGL 8.3 UI running on a Raspberry Pi 3A+ with a Waveshare
9" DSI touch panel (720x1280 portrait, used as 1280x720 landscape).

## Workflow

**Mac** — edit `LVGL Widgets Demo.eez-project` in EEZ Studio. Its build
output goes to `src/ui/` in this repo, so just build, then:

    git add -A && git commit -m "..." && git push

**Pi** — `~/lvgl_app` is a clone of this repo:

    ssh raspi
    ~/lvgl_app/deploy.sh        # pull + make + restart

## First build on a new machine

    ./setup.sh    # clones LVGL release/v8.3, generates lv_conf.h, builds

LVGL itself is not vendored (127MB); `setup.sh` fetches it. `lv_conf.h`
*is* tracked, so local settings survive a fresh clone.

## Gotchas baked into main.c

- **Software rotation only.** `/dev/fb0` is 720x1280; the kernel cmdline
  `rotate=90` rotates console text, not the framebuffer. LVGL gets the
  native size with `LV_DISP_ROT_90 + sw_rotate` and reports 1280x720.
- **Never set `full_refresh`** with `sw_rotate` — `lv_refr.c:1181` bails
  before `flush_cb` and nothing reaches the panel.
- **Touch is found by name**, not `/dev/input/eventN` (the index moves
  across reboots). Ranges come from `EVIOCGABS`; the Goodix reports
  0-4095, not pixels.
- **Touch coords go to LVGL unrotated** — `indev_pointer_proc()` already
  applies `disp_drv.rotated`. Rotating here too double-rotates.

Calibration knobs, no rebuild needed: `TOUCH_SWAP`, `TOUCH_INVX`,
`TOUCH_INVY`, `TOUCH_CURSOR=1` (red dot), `TOUCH_DEBUG=1` (coords to stderr).

Run detached — stdout paints over the UI on the panel console.
