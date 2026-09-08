# cat dashboard

A medicine tracker for the cat, on a Raspberry Pi 3A+ with a Waveshare
9" DSI touch panel (720x1280 portrait, used as 1280x720 landscape).
LVGL 8.3, hand-written C, no UI builder.

## Screens

Navigation is a left icon rail - home, calendar, gear - not a tab bar.

- **Today** - her photo and name, whether today is a medicine day, the
  dose count for the week, a Mon..Sun strip, and buttons to log a dose or
  an event. Once a dose is logged the button becomes "Undo today's dose":
  that is the only way to take back a mis-tap.
- **Calendar** - a month grid, green for a dose and red for an event,
  with month/streak/event stats and a recent list.
- **Settings** - backlight, idle dim, which weekdays are medicine days,
  a reminder toggle, and buttons to export or reset the log.

Reset data deletes every logged dose and event after a confirmation.
Settings and the schedule survive it.

Exit to shell quits the dashboard and puts the console back on the panel
- the unit is `Restart=on-failure`, so a clean exit stays stopped, and
`ExecStopPost` rebinds fbcon. Start it again with:

    sudo systemctl start lvglapp

## State

Two plain text files next to the binary. Both survive a rebuild, both
are readable and editable without this app, and both are gitignored
(they are device data, not source):

    cat_log.csv    2026-09-08T19:47,med      append-only, one event per line
                   2026-09-07T22:03,vomit
    cat_cfg.txt    days=42                   medicine-day bitmask, bit0=Sunday
                   name=Mimi
                   dim=5                     idle minutes before dimming, 0=never
                   reminder=1                flash the status card when overdue
                   reminder_h=9              reminder time; no picker in the UI
                   reminder_m=0

Default schedule is Mon/Wed/Fri - three a week. Change it in Settings,
not in the source. A bare integer in cat_cfg.txt is still read as the day
mask, which is what the first version of this file held.

Idle dimming uses LVGL's own `lv_disp_get_inactive_time()`. It drops the
panel to the dimmest the hardware will go while still lit - deliberately
below the `BL_MIN_PCT` floor that keeps the slider visible - and restores
the slider's brightness on the next touch.

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
- **Only ASCII, U+00B0 and U+2022 exist in the built-in fonts**, plus the
  `LV_SYMBOL_*` glyphs. A middle dot (U+00B7) renders as a tofu box; the
  separator is `LV_SYMBOL_BULLET`. Check `unicode_list_1` in
  `lvgl/src/font/lv_font_montserrat_*.c` before using any other character.
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
