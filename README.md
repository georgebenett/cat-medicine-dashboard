# cat dashboard

A medicine tracker for the cat, on a Raspberry Pi 3A+ with a Waveshare
9" DSI touch panel (720x1280 portrait, used as 1280x720 landscape).
LVGL 8.3, hand-written C, no UI builder.

## Tabs

- **Today** - her photo, whether today is a medicine day, a button to
  log the dose (greys out once logged, so a double tap can't
  double-log), a button to log vomiting, and how the week has gone.
- **Calendar** - every day the medicine was actually given, highlighted.
- **Settings** - backlight slider, and which weekdays are medicine days.

## State

Two plain text files next to the binary. Both survive a rebuild, both
are readable and editable without this app, and both are gitignored
(they are device data, not source):

    cat_log.csv    2026-09-08T19:47,med      append-only, one event per line
                   2026-09-07T22:03,vomit
    cat_cfg.txt    42                        medicine-day bitmask, bit0=Sunday

Default schedule is Mon/Wed/Fri - three a week. Change it in Settings,
not in the source.

Paths are overridable: `CAT_LOG`, `CAT_CFG`, `CAT_IMG`.

## Her photo

Drop a PNG at `cat.png` next to the binary and restart. It is loaded at
runtime through LVGL's POSIX filesystem driver, *not* compiled in as a C
array, so changing the picture needs no rebuild. Keep it under ~1200px
on the long side: it decodes to 4 bytes per pixel and has to fit in
`LV_MEM_SIZE` (8MB). Oversized or missing, the app draws a placeholder
and says so on stderr rather than failing.

To have it deploy with a `git pull`, commit it:

    git add -f cat.png && git commit -m "her" && git push

## Workflow

**Mac** - edit, commit, push. **Pi** - `~/lvgl_app/deploy.sh` pulls,
rebuilds, and restarts the service.

## First build on a new machine

    ./setup.sh    # clones LVGL release/v8.3, builds

LVGL itself is not vendored (125MB); `setup.sh` fetches it. `lv_conf.h`
*is* tracked, so local settings survive a fresh clone.

## Gotchas baked into main.c

- **Software rotation only.** `/dev/fb0` is 720x1280; the kernel cmdline
  `rotate=90` rotates console text, not the framebuffer. LVGL gets the
  native size with `LV_DISP_ROT_90 + sw_rotate` and reports 1280x720.
- **Never set `full_refresh`** with `sw_rotate` - `lv_refr.c:1181` bails
  before `flush_cb` and nothing reaches the panel.
- **Touch is found by name**, not `/dev/input/eventN` (the index moves
  across reboots). Ranges come from `EVIOCGABS`; the Goodix reports
  0-4095, not pixels.
- **Touch coords go to LVGL unrotated** - `indev_pointer_proc()` already
  applies `disp_drv.rotated`. Rotating here too double-rotates.
- **Objects depend on `lv_conf.h`** in the Makefile. It reshapes LVGL's
  structs, and stale `.o` files link fine and then misbehave.

Calibration knobs, no rebuild needed: `TOUCH_SWAP`, `TOUCH_INVX`,
`TOUCH_INVY`, `TOUCH_CURSOR=1` (red dot), `TOUCH_DEBUG=1` (coords to stderr).

## Looking at it without the service in the way

    ~/lvgl_app/run.sh     # foreground, Ctrl+C to quit

Two other things paint into `/dev/fb0`: the `lvglapp` service (a second
instance fighting for the framebuffer) and **fbcon**, the kernel's
framebuffer console - `console=tty1` on the kernel cmdline is why
"Undervoltage detected!" lands across the design. `run.sh` stops the
service and unbinds fbcon for the duration, and puts both back on exit.

Run detached otherwise - stdout paints over the UI on the panel console.
