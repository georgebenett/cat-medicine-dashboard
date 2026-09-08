# cat dashboard

A medicine tracker for the cat, on a Raspberry Pi 3A+ with a Waveshare
9" DSI touch panel (720x1280 portrait, used as 1280x720 landscape).
LVGL 8.3, hand-written C, no UI builder.

## Screens

Navigation is a left icon rail - home, calendar, gear - not a tab bar.

- **Today** - her photos and name, whether today is a medicine day, the
  dose count for the week, a Mon..Sun strip, and buttons to log a dose or
  an event. Once a dose is logged the button becomes "Undo today's dose":
  that is the only way to take back a mis-tap. Log event asks which kind:
  vomiting or food.
- **Calendar** - a month grid, green for a dose and red for vomiting,
  with month/streak/vomiting stats and a recent list. Food is logged and
  listed but deliberately not marked on the grid: it happens most days
  and would colour in every cell.
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
                   2026-09-07T22:03,vomit     kinds: med | vomit | food
    cat_cfg.txt    days=42                   medicine-day bitmask, bit0=Sunday
                   name=Mimi
                   dim=5                     idle minutes before dimming, 0=never
                   backlight=70              remembered brightness, restored at startup
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

## Her photos

Drop PNGs in `photos/` next to the binary and restart. They shuffle like
a digital portrait, one a minute (`photo_secs` in `cat_cfg.txt`), in a
random order that shows the whole set before repeating. A single
`cat.png` still works as a fallback if `photos/` is empty.

Loaded at runtime through LVGL's POSIX filesystem driver, *not* compiled
in as C arrays, so changing the pictures needs no rebuild. Each decodes
to 4 bytes per pixel and has to fit in `LV_MEM_SIZE` (8MB), so anything
that would decode past 6MB is skipped with a line on stdout - a stock
phone photo is ~4000x3000, which is 48MB. Resize first:

    sips -r 90 IMG_1234.jpg              # see below
    sips -Z 520 -s format png IMG_1234.jpg --out photos/kim_01.png

**Apply EXIF orientation first.** Phone photos are stored as a landscape
raster plus an orientation tag, and `sips -Z` ignores that tag, so every
picture arrives on the panel rotated. It is not one fixed rotation
either: of 14 photos here, 12 were orientation 6 (rotate 90 clockwise)
and 2 were orientation 8 (90 anticlockwise). Read the tag per photo and
rotate accordingly - a blanket rotation leaves some upside down.

`pill.png` (the icon on the dose button) is loaded the same way - LVGL's
built-in symbol font has no pill glyph. Drop a different 44x44 PNG there
to change it.

`photos/` and `cat.png` are gitignored: they are personal and this repo
has a public remote. Copy them over with scp instead.

## Power

This Pi browns out. `vcgencmd get_throttled` returns `0x50000`: bit 16
(under-voltage has occurred) and bit 18 (throttling has occurred), and it
logs `Undervoltage detected!` repeatedly. It rebooted on its own twice on
2026-09-08, roughly three hours apart.

That is the cause of most apparent "wifi drops" - the machine is
power-cycling, not losing its association. `cat-wifi.timer` is a safety
net for a genuinely wedged link, not a fix for this. **Replace the supply
or the cable.**

Unexpected reboots also corrupt SD cards, which is why `cat_log.csv` is
backed up off the device.

## Weather

`weather.py` fetches from Open-Meteo (no API key) into `weather.txt`,
every 20 minutes via `cat-weather.timer`. The UI only ever reads that
file.

Deliberately not fetched from the C app: that would mean linking libcurl
and making a blocking HTTP call inside the LVGL loop, stalling the panel
for as long as the network takes. Same shape as the backup - a timer
writes a file, the UI reads it.

Location is `lat`/`lon` in `cat_cfg.txt`, defaulting to Malmo. A failed
fetch keeps the last file rather than blanking the panel; anything older
than three hours is greyed out so stale numbers are not shown as current.

Icons are PNGs (`wx_*.png`), five of them, with the WMO codes collapsed
onto those five in `wx_icon_file()`.

## Wifi watchdog

`wifi_watchdog.sh` pings the default gateway every two minutes via
`cat-wifi.timer` and bounces the connection if it cannot be reached. It
finds the connection by type rather than by SSID.

NetworkManager is already `autoconnect=yes` with `autoconnect-retries=0`
(forever), so this only covers the case where the supplicant is wedged
rather than retrying. Check it with `journalctl -t wifi-watchdog`.

## Backing up the log

`cat_log.csv` is the only irreplaceable thing here, and it lives on an SD
card in a board that logs undervoltage. `backup.sh` copies it into a
clone of a **private** repo and pushes, run hourly by
`cat-backup.timer`. It commits only when the log has actually changed, so
the history is one commit per real change, and a wifi drop is not an
error - the commit is on disk and the next run pushes it.

No new credentials: the Pi already authenticates to github as
georgebenett over ssh, which is how `deploy.sh` pulls.

First-time setup on a new Pi:

    git clone git@github.com:georgebenett/cat-log-backup.git ~/cat_backup
    ~/lvgl_app/backup.sh          # check it works, then deploy.sh installs the timer

To restore the log:

    git clone git@github.com:georgebenett/cat-log-backup.git /tmp/restore
    cp /tmp/restore/cat_log.csv ~/lvgl_app/
    sudo systemctl restart lvglapp

To restore an *older* state - say a bad shutdown truncated the file:

    cd /tmp/restore && git log --oneline        # every backup is a commit
    git checkout <commit> -- cat_log.csv
    cp cat_log.csv ~/lvgl_app/

`$CAT_BACKUP_REPO` overrides the clone location.

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

`CAT_SCREEN=0|1|2` opens on Today, Calendar or Settings, `CAT_POPUP=1`
opens the Log event sheet, and `CAT_TOAST="text"` pins a toast up - handy
for looking at any of them over ssh without touching the panel.

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
