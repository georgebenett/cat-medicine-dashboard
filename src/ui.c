/* Cat medicine dashboard.
 *
 * Layout follows cat_med_dashboard_mockup.html: a left icon rail with three
 * screens - Home (photo, dose status, this week), Calendar (month grid plus
 * stats and recent events), Settings (backlight, dim, schedule, reminder).
 *
 * State lives in two plain text files next to the binary, so the log
 * survives a rebuild and can be read or edited without this app:
 *   cat_log.csv   "2026-09-08T19:47,med" | "...,vomit", append-only
 *   cat_cfg.txt   key=value settings, see cfg_load()
 * Override the paths with $CAT_LOG / $CAT_CFG, the photo with $CAT_IMG.
 */
#include "ui.h"
#include "sched.h"

/* 60px clock face: bigger than LVGL ships Montserrat, so it is generated
 * into src/font_clock_60.c - digits and colon only. */
LV_FONT_DECLARE(font_clock_60);
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <glob.h>

/* Apple's dark-mode system palette. Surfaces are near-black and colour is
 * spent only on state and the one primary action - the previous palette
 * filled whole cards with saturated green, which read as a warning. */
#define C_BG        0x000000    /* systemBackground */
#define C_SURF1     0x1C1C1E    /* secondarySystemBackground */
#define C_BORDER    0x38383A    /* separator */
#define C_BORDER_ST 0x2C2C2E    /* tertiarySystemBackground, secondary fills */
#define C_TEXT      0xFFFFFF    /* label */
#define C_TEXT2     0x98989F    /* secondaryLabel */
#define C_MUTED     0x5A5A5F    /* tertiaryLabel */
#define C_ACC_FILL  0x0A84FF    /* systemBlue (dark) */
#define C_ACC_ON    0xFFFFFF    /* label on a filled accent */
#define C_ACC_BG    0x2C2C2E
#define C_ACC_TEXT  0x0A84FF
#define C_OK_BG     0x1C1C1E    /* status cards are flat now, not tinted */
#define C_OK_TEXT   0x30D158    /* systemGreen (dark) */
#define C_OK_FILL   0x30D158
#define C_BAD_BG    0x1C1C1E
#define C_BAD_TEXT  0xFF453A    /* systemRed (dark) */
#define C_BAD_FILL  0xFF453A
#define C_WARN_BG   0x2C2C2E
#define C_WARN_TEXT 0xFF9F0A    /* systemOrange (dark) */

/* 1280x720. The mockup is drawn at 382px tall, so its numbers are scaled
 * by 720/382 ~ 1.885 throughout. */
#define SCR_W    1280
#define SCR_H    720
#define RAIL_W   92
#define PAD      22      /* was 40; 17% of the panel was margin */
/* The rail is an overlay now, so the body keeps the full width and only
 * loses the left 92px while the rail is actually on screen. */
#define BODY_X   PAD
#define BODY_W   (SCR_W - 2 * PAD)
#define BODY_H   (SCR_H - 2 * PAD)
#define PHOTO_W  500     /* with PHOTO_H this is ~3:4, matching the photos */
#define PHOTO_H  BODY_H   /* fills the body, bottom-aligned with the week card */
#define GAP      24

/* Right-column stack. Heights and gaps add up to BODY_H exactly, so growing
 * the body grows the cards rather than opening one large hole above the
 * week strip. */
#define CARD_GAP  16
/* The stack sums to exactly BODY_H - change one and change another. The
 * clock takes the space: at arm's length across a room the time is the
 * thing being read, and 40px was not carrying that far. */
#define STATUS_H  144
#define BTNS_H    108
#define CLOCK_H   176
#define WEEK_H    200
#define STATUS_Y  0
#define BTNS_Y    (STATUS_H + CARD_GAP)
#define CLOCK_Y   (BTNS_Y + BTNS_H + CARD_GAP)
#define WEEK_Y    (CLOCK_Y + CLOCK_H + CARD_GAP)
#define EDGE_W   40      /* swipe-from-here strip */
#define RAIL_SECS 30     /* auto-hide */
#define HOME_SECS 30     /* idle on any other screen -> back to Today */
#define RIGHT_X  (PHOTO_W + GAP)
#define RIGHT_W  (BODY_W - RIGHT_X)
#define BTN_W    ((RIGHT_W - 22) / 2)

/* --- store ---------------------------------------------------------- */

#define MAX_EVTS 4000
typedef struct { int y, mo, d, h, mi; char t; } evt_t;   /* 'm'edicine | 'v'omit | 'f'ood */

static const char *evt_word(char t) { return t == 'v' ? "vomit" : t == 'f' ? "food" : "med"; }
static const char *evt_label(char t) { return t == 'v' ? "Vomiting" : t == 'f' ? "Food" : "Dose given"; }
static const char *evt_icon(char t)
{
    return t == 'v' ? LV_SYMBOL_WARNING : t == 'f' ? LV_SYMBOL_PLUS : LV_SYMBOL_OK;
}

static evt_t evts[MAX_EVTS];
static int   n_evts;

/* Settings, with the defaults the mockup shows. */
static int  med_mask = (1 << 1) | (1 << 3) | (1 << 5);   /* bit0=Sun; Mon/Wed/Fri */
static char cat_name[64] = "Kim";    /* sized to match the cfg value buffer */
static int  quiet_from = 23, quiet_to = 5;               /* night dim window */
static int  dimmed;                                      /* inside that window now */
static int  dim_pct = 15;                                /* idle level, not off */
static int  reminder_on = 1, reminder_h = 9, reminder_m = 0;
static int  backlight_pct = 50;                          /* remembered across restarts */
static char cfg_tkey[64]  = "";        /* trafiklab key; transit.py uses these */
static char cfg_tfrom[64] = "";
static char cfg_tto[64]   = "";
static int  transit_start = 8 * 60, transit_end = 9 * 60 + 30;   /* minutes since midnight */
static int  transit_lead = 8;          /* minutes needed to reach the stop */
static int  rain_pct = 40;             /* weather.py: chance that means "coat" */
static int  hue_auto;                  /* daylight curve; hue.py does the work */
static char cfg_lat[64] = "55.6078";   /* sized to the cfg value buffer */
static char cfg_lon[64] = "12.9982";   /* weather.py reads both */

/* "HH:MM" to minutes since midnight, or dflt if it is not that shape. */
static int parse_hhmm(const char *s, int dflt)
{
    int h, m;
    if (sscanf(s, "%d:%d", &h, &m) != 2) return dflt;
    if (h < 0 || h > 23 || m < 0 || m > 59) return dflt;
    return h * 60 + m;
}

static const char *env_or(const char *var, const char *dflt)
{
    const char *e = getenv(var);
    return (e && *e) ? e : dflt;
}
static const char *log_path(void) { return env_or("CAT_LOG", "cat_log.csv"); }
static const char *cfg_path(void) { return env_or("CAT_CFG", "cat_cfg.txt"); }
static const char *img_path(void) { return env_or("CAT_IMG", "cat.png"); }

static void store_load(void)
{
    n_evts = 0;
    FILE *f = fopen(log_path(), "r");
    if (!f) { printf("cat log: %s not there yet, starting empty\n", log_path()); return; }
    char line[128];
    while (n_evts < MAX_EVTS && fgets(line, sizeof line, f)) {
        evt_t e; char kind[16];
        if (sscanf(line, "%d-%d-%dT%d:%d,%15s", &e.y, &e.mo, &e.d, &e.h, &e.mi, kind) == 6) {
            e.t = (kind[0] == 'v') ? 'v' : (kind[0] == 'f') ? 'f' : 'm';
            evts[n_evts++] = e;
        }
    }
    fclose(f);
    printf("cat log: %d events from %s\n", n_evts, log_path());
}

static void store_append(char t)
{
    time_t now = time(NULL);
    struct tm lt = *localtime(&now);
    evt_t e = { lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, t };

    /* Write through immediately: the Pi loses power without warning. */
    FILE *f = fopen(log_path(), "a");
    if (f) {
        fprintf(f, "%04d-%02d-%02dT%02d:%02d,%s\n", e.y, e.mo, e.d, e.h, e.mi,
                evt_word(t));
        fclose(f);
    } else {
        perror("cat log append");
    }
    if (n_evts < MAX_EVTS) evts[n_evts++] = e;
}

/* Rewrites the whole file. The log is a few hundred lines at most, so
 * this is simpler than trying to edit a line out in place. */
static void store_rewrite(void)
{
    FILE *f = fopen(log_path(), "w");
    if (!f) { perror("cat log rewrite"); return; }
    for (int i = 0; i < n_evts; i++)
        fprintf(f, "%04d-%02d-%02dT%02d:%02d,%s\n", evts[i].y, evts[i].mo, evts[i].d,
                evts[i].h, evts[i].mi, evt_word(evts[i].t));
    fclose(f);
}

/* Rename, never delete. Reset sits next to Exit to shell, and a mis-tap
 * should not be able to destroy months of history - the confirm dialog
 * should not be the only thing standing between the two. backup.sh picks
 * archive_*.csv up, so it reaches the backup repo too. */
static void store_clear_all(void)
{
    time_t now = time(NULL);
    struct tm t = *localtime(&now);
    char dst[256];

    n_evts = 0;
    if (access(log_path(), F_OK) != 0) return;            /* nothing to keep */

    snprintf(dst, sizeof dst, "archive_%04d%02d%02d-%02d%02d.csv",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    if (rename(log_path(), dst) != 0) perror("cat log archive");
    else printf("log archived to %s\n", dst);
}

static void cfg_load(void)
{
    FILE *f = fopen(cfg_path(), "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char k[32], v[64];
        if (sscanf(line, "%31[^=]=%63[^\n]", k, v) != 2) {
            /* The first version of this file was a bare integer mask. */
            int m = atoi(line);
            if (m > 0 && m < 128) med_mask = m;
            continue;
        }
        if      (!strcmp(k, "days"))       { int m = atoi(v); if (m >= 0 && m < 128) med_mask = m; }
        else if (!strcmp(k, "name"))       snprintf(cat_name, sizeof cat_name, "%s", v);
        else if (!strcmp(k, "quiet_from")) { int h = atoi(v); if (h >= 0 && h < 24) quiet_from = h; }
        else if (!strcmp(k, "quiet_to"))   { int h = atoi(v); if (h >= 0 && h < 24) quiet_to = h; }
        else if (!strcmp(k, "dim_pct"))    { int p = atoi(v); if (p >= 5 && p <= 60) dim_pct = p; }
        else if (!strcmp(k, "backlight"))  { int b = atoi(v); if (b >= 5 && b <= 100) backlight_pct = b; }
        else if (!strcmp(k, "lat"))        snprintf(cfg_lat, sizeof cfg_lat, "%s", v);
        else if (!strcmp(k, "lon"))        snprintf(cfg_lon, sizeof cfg_lon, "%s", v);
        else if (!strcmp(k, "hue_auto"))     hue_auto = atoi(v);
        else if (!strcmp(k, "transit_key"))  snprintf(cfg_tkey, sizeof cfg_tkey, "%s", v);
        else if (!strcmp(k, "transit_from")) snprintf(cfg_tfrom, sizeof cfg_tfrom, "%s", v);
        else if (!strcmp(k, "transit_to"))   snprintf(cfg_tto, sizeof cfg_tto, "%s", v);
        else if (!strcmp(k, "transit_start")) transit_start = parse_hhmm(v, transit_start);
        else if (!strcmp(k, "transit_end"))   transit_end   = parse_hhmm(v, transit_end);
        else if (!strcmp(k, "transit_lead"))  { int m = atoi(v); if (m >= 0 && m <= 120) transit_lead = m; }
        else if (!strcmp(k, "rain_pct"))      { int p = atoi(v); if (p > 0 && p <= 100) rain_pct = p; }
        else if (!strcmp(k, "reminder"))   reminder_on = atoi(v) ? 1 : 0;
        else if (!strcmp(k, "reminder_h")) { int h = atoi(v); if (h >= 0 && h < 24) reminder_h = h; }
        else if (!strcmp(k, "reminder_m")) { int m = atoi(v); if (m >= 0 && m < 60) reminder_m = m; }
    }
    fclose(f);
}

/* Keys this file owns and rewrites below. Anything else in cat_cfg.txt
 * belongs to one of the scripts and is carried across untouched - hue.py
 * alone has a dozen, and the pairing key among them cannot be regenerated
 * without walking over and pressing the button on the bridge. */
static int cfg_owned(const char *k)
{
    static const char *owned[] = {
        "days", "name", "quiet_from", "quiet_to", "dim_pct", "backlight",
        "reminder", "reminder_h", "reminder_m", "lat", "lon",
        "transit_key", "transit_from", "transit_to",
        "transit_start", "transit_end", "transit_lead", "rain_pct",
        "hue_auto",
    };
    for (unsigned i = 0; i < sizeof owned / sizeof owned[0]; i++)
        if (!strcmp(k, owned[i])) return 1;
    return 0;
}

static void cfg_save(void)
{
    /* Read the foreign lines before truncating, or they are gone. */
    char keep[2048];
    size_t n_keep = 0;
    FILE *in = fopen(cfg_path(), "r");
    if (in) {
        char line[256];
        while (fgets(line, sizeof line, in)) {
            char key[64];
            const char *eq = strchr(line, '=');
            if (!eq || (size_t)(eq - line) >= sizeof key) continue;
            memcpy(key, line, eq - line);
            key[eq - line] = '\0';
            if (cfg_owned(key)) continue;
            size_t len = strlen(line);
            if (n_keep + len + 2 >= sizeof keep) break;   /* keep what fits */
            memcpy(keep + n_keep, line, len);
            n_keep += len;
            if (line[len - 1] != '\n') keep[n_keep++] = '\n';
        }
        fclose(in);
    }
    keep[n_keep] = '\0';

    FILE *f = fopen(cfg_path(), "w");
    if (!f) { perror("cat cfg save"); return; }
    /* lat/lon are written back even though nothing in the UI edits them:
     * they are in the owned list, so nothing else would preserve them. */
    fprintf(f, "days=%d\nname=%s\nquiet_from=%d\nquiet_to=%d\ndim_pct=%d\nbacklight=%d\n"
               "reminder=%d\nreminder_h=%d\nreminder_m=%d\nlat=%s\nlon=%s\n"
               "transit_key=%s\ntransit_from=%s\ntransit_to=%s\n"
               "transit_start=%02d:%02d\ntransit_end=%02d:%02d\ntransit_lead=%d\n"
               "rain_pct=%d\nhue_auto=%d\n%s",
            med_mask, cat_name, quiet_from, quiet_to, dim_pct, backlight_pct,
            reminder_on, reminder_h, reminder_m, cfg_lat, cfg_lon,
            cfg_tkey, cfg_tfrom, cfg_tto,
            transit_start / 60, transit_start % 60,
            transit_end / 60, transit_end % 60, transit_lead, rain_pct,
            hue_auto, keep);
    fclose(f);
}

/* --- weather ---------------------------------------------------------
 * weather.py writes weather.txt; this only ever reads it. Stale data is
 * shown greyed rather than hidden - "17 degrees an hour ago" is more use
 * than a blank panel. */

static int  wx_temp, wx_code = -1, wx_hi, wx_lo;
static int  wx_rain_from = -1, wx_rain_max;
static long wx_updated;

static void weather_load(void)
{
    wx_rain_from = -1;
    wx_rain_max = 0;
    FILE *f = fopen(env_or("CAT_WEATHER", "weather.txt"), "r");
    if (!f) return;
    char line[64], k[24], v[32];
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%23[^=]=%31[^\n]", k, v) != 2) continue;
        if      (!strcmp(k, "temp"))    wx_temp = atoi(v);
        else if (!strcmp(k, "code"))    wx_code = atoi(v);
        else if (!strcmp(k, "hi"))      wx_hi = atoi(v);
        else if (!strcmp(k, "lo"))      wx_lo = atoi(v);
        else if (!strcmp(k, "rain_from")) wx_rain_from = atoi(v);
        else if (!strcmp(k, "rain_max"))  wx_rain_max = atoi(v);
        else if (!strcmp(k, "updated")) wx_updated = atol(v);
    }
    fclose(f);
}

/* WMO weather codes, collapsed to the five icons that exist. */
static const char *wx_icon_file(int code)
{
    if (code == 0)                 return "A:icons/wx_clear.png";
    if (code <= 2)                 return "A:icons/wx_partly.png";
    if (code == 3 || code == 45 || code == 48) return "A:icons/wx_cloud.png";
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return "A:icons/wx_snow.png";
    return "A:icons/wx_rain.png";        /* drizzle, rain, showers, thunder */
}

static const char *wx_words(int code)
{
    switch (code) {
        case 0:  return "Clear";
        case 1:  return "Mostly clear";
        case 2:  return "Partly cloudy";
        case 3:  return "Overcast";
        case 45: case 48: return "Fog";
        case 51: case 53: case 55: case 56: case 57: return "Drizzle";
        case 61: case 63: case 66: return "Rain";
        case 65: case 67: return "Heavy rain";
        case 71: case 73: case 77: case 85: return "Snow";
        case 75: case 86: return "Heavy snow";
        case 80: case 81: return "Showers";
        case 82: return "Heavy showers";
        case 95: case 96: case 99: return "Thunderstorm";
        default: return "";
    }
}

/* --- transit ---------------------------------------------------------
 * transit.py writes transit.txt; this only reads it. Shown on weekday
 * mornings in place of the week strip - at 07:30 the next bus matters
 * more than how the week has gone. */

#define MAX_TRIPS 6      /* fetched */
#define SHOW_TRIPS 3     /* displayed after filtering */
static struct { char dep[8], arr[8], line[28]; int mins, changes; } trips[MAX_TRIPS];
static int  n_trips;
static long transit_updated;

static void transit_load(void)
{
    FILE *f = fopen(env_or("CAT_TRANSIT", "transit.txt"), "r");
    n_trips = 0;
    transit_updated = 0;
    if (!f) return;
    char line[160];
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "updated=", 8)) { transit_updated = atol(line + 8); continue; }
        if (strncmp(line, "trip=", 5) || n_trips >= MAX_TRIPS) continue;
        char *p = line + 5, *tok;
        char *save = NULL;
        int field = 0;
        for (tok = strtok_r(p, "|\n", &save); tok && field < 5;
             tok = strtok_r(NULL, "|\n", &save), field++) {
            switch (field) {
                case 0: snprintf(trips[n_trips].dep, sizeof trips[0].dep, "%s", tok); break;
                case 1: snprintf(trips[n_trips].arr, sizeof trips[0].arr, "%s", tok); break;
                case 2: trips[n_trips].mins = atoi(tok); break;
                case 3: trips[n_trips].changes = atoi(tok); break;
                case 4: snprintf(trips[n_trips].line, sizeof trips[0].line, "%s", tok); break;
            }
        }
        if (field >= 4) n_trips++;
    }
    fclose(f);
}

/* --- lights ---------------------------------------------------------- */

/* hue.py writes the state and consumes the commands; nothing here touches
 * the network. A blocking HTTP call in this loop would stall the panel for
 * the length of the request, which is what the file hand-off exists to
 * avoid. Rooms are addressed by index - hue.py sorts them by name. */
#define MAX_ROOMS 4
static struct { char name[32]; int on, bri, reachable, mirek; } rooms[MAX_ROOMS];
static int  n_rooms;
static long hue_updated;
/* Reported by hue.py: whether the curve is enabled, and what it is
 * actually doing - "active", "summer", "closed", "manual". */
static int  auto_on;
static char auto_why[16] = "off";
static char auto_room[32];

/* A room icon from the symbols the font actually carries - there is no
 * emoji coverage here, only LV_SYMBOL_*. Unmatched rooms get the bulb. */
static const char *room_icon(const char *name)
{
    static const struct { const char *key, *sym; } map[] = {
        { "hall",    LV_SYMBOL_HOME     },
        { "living",  LV_SYMBOL_VIDEO    },
        { "lounge",  LV_SYMBOL_VIDEO    },
        { "studio",  LV_SYMBOL_KEYBOARD },
        { "office",  LV_SYMBOL_KEYBOARD },
        { "desk",    LV_SYMBOL_KEYBOARD },
        { "bath",    LV_SYMBOL_TINT     },
        { "kitchen", LV_SYMBOL_LIST     },
        { "bed",     LV_SYMBOL_EYE_CLOSE},
    };
    char low[32];
    size_t i;
    for (i = 0; i + 1 < sizeof low && name[i]; i++)
        low[i] = (char)tolower((unsigned char)name[i]);
    low[i] = '\0';
    for (i = 0; i < sizeof map / sizeof map[0]; i++)
        if (strstr(low, map[i].key)) return map[i].sym;
    return LV_SYMBOL_CHARGE;
}

/* Hue reports colour temperature in mireks (153 cool .. 500 warm). This
 * paints the warmth slider itself, so the control shows the white it is
 * about to set. A straight lerp between the two ends is plenty; nobody is
 * colour-matching a wall panel.
 *
 * Deliberately NOT used to tint the rest of the row: a room with only
 * fixed-white bulbs has no temperature to report, so tinting by it gave
 * two lit rooms two different accent colours for no reason the person
 * standing at the panel could see. */
#define MIREK_MIN 153
#define MIREK_MAX 500
static uint32_t mirek_color(int mirek)
{
    if (mirek < MIREK_MIN) mirek = MIREK_MIN;
    if (mirek > MIREK_MAX) mirek = MIREK_MAX;
    int t = (mirek - MIREK_MIN) * 255 / (MIREK_MAX - MIREK_MIN);
    int g = 244 - 97 * t / 255;
    int b = 255 - 214 * t / 255;
    return (uint32_t)((255 << 16) | (g << 8) | b);
}

/* A value we just sent is not in hue.txt yet: the daemon has to reach the
 * bridge and poll it back, which takes a moment. Reloading blindly in that
 * window puts the old value back and the knob springs backwards under the
 * finger that just moved it. Hold ours until the file agrees or the wait
 * runs out - whichever comes first, so a bridge that never applies the
 * command still converges on the truth. */
#define HUE_PENDING_SECS 4
static time_t room_pending[MAX_ROOMS];

static void hue_load(void)
{
    struct { int on, bri, mirek; } keep[MAX_ROOMS];
    int  n_keep = n_rooms;
    time_t now = time(NULL);
    for (int i = 0; i < n_keep; i++) {
        keep[i].on = rooms[i].on;
        keep[i].bri = rooms[i].bri;
        keep[i].mirek = rooms[i].mirek;
    }

    FILE *f = fopen(env_or("CAT_HUE", "hue.txt"), "r");
    n_rooms = 0;
    hue_updated = 0;
    if (!f) return;
    char line[160];
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "updated=", 8)) { hue_updated = atol(line + 8); continue; }
        if (!strncmp(line, "auto=", 5)) {
            char *save = NULL;
            char *t = strtok_r(line + 5, "|\n", &save);
            if (t) auto_on = atoi(t);
            if ((t = strtok_r(NULL, "|\n", &save)))
                snprintf(auto_why, sizeof auto_why, "%s", t);
            if ((t = strtok_r(NULL, "|\n", &save)))
                snprintf(auto_room, sizeof auto_room, "%s", t);
            continue;
        }
        if (strncmp(line, "room=", 5) || n_rooms >= MAX_ROOMS) continue;
        char *save = NULL;
        int field = 0;
        /* Assume reachable: an older hue.py writes three fields, and a
         * room you cannot control is the worse thing to guess wrong. */
        rooms[n_rooms].reachable = 1;
        rooms[n_rooms].mirek = 0;
        for (char *tok = strtok_r(line + 5, "|\n", &save); tok && field < 5;
             tok = strtok_r(NULL, "|\n", &save), field++) {
            switch (field) {
                case 0: snprintf(rooms[n_rooms].name, sizeof rooms[0].name, "%s", tok); break;
                case 1: rooms[n_rooms].on  = atoi(tok); break;
                case 2: rooms[n_rooms].bri = atoi(tok); break;
                case 3: rooms[n_rooms].reachable = atoi(tok); break;
                case 4: rooms[n_rooms].mirek = atoi(tok); break;
            }
        }
        if (field >= 3) n_rooms++;
    }
    fclose(f);

    for (int i = 0; i < n_rooms && i < n_keep; i++) {
        if (!room_pending[i]) continue;
        /* The file caught up, or we waited long enough. Either way stop
         * second-guessing it. */
        if (now >= room_pending[i] ||
            (rooms[i].on == keep[i].on && rooms[i].bri == keep[i].bri &&
             rooms[i].mirek == keep[i].mirek)) {
            room_pending[i] = 0;
            continue;
        }
        rooms[i].on = keep[i].on;
        rooms[i].bri = keep[i].bri;
        rooms[i].mirek = keep[i].mirek;
    }
}

/* The bridge is on the LAN, but the round trip still runs through a file
 * and a 2s poll. The widget moves now and the next poll confirms it. */
static void hue_send(int idx)
{
    FILE *f = fopen(env_or("CAT_HUE_CMD", "hue.cmd"), "a");
    if (!f) return;
    /* mirek 0 means "leave the temperature alone" - which is what a room
     * of fixed-white bulbs always sends. */
    fprintf(f, "set %d %d %d %d\n", idx, rooms[idx].on, rooms[idx].bri, rooms[idx].mirek);
    fclose(f);
    room_pending[idx] = time(NULL) + HUE_PENDING_SECS;
}

static int hue_ok(void) { return hue_updated && (long)time(NULL) - hue_updated < 30; }

/* Minutes from now until "HH:MM", negative if it has been and gone. */
static int mins_until(const char *hhmm, const struct tm *t)
{
    int h, m;
    if (sscanf(hhmm, "%d:%d", &h, &m) != 2) return -1;
    return (h * 60 + m) - (t->tm_hour * 60 + t->tm_min);
}

/* Weekday mornings only, and only while the data is worth trusting: a
 * departure board ten minutes stale is worse than none. */
static int transit_show(const struct tm *t)
{
    if (n_trips == 0 || t->tm_wday == 0 || t->tm_wday == 6) return 0;
    if (!sched_in_window(t->tm_hour * 60 + t->tm_min, transit_start, transit_end)) return 0;
    return transit_updated && (long)time(NULL) - transit_updated < 15 * 60;
}

/* --- queries -------------------------------------------------------- */

static const char *DAY[]   = { "Sunday", "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday" };
static const char *DAY3[]  = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *MONTH[] = { "January", "February", "March", "April", "May", "June",
                               "July", "August", "September", "October", "November", "December" };
static const char *MON3[]  = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static struct tm now_tm(void) { time_t n = time(NULL); return *localtime(&n); }
static long evt_day(const evt_t *e) { return sched_day_num(e->y, e->mo, e->d); }
static long today_num(void) { struct tm t = now_tm(); return sched_day_num(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday); }

/* Drop every event of kind `t` on day `dn` - the undo for a mistaken tap. */
static void store_remove_day(long dn, char t);

static int scheduled_per_week(void)
{
    int n = 0;
    for (int i = 0; i < 7; i++) if (med_mask & (1 << i)) n++;
    return n;
}

/* Latest event of kind `t` on day `dn`, or NULL. */
static const evt_t *evt_on_day(long dn, char t)
{
    for (int i = n_evts - 1; i >= 0; i--)
        if (evts[i].t == t && evt_day(&evts[i]) == dn) return &evts[i];
    return NULL;
}

static int count_between(long from, long to, char t)   /* inclusive */
{
    int n = 0;
    for (int i = 0; i < n_evts; i++) {
        long d = evt_day(&evts[i]);
        if (evts[i].t == t && d >= from && d <= to) n++;
    }
    return n;
}

static void store_remove_day(long dn, char t)
{
    int w = 0;
    for (int i = 0; i < n_evts; i++)
        if (!(evts[i].t == t && evt_day(&evts[i]) == dn)) evts[w++] = evts[i];
    n_evts = w;
    store_rewrite();
}

/* Consecutive fully-complete weeks before the current one: every scheduled
 * day in the week has a dose logged. Stops at the first miss, so an empty
 * log gives 0. */
static int streak_weeks(void)
{
    if (!scheduled_per_week()) return 0;
    long this_mon = sched_monday(today_num());
    int weeks = 0;
    for (int w = 1; w <= 520; w++) {                     /* 10 years is plenty */
        long mon = this_mon - 7L * w;
        int complete = 1;
        for (int i = 0; i < 7 && complete; i++) {
            long d = mon + i;
            if ((med_mask >> sched_wday(d)) & 1) complete = evt_on_day(d, 'm') != NULL;
        }
        if (!complete) break;
        weeks++;
    }
    return weeks;
}

/* Doses scheduled in the given month up to and including today. */
static void month_progress(int y, int m, int *given, int *due)
{
    long today = today_num();
    int nd = sched_days_in_month(y, m);
    *given = 0; *due = 0;
    for (int d = 1; d <= nd; d++) {
        long dn = sched_day_num(y, m, d);
        if (dn > today) break;
        if ((med_mask >> sched_wday(dn)) & 1) (*due)++;
        if (evt_on_day(dn, 'm')) (*given)++;
    }
}

/* --- widget helpers ------------------------------------------------- */

static lv_obj_t *ui_backlight_slider;

static lv_obj_t *screens[4];
static int  cur_screen;
static int  cal_y, cal_m;                                /* month the calendar shows */
static lv_obj_t *rail_items[4];

/* A day in the week strip or the month grid: the number, and up to two
 * dots under it. Apple Calendar's language - a filled circle marks today
 * and small dots mark what happened, instead of flooding the cell with
 * colour, which drowned the vomiting marks in a wall of green. */
typedef struct { lv_obj_t *cell, *num, *dots, *dot[2]; } daycell_t;

static lv_obj_t *card_status, *lbl_status, *lbl_status_sub;
static lv_obj_t *status_dot, *btn_dose, *lbl_btn_dose, *icon_dose, *week_wd[7];
static lv_obj_t *lbl_week_no, *lbl_home_clock, *pop_event;
static lv_obj_t *lbl_home_date, *wx_img, *lbl_wx_temp, *lbl_wx_desc;
static lv_obj_t *card_week, *card_transit, *lbl_trip[SHOW_TRIPS];
static int overdue_now;
static daycell_t week_cell[7], cal_cell[42];
static lv_obj_t *lbl_cal_month, *rail_panel;
static time_t    rail_shown_at;
static lv_obj_t *lbl_stat_month, *lbl_stat_streak, *lbl_stat_events, *lbl_recent;
static lv_obj_t *lbl_bl_val, *sld_dim, *lbl_dim_val, *day_pill[7], *sw_reminder;
static lv_obj_t *lbl_reminder, *lbl_footer, *lbl_toast;
static struct {
    lv_obj_t *card, *ico, *name, *sld, *val, *val_ct;
} room_row[MAX_ROOMS];
static lv_obj_t *lbl_hue_none, *btn_auto, *lbl_auto;
static int       laid_out_rooms = -1;   /* card geometry is sized to the count */
static time_t    toast_until;
static int       blink_on;

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *text(lv_obj_t *parent, int x, int y, const char *s,
                      const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, s);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t *dot(lv_obj_t *parent, int d, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

static void daycell_create(daycell_t *c, lv_obj_t *parent, int x, int y, int d,
                           const lv_font_t *font)
{
    c->cell = box(parent, x, y, d, d, C_SURF1, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(c->cell, LV_OPA_0, 0);

    c->num = lv_label_create(c->cell);
    lv_obj_set_style_text_font(c->num, font, 0);
    lv_label_set_text(c->num, "");
    lv_obj_align(c->num, LV_ALIGN_CENTER, 0, -5);

    /* Flex centres whatever is visible, and LVGL skips hidden children in
     * layout, so one dot centres itself and two sit either side. */
    c->dots = lv_obj_create(c->cell);
    lv_obj_remove_style_all(c->dots);
    lv_obj_set_size(c->dots, d, 10);
    lv_obj_align(c->dots, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_flex_flow(c->dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c->dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(c->dots, 6, 0);
    lv_obj_clear_flag(c->dots, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 2; i++) {
        c->dot[i] = dot(c->dots, 8, C_OK_FILL);
        lv_obj_add_flag(c->dot[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void daycell_set(daycell_t *c, int dom, int today, uint32_t fg,
                        int dose, int vomit)
{
    lv_label_set_text_fmt(c->num, "%d", dom);
    if (today) {
        lv_obj_set_style_bg_color(c->cell, lv_color_hex(C_ACC_FILL), 0);
        lv_obj_set_style_bg_opa(c->cell, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(c->num, lv_color_hex(C_ACC_ON), 0);
    } else {
        lv_obj_set_style_bg_opa(c->cell, LV_OPA_0, 0);
        lv_obj_set_style_text_color(c->num, lv_color_hex(fg), 0);
    }
    int n = 0;
    if (dose)  { lv_obj_set_style_bg_color(c->dot[n], lv_color_hex(C_OK_FILL), 0);
                 lv_obj_clear_flag(c->dot[n], LV_OBJ_FLAG_HIDDEN); n++; }
    if (vomit) { lv_obj_set_style_bg_color(c->dot[n], lv_color_hex(C_BAD_FILL), 0);
                 lv_obj_clear_flag(c->dot[n], LV_OBJ_FLAG_HIDDEN); n++; }
    while (n < 2) lv_obj_add_flag(c->dot[n++], LV_OBJ_FLAG_HIDDEN);
}

/* Every button here is flat. lv_btn's default style draws a shadow under
 * it, which reads as a dark lip along the bottom edge and does not match
 * the cards. Going through this stops the next button reintroducing it. */
static lv_obj_t *flat_btn(lv_obj_t *parent)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_style_shadow_width(b, 0, 0);
    return b;
}

static void refresh(void);

/* --- events --------------------------------------------------------- */

static void toast(const char *msg)
{
    lv_label_set_text(lbl_toast, msg);
    lv_obj_align(lbl_toast, LV_ALIGN_BOTTOM_MID, 0, -16);   /* width changed */
    lv_obj_clear_flag(lbl_toast, LV_OBJ_FLAG_HIDDEN);
    toast_until = time(NULL) + 3;
}

static void rail_show(void)
{
    if (!rail_panel) return;
    lv_obj_clear_flag(rail_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(rail_panel);
    rail_shown_at = time(NULL);
}

static void rail_hide(void)
{
    if (rail_panel) lv_obj_add_flag(rail_panel, LV_OBJ_FLAG_HIDDEN);
    rail_shown_at = 0;
}

/* Tap the rail anywhere that is not an icon: the reliable way to dismiss. */
static void rail_click_cb(lv_event_t *e)
{
    (void)e;
    rail_hide();
}

/* A swipe from a 40px strip is a fiddly thing to land on a wall panel, so
 * a plain tap on the grip opens it too. Both are harmless to fire twice. */
static void edge_click_cb(lv_event_t *e)
{
    (void)e;
    rail_show();
}

static void show_screen(int i)
{
    cur_screen = i;
    if (pop_event) lv_obj_add_flag(pop_event, LV_OBJ_FLAG_HIDDEN);
    for (int k = 0; k < 4; k++) {
        if (k == i) lv_obj_clear_flag(screens[k], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(screens[k], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(rail_items[k], k == i ? LV_OPA_COVER : LV_OPA_0, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(rail_items[k], 0),
                                    lv_color_hex(k == i ? C_ACC_TEXT : C_TEXT2), 0);
    }
    /* Picking a screen is the end of what the rail is for. */
    if (rail_shown_at) rail_hide();
    refresh();
}

static void rail_cb(lv_event_t *e)   { show_screen((int)(intptr_t)lv_event_get_user_data(e)); }
static void dose_cb(lv_event_t *e)
{
    (void)e;
    long today = today_num();
    if (evt_on_day(today, 'm')) { store_remove_day(today, 'm'); toast("Today's dose cleared"); }
    else                        { store_append('m');            toast("Dose logged"); }
    refresh();
}
/* lv_msgbox defaults to LV_DPI_DEF*2 = 260px wide with 86px buttons. On a
 * 1280px panel with 24pt text that clips the labels ("Vomiting" alone is
 * wider than its button), so every dialog gets sized explicitly. */
static lv_obj_t *dialog(const char *title, const char *body,
                        const char **btns, lv_event_cb_t cb)
{
    lv_obj_t *mb = lv_msgbox_create(NULL, title, body, btns, false);

    /* The backdrop defaults to translucent black, which forces every widget
     * underneath to be redrawn and alpha-blended - the most expensive thing
     * this UI can ask for, and the reason the dialog crawled down the screen.
     * Opaque lets LVGL's cover check skip them entirely, and over a black
     * ground it looks the same. */
    lv_obj_t *bg = lv_obj_get_parent(mb);
    if (bg) {
        lv_obj_set_style_bg_color(bg, lv_color_hex(C_BG), 0);
        lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, 0);
    }

    lv_obj_set_width(mb, 780);
    lv_obj_set_style_bg_color(mb, lv_color_hex(C_SURF1), 0);
    lv_obj_set_style_text_color(mb, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(mb, &lv_font_montserrat_24, 0);
    lv_obj_set_style_border_width(mb, 0, 0);
    lv_obj_set_style_radius(mb, 22, 0);
    lv_obj_set_style_pad_all(mb, 28, 0);

    lv_obj_t *t = lv_msgbox_get_title(mb);
    if (t) lv_obj_set_style_text_font(t, &lv_font_montserrat_32, 0);

    lv_obj_t *b = lv_msgbox_get_btns(mb);
    if (b) {
        lv_obj_set_size(b, 724, 78);
        lv_obj_set_style_text_font(b, &lv_font_montserrat_24, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_0, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_pad_column(b, 14, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(C_ACC_BG), LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_text_color(b, lv_color_hex(C_TEXT), LV_PART_ITEMS);
        lv_obj_set_style_radius(b, 14, LV_PART_ITEMS);
    }
    lv_obj_add_event_cb(mb, cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mb);
    return mb;
}

/* An action sheet anchored under the button, not a modal. A full-screen
 * dialog for a two-way choice meant invalidating all 1280x720 and redrawing
 * every widget beneath it; this invalidates 310x202 and nothing else. */
static void pop_pick_cb(lv_event_t *e)
{
    char t = (char)(intptr_t)lv_event_get_user_data(e);
    lv_obj_add_flag(pop_event, LV_OBJ_FLAG_HIDDEN);
    store_append(t);
    toast(t == 'v' ? "Vomiting logged" : "Food logged");
    refresh();
}

static void event_cb(lv_event_t *e)
{
    (void)e;
    /* Tapping the button again closes it, which is why there is no Cancel
     * row and no full-screen tap-catcher - a catcher would invalidate the
     * whole screen, which is the cost this change exists to avoid. */
    if (lv_obj_has_flag(pop_event, LV_OBJ_FLAG_HIDDEN))
        lv_obj_clear_flag(pop_event, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(pop_event, LV_OBJ_FLAG_HIDDEN);
}

static void cal_step_cb(lv_event_t *e)
{
    cal_m += (int)(intptr_t)lv_event_get_user_data(e);
    if (cal_m < 1)  { cal_m = 12; cal_y--; }
    if (cal_m > 12) { cal_m = 1;  cal_y++; }
    refresh();
}

static void day_pill_cb(lv_event_t *e)
{
    med_mask ^= 1 << (int)(intptr_t)lv_event_get_user_data(e);
    cfg_save();
    refresh();
}

/* The value labels are written here as well as in refresh_settings, because
 * refresh_settings only runs once a minute now - the sliders have to track
 * the drag themselves. */
static void backlight_label(int pct) { lv_label_set_text_fmt(lbl_bl_val, "%d%%", pct); }

static void dim_label(int pct) { lv_label_set_text_fmt(lbl_dim_val, "%d%%", pct); }

static void backlight_cb(lv_event_t *e)
{
    int pct = (int)lv_slider_get_value(lv_event_get_target(e));
    ui_backlight_apply(pct);
    backlight_label(pct);
}

/* Saved on release, not on every value change: dragging the slider would
 * otherwise write to the SD card a few hundred times per swipe. */
static void backlight_save_cb(lv_event_t *e)
{
    backlight_pct = (int)lv_slider_get_value(lv_event_get_target(e));
    cfg_save();
}

static void dim_cb(lv_event_t *e)
{
    dim_pct = (int)lv_slider_get_value(lv_event_get_target(e));
    dim_label(dim_pct);
    /* Apply live if we are already in the window, so the slider shows what
     * it will actually look like tonight. */
    if (dimmed) ui_backlight_apply(dim_pct);
}

/* Saved on release like the backlight slider. Doing it per value change
 * wrote the SD card - and ran a full refresh(), rebuilding the calendar -
 * on every pixel of the drag. */
static void dim_save_cb(lv_event_t *e)
{
    (void)e;
    cfg_save();
}

static void reminder_cb(lv_event_t *e)
{
    reminder_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    cfg_save();
    refresh();
}

static void exit_confirm_cb(lv_event_t *e)
{
    lv_obj_t *mb = lv_event_get_current_target(e);
    if (lv_msgbox_get_active_btn(mb) == 0) {
        printf("exit requested from the UI\n");
        fflush(stdout);
        /* Clean exit(0). The unit is Restart=on-failure, so systemd leaves it
         * stopped rather than bouncing it back in 3s, and ExecStopPost puts
         * the console back on the framebuffer - which is the whole point of
         * the button. Nothing to flush: the log is written through on append. */
        exit(0);
    }
    lv_msgbox_close(mb);
}

static void exit_cb(lv_event_t *e)
{
    (void)e;
    static const char *btns[] = { "Quit to shell", "Cancel", "" };
    dialog("Exit dashboard",
           "Stops the dashboard and puts the console back on the panel.\n"
           "Start it again with:  sudo systemctl start lvglapp",
           btns, exit_confirm_cb);
}

static void reset_confirm_cb(lv_event_t *e)
{
    lv_obj_t *mb = lv_event_get_current_target(e);
    if (lv_msgbox_get_active_btn(mb) == 0) {
        store_clear_all();
        toast("All logged data cleared");
        refresh();
    }
    lv_msgbox_close(mb);
}

static void reset_cb(lv_event_t *e)
{
    (void)e;
    static const char *btns[] = { "Delete everything", "Cancel", "" };
    dialog("Reset data",
           "Deletes every logged dose and event.\n"
           "Settings and the schedule are kept. This cannot be undone.",
           btns, reset_confirm_cb);
}

/* The log is already a CSV; "export" just drops a dated copy beside it that
 * can be scp'd off without touching the live file the app appends to. */
static void export_cb(lv_event_t *e)
{
    (void)e;
    struct tm t = now_tm();
    char dst[256], buf[4096];
    snprintf(dst, sizeof dst, "cat_log_%04d-%02d-%02d_%02d%02d.csv",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    FILE *in = fopen(log_path(), "r");
    if (!in) { toast("Nothing to export yet"); return; }
    FILE *out = fopen(dst, "w");
    if (!out) { fclose(in); toast("Export failed"); return; }
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
    fclose(in); fclose(out);
    printf("exported log to %s\n", dst);
    toast(dst);
}

/* --- screens -------------------------------------------------------- */

/* Rail link-status icons ------------------------------------------------
 * Green when up, grey when not. Both checks are one cheap sysfs lookup, so
 * they run straight off the 1Hz tick with no helper thread or polling of
 * nmcli/bluetoothctl (spawning those every few seconds on a Pi 3A+ would
 * cost far more than the icons are worth). */
static lv_obj_t *ico_wifi, *ico_bt;

/* CPU busy% between calls, from /proc/stat jiffies. Needs a previous
 * sample to diff against, which the 5s tick supplies; the first call has
 * nothing to compare and reports "--". */
/* MiB in use. MemAvailable, not MemFree: on 424MB of RAM the page cache
 * makes MemFree look alarming while the memory is in fact reclaimable. */
static int wifi_up(void)
{
    char st[16] = { 0 };
    FILE *f = fopen("/sys/class/net/wlan0/operstate", "r");
    if (!f) return 0;
    char *ok = fgets(st, sizeof st, f);
    fclose(f);
    return ok && strncmp(st, "up", 2) == 0;
}

static int bt_connected(void)
{
    /* Every live connection is a /sys/class/bluetooth/hci0/hci0:<handle>
     * directory, so the glob answers "is anything connected" without
     * naming a specific device. */
    glob_t g;
    int n = 0;
    if (glob("/sys/class/bluetooth/hci0/hci0:*", 0, NULL, &g) == 0) n = (int)g.gl_pathc;
    globfree(&g);
    return n > 0;
}

static void refresh_status_icons(void)
{
    if (ico_wifi)
        lv_obj_set_style_text_color(ico_wifi, lv_color_hex(wifi_up() ? C_OK_FILL : C_MUTED), 0);
    if (ico_bt)
        lv_obj_set_style_text_color(ico_bt, lv_color_hex(bt_connected() ? C_OK_FILL : C_MUTED), 0);

}

static void build_rail(lv_obj_t *parent)
{
    static const char *icons[4] = { LV_SYMBOL_HOME, LV_SYMBOL_LIST,
                                    LV_SYMBOL_CHARGE, LV_SYMBOL_SETTINGS };
    (void)parent;

    /* Tap strip down the left edge that summons the rail. */
    lv_obj_t *edge = box(lv_layer_top(), 0, 0, EDGE_W, SCR_H, C_BG, 0);
    lv_obj_set_style_bg_opa(edge, LV_OPA_0, 0);
    lv_obj_add_flag(edge, LV_OBJ_FLAG_CLICKABLE);
    /* LVGL re-searches the object under the finger every poll unless this
     * is set, so a press that drifts off the strip would click whatever is
     * underneath - the photo, in practice. */
    lv_obj_add_flag(edge, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(edge, edge_click_cb, LV_EVENT_CLICKED, NULL);

    /* Something to aim at: a drawer grip, so the rail is not invisible
     * affordance-wise. Not clickable itself - the strip behind it takes
     * the press, and a child would steal it. */
    lv_obj_t *grip = box(edge, 9, SCR_H / 2 - 30, 5, 60, C_MUTED, LV_RADIUS_CIRCLE);
    lv_obj_clear_flag(grip, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *rail = box(lv_layer_top(), 0, 0, RAIL_W, SCR_H, C_SURF1, 0);
    rail_panel = rail;
    lv_obj_add_flag(rail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rail, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(rail, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(rail, rail_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(rail, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(rail, 1, 0);

    for (int i = 0; i < 4; i++) {
        lv_obj_t *it = box(rail, 14, 20 + i * 80, 64, 64, C_ACC_BG, 16);
        lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(it, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(it, rail_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(it);
        lv_label_set_text(l, icons[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_32, 0);
        lv_obj_center(l);
        rail_items[i] = it;
    }

    /* Status icons sit at the foot of the rail, clear of the three nav
     * items (which end at y=297). Labels, so they are not clickable. */
    static const struct {
        const char *sym; int y; const lv_font_t *font; uint32_t col; lv_obj_t **out;
    } st[] = {
        { LV_SYMBOL_WIFI,      SCR_H - 170, &lv_font_montserrat_28, C_MUTED, &ico_wifi },
        { LV_SYMBOL_BLUETOOTH, SCR_H - 126, &lv_font_montserrat_28, C_MUTED, &ico_bt   },
    };
    for (unsigned i = 0; i < sizeof st / sizeof st[0]; i++) {
        lv_obj_t *l = lv_label_create(rail);
        lv_label_set_text(l, st[i].sym);
        lv_obj_set_style_text_font(l, st[i].font, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(st[i].col), 0);
        lv_obj_set_width(l, RAIL_W);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(l, 0, st[i].y);
        *st[i].out = l;
    }
    refresh_status_icons();
}

/* Photos are files, not compiled-in C arrays: drop PNGs in photos/ (or a
 * single cat.png) and restart, no rebuild. They shuffle like a digital
 * portrait, one a day at midnight. */
#define MAX_PHOTOS 64
#define PHOTO_PAD  5       /* per side, so the photo sits just inside the card */
#define PHOTO_RADIUS 15      /* card radius 20 less the 5px inset, so it stays concentric */

static char      photo_src[MAX_PHOTOS][288];
static int       n_photos, photo_i;
static lv_obj_t *photo_img, *photo_wrap;

static void photos_scan(void)
{
    glob_t g;
    n_photos = 0;
    if (glob("photos/*.png", 0, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc && n_photos < MAX_PHOTOS; i++) {
            char cand[288];
            lv_img_header_t hdr;
            snprintf(cand, sizeof cand, "A:%s", g.gl_pathv[i]);
            /* A full-size phone photo decodes to ~48MB and would blow
             * LV_MEM_SIZE, so refuse it here rather than at draw time. */
            if (lv_img_decoder_get_info(cand, &hdr) != LV_RES_OK ||
                (long)hdr.w * hdr.h * 4 >= 6L * 1024 * 1024) {
                printf("photo: skipping %s, too large to decode\n", g.gl_pathv[i]);
                continue;
            }
            memcpy(photo_src[n_photos++], cand, sizeof cand);
        }
    }
    globfree(&g);

    if (n_photos == 0 && access(img_path(), R_OK) == 0)      /* single-photo fallback */
        snprintf(photo_src[n_photos++], sizeof photo_src[0], "A:%s", img_path());
    printf("photos: %d usable\n", n_photos);
}

/* Fisher-Yates, once, with a fixed seed: this is the running order the
 * daily rotation walks through, not something reshuffled as it goes.
 * Shuffled rather than alphabetical so a month does not land entirely
 * inside one import batch. */
static void photos_shuffle(void)
{
    for (int i = n_photos - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        char t[288];
        memcpy(t, photo_src[i], sizeof t);
        memcpy(photo_src[i], photo_src[j], sizeof t);
        memcpy(photo_src[j], t, sizeof t);
    }
}

static void photo_show(int i)
{
    lv_img_header_t hdr;
    if (!photo_img || n_photos == 0) return;
    lv_img_set_src(photo_img, photo_src[i]);
    /* Each photo has its own dimensions, so the zoom is per-photo. Scale
     * down to fit; never upscale, a small photo would just blur. */
    if (lv_img_decoder_get_info(photo_src[i], &hdr) == LV_RES_OK && hdr.w > 0 && hdr.h > 0) {
        /* Cover, not fit: scale so the photo covers BOTH axes and let the
         * wrapper clip whatever hangs over. Upscaling is allowed now -
         * filling the card edge to edge is the point - so a photo smaller
         * than the window softens rather than letterboxing. */
        const int vw = PHOTO_W - 2 * PHOTO_PAD, vh = PHOTO_H - 2 * PHOTO_PAD;
        /* Round the zoom UP: truncating leaves a 1-2px sliver of card
         * showing along one edge, which is exactly what cover must not do. */
        int zx = (256 * vw + hdr.w - 1) / hdr.w;
        int zy = (256 * vh + hdr.h - 1) / hdr.h;
        int zoom = zx > zy ? zx : zy;
        if (zoom < 16)   zoom = 16;
        if (zoom > 1024) zoom = 1024;
        lv_img_set_zoom(photo_img, (uint16_t)zoom);
        if (zoom > 256)
            printf("photo %d: %dx%d upscaled to %d%% - re-export at >=%dx%d to keep it sharp\n",
                   i, hdr.w, hdr.h, zoom * 100 / 256, vw, vh);

        /* Round the picture's own corners. clip_corner masks an object's
         * CHILDREN, so the mask has to live on a wrapper sized to the drawn
         * image - the card is bigger than the photo, so its corners are
         * nowhere near them. lv_img_get_transformed_size is not public in
         * 8.3, but zoom and the header give the same answer. */
        /* The wrapper is now the visible window, fixed to the card rather
         * than sized to the image, so LVGL's default child clipping crops
         * the overflow to it. */
        lv_obj_set_size(photo_wrap, vw, vh);
        lv_obj_center(photo_wrap);
    }
    lv_obj_center(photo_img);
}

/* One photo a day, picked by the date rather than at random, so a restart
 * shows the same picture the rest of the day does. */
static void photo_for_today(void)
{
    if (n_photos < 1) return;
    photo_i = (int)(today_num() % n_photos);
    photo_show(photo_i);
}

/* A tap overrides the day's pick until midnight. */
static void photo_tap_cb(lv_event_t *e)
{
    (void)e;
    if (n_photos < 2) return;
    if (++photo_i >= n_photos) photo_i = 0;
    photo_show(photo_i);
}

static void build_photo(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *c = box(parent, x, y, w, h, C_SURF1, 20);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, photo_tap_cb, LV_EVENT_CLICKED, NULL);
    photos_scan();

    if (n_photos > 0) {
        photos_shuffle();
        photo_wrap = box(c, 0, 0, 10, 10, C_SURF1, PHOTO_RADIUS);
        lv_obj_clear_flag(photo_wrap, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(photo_wrap, LV_OPA_0, 0);
        lv_obj_set_style_clip_corner(photo_wrap, true, 0);
        photo_img = lv_img_create(photo_wrap);
        lv_obj_clear_flag(photo_img, LV_OBJ_FLAG_CLICKABLE);
        lv_img_set_antialias(photo_img, true);
        photo_for_today();
    } else {
        printf("photos: none found - showing placeholder\n");
        lv_obj_set_style_bg_color(c, lv_color_hex(C_WARN_BG), 0);
        lv_obj_t *l = text(c, 0, 0, "Put her photos in\nphotos/", &lv_font_montserrat_24, C_WARN_TEXT);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(l);
    }
}

static void build_home(lv_obj_t *s)
{
    build_photo(s, 0, 0, PHOTO_W, PHOTO_H);

    card_status = box(s, RIGHT_X, STATUS_Y, RIGHT_W, STATUS_H, C_SURF1, 20);
    status_dot     = dot(card_status, 14, C_OK_FILL);
    lbl_status     = text(card_status, 0, 0, "", &lv_font_montserrat_32, C_TEXT);
    lbl_status_sub = text(card_status, 0, 0, "", &lv_font_montserrat_24, C_TEXT2);
    /* Centred off the fonts' own line heights rather than measured by eye,
     * so changing a size does not silently leave the block off-centre. */
    {
        int h1 = lv_font_get_line_height(&lv_font_montserrat_32);
        int h2 = lv_font_get_line_height(&lv_font_montserrat_24);
        int blk = h1 + 10 + h2;
        lv_obj_align(lbl_status,     LV_ALIGN_LEFT_MID, 62, -(blk - h1) / 2);
        lv_obj_align(lbl_status_sub, LV_ALIGN_LEFT_MID, 62,  (blk - h2) / 2);
        lv_obj_align(status_dot,     LV_ALIGN_LEFT_MID, 32, -(blk - h1) / 2);
    }

    int bw = BTN_W;
    btn_dose = flat_btn(s);
    lv_obj_set_pos(btn_dose, RIGHT_X, BTNS_Y);
    lv_obj_set_size(btn_dose, bw, BTNS_H);
    lv_obj_set_style_bg_color(btn_dose, lv_color_hex(C_ACC_FILL), 0);
    lv_obj_set_style_radius(btn_dose, 16, 0);
    /* Explicit, so the fit is arithmetic rather than theme-dependent:
     * BTN_W 291 - 20 = 271 usable, and the widest label
     * "<refresh>  Undo today's dose" measures 257 at montserrat_24. */
    lv_obj_set_style_pad_hor(btn_dose, 10, 0);
    lv_obj_add_event_cb(btn_dose, dose_cb, LV_EVENT_CLICKED, NULL);
    /* Icon + label in a flex row. LVGL's symbol font has no pill glyph, so
     * it is a small PNG loaded at runtime like the photos - flex skips
     * hidden children, so hiding it re-centres the label on its own. */
    lv_obj_t *row = lv_obj_create(btn_dose);
    lv_obj_remove_style_all(row);
    /* lv_obj_create() is CLICKABLE by default and remove_style_all() does not
     * touch flags. Centred over the button, the row swallowed every tap that
     * landed on the icon or label - i.e. the middle, where people actually
     * press - so only edge taps reached btn_dose. */
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 16, 0);
    lv_obj_center(row);

    icon_dose = lv_img_create(row);
    if (access("icons/pill.png", R_OK) == 0) lv_img_set_src(icon_dose, "A:icons/pill.png");
    else printf("icons/pill.png missing - button shows text only\n");

    lbl_btn_dose = lv_label_create(row);
    /* 24, not 28: the button is BTN_W (310px) wide and the toggled label
     * "<refresh>  Undo today's dose" overflows at 28. Same size in both
     * states so the button does not visibly resize its text on tap. */
    lv_obj_set_style_text_font(lbl_btn_dose, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_btn_dose, lv_color_hex(C_ACC_ON), 0);
    lv_label_set_text(lbl_btn_dose, "Log dose given");

    lv_obj_t *b2 = flat_btn(s);
    lv_obj_set_pos(b2, RIGHT_X + bw + 22, BTNS_Y);
    lv_obj_set_size(b2, bw, BTNS_H);
    lv_obj_set_style_bg_color(b2, lv_color_hex(C_BORDER_ST), 0);
    lv_obj_set_style_bg_opa(b2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b2, 0, 0);
    lv_obj_set_style_radius(b2, 16, 0);
    lv_obj_set_style_pad_hor(b2, 10, 0);
    lv_obj_add_event_cb(b2, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l2 = text(b2, 0, 0, LV_SYMBOL_WARNING "  Log event", &lv_font_montserrat_28, C_TEXT);
    lv_obj_center(l2);

    /* Clock and weather share one card in the gap between the buttons and
     * the week strip: time and date left, conditions right. */
    lv_obj_t *ck = box(s, RIGHT_X, CLOCK_Y, RIGHT_W, CLOCK_H, C_SURF1, 20);
    lbl_home_clock = text(ck, 0, 0, "", &font_clock_60, C_TEXT);
    lbl_home_date  = text(ck, 0, 0, "", &lv_font_montserrat_20, C_TEXT2);
    {
        int hc = lv_font_get_line_height(&font_clock_60);
        int hd = lv_font_get_line_height(&lv_font_montserrat_20);
        int blk = hc + 6 + hd;
        lv_obj_align(lbl_home_clock, LV_ALIGN_LEFT_MID, 28, -(blk - hc) / 2);
        lv_obj_align(lbl_home_date,  LV_ALIGN_LEFT_MID, 28,  (blk - hd) / 2);
    }

    wx_img = lv_img_create(ck);
    lv_obj_align(wx_img, LV_ALIGN_RIGHT_MID, -26, 0);

    lbl_wx_temp = text(ck, 0, 0, "", &lv_font_montserrat_32, C_TEXT);
    lbl_wx_desc = text(ck, 0, 0, "", &lv_font_montserrat_20, C_TEXT2);
    {
        int ht = lv_font_get_line_height(&lv_font_montserrat_32);
        int hw = lv_font_get_line_height(&lv_font_montserrat_20);
        int blk = ht + 4 + hw;
        lv_obj_align(lbl_wx_temp, LV_ALIGN_RIGHT_MID, -100, -(blk - ht) / 2);
        lv_obj_align(lbl_wx_desc, LV_ALIGN_RIGHT_MID, -100,  (blk - hw) / 2);
    }

    lv_obj_t *wk = box(s, RIGHT_X, WEEK_Y, RIGHT_W, WEEK_H, C_SURF1, 20);
    card_week = wk;
    /* header, weekday row, day cells - centred as one block. */
    int hh = lv_font_get_line_height(&lv_font_montserrat_20);
    int wk_top = (WEEK_H - (hh + 16 + hh + 6 + 62)) / 2;
    int wd_y   = wk_top + hh + 16;
    int cell_y = wd_y + hh + 6;
    text(wk, 30, wk_top, "This week", &lv_font_montserrat_20, C_TEXT2);
    lbl_week_no = text(wk, 0, 0, "", &lv_font_montserrat_20, C_MUTED);
    lv_obj_align(lbl_week_no, LV_ALIGN_TOP_RIGHT, -30, wk_top);
    int step = (RIGHT_W - 60) / 7;
    for (int i = 0; i < 7; i++) {
        int x = 30 + i * step;
        week_wd[i] = text(wk, x, wd_y, "", &lv_font_montserrat_20, C_MUTED);
        lv_obj_set_width(week_wd[i], step - 8);
        lv_obj_set_style_text_align(week_wd[i], LV_TEXT_ALIGN_CENTER, 0);
        daycell_create(&week_cell[i], wk, x + (step - 8 - 62) / 2, cell_y, 62,
                       &lv_font_montserrat_24);
    }
}

#define POP_W 310
#define POP_RH 62

static void pop_row(int idx, int rows, const char *icon, const char *label,
                    uint32_t color, char kind)
{
    lv_obj_t *b = flat_btn(pop_event);
    lv_obj_set_size(b, POP_W - 16, POP_RH);
    lv_obj_set_pos(b, 0, idx * POP_RH);
    lv_obj_set_style_bg_opa(b, LV_OPA_0, 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_BORDER), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(b, pop_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)kind);

    lv_obj_align(text(b, 0, 0, icon, &lv_font_montserrat_24, color), LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_align(text(b, 0, 0, label, &lv_font_montserrat_24, color), LV_ALIGN_LEFT_MID, 58, 0);

    if (idx < rows - 1)
        box(pop_event, 58, (idx + 1) * POP_RH - 1, POP_W - 16 - 58, 1, C_BORDER, 0);
}

/* On lv_layer_top so it floats over whatever screen is showing, positioned
 * just under the Log event button in absolute screen coordinates. */
static void build_event_popover(void)
{
    const int x = BODY_X + RIGHT_X + BTN_W + 22;
    const int y = PAD + BTNS_Y + BTNS_H + 10;
    pop_event = box(lv_layer_top(), x, y, POP_W, 2 * POP_RH + 16, C_BORDER_ST, 18);
    lv_obj_set_style_pad_all(pop_event, 8, 0);
    lv_obj_set_style_border_width(pop_event, 1, 0);
    lv_obj_set_style_border_color(pop_event, lv_color_hex(C_BORDER), 0);
    lv_obj_add_flag(pop_event, LV_OBJ_FLAG_HIDDEN);

    pop_row(0, 2, LV_SYMBOL_WARNING, "Vomiting", C_BAD_TEXT, 'v');
    pop_row(1, 2, LV_SYMBOL_PLUS,    "Food",     C_TEXT,     'f');
}

/* Same rect as the week strip; refresh_home shows one or the other. The
 * built-in font is ASCII plus a few symbols, so no arrow glyph and no
 * Swedish vowels - LV_SYMBOL_RIGHT and plain spellings instead. */
static void build_transit(lv_obj_t *s)
{
    card_transit = box(s, RIGHT_X, WEEK_Y, RIGHT_W, WEEK_H, C_SURF1, 20);
    lv_obj_add_flag(card_transit, LV_OBJ_FLAG_HIDDEN);

    int hh = lv_font_get_line_height(&lv_font_montserrat_20);
    int ht = lv_font_get_line_height(&lv_font_montserrat_24);
    int top = (WEEK_H - (hh + 16 + (SHOW_TRIPS - 1) * 42 + ht)) / 2;
    text(card_transit, 30, top, "Varnhem " LV_SYMBOL_RIGHT " Scheeleparken",
         &lv_font_montserrat_20, C_TEXT2);
    for (int i = 0; i < SHOW_TRIPS; i++)
        lbl_trip[i] = text(card_transit, 30, top + hh + 16 + i * 42, "",
                           &lv_font_montserrat_24, C_TEXT);
}

static void build_calendar(lv_obj_t *s)
{
    /* Sized so six rows plus the legend land inside BODY_H with room to
     * spare: 4 + 56 header, 104 grid start, 6 rows of 74, legend at 556. */
    const int CAL_W = 540, CELL = 64, STEP = 74, GRID_Y = 104;

    lv_obj_t *arrow[2];
    for (int i = 0; i < 2; i++) {
        arrow[i] = flat_btn(s);
        lv_obj_set_pos(arrow[i], i ? CAL_W - 56 : 0, 4);
        lv_obj_set_size(arrow[i], 56, 56);
        lv_obj_set_style_bg_opa(arrow[i], LV_OPA_0, 0);
        lv_obj_set_style_radius(arrow[i], 14, 0);
        lv_obj_set_style_bg_color(arrow[i], lv_color_hex(C_BORDER_ST), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(arrow[i], LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_add_event_cb(arrow[i], cal_step_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)(i ? 1 : -1));
        lv_obj_center(text(arrow[i], 0, 0, i ? LV_SYMBOL_RIGHT : LV_SYMBOL_LEFT,
                           &lv_font_montserrat_28, C_TEXT2));
    }

    lbl_cal_month = text(s, 56, 14, "", &lv_font_montserrat_32, C_TEXT);
    lv_obj_set_width(lbl_cal_month, CAL_W - 112);
    lv_obj_set_style_text_align(lbl_cal_month, LV_TEXT_ALIGN_CENTER, 0);

    /* Weeks run Mon..Sun, like the mockup. */
    static const char *wd[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    for (int i = 0; i < 7; i++) {
        lv_obj_t *l = text(s, i * STEP, 72, wd[i], &lv_font_montserrat_20, C_MUTED);
        lv_obj_set_width(l, CELL);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    }
    for (int i = 0; i < 42; i++)
        daycell_create(&cal_cell[i], s, (i % 7) * STEP, GRID_Y + (i / 7) * STEP, CELL,
                       &lv_font_montserrat_24);

    int ly = GRID_Y + 5 * STEP + CELL + 18;
    box(s, 0, ly + 8, 18, 18, C_OK_FILL, LV_RADIUS_CIRCLE);
    text(s, 28, ly, "Dose given", &lv_font_montserrat_20, C_TEXT2);
    box(s, 190, ly + 8, 18, 18, C_BAD_FILL, LV_RADIUS_CIRCLE);
    text(s, 218, ly, "Event", &lv_font_montserrat_20, C_TEXT2);

    /* Right column: three stat cards over the recent list. */
    const int RX = CAL_W + GAP, RW = BODY_W - RX, SW = (RW - 24) / 3;
    const char *names[3] = { "This month", "Streak", "Vomiting" };
    lv_obj_t **vals[3] = { &lbl_stat_month, &lbl_stat_streak, &lbl_stat_events };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = box(s, RX + i * (SW + 12), 0, SW, 110, C_SURF1, 16);
        text(c, 16, 16, names[i], &lv_font_montserrat_20, C_TEXT2);
        *vals[i] = text(c, 16, 48, "", &lv_font_montserrat_32, C_TEXT);
    }
    text(s, RX, 132, "Recent", &lv_font_montserrat_20, C_TEXT2);
    lbl_recent = text(s, RX, 172, "", &lv_font_montserrat_20, C_TEXT);
    lv_obj_set_width(lbl_recent, RW);
    lv_label_set_long_mode(lbl_recent, LV_LABEL_LONG_CLIP);
}

#define SET_RH   112
#define SET_ROWS 4

static lv_obj_t *settings_group;

/* One card holding all the rows, split by hairlines inset past the icon -
 * four separate floating cards was a lot of chrome for four settings. */
static lv_obj_t *settings_row(int idx, const char *icon, const char *label)
{
    lv_obj_t *c = box(settings_group, 0, idx * SET_RH, BODY_W, SET_RH, C_SURF1, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_0, 0);
    lv_obj_t *i = text(c, 0, 0, icon, &lv_font_montserrat_24, C_TEXT2);
    lv_obj_align(i, LV_ALIGN_LEFT_MID, 32, 0);
    lv_obj_t *l = text(c, 0, 0, label, &lv_font_montserrat_24, C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 88, 0);
    if (idx < SET_ROWS - 1)
        box(settings_group, 88, (idx + 1) * SET_RH - 1, BODY_W - 88, 1, C_BORDER, 0);
    return c;
}

/* --- lights screen --------------------------------------------------- */

static void refresh_lights(void);

static void room_set_on(int i, int on)
{
    if (i >= n_rooms || !rooms[i].reachable) return;
    rooms[i].on = on;
    /* Switching on a room the bridge last saw at 0% would turn it on at
     * nothing. Give it a level worth seeing. */
    if (rooms[i].on && rooms[i].bri == 0) rooms[i].bri = 60;
    hue_send(i);
    refresh_lights();
}

/* The whole row is the tap target, which is why there is no on/off switch
 * on it: an 84px toggle is a small thing to hit standing at a wall panel,
 * and two ways to do the same thing on one row is one too many. The
 * sliders are children and LVGL does not bubble events by default, so a
 * press that lands on one never reaches this. */
/* Three whites rather than a slider: a 347-step range is a lot of choice
 * for a thing people actually want in one of about three states, and a
 * thin track is an awkward target on a wall. Values follow the usual
 * lighting guidance - under 3000K to wind down, ~4500K neutral, and the
 * bulb's cool limit to wake up. hue.py clamps each to what the room can
 * physically produce. */
static const struct { const char *name; int mirek, kelvin; } CT_PRESET[] = {
    { "Relax",    312, 3200 },
    { "Daylight", 222, 4500 },
    { "Energize", 182, 5500 },
};
#define N_CT_PRESET ((int)(sizeof CT_PRESET / sizeof CT_PRESET[0]))
/* Two stacked lines of montserrat_20 (25px), laid out explicitly. */
#define CT_LINE   25
#define CT_GAP    10
#define CT_PAD_Y  18
#define CT_BTN_H  (CT_PAD_Y * 2 + CT_LINE * 2 + CT_GAP)

static lv_obj_t *pop_ct;
static int       pop_ct_room = -1;
/* LVGL sends CLICKED on release even when LONG_PRESSED already fired, so
 * without this a long press would open the sheet and toggle the room. */
static int       ct_long_pressed;

static void ct_pop_hide(void)
{
    if (pop_ct) lv_obj_add_flag(pop_ct, LV_OBJ_FLAG_HIDDEN);
    pop_ct_room = -1;
}

static void ct_preset_cb(lv_event_t *e)
{
    int p = (int)(intptr_t)lv_event_get_user_data(e);
    int i = pop_ct_room;
    if (i < 0 || i >= n_rooms || !rooms[i].reachable) { ct_pop_hide(); return; }
    rooms[i].mirek = CT_PRESET[p].mirek;
    /* Setting a white on a dark room is how you preview it. */
    if (!rooms[i].on) rooms[i].on = 1;
    if (rooms[i].bri == 0) rooms[i].bri = 60;
    hue_send(i);
    ct_pop_hide();
    refresh_lights();
    toast(CT_PRESET[p].name);
}

/* One row: icon and name on the left, brightness above warmth on the
 * right. Both sliders are full-height children so a press on either never
 * reaches the card underneath, which is what toggles the room. */
#define AUTO_BAR_H 64
#define AUTO_BAR_Y (BODY_H - AUTO_BAR_H)
#define LIGHTS_H   (BODY_H - AUTO_BAR_H - CARD_GAP)
#define ROW_SLD_X  400
#define ROW_SLD_W  540
#define ROW_VAL_X  980

static void room_card_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= n_rooms) return;
    if (ct_long_pressed) { ct_long_pressed = 0; return; }   /* the sheet took it */
    if (pop_ct_room >= 0) { ct_pop_hide(); return; }        /* tap away = dismiss */
    room_set_on(i, !rooms[i].on);
}

/* Hold a room to pick its white. Rooms with fixed-white bulbs have nothing
 * to offer, so they just toggle. */
static void room_hold_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= n_rooms || !rooms[i].reachable || rooms[i].mirek <= 0) return;
    ct_long_pressed = 1;
    pop_ct_room = i;

    /* Sits on the row it belongs to, so there is no doubt which room is
     * about to change. */
    lv_obj_t *card = room_row[i].card;
    lv_obj_clear_flag(pop_ct, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align_to(pop_ct, card, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_move_foreground(pop_ct);

    /* Ring the nearest preset, and only if it is close enough to call set.
     * Nearest rather than a per-button window: the presets are 40 mireks
     * apart at the cool end, so any fixed window wide enough to feel
     * forgiving rings two buttons at once - 200 mirek, which is exactly
     * where the daylight automation parks the room, sits between Daylight
     * and Energize. */
    int best = -1;
    for (int p = 0; p < N_CT_PRESET; p++)
        if (best < 0 || abs(rooms[i].mirek - CT_PRESET[p].mirek)
                      < abs(rooms[i].mirek - CT_PRESET[best].mirek))
            best = p;
    if (abs(rooms[i].mirek - CT_PRESET[best].mirek) >= 30) best = -1;

    for (int p = 0; p < N_CT_PRESET; p++) {
        lv_obj_t *b = lv_obj_get_child(pop_ct, p);
        lv_obj_set_style_border_width(b, p == best ? 3 : 0, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(C_TEXT), 0);
    }
}

/* Dragging fires this per pixel. Writing a command each time would queue
 * hundreds of bridge calls for one gesture, so only the label moves here -
 * the command goes out on release, the same split as the backlight slider. */
static void room_sld_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= n_rooms) return;
    rooms[i].bri = lv_slider_get_value(lv_event_get_target(e));
    lv_label_set_text_fmt(room_row[i].val, "%d%%", rooms[i].bri);
}

static void room_sld_save_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= n_rooms || !rooms[i].reachable) return;
    /* Dragging a dark room up is how you turn it on; no second tap. */
    if (rooms[i].bri > 0 && !rooms[i].on) rooms[i].on = 1;
    hue_send(i);
    refresh_lights();
}

/* Kelvin is what the number on a bulb box says; mireks are the API's unit.
 * Rounded to 100K: nobody adjusts a lamp to 2843K, and at 50K the cool end
 * printed 6550 next to a preset button that says 6500. */
static int mirek_kelvin(int mirek)
{
    if (mirek < MIREK_MIN) mirek = MIREK_MIN;
    return ((1000000 / mirek) + 50) / 100 * 100;
}

/* The toggle only writes the setting; hue.py picks it up on its next pass
 * and reports back through hue.txt, so the label follows what is actually
 * happening rather than what was asked for. */
static void auto_btn_cb(lv_event_t *e)
{
    (void)e;
    hue_auto = !hue_auto;
    cfg_save();
    auto_on = hue_auto;
    toast(hue_auto ? "Daylight automation on" : "Daylight automation off");
    refresh_lights();
}

static void build_lights(lv_obj_t *s)
{
    for (int i = 0; i < MAX_ROOMS; i++) {
        lv_obj_t *c = box(s, 0, 0, BODY_W, 100, C_SURF1, 20);
        lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_event_cb(c, room_card_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        room_row[i].card = c;

        lv_obj_t *ico = text(c, 0, 0, LV_SYMBOL_CHARGE, &lv_font_montserrat_28, C_WARN_TEXT);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 28, 0);
        room_row[i].ico = ico;

        room_row[i].name = text(c, 0, 0, "", &lv_font_montserrat_28, C_TEXT);
        lv_obj_align(room_row[i].name, LV_ALIGN_LEFT_MID, 80, 0);

        lv_obj_t *sl = lv_slider_create(c);
        lv_obj_set_size(sl, ROW_SLD_W, 12);
        lv_obj_align(sl, LV_ALIGN_LEFT_MID, ROW_SLD_X, 0);
        lv_obj_set_style_pad_all(sl, 8, LV_PART_KNOB);
        lv_slider_set_range(sl, 0, 100);
        lv_obj_add_event_cb(sl, room_sld_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(sl, room_sld_save_cb, LV_EVENT_RELEASED, (void *)(intptr_t)i);
        lv_obj_set_style_bg_color(sl, lv_color_hex(C_WARN_TEXT), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sl, lv_color_hex(C_WARN_TEXT), LV_PART_KNOB);
        room_row[i].sld = sl;

        room_row[i].val = text(c, 0, 0, "", &lv_font_montserrat_24, C_TEXT2);
        lv_obj_align(room_row[i].val, LV_ALIGN_LEFT_MID, ROW_VAL_X, 0);

        /* Hint that the row has more behind a hold. Only shown on rooms
         * that can actually change white - see refresh_lights. */
        room_row[i].val_ct = text(c, 0, 0, "", &lv_font_montserrat_20, C_MUTED);
        lv_obj_align(room_row[i].val_ct, LV_ALIGN_LEFT_MID, 80, 18);

        lv_obj_add_event_cb(c, room_hold_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
    }

    /* One sheet reused by every row; room_hold_cb moves it. */
    pop_ct = box(s, 0, 0, N_CT_PRESET * 150 + 20, CT_BTN_H + 24, C_BORDER_ST, 20);
    lv_obj_add_flag(pop_ct, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_width(pop_ct, 1, 0);
    lv_obj_set_style_border_color(pop_ct, lv_color_hex(C_BORDER), 0);
    for (int p = 0; p < N_CT_PRESET; p++) {
        lv_obj_t *b = flat_btn(pop_ct);
        lv_obj_set_size(b, 138, CT_BTN_H);
        lv_obj_set_pos(b, 10 + p * 150, 12);
        lv_obj_set_style_radius(b, 16, 0);
        /* lv_btn inherits the theme's padding, and aligning one label to
         * TOP_MID and the other to BOTTOM_MID measures both from inside
         * it - on a short button the two land on each other. Zero the
         * padding and place both from the top, so the spacing is the
         * arithmetic below and not whatever the theme happens to use. */
        lv_obj_set_style_pad_all(b, 0, 0);
        /* Each swatch is painted the white it sets, so the choice is
         * visible rather than a word you have to translate to a colour. */
        lv_obj_set_style_bg_color(b, lv_color_hex(mirek_color(CT_PRESET[p].mirek)), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_add_event_cb(b, ct_preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)p);
        lv_obj_t *l = text(b, 0, 0, CT_PRESET[p].name, &lv_font_montserrat_20, C_BG);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, CT_PAD_Y);
        lv_obj_t *k = lv_label_create(b);
        lv_label_set_text_fmt(k, "%dK", CT_PRESET[p].kelvin);
        lv_obj_set_style_text_font(k, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(k, lv_color_hex(C_BG), 0);
        lv_obj_set_style_text_opa(k, LV_OPA_60, 0);
        lv_obj_align(k, LV_ALIGN_TOP_MID, 0, CT_PAD_Y + CT_LINE + CT_GAP);
    }

    btn_auto = flat_btn(s);
    lv_obj_set_size(btn_auto, 420, AUTO_BAR_H);
    lv_obj_set_pos(btn_auto, 0, AUTO_BAR_Y);
    lv_obj_set_style_bg_color(btn_auto, lv_color_hex(C_BORDER_ST), 0);
    lv_obj_set_style_bg_opa(btn_auto, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_auto, 0, 0);
    lv_obj_set_style_radius(btn_auto, 16, 0);
    lv_obj_set_style_pad_all(btn_auto, 0, 0);
    lv_obj_add_event_cb(btn_auto, auto_btn_cb, LV_EVENT_CLICKED, NULL);
    lbl_auto = text(btn_auto, 0, 0, "", &lv_font_montserrat_20, C_TEXT);
    lv_obj_align(lbl_auto, LV_ALIGN_LEFT_MID, 20, 0);

    /* Shown when hue.py is not writing: a wall panel that silently does
     * nothing is worse than one that says why. */
    lbl_hue_none = text(s, 0, 0, "", &lv_font_montserrat_24, C_MUTED);
    lv_obj_align(lbl_hue_none, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(lbl_hue_none, LV_OBJ_FLAG_HIDDEN);
}

/* Enabled and doing nothing is the confusing state - a summer week, or
 * the hours outside the window - so the button says which it is instead
 * of just "On". */
static void refresh_auto_btn(void)
{
    const char *room = auto_room[0] ? auto_room : "Hallway";
    uint32_t col = C_TEXT2;
    char buf[96];

    if (!hue_auto) {
        snprintf(buf, sizeof buf, LV_SYMBOL_POWER "  %s daylight  -  off", room);
    } else if (!strcmp(auto_why, "active")) {
        snprintf(buf, sizeof buf, LV_SYMBOL_POWER "  %s daylight  -  on", room);
        col = C_OK_TEXT;
    } else if (!strcmp(auto_why, "summer")) {
        snprintf(buf, sizeof buf, LV_SYMBOL_POWER "  %s daylight  -  idle, long day", room);
        col = C_WARN_TEXT;
    } else if (!strcmp(auto_why, "manual")) {
        snprintf(buf, sizeof buf, LV_SYMBOL_POWER "  %s daylight  -  you took over", room);
        col = C_WARN_TEXT;
    } else if (!strcmp(auto_why, "unreachable")) {
        snprintf(buf, sizeof buf, LV_SYMBOL_POWER "  %s daylight  -  no bulbs", room);
        col = C_BAD_TEXT;
    } else {
        snprintf(buf, sizeof buf, LV_SYMBOL_POWER "  %s daylight  -  outside hours", room);
    }
    lv_label_set_text(lbl_auto, buf);
    lv_obj_set_style_text_color(lbl_auto, lv_color_hex(col), 0);
}

static void refresh_lights(void)
{
    int live = hue_ok();
    refresh_auto_btn();

    if (!live || n_rooms == 0) {
        for (int i = 0; i < MAX_ROOMS; i++) lv_obj_add_flag(room_row[i].card, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_hue_none, hue_updated ? "Hue bridge not responding"
                                                    : "No lights configured");
        lv_obj_clear_flag(lbl_hue_none, LV_OBJ_FLAG_HIDDEN);
        laid_out_rooms = -1;
        return;
    }
    lv_obj_add_flag(lbl_hue_none, LV_OBJ_FLAG_HIDDEN);

    /* Cards divide the body evenly, so the screen is full whether the
     * bridge reports one room or four. Only redone when the count moves. */
    if (n_rooms != laid_out_rooms) {
        int h = (LIGHTS_H - (n_rooms - 1) * CARD_GAP) / n_rooms;
        for (int i = 0; i < MAX_ROOMS; i++) {
            if (i < n_rooms) {
                lv_obj_set_size(room_row[i].card, BODY_W, h);
                lv_obj_set_pos(room_row[i].card, 0, i * (h + CARD_GAP));
                lv_obj_clear_flag(room_row[i].card, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(room_row[i].card, LV_OBJ_FLAG_HIDDEN);
            }
        }
        laid_out_rooms = n_rooms;
    }

    for (int i = 0; i < n_rooms; i++) {
        int live = rooms[i].reachable;
        int lit  = live && rooms[i].on;
        /* One accent for every room, so two lit rooms never disagree about
         * what "on" looks like. Colour belongs on the warmth slider, which
         * is the only place it carries information. */
        uint32_t accent = lit ? C_WARN_TEXT : C_MUTED;
        int tunable = live && rooms[i].mirek > 0;

        lv_label_set_text(room_row[i].ico, room_icon(rooms[i].name));
        lv_obj_set_style_text_color(room_row[i].ico, lv_color_hex(accent), 0);
        lv_label_set_text(room_row[i].name, rooms[i].name);
        lv_obj_set_style_text_color(room_row[i].name,
                                    lv_color_hex(live ? C_TEXT : C_MUTED), 0);

        /* Unreachable is not off. Showing "0%" next to live controls
         * invites taps that silently do nothing. */
        if (!live) {
            lv_label_set_text(room_row[i].val, LV_SYMBOL_WARNING "  unreachable");
            lv_obj_set_style_text_color(room_row[i].val, lv_color_hex(C_MUTED), 0);
            lv_obj_align(room_row[i].val, LV_ALIGN_LEFT_MID, ROW_SLD_X, 0);
            lv_obj_add_flag(room_row[i].sld, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(room_row[i].val_ct, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(room_row[i].card, LV_OBJ_FLAG_CLICKABLE);
            continue;
        }

        lv_obj_clear_flag(room_row[i].sld, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(room_row[i].card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_text_color(room_row[i].val, lv_color_hex(C_TEXT2), 0);
        lv_obj_set_style_bg_color(room_row[i].sld, lv_color_hex(accent), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(room_row[i].sld, lv_color_hex(accent), LV_PART_KNOB);

        lv_obj_align(room_row[i].sld, LV_ALIGN_LEFT_MID, ROW_SLD_X, 0);
        lv_obj_align(room_row[i].val, LV_ALIGN_LEFT_MID, ROW_VAL_X, 0);

        /* Tunable rooms say what white they are on and that a hold changes
         * it; nothing else advertises the gesture. */
        if (tunable) {
            lv_label_set_text_fmt(room_row[i].val_ct, "%dK  -  hold to change",
                                  mirek_kelvin(rooms[i].mirek));
            lv_obj_clear_flag(room_row[i].val_ct, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(room_row[i].name, LV_ALIGN_LEFT_MID, 80, -16);
        } else {
            lv_obj_add_flag(room_row[i].val_ct, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(room_row[i].name, LV_ALIGN_LEFT_MID, 80, 0);
        }

        /* Do not yank the knob out from under a finger mid-drag. */
        if (!lv_obj_has_state(room_row[i].sld, LV_STATE_PRESSED)) {
            lv_slider_set_value(room_row[i].sld, rooms[i].bri, LV_ANIM_OFF);
            lv_label_set_text_fmt(room_row[i].val, "%d%%", rooms[i].bri);
        }
    }
}

static void build_settings(lv_obj_t *s)
{
    const int VX = 300;
    const int SLD_W = 380, SLD_H = 12;      /* the first pass was full-width and 26 thick */

    settings_group = box(s, 0, 0, BODY_W, SET_ROWS * SET_RH, C_SURF1, 20);
    lv_obj_t *r = settings_row(0, LV_SYMBOL_EYE_OPEN, "Backlight");
    ui_backlight_slider = lv_slider_create(r);
    lv_obj_set_size(ui_backlight_slider, SLD_W, SLD_H);
    lv_obj_align(ui_backlight_slider, LV_ALIGN_LEFT_MID, VX, 0);
    lv_obj_set_style_pad_all(ui_backlight_slider, 8, LV_PART_KNOB);
    lv_slider_set_range(ui_backlight_slider, 5, 100);
    lv_slider_set_value(ui_backlight_slider, backlight_pct, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_backlight_slider, backlight_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_backlight_slider, backlight_save_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_set_style_bg_color(ui_backlight_slider, lv_color_hex(C_ACC_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_backlight_slider, lv_color_hex(C_ACC_FILL), LV_PART_KNOB);
    lbl_bl_val = text(r, 0, 0, "", &lv_font_montserrat_24, C_TEXT);
    lv_obj_align(lbl_bl_val, LV_ALIGN_RIGHT_MID, -28, 0);

    r = settings_row(1, LV_SYMBOL_POWER, "Night dim");
    sld_dim = lv_slider_create(r);
    lv_obj_set_size(sld_dim, SLD_W, SLD_H);
    lv_obj_align(sld_dim, LV_ALIGN_LEFT_MID, VX, 0);
    lv_obj_set_style_pad_all(sld_dim, 8, LV_PART_KNOB);
    lv_slider_set_range(sld_dim, 5, 50);
    lv_slider_set_value(sld_dim, dim_pct, LV_ANIM_OFF);
    lv_obj_add_event_cb(sld_dim, dim_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sld_dim, dim_save_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_set_style_bg_color(sld_dim, lv_color_hex(C_ACC_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sld_dim, lv_color_hex(C_ACC_FILL), LV_PART_KNOB);
    lbl_dim_val = text(r, 0, 0, "", &lv_font_montserrat_24, C_TEXT);
    lv_obj_align(lbl_dim_val, LV_ALIGN_RIGHT_MID, -28, 0);

    r = settings_row(2, LV_SYMBOL_OK, "Medicine days");
    for (int i = 0; i < 7; i++) {
        /* Displayed Mon..Sun, stored bit0=Sun. */
        int wday = (i + 1) % 7;
        lv_obj_t *p = flat_btn(r);
        lv_obj_set_size(p, 92, 56);
        lv_obj_align(p, LV_ALIGN_LEFT_MID, VX + i * 102, 0);
        lv_obj_set_style_radius(p, 28, 0);
        lv_obj_add_event_cb(p, day_pill_cb, LV_EVENT_CLICKED, (void *)(intptr_t)wday);
        lv_obj_t *l = text(p, 0, 0, DAY3[wday], &lv_font_montserrat_20, C_TEXT);
        lv_obj_center(l);
        day_pill[i] = p;
    }

    r = settings_row(3, LV_SYMBOL_BELL, "Reminder");
    lbl_reminder = text(r, 0, 0, "", &lv_font_montserrat_24, C_TEXT2);
    lv_obj_align(lbl_reminder, LV_ALIGN_LEFT_MID, VX, 0);
    sw_reminder = lv_switch_create(r);
    lv_obj_set_size(sw_reminder, 84, 44);
    lv_obj_align(sw_reminder, LV_ALIGN_RIGHT_MID, -28, 0);
    lv_obj_set_style_bg_color(sw_reminder, lv_color_hex(C_OK_FILL), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_reminder, reminder_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lbl_footer = text(s, 0, BODY_H - 44, "", &lv_font_montserrat_20, C_MUTED);
    lv_obj_set_width(lbl_footer, BODY_W - 3 * 210 - 2 * 16 - 20);
    const int BW = 210, BG2 = 16, BY = BODY_H - 62;

    lv_obj_t *xt = flat_btn(s);
    lv_obj_set_size(xt, BW, 62);
    lv_obj_set_pos(xt, BODY_W - 3 * BW - 2 * BG2, BY);
    lv_obj_set_style_bg_color(xt, lv_color_hex(C_BORDER_ST), 0);
    lv_obj_set_style_bg_opa(xt, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(xt, 0, 0);
    lv_obj_set_style_radius(xt, 14, 0);
    lv_obj_add_event_cb(xt, exit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(text(xt, 0, 0, LV_SYMBOL_POWER "  Exit to shell", &lv_font_montserrat_20, C_TEXT2));

    lv_obj_t *rs = flat_btn(s);
    lv_obj_set_size(rs, BW, 62);
    lv_obj_set_pos(rs, BODY_W - 2 * BW - BG2, BY);
    lv_obj_set_style_bg_color(rs, lv_color_hex(C_BORDER_ST), 0);
    lv_obj_set_style_bg_opa(rs, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(rs, 0, 0);
    lv_obj_set_style_radius(rs, 14, 0);
    lv_obj_add_event_cb(rs, reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(text(rs, 0, 0, LV_SYMBOL_TRASH "  Reset data", &lv_font_montserrat_20, C_BAD_TEXT));

    lv_obj_t *ex = flat_btn(s);
    lv_obj_set_size(ex, BW, 62);
    lv_obj_set_pos(ex, BODY_W - BW, BY);
    lv_obj_set_style_bg_color(ex, lv_color_hex(C_BORDER_ST), 0);
    lv_obj_set_style_bg_opa(ex, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ex, 0, 0);
    lv_obj_set_style_radius(ex, 14, 0);
    lv_obj_add_event_cb(ex, export_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(text(ex, 0, 0, LV_SYMBOL_DOWNLOAD "  Export log", &lv_font_montserrat_20, C_TEXT));
}

/* --- refresh -------------------------------------------------------- */

static int dose_overdue(const struct tm *t, int is_med_day, const evt_t *given)
{
    if (!reminder_on || !is_med_day || given) return 0;
    return t->tm_hour * 60 + t->tm_min >= reminder_h * 60 + reminder_m;
}

static void refresh_home(const struct tm *t, long today)
{
    char buf[256];
    int is_med_day = (med_mask >> t->tm_wday) & 1;
    const evt_t *given = evt_on_day(today, 'm');

    long mon = sched_monday(today);
    int done = count_between(mon, mon + 6, 'm');
    int per_week = scheduled_per_week();
    int nx = sched_next_wday(med_mask, t->tm_wday);
    int overdue = dose_overdue(t, is_med_day, given);

    uint32_t accent = C_MUTED;
    if (given)           accent = C_OK_FILL;
    else if (overdue)    accent = C_BAD_FILL;
    else if (is_med_day) accent = C_OK_FILL;

    if (given) {
        lv_label_set_text(lbl_status, "Dose logged today");
        snprintf(buf, sizeof buf, "at %02d:%02d " LV_SYMBOL_BULLET " dose %d of %d this week",
                 given->h, given->mi, done, per_week);
    } else if (overdue) {
        lv_label_set_text(lbl_status, "Dose overdue");
        snprintf(buf, sizeof buf, "due at %02d:%02d " LV_SYMBOL_BULLET " not logged yet", reminder_h, reminder_m);
    } else if (is_med_day) {
        lv_label_set_text(lbl_status, "Medicine day");
        snprintf(buf, sizeof buf, "%s " LV_SYMBOL_BULLET " dose %d of %d this week " LV_SYMBOL_BULLET " next: %s",
                 DAY[t->tm_wday], done + 1, per_week, nx >= 0 ? DAY[nx] : "not set");
    } else {
        lv_label_set_text(lbl_status, "No medicine today");
        if (nx >= 0) snprintf(buf, sizeof buf, "%s " LV_SYMBOL_BULLET " next dose: %s", DAY[t->tm_wday], DAY[nx]);
        else         snprintf(buf, sizeof buf, "No days scheduled - set them in Settings");
    }
    lv_label_set_text(lbl_status_sub, buf);

    /* Overdue pulses the dot and reddens the title. The old version flashed
     * the whole card, which was hard to look at and buried the text. One
     * opacity swap per second in ui_tick, no animation - cheap on a
     * software-rotated panel. */
    lv_obj_set_style_bg_color(status_dot, lv_color_hex(accent), 0);
    lv_obj_set_style_bg_opa(status_dot,
                            (overdue && !blink_on) ? LV_OPA_30 : LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(lbl_status,
                                lv_color_hex(overdue ? C_BAD_TEXT : C_TEXT), 0);
    transit_load();
    /* Filtered here, not in transit.py: the file is rewritten every five
     * minutes but read every minute, so a departure that was catchable at
     * fetch time is not by the time it is drawn. */
    int shown = 0;
    if (transit_show(t)) {
        char row[128];
        for (int i = 0; i < n_trips && shown < SHOW_TRIPS; i++) {
            int away = mins_until(trips[i].dep, t);
            if (away < transit_lead) continue;      /* cannot reach it in time */
            /* Direct is the normal case here and saying so every line was
             * noise; only a change is worth calling out. */
            char via[40] = "";
            if (trips[i].changes == 1)
                snprintf(via, sizeof via, " " LV_SYMBOL_BULLET " 1 change");
            else if (trips[i].changes > 1)
                snprintf(via, sizeof via, " " LV_SYMBOL_BULLET " %d changes", trips[i].changes);
            snprintf(row, sizeof row, "%.5s " LV_SYMBOL_RIGHT " %.5s   in %d min%.39s",
                     trips[i].dep, trips[i].arr, away, via);
            lv_label_set_text(lbl_trip[shown++], row);
        }
        for (int i = shown; i < SHOW_TRIPS; i++) lv_label_set_text(lbl_trip[i], "");
    }
    if (shown) {
        lv_obj_clear_flag(card_transit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card_week, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(card_transit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(card_week, LV_OBJ_FLAG_HIDDEN);
    }

    overdue_now = overdue;
    lv_label_set_text_fmt(lbl_week_no, "Week %d", sched_iso_week(today));
    lv_label_set_text_fmt(lbl_home_clock, "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text_fmt(lbl_home_date, "%s %d %s",
                          DAY[t->tm_wday], t->tm_mday, MONTH[t->tm_mon]);

    weather_load();
    if (wx_code < 0) {
        lv_label_set_text(lbl_wx_temp, "--");
        lv_label_set_text(lbl_wx_desc, "no data");
        lv_obj_add_flag(wx_img, LV_OBJ_FLAG_HIDDEN);
    } else {
        /* Older than three hours means the fetch has been failing, so say so
         * by greying it rather than presenting stale numbers as current. */
        int stale = wx_updated && (long)time(NULL) - wx_updated > 3 * 3600;
        lv_obj_clear_flag(wx_img, LV_OBJ_FLAG_HIDDEN);
        lv_img_set_src(wx_img, wx_icon_file(wx_code));
        lv_obj_set_style_img_opa(wx_img, stale ? LV_OPA_40 : LV_OPA_COVER, 0);
        lv_label_set_text_fmt(lbl_wx_temp, "%d\xC2\xB0", wx_temp);
        char head[24];
        if (wx_rain_from < 0)
            snprintf(head, sizeof head, "%.14s", wx_words(wx_code));
        else if (wx_rain_from <= t->tm_hour)
            /* The hour we are already in reads as past tense if you print
             * it: at 11:19, "Rain 11:00" looks like it has been and gone. */
            snprintf(head, sizeof head, "Rain now");
        else
            snprintf(head, sizeof head, "Rain %02d:00", wx_rain_from);
        lv_label_set_text_fmt(lbl_wx_desc, "%s  %d\xC2\xB0/%d\xC2\xB0",
                              head, wx_hi, wx_lo);
        lv_obj_set_style_text_color(lbl_wx_temp,
                                    lv_color_hex(stale ? C_MUTED : C_TEXT), 0);
        lv_obj_set_style_text_color(lbl_wx_desc,
                                    lv_color_hex(stale ? C_MUTED : C_TEXT2), 0);
    }

    /* Stays tappable once logged: tapping again clears today's dose, which
     * is the only way to take back a mis-tap. */
    if (given) {
        lv_obj_set_style_bg_color(btn_dose, lv_color_hex(C_BORDER_ST), 0);
        lv_obj_add_flag(icon_dose, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_btn_dose, LV_SYMBOL_REFRESH "  Undo today's dose");
        lv_obj_set_style_text_color(lbl_btn_dose, lv_color_hex(C_TEXT), 0);
    } else {
        lv_obj_set_style_bg_color(btn_dose, lv_color_hex(C_ACC_FILL), 0);
        lv_obj_clear_flag(icon_dose, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_btn_dose, "Log dose given");
        lv_obj_set_style_text_color(lbl_btn_dose, lv_color_hex(C_ACC_ON), 0);
    }

    for (int i = 0; i < 7; i++) {
        long dn = mon + i;
        int y2, m2, d2;
        sched_civil(dn, &y2, &m2, &d2);
        lv_label_set_text(week_wd[i], DAY3[sched_wday(dn)]);
        /* Every day in the week reads the same weight; only today is
         * emphasised. Dimming the future days made an upcoming dose - the
         * thing most worth noticing - the faintest thing on the card. */
        daycell_set(&week_cell[i], d2, dn == today, C_TEXT,
                    evt_on_day(dn, 'm') != NULL, evt_on_day(dn, 'v') != NULL);
    }
}

static void refresh_calendar(long today)
{
    char buf[1024];
    lv_label_set_text_fmt(lbl_cal_month, "%s %d", MONTH[cal_m - 1], cal_y);

    long first = sched_day_num(cal_y, cal_m, 1);
    long grid0 = sched_monday(first);                    /* grid starts on a Monday */
    int  ndays = sched_days_in_month(cal_y, cal_m);
    /* Most months fit in five rows; showing a sixth row of nothing but next
     * month's greyed-out days wasted 74px and made the screen feel full. */
    int  rows_used = (int)((first - grid0) + ndays + 6) / 7;

    for (int i = 0; i < 42; i++) {
        if (i / 7 >= rows_used) { lv_obj_add_flag(cal_cell[i].cell, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(cal_cell[i].cell, LV_OBJ_FLAG_HIDDEN);
        long dn = grid0 + i;
        long off = dn - first;
        int in_month = off >= 0 && off < ndays;
        int cy, cm, dom;
        sched_civil(dn, &cy, &cm, &dom);
        int has_dose  = evt_on_day(dn, 'm') != NULL;
        int has_event = evt_on_day(dn, 'v') != NULL;
        daycell_set(&cal_cell[i], dom, in_month && dn == today,
                    in_month ? C_TEXT : C_MUTED,
                    in_month && has_dose, in_month && has_event);
    }

    int given, due;
    month_progress(cal_y, cal_m, &given, &due);
    lv_label_set_text_fmt(lbl_stat_month, "%d / %d", given, due);
    lv_label_set_text_fmt(lbl_stat_streak, "%d wk", streak_weeks());
    lv_label_set_text_fmt(lbl_stat_events, "%d",
                          count_between(sched_day_num(cal_y, cal_m, 1),
                                        sched_day_num(cal_y, cal_m, sched_days_in_month(cal_y, cal_m)), 'v'));

    size_t off = 0;
    buf[0] = 0;
    for (int i = n_evts - 1, shown = 0; i >= 0 && shown < 9; i--, shown++) {
        long dn = evt_day(&evts[i]);
        int n = snprintf(buf + off, sizeof buf - off, "%s  %-12s %s %d %s %02d:%02d\n",
                         evt_icon(evts[i].t), evt_label(evts[i].t),
                         DAY3[sched_wday(dn)], evts[i].d, MON3[evts[i].mo - 1],
                         evts[i].h, evts[i].mi);
        if (n < 0 || (size_t)n >= sizeof buf - off) break;
        off += (size_t)n;
    }
    lv_label_set_text(lbl_recent, buf[0] ? buf : "Nothing logged yet.");
}

static void refresh_settings(const struct tm *t)
{
    if (ui_backlight_slider)
        backlight_label((int)lv_slider_get_value(ui_backlight_slider));
    dim_label(dim_pct);

    for (int i = 0; i < 7; i++) {
        int wday = (i + 1) % 7;
        int on = (med_mask >> wday) & 1;
        lv_obj_set_style_bg_color(day_pill[i], lv_color_hex(on ? C_ACC_FILL : C_BORDER_ST), 0);
        lv_obj_set_style_bg_opa(day_pill[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(day_pill[i], 0, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(day_pill[i], 0),
                                    lv_color_hex(on ? C_ACC_ON : C_TEXT2), 0);
    }

    lv_label_set_text_fmt(lbl_reminder, "%02d:%02d " LV_SYMBOL_BULLET " flash screen until logged",
                          reminder_h, reminder_m);
    if (reminder_on) lv_obj_add_state(sw_reminder, LV_STATE_CHECKED);
    else             lv_obj_clear_state(sw_reminder, LV_STATE_CHECKED);

    lv_label_set_text_fmt(lbl_footer, "%d %s %d " LV_SYMBOL_BULLET " %02d:%02d " LV_SYMBOL_BULLET " %d event%s logged",
                          t->tm_mday, MON3[t->tm_mon], t->tm_year + 1900,
                          t->tm_hour, t->tm_min, n_evts, n_evts == 1 ? "" : "s");
}

static void refresh(void)
{
    struct tm t = now_tm();
    long today = sched_day_num(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    refresh_home(&t, today);
    refresh_calendar(today);
    hue_load();
    refresh_lights();
    refresh_settings(&t);
}

/* --- entry points --------------------------------------------------- */

void ui_init(void)
{
    cfg_load();
    store_load();
    /* Fixed seed on purpose. The shuffle decides which photo belongs to
     * which day, so a random seed would hand you a different picture every
     * time the Pi restarts - and it restarts on its own. */
    srand(20260913);

    struct tm t = now_tm();
    cal_y = t.tm_year + 1900;
    cal_m = t.tm_mon + 1;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    /* Nothing here scrolls, and a latched scroll swallows presses. */
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(lv_layer_top(), LV_OBJ_FLAG_SCROLLABLE);

    build_rail(scr);

    for (int i = 0; i < 4; i++) {
        screens[i] = box(scr, BODY_X, PAD, BODY_W, BODY_H, C_BG, 0);
        lv_obj_set_style_bg_opa(screens[i], LV_OPA_0, 0);
    }
    build_home(screens[0]);
    build_transit(screens[0]);
    build_event_popover();
    build_calendar(screens[1]);
    build_lights(screens[2]);
    build_settings(screens[3]);

    /* Plain white text at the bottom of the screen. It was a blue capsule
     * at font 24 with 16px padding, which for a three-second confirmation
     * was shouting. Centred on the screen, not on the content area. */
    lbl_toast = text(scr, 0, 0, "", &lv_font_montserrat_20, C_TEXT);
    lv_obj_align(lbl_toast, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_flag(lbl_toast, LV_OBJ_FLAG_HIDDEN);

    /* CAT_SCREEN=0|1|2|3 picks the screen to open on. Same spirit as
     * TOUCH_DEBUG in main.c: a way to look at a screen without a finger. */
    const char *sc = getenv("CAT_SCREEN");
    show_screen(sc ? atoi(sc) % 4 : 0);
    if (getenv("CAT_POPUP")) lv_obj_clear_flag(pop_event, LV_OBJ_FLAG_HIDDEN);
    if (getenv("CAT_TOAST")) { toast(getenv("CAT_TOAST")); toast_until = 0; }  /* 0 = stays up */

    /* main.c fades up to this once the UI is built; applying it here would
     * light the panel before there was anything on it. */
    printf("backlight target %d%%\n", backlight_pct);
}

int ui_backlight_pct(void) { return backlight_pct; }

void ui_tick(void)
{
    static time_t last_sec;
    static int last_yday = -1, last_min = -1;

    time_t now = time(NULL);
    if (now == last_sec) return;             /* the loop runs at ~200Hz; this needs 1Hz */
    /* The Pi has no RTC, so at boot the clock is whatever was saved at
     * shutdown until timesyncd corrects it - a jump of minutes or hours.
     * Without this the display waited for the minute to roll before
     * catching up, which is the delay you see on a cold start. */
    int jumped = last_sec != 0 && (now < last_sec || now - last_sec > 2);
    last_sec = now;
    blink_on = !blink_on;

    /* Link state changes on its own schedule, so it needs its own cadence
     * rather than riding the once-a-minute redraw. */
    static int status_cd;
    if (--status_cd <= 0) { status_cd = 5; refresh_status_icons(); }

    struct tm t = *localtime(&now);

    if (lbl_toast && toast_until && now >= toast_until) {
        lv_obj_add_flag(lbl_toast, LV_OBJ_FLAG_HIDDEN);
        toast_until = 0;
    }

    /* Dim by the clock, not by idleness. An idle timer put the panel to
     * sleep in the middle of the day, which is exactly when an unlogged
     * dose most needs to catch someone's eye. */
    {
        int night = sched_in_window(t.tm_hour * 60 + t.tm_min,
                                    quiet_from * 60, quiet_to * 60);
        if (night != dimmed) {
            int awake = (int)lv_slider_get_value(ui_backlight_slider);
            /* Never dim *up*: if the slider sits below dim_pct, keep it. */
            int low = dim_pct < awake ? dim_pct : awake;
            dimmed = night;
            ui_backlight_apply(night ? low : awake);
        }
    }

    /* Without seconds nothing on screen changes faster than once a minute,
     * so the only per-second work left is the overdue pulse. The clock is
     * set in refresh_home along with everything else. */
    if (status_dot && overdue_now)
        lv_obj_set_style_bg_opa(status_dot, blink_on ? LV_OPA_COVER : LV_OPA_30, 0);

    if (jumped || t.tm_min != last_min) {
        last_min = t.tm_min;
        if (cur_screen == 0) refresh_home(&t, sched_day_num(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday));
        if (cur_screen == 3) refresh_settings(&t);
    }

    /* Lights change from wall switches and phones too, so while that screen
     * is up it follows the daemon's file every second rather than waiting
     * for the minute tick the rest of the UI runs on. */
    if (cur_screen == 2) { hue_load(); refresh_lights(); }

    /* Every screen but Today is one you opened to do a single thing. Go
     * back once the finger stops, so whoever next walks past the panel
     * finds the dose status on it rather than whatever was left open.
     * LVGL already tracks this: any press or release resets the clock, so
     * a long slider drag counts as activity throughout. */
    if (cur_screen != 0 && lv_disp_get_inactive_time(NULL) > HOME_SECS * 1000)
        show_screen(0);

    if (rail_shown_at && now - rail_shown_at >= RAIL_SECS) rail_hide();

    if (t.tm_yday != last_yday) {            /* midnight: new day, new photo */
        last_yday = t.tm_yday;
        photo_for_today();
        refresh();
    }
}
