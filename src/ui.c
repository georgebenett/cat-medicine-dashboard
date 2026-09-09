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
#define RAIL_W   120
#define PAD      40
#define BODY_X   (RAIL_W + PAD)
#define BODY_W   (SCR_W - RAIL_W - 2 * PAD)
#define BODY_H   (SCR_H - 2 * PAD)
#define PHOTO_W  400
#define PHOTO_H  540      /* 3:4-ish, to match the photos rather than crop them */
#define GAP      38
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
static int  dim_min = 5;                                 /* 0 = never dim */
static int  reminder_on = 1, reminder_h = 9, reminder_m = 0;
static int  backlight_pct = 70;                          /* remembered across restarts */
static int  photo_secs = 60;                             /* portrait shuffle interval */
static char cfg_lat[64] = "55.6078";   /* sized to the cfg value buffer */
static char cfg_lon[64] = "12.9982";   /* weather.py reads both */

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

static void store_clear_all(void)
{
    n_evts = 0;
    if (remove(log_path()) != 0 && access(log_path(), F_OK) == 0)
        perror("cat log remove");
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
        else if (!strcmp(k, "dim"))        { int m = atoi(v); if (m >= 0 && m <= 60) dim_min = m; }
        else if (!strcmp(k, "backlight"))  { int b = atoi(v); if (b >= 5 && b <= 100) backlight_pct = b; }
        else if (!strcmp(k, "photo_secs")) { int s = atoi(v); if (s >= 5 && s <= 3600) photo_secs = s; }
        else if (!strcmp(k, "lat"))        snprintf(cfg_lat, sizeof cfg_lat, "%s", v);
        else if (!strcmp(k, "lon"))        snprintf(cfg_lon, sizeof cfg_lon, "%s", v);
        else if (!strcmp(k, "reminder"))   reminder_on = atoi(v) ? 1 : 0;
        else if (!strcmp(k, "reminder_h")) { int h = atoi(v); if (h >= 0 && h < 24) reminder_h = h; }
        else if (!strcmp(k, "reminder_m")) { int m = atoi(v); if (m >= 0 && m < 60) reminder_m = m; }
    }
    fclose(f);
}

static void cfg_save(void)
{
    FILE *f = fopen(cfg_path(), "w");
    if (!f) { perror("cat cfg save"); return; }
    /* lat/lon are written back even though nothing in the UI edits them:
     * cfg_save rewrites the whole file, so a key it does not know about
     * would be silently dropped the first time a setting changed. */
    fprintf(f, "days=%d\nname=%s\ndim=%d\nbacklight=%d\nphoto_secs=%d\n"
               "reminder=%d\nreminder_h=%d\nreminder_m=%d\nlat=%s\nlon=%s\n",
            med_mask, cat_name, dim_min, backlight_pct, photo_secs,
            reminder_on, reminder_h, reminder_m, cfg_lat, cfg_lon);
    fclose(f);
}

/* --- weather ---------------------------------------------------------
 * weather.py writes weather.txt; this only ever reads it. Stale data is
 * shown greyed rather than hidden - "17 degrees an hour ago" is more use
 * than a blank panel. */

static int  wx_temp, wx_code = -1, wx_hi, wx_lo;
static long wx_updated;

static void weather_load(void)
{
    FILE *f = fopen(env_or("CAT_WEATHER", "weather.txt"), "r");
    if (!f) return;
    char line[64], k[24], v[32];
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%23[^=]=%31[^\n]", k, v) != 2) continue;
        if      (!strcmp(k, "temp"))    wx_temp = atoi(v);
        else if (!strcmp(k, "code"))    wx_code = atoi(v);
        else if (!strcmp(k, "hi"))      wx_hi = atoi(v);
        else if (!strcmp(k, "lo"))      wx_lo = atoi(v);
        else if (!strcmp(k, "updated")) wx_updated = atol(v);
    }
    fclose(f);
}

/* WMO weather codes, collapsed to the five icons that exist. */
static const char *wx_icon_file(int code)
{
    if (code == 0)                 return "A:wx_clear.png";
    if (code <= 2)                 return "A:wx_partly.png";
    if (code == 3 || code == 45 || code == 48) return "A:wx_cloud.png";
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return "A:wx_snow.png";
    return "A:wx_rain.png";        /* drizzle, rain, showers, thunder */
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

static const evt_t *last_of(char t)
{
    for (int i = n_evts - 1; i >= 0; i--) if (evts[i].t == t) return &evts[i];
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

static lv_obj_t *screens[3];
static int  cur_screen;
static int  cal_y, cal_m;                                /* month the calendar shows */
static lv_obj_t *rail_items[3];

/* A day in the week strip or the month grid: the number, and up to two
 * dots under it. Apple Calendar's language - a filled circle marks today
 * and small dots mark what happened, instead of flooding the cell with
 * colour, which drowned the vomiting marks in a wall of green. */
typedef struct { lv_obj_t *cell, *num, *dots, *dot[2]; } daycell_t;

static lv_obj_t *lbl_name, *lbl_last_dose, *card_status, *lbl_status, *lbl_status_sub;
static lv_obj_t *status_dot, *btn_dose, *lbl_btn_dose, *icon_dose, *week_wd[7];
static lv_obj_t *lbl_week_no, *lbl_home_clock, *pop_event;
static lv_obj_t *lbl_home_date, *wx_img, *lbl_wx_temp, *lbl_wx_desc;
static int overdue_now;
static daycell_t week_cell[7], cal_cell[42];
static lv_obj_t *lbl_cal_month;
static lv_obj_t *lbl_stat_month, *lbl_stat_streak, *lbl_stat_events, *lbl_recent;
static lv_obj_t *lbl_bl_val, *sld_dim, *lbl_dim_val, *day_pill[7], *sw_reminder;
static lv_obj_t *lbl_reminder, *lbl_footer, *lbl_toast;
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

static void refresh(void);

/* --- events --------------------------------------------------------- */

static void toast(const char *msg)
{
    lv_label_set_text(lbl_toast, msg);
    lv_obj_align(lbl_toast, LV_ALIGN_BOTTOM_MID, 0, -16);   /* width changed */
    lv_obj_clear_flag(lbl_toast, LV_OBJ_FLAG_HIDDEN);
    toast_until = time(NULL) + 3;
}

static void show_screen(int i)
{
    cur_screen = i;
    if (pop_event) lv_obj_add_flag(pop_event, LV_OBJ_FLAG_HIDDEN);
    for (int k = 0; k < 3; k++) {
        if (k == i) lv_obj_clear_flag(screens[k], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(screens[k], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(rail_items[k], k == i ? LV_OPA_COVER : LV_OPA_0, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(rail_items[k], 0),
                                    lv_color_hex(k == i ? C_ACC_TEXT : C_TEXT2), 0);
    }
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

static void dim_label(int m)
{
    if (m) lv_label_set_text_fmt(lbl_dim_val, "%d min", m);
    else   lv_label_set_text(lbl_dim_val, "never");
}

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
    dim_min = (int)lv_slider_get_value(lv_event_get_target(e));
    dim_label(dim_min);
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

static void build_rail(lv_obj_t *parent)
{
    static const char *icons[3] = { LV_SYMBOL_HOME, LV_SYMBOL_LIST, LV_SYMBOL_SETTINGS };
    lv_obj_t *rail = box(parent, 0, 0, RAIL_W, SCR_H, C_SURF1, 0);
    lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(rail, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(rail, 1, 0);

    for (int i = 0; i < 3; i++) {
        lv_obj_t *it = box(rail, 18, 22 + i * 96, 83, 83, C_ACC_BG, 18);
        lv_obj_add_flag(it, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(it, rail_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(it);
        lv_label_set_text(l, icons[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_40, 0);
        lv_obj_center(l);
        rail_items[i] = it;
    }
}

/* Photos are files, not compiled-in C arrays: drop PNGs in photos/ (or a
 * single cat.png) and restart, no rebuild. They shuffle like a digital
 * portrait - see photo_secs in cat_cfg.txt. */
#define MAX_PHOTOS 64
#define PHOTO_PAD  24
#define PHOTO_RADIUS 12      /* card radius 20 less the 12px inset, so it stays concentric */

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

/* Fisher-Yates. Reshuffled only after the last one has been shown, so the
 * whole set goes past before anything repeats. */
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
        /* Fit inside both axes of the card. The old long-side-only version
         * was fine while the card was square; it is not any more. */
        int zx = 256 * (PHOTO_W - PHOTO_PAD) / hdr.w;
        int zy = 256 * (PHOTO_H - PHOTO_PAD) / hdr.h;
        int zoom = zx < zy ? zx : zy;
        if (zoom > 256) zoom = 256;      /* never upscale, it would just blur */
        if (zoom < 16)  zoom = 16;
        /* At exactly 256 lv_img takes its plain blit path and skips the
         * transform entirely, so the photos are pre-scaled to land here. */
        lv_img_set_zoom(photo_img, (uint16_t)zoom);
        if (zoom != 256) printf("photo %d: zoom %d, transforming every draw\n", i, zoom);

        /* Round the picture's own corners. clip_corner masks an object's
         * CHILDREN, so the mask has to live on a wrapper sized to the drawn
         * image - the card is bigger than the photo, so its corners are
         * nowhere near them. lv_img_get_transformed_size is not public in
         * 8.3, but zoom and the header give the same answer. */
        lv_obj_set_size(photo_wrap, hdr.w * zoom / 256, hdr.h * zoom / 256);
        lv_obj_center(photo_wrap);
    }
    lv_obj_center(photo_img);
}

static void build_photo(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *c = box(parent, x, y, w, h, C_SURF1, 20);
    photos_scan();

    if (n_photos > 0) {
        photos_shuffle();
        photo_wrap = box(c, 0, 0, 10, 10, C_SURF1, PHOTO_RADIUS);
        lv_obj_set_style_bg_opa(photo_wrap, LV_OPA_0, 0);
        lv_obj_set_style_clip_corner(photo_wrap, true, 0);
        photo_img = lv_img_create(photo_wrap);
        lv_img_set_antialias(photo_img, true);
        photo_show(0);
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
    lbl_name = text(s, 0, PHOTO_H + 18, "", &lv_font_montserrat_32, C_TEXT);
    lv_obj_set_width(lbl_name, PHOTO_W);
    lv_obj_set_style_text_align(lbl_name, LV_TEXT_ALIGN_CENTER, 0);
    lbl_last_dose = text(s, 0, PHOTO_H + 62, "", &lv_font_montserrat_20, C_TEXT2);
    lv_obj_set_width(lbl_last_dose, PHOTO_W);
    lv_obj_set_style_text_align(lbl_last_dose, LV_TEXT_ALIGN_CENTER, 0);

    card_status = box(s, RIGHT_X, 0, RIGHT_W, 160, C_SURF1, 20);
    status_dot     = dot(card_status, 14, C_OK_FILL);
    lv_obj_set_pos(status_dot, 32, 47);
    lbl_status     = text(card_status, 62, 30, "", &lv_font_montserrat_32, C_TEXT);
    lbl_status_sub = text(card_status, 62, 88, "", &lv_font_montserrat_24, C_TEXT2);

    int bw = BTN_W;
    btn_dose = lv_btn_create(s);
    lv_obj_set_pos(btn_dose, RIGHT_X, 182);
    lv_obj_set_size(btn_dose, bw, 120);
    lv_obj_set_style_bg_color(btn_dose, lv_color_hex(C_ACC_FILL), 0);
    lv_obj_set_style_radius(btn_dose, 16, 0);
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
    if (access("pill.png", R_OK) == 0) lv_img_set_src(icon_dose, "A:pill.png");
    else printf("pill.png missing - button shows text only\n");

    lbl_btn_dose = lv_label_create(row);
    /* 24, not 28: the button is BTN_W (310px) wide and the toggled label
     * "<refresh>  Undo today's dose" overflows at 28. Same size in both
     * states so the button does not visibly resize its text on tap. */
    lv_obj_set_style_text_font(lbl_btn_dose, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_btn_dose, lv_color_hex(C_ACC_ON), 0);
    lv_label_set_text(lbl_btn_dose, "Log dose given");

    lv_obj_t *b2 = lv_btn_create(s);
    lv_obj_set_pos(b2, RIGHT_X + bw + 22, 182);
    lv_obj_set_size(b2, bw, 120);
    lv_obj_set_style_bg_color(b2, lv_color_hex(C_BORDER_ST), 0);
    lv_obj_set_style_bg_opa(b2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b2, 0, 0);
    lv_obj_set_style_radius(b2, 16, 0);
    lv_obj_add_event_cb(b2, event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l2 = text(b2, 0, 0, LV_SYMBOL_WARNING "  Log event", &lv_font_montserrat_28, C_TEXT);
    lv_obj_center(l2);

    /* Clock and weather share one card in the gap between the buttons and
     * the week strip: time and date left, conditions right. */
    lv_obj_t *ck = box(s, RIGHT_X, 314, RIGHT_W, 124, C_SURF1, 20);
    lbl_home_clock = text(ck, 28, 18, "", &lv_font_montserrat_40, C_TEXT);
    lbl_home_date  = text(ck, 28, 76, "", &lv_font_montserrat_20, C_TEXT2);

    wx_img = lv_img_create(ck);
    lv_obj_align(wx_img, LV_ALIGN_RIGHT_MID, -26, 0);

    lbl_wx_temp = text(ck, 0, 0, "", &lv_font_montserrat_32, C_TEXT);
    lv_obj_align(lbl_wx_temp, LV_ALIGN_RIGHT_MID, -100, -16);
    lbl_wx_desc = text(ck, 0, 0, "", &lv_font_montserrat_20, C_TEXT2);
    lv_obj_align(lbl_wx_desc, LV_ALIGN_RIGHT_MID, -100, 20);

    lv_obj_t *wk = box(s, RIGHT_X, BODY_H - 190, RIGHT_W, 190, C_SURF1, 20);
    text(wk, 30, 22, "This week", &lv_font_montserrat_20, C_TEXT2);
    lbl_week_no = text(wk, 0, 0, "", &lv_font_montserrat_20, C_MUTED);
    lv_obj_align(lbl_week_no, LV_ALIGN_TOP_RIGHT, -30, 22);
    int step = (RIGHT_W - 60) / 7;
    for (int i = 0; i < 7; i++) {
        int x = 30 + i * step;
        week_wd[i] = text(wk, x, 66, "", &lv_font_montserrat_20, C_MUTED);
        lv_obj_set_width(week_wd[i], step - 8);
        lv_obj_set_style_text_align(week_wd[i], LV_TEXT_ALIGN_CENTER, 0);
        daycell_create(&week_cell[i], wk, x + (step - 8 - 62) / 2, 96, 62,
                       &lv_font_montserrat_24);
    }
}

#define POP_W 310
#define POP_RH 62

static void pop_row(int idx, int rows, const char *icon, const char *label,
                    uint32_t color, char kind)
{
    lv_obj_t *b = lv_btn_create(pop_event);
    lv_obj_set_size(b, POP_W - 16, POP_RH);
    lv_obj_set_pos(b, 0, idx * POP_RH);
    lv_obj_set_style_bg_opa(b, LV_OPA_0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
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
    const int y = PAD + 182 + 120 + 10;
    pop_event = box(lv_layer_top(), x, y, POP_W, 2 * POP_RH + 16, C_BORDER_ST, 18);
    lv_obj_set_style_pad_all(pop_event, 8, 0);
    lv_obj_set_style_border_width(pop_event, 1, 0);
    lv_obj_set_style_border_color(pop_event, lv_color_hex(C_BORDER), 0);
    lv_obj_add_flag(pop_event, LV_OBJ_FLAG_HIDDEN);

    pop_row(0, 2, LV_SYMBOL_WARNING, "Vomiting", C_BAD_TEXT, 'v');
    pop_row(1, 2, LV_SYMBOL_PLUS,    "Food",     C_TEXT,     'f');
}

static void build_calendar(lv_obj_t *s)
{
    /* Sized so six rows plus the legend land inside BODY_H with room to
     * spare: 4 + 56 header, 104 grid start, 6 rows of 74, legend at 556. */
    const int CAL_W = 540, CELL = 64, STEP = 74, GRID_Y = 104;

    lv_obj_t *arrow[2];
    for (int i = 0; i < 2; i++) {
        arrow[i] = lv_btn_create(s);
        lv_obj_set_pos(arrow[i], i ? CAL_W - 56 : 0, 4);
        lv_obj_set_size(arrow[i], 56, 56);
        /* The default button style draws a shadow outside the object, and
         * at y=0 the parent clipped it - which is what cut the tops off. */
        lv_obj_set_style_shadow_width(arrow[i], 0, 0);
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

    r = settings_row(1, LV_SYMBOL_POWER, "Dim after");
    sld_dim = lv_slider_create(r);
    lv_obj_set_size(sld_dim, SLD_W, SLD_H);
    lv_obj_align(sld_dim, LV_ALIGN_LEFT_MID, VX, 0);
    lv_obj_set_style_pad_all(sld_dim, 8, LV_PART_KNOB);
    lv_slider_set_range(sld_dim, 0, 30);
    lv_slider_set_value(sld_dim, dim_min, LV_ANIM_OFF);
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
        lv_obj_t *p = lv_btn_create(r);
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

    lv_obj_t *xt = lv_btn_create(s);
    lv_obj_set_size(xt, BW, 62);
    lv_obj_set_pos(xt, BODY_W - 3 * BW - 2 * BG2, BY);
    lv_obj_set_style_bg_color(xt, lv_color_hex(C_BORDER_ST), 0);
    lv_obj_set_style_bg_opa(xt, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(xt, 0, 0);
    lv_obj_set_style_radius(xt, 14, 0);
    lv_obj_add_event_cb(xt, exit_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(text(xt, 0, 0, LV_SYMBOL_POWER "  Exit to shell", &lv_font_montserrat_20, C_TEXT2));

    lv_obj_t *rs = lv_btn_create(s);
    lv_obj_set_size(rs, BW, 62);
    lv_obj_set_pos(rs, BODY_W - 2 * BW - BG2, BY);
    lv_obj_set_style_bg_color(rs, lv_color_hex(C_BORDER_ST), 0);
    lv_obj_set_style_bg_opa(rs, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(rs, 0, 0);
    lv_obj_set_style_radius(rs, 14, 0);
    lv_obj_add_event_cb(rs, reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(text(rs, 0, 0, LV_SYMBOL_TRASH "  Reset data", &lv_font_montserrat_20, C_BAD_TEXT));

    lv_obj_t *ex = lv_btn_create(s);
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
    const evt_t *last  = last_of('m');

    lv_label_set_text(lbl_name, cat_name);
    if (last) {
        long dn = evt_day(last);
        snprintf(buf, sizeof buf, "Last dose: %s %d %s, %02d:%02d",
                 DAY3[sched_wday(dn)], last->d, MON3[last->mo - 1], last->h, last->mi);
    } else {
        snprintf(buf, sizeof buf, "No dose logged yet");
    }
    lv_label_set_text(lbl_last_dose, buf);

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
        lv_label_set_text_fmt(lbl_wx_desc, "%s  %d\xC2\xB0/%d\xC2\xB0",
                              wx_words(wx_code), wx_hi, wx_lo);
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
    dim_label(dim_min);

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
    refresh_settings(&t);
}

/* --- entry points --------------------------------------------------- */

void ui_init(void)
{
    cfg_load();
    store_load();
    srand((unsigned)time(NULL));

    struct tm t = now_tm();
    cal_y = t.tm_year + 1900;
    cal_m = t.tm_mon + 1;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_rail(scr);

    for (int i = 0; i < 3; i++) {
        screens[i] = box(scr, BODY_X, PAD, BODY_W, BODY_H, C_BG, 0);
        lv_obj_set_style_bg_opa(screens[i], LV_OPA_0, 0);
    }
    build_home(screens[0]);
    build_event_popover();
    build_calendar(screens[1]);
    build_settings(screens[2]);

    /* Plain white text at the bottom of the screen. It was a blue capsule
     * at font 24 with 16px padding, which for a three-second confirmation
     * was shouting. Centred on the screen, not on the content area. */
    lbl_toast = text(scr, 0, 0, "", &lv_font_montserrat_20, C_TEXT);
    lv_obj_align(lbl_toast, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_flag(lbl_toast, LV_OBJ_FLAG_HIDDEN);

    /* CAT_SCREEN=0|1|2 picks the screen to open on. Same spirit as
     * TOUCH_DEBUG in main.c: a way to look at a screen without a finger. */
    const char *sc = getenv("CAT_SCREEN");
    show_screen(sc ? atoi(sc) % 3 : 0);
    if (getenv("CAT_POPUP")) lv_obj_clear_flag(pop_event, LV_OBJ_FLAG_HIDDEN);
    if (getenv("CAT_TOAST")) { toast(getenv("CAT_TOAST")); toast_until = 0; }  /* 0 = stays up */

    /* Push the remembered brightness to the panel. Without this a restart
     * while dimmed would leave it at raw 1 with no sign of why. */
    ui_backlight_apply(backlight_pct);
    printf("backlight restored to %d%%\n", backlight_pct);
}

void ui_tick(void)
{
    static time_t last_sec, last_photo;
    static int last_yday = -1, last_min = -1, dimmed;

    time_t now = time(NULL);
    if (now == last_sec) return;             /* the loop runs at ~200Hz; this needs 1Hz */
    last_sec = now;
    blink_on = !blink_on;

    struct tm t = *localtime(&now);

    if (lbl_toast && toast_until && now >= toast_until) {
        lv_obj_add_flag(lbl_toast, LV_OBJ_FLAG_HIDDEN);
        toast_until = 0;
    }

    /* Idle dimming. lv_disp_get_inactive_time() already tracks the last
     * input event, so there is nothing to wire up here. */
    if (dim_min > 0) {
        int idle = lv_disp_get_inactive_time(NULL) > (uint32_t)dim_min * 60000;
        if (idle != dimmed) {
            dimmed = idle;
            ui_backlight_apply(idle ? 0 : (int)lv_slider_get_value(ui_backlight_slider));
        }
    } else if (dimmed) {
        dimmed = 0;
        ui_backlight_apply((int)lv_slider_get_value(ui_backlight_slider));
    }

    /* Without seconds nothing on screen changes faster than once a minute,
     * so the only per-second work left is the overdue pulse. The clock is
     * set in refresh_home along with everything else. */
    if (status_dot && overdue_now)
        lv_obj_set_style_bg_opa(status_dot, blink_on ? LV_OPA_COVER : LV_OPA_30, 0);

    if (t.tm_min != last_min) {
        last_min = t.tm_min;
        if (cur_screen == 0) refresh_home(&t, sched_day_num(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday));
        if (cur_screen == 2) refresh_settings(&t);
    }

    if (n_photos > 1 && now - last_photo >= photo_secs) {
        last_photo = now;
        if (++photo_i >= n_photos) { photo_i = 0; photos_shuffle(); }
        photo_show(photo_i);
    }

    if (t.tm_yday != last_yday) {            /* midnight: today's status changed */
        last_yday = t.tm_yday;
        refresh();
    }
}
