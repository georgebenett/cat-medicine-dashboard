/* Cat medicine dashboard.
 *
 * Three tabs: Today (is it a medicine day, log the dose, log a vomit),
 * Calendar (which days the medicine actually got given), Settings.
 *
 * State lives in two plain text files next to the binary, so the log
 * survives a rebuild and can be read/edited without this app:
 *   cat_log.csv   "2026-09-08T19:47,med" | "...,vomit", append-only
 *   cat_cfg.txt   medicine-day bitmask, one integer (bit0=Sunday)
 * Override the paths with $CAT_LOG / $CAT_CFG, the photo with $CAT_IMG.
 */
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define C_BG      0x15151b
#define C_CARD    0x24242e
#define C_TEXT    0xf2f2f7
#define C_MUTED   0x8e8e9e
#define C_GREEN   0x3ddc84
#define C_AMBER   0xffb020
#define C_RED     0xff5c5c
#define C_BLUE    0x5aa9ff

/* --- store ---------------------------------------------------------- */

#define MAX_EVTS 4000
typedef struct { int y, mo, d, h, mi; char t; } evt_t;   /* t: 'm'edicine | 'v'omit */

static evt_t evts[MAX_EVTS];
static int   n_evts;
/* Mon/Wed/Fri by default - three a week, changeable in Settings. */
static int   med_mask = (1 << 1) | (1 << 3) | (1 << 5);   /* bit0 = Sunday */

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
            e.t = (kind[0] == 'v') ? 'v' : 'm';
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
                t == 'v' ? "vomit" : "med");
        fclose(f);
    } else {
        perror("cat log append");
    }
    if (n_evts < MAX_EVTS) evts[n_evts++] = e;
}

static void cfg_load(void)
{
    FILE *f = fopen(cfg_path(), "r");
    if (!f) return;
    int m;
    if (fscanf(f, "%d", &m) == 1 && m > 0 && m < 128) med_mask = m;
    fclose(f);
}

static void cfg_save(void)
{
    FILE *f = fopen(cfg_path(), "w");
    if (f) { fprintf(f, "%d\n", med_mask); fclose(f); }
    else perror("cat cfg save");
}

/* --- queries -------------------------------------------------------- */

static const char *DAY[]   = { "Sunday", "Monday", "Tuesday", "Wednesday",
                               "Thursday", "Friday", "Saturday" };
static const char *MONTH[] = { "January", "February", "March", "April", "May", "June",
                               "July", "August", "September", "October", "November", "December" };
static const char *MON3[]  = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static struct tm now_tm(void) { time_t n = time(NULL); return *localtime(&n); }

/* Days since the epoch. Noon avoids the DST hour shifting the answer. */
static long day_num(int y, int mo, int d)
{
    struct tm t = { 0 };
    t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = 12;
    t.tm_isdst = -1;
    return (long)(mktime(&t) / 86400);
}

/* Latest medicine event today, or NULL. */
static const evt_t *med_today(const struct tm *t)
{
    for (int i = n_evts - 1; i >= 0; i--)
        if (evts[i].t == 'm' && evts[i].y == t->tm_year + 1900 &&
            evts[i].mo == t->tm_mon + 1 && evts[i].d == t->tm_mday)
            return &evts[i];
    return NULL;
}

static const evt_t *last_vomit(void)
{
    for (int i = n_evts - 1; i >= 0; i--) if (evts[i].t == 'v') return &evts[i];
    return NULL;
}

static int vomits_last_7d(const struct tm *t)
{
    long today = day_num(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    int n = 0;
    for (int i = 0; i < n_evts; i++)
        if (evts[i].t == 'v' && today - day_num(evts[i].y, evts[i].mo, evts[i].d) < 7) n++;
    return n;
}

/* Weekday of the next scheduled dose after today, or -1 if nothing is set. */
static int next_med_wday(int wday)
{
    if (!med_mask) return -1;
    for (int i = 1; i <= 7; i++) {
        int w = (wday + i) % 7;
        if (med_mask & (1 << w)) return w;
    }
    return -1;
}

/* --- widgets -------------------------------------------------------- */

lv_obj_t *ui_backlight_slider;

static lv_obj_t *lbl_date, *lbl_status, *lbl_sub, *btn_med, *lbl_med, *lbl_stats;
static lv_obj_t *cal, *lbl_recent, *lbl_bl_val, *bm_days, *lbl_clock;
static lv_calendar_date_t hl_dates[512];

static lv_obj_t *card(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 18, 0);
    lv_obj_set_style_pad_all(o, 16, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *text(lv_obj_t *parent, const char *s, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, s);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

static void refresh(void);

static void med_cb(lv_event_t *e)  { (void)e; store_append('m'); refresh(); }
static void vomit_cb(lv_event_t *e){ (void)e; store_append('v'); refresh(); }

static void days_cb(lv_event_t *e)
{
    lv_obj_t *bm = lv_event_get_target(e);
    int mask = 0;
    for (int i = 0; i < 7; i++)
        if (lv_btnmatrix_has_btn_ctrl(bm, i, LV_BTNMATRIX_CTRL_CHECKED)) mask |= (1 << i);
    med_mask = mask;
    cfg_save();
    refresh();
}

static lv_obj_t *big_btn(lv_obj_t *parent, int x, int y, int w, int h,
                         uint32_t color, const char *txt, lv_event_cb_t cb, lv_obj_t **out_lbl)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_radius(b, 16, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = text(b, txt, &lv_font_montserrat_28, 0x101014);
    lv_obj_center(l);
    if (out_lbl) *out_lbl = l;
    return b;
}

/* The photo is a file, not a compiled-in C array: drop any PNG at
 * cat.png next to the binary and restart, no rebuild. */
static void build_photo(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *c = card(parent, x, y, w, h);
    const char *file = img_path();
    static char src[512];
    snprintf(src, sizeof src, "A:%s", file);

    lv_img_header_t hdr;
    if (access(file, R_OK) == 0 && lv_img_decoder_get_info(src, &hdr) == LV_RES_OK &&
        hdr.w > 0 && hdr.h > 0 && (long)hdr.w * hdr.h * 4 < 6L * 1024 * 1024) {
        lv_obj_t *img = lv_img_create(c);
        lv_img_set_src(img, src);
        /* Scale down to fit the card; never upscale (a small photo would blur). */
        int box = (w < h ? w : h) - 32;
        int longest = hdr.w > hdr.h ? hdr.w : hdr.h;
        int zoom = 256 * box / longest;
        if (zoom > 256) zoom = 256;
        if (zoom < 16)  zoom = 16;
        lv_img_set_zoom(img, (uint16_t)zoom);
        lv_img_set_antialias(img, true);
        lv_obj_center(img);
        printf("photo: %s %dx%d zoom %d/256\n", file, hdr.w, hdr.h, zoom);
    } else {
        printf("photo: %s missing or too large to decode - showing placeholder\n", file);
        lv_obj_t *l = text(c, "Put a photo of her at\n\ncat.png\n\n(PNG, under ~1200px)",
                           &lv_font_montserrat_20, C_MUTED);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(l);
    }
}

static void build_today(lv_obj_t *tab)
{
    build_photo(tab, 20, 8, 500, 620);

    lv_obj_t *c = card(tab, 540, 8, 700, 620);
    lbl_date   = text(c, "", &lv_font_montserrat_20, C_MUTED);
    lv_obj_set_pos(lbl_date, 4, 0);

    lbl_status = text(c, "", &lv_font_montserrat_48, C_TEXT);
    lv_obj_set_pos(lbl_status, 4, 36);

    lbl_sub    = text(c, "", &lv_font_montserrat_20, C_MUTED);
    lv_obj_set_pos(lbl_sub, 4, 104);

    btn_med = big_btn(c, 4, 156, 660, 150, C_GREEN, "Medicine given", med_cb, &lbl_med);
    big_btn(c, 4, 326, 660, 110, C_AMBER, "Log vomiting", vomit_cb, NULL);

    lbl_stats = text(c, "", &lv_font_montserrat_20, C_MUTED);
    lv_obj_set_pos(lbl_stats, 4, 462);
}

static void build_calendar(lv_obj_t *tab)
{
    lv_obj_t *c = card(tab, 20, 8, 640, 620);
    cal = lv_calendar_create(c);
    lv_obj_set_size(cal, 600, 570);
    lv_obj_center(cal);
    lv_calendar_header_arrow_create(cal);

    lv_obj_t *r = card(tab, 680, 8, 560, 620);
    lv_obj_t *t = text(r, "Recent", &lv_font_montserrat_28, C_TEXT);
    lv_obj_set_pos(t, 4, 0);
    lbl_recent = text(r, "", &lv_font_montserrat_20, C_MUTED);
    lv_obj_set_pos(lbl_recent, 4, 48);
    lv_label_set_long_mode(lbl_recent, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_recent, 510);
}

static void build_settings(lv_obj_t *tab)
{
    lv_obj_t *c = card(tab, 20, 8, 620, 300);
    lv_obj_t *t = text(c, "Backlight", &lv_font_montserrat_28, C_TEXT);
    lv_obj_set_pos(t, 4, 0);
    lbl_bl_val = text(c, "", &lv_font_montserrat_48, C_BLUE);
    lv_obj_set_pos(lbl_bl_val, 4, 48);

    ui_backlight_slider = lv_slider_create(c);
    lv_obj_set_size(ui_backlight_slider, 560, 40);
    lv_obj_set_pos(ui_backlight_slider, 4, 150);
    lv_slider_set_range(ui_backlight_slider, 5, 100);
    lv_obj_set_style_bg_color(ui_backlight_slider, lv_color_hex(C_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_backlight_slider, lv_color_hex(C_BLUE), LV_PART_KNOB);

    lv_obj_t *d = card(tab, 660, 8, 580, 300);
    t = text(d, "Medicine days", &lv_font_montserrat_28, C_TEXT);
    lv_obj_set_pos(t, 4, 0);

    static const char *map[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "" };
    bm_days = lv_btnmatrix_create(d);
    lv_btnmatrix_set_map(bm_days, map);
    lv_obj_set_size(bm_days, 540, 90);
    lv_obj_set_pos(bm_days, 4, 60);
    lv_obj_set_style_bg_opa(bm_days, LV_OPA_0, 0);
    lv_obj_set_style_border_width(bm_days, 0, 0);
    lv_obj_set_style_bg_color(bm_days, lv_color_hex(C_GREEN), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(bm_days, lv_color_hex(0x101014), LV_PART_ITEMS | LV_STATE_CHECKED);
    for (int i = 0; i < 7; i++) {
        lv_btnmatrix_set_btn_ctrl(bm_days, i, LV_BTNMATRIX_CTRL_CHECKABLE);
        if (med_mask & (1 << i)) lv_btnmatrix_set_btn_ctrl(bm_days, i, LV_BTNMATRIX_CTRL_CHECKED);
    }
    lv_obj_add_event_cb(bm_days, days_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *e = card(tab, 20, 328, 1220, 300);
    lbl_clock = text(e, "", &lv_font_montserrat_48, C_TEXT);
    lv_obj_set_pos(lbl_clock, 4, 0);
    char buf[600];
    snprintf(buf, sizeof buf, "log: %s\ncfg: %s\nphoto: %s", log_path(), cfg_path(), img_path());
    lv_obj_t *p = text(e, buf, &lv_font_montserrat_20, C_MUTED);
    lv_obj_set_pos(p, 4, 90);
}

/* --- refresh -------------------------------------------------------- */

static void refresh(void)
{
    struct tm t = now_tm();
    char buf[1024];

    snprintf(buf, sizeof buf, "%s, %d %s %d",
             DAY[t.tm_wday], t.tm_mday, MONTH[t.tm_mon], t.tm_year + 1900);
    lv_label_set_text(lbl_date, buf);

    int is_med_day = (med_mask >> t.tm_wday) & 1;
    const evt_t *given = med_today(&t);

    if (given) {
        lv_label_set_text(lbl_status, "All done today");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(C_GREEN), 0);
        snprintf(buf, sizeof buf, "Medicine given at %02d:%02d.%s",
                 given->h, given->mi, is_med_day ? "" : "  (not a scheduled day)");
    } else if (is_med_day) {
        lv_label_set_text(lbl_status, "MEDICINE DAY");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(C_AMBER), 0);
        snprintf(buf, sizeof buf, "Not given yet today.");
    } else {
        lv_label_set_text(lbl_status, "No medicine today");
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(C_MUTED), 0);
        int nx = next_med_wday(t.tm_wday);
        if (nx >= 0) snprintf(buf, sizeof buf, "Next dose: %s.", DAY[nx]);
        else         snprintf(buf, sizeof buf, "No days scheduled - set them in Settings.");
    }
    lv_label_set_text(lbl_sub, buf);

    /* Already logged today: grey the button out so a double tap can't double-dose the log. */
    if (given) {
        lv_obj_add_state(btn_med, LV_STATE_DISABLED);
        lv_label_set_text(lbl_med, "Already given today");
    } else {
        lv_obj_clear_state(btn_med, LV_STATE_DISABLED);
        lv_label_set_text(lbl_med, "Medicine given");
    }

    const evt_t *v = last_vomit();
    int v7 = vomits_last_7d(&t);
    if (v) {
        long ago = day_num(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) - day_num(v->y, v->mo, v->d);
        snprintf(buf, sizeof buf, "Last vomit: %s (%ld %s ago)   -   %d in the last 7 days",
                 ago == 0 ? "today" : "", ago, ago == 1 ? "day" : "days", v7);
        if (ago == 0)
            snprintf(buf, sizeof buf, "Last vomit: today at %02d:%02d   -   %d in the last 7 days",
                     v->h, v->mi, v7);
    } else {
        snprintf(buf, sizeof buf, "No vomiting logged yet.");
    }
    lv_label_set_text(lbl_stats, buf);

    /* Calendar: highlight the days she actually got it. */
    int n = 0;
    for (int i = n_evts - 1; i >= 0 && n < (int)(sizeof hl_dates / sizeof hl_dates[0]); i--) {
        if (evts[i].t != 'm') continue;
        hl_dates[n].year  = (uint16_t)evts[i].y;
        hl_dates[n].month = (int8_t)evts[i].mo;
        hl_dates[n].day   = (int8_t)evts[i].d;
        n++;
    }
    lv_calendar_set_today_date(cal, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    lv_calendar_set_highlighted_dates(cal, hl_dates, (uint16_t)n);

    /* Recent list, newest first. */
    size_t off = 0;
    buf[0] = 0;
    for (int i = n_evts - 1, shown = 0; i >= 0 && shown < 14; i--, shown++) {
        off += snprintf(buf + off, sizeof buf - off, "%s %2d   %02d:%02d   %s\n",
                        MON3[evts[i].mo - 1], evts[i].d, evts[i].h, evts[i].mi,
                        evts[i].t == 'v' ? "Vomit" : "Medicine");
        if (off >= sizeof buf) break;
    }
    lv_label_set_text(lbl_recent, buf[0] ? buf : "Nothing logged yet.");
}

/* --- entry points --------------------------------------------------- */

void ui_init(void)
{
    cfg_load();
    store_load();

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(C_BG), 0);

    lv_obj_t *tv = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 64);
    lv_obj_set_style_bg_color(tv, lv_color_hex(C_BG), 0);

    lv_obj_t *btns = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_text_font(btns, &lv_font_montserrat_20, 0);
    lv_obj_set_style_bg_color(btns, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_text_color(btns, lv_color_hex(C_MUTED), 0);
    lv_obj_set_style_text_color(btns, lv_color_hex(C_TEXT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(btns, lv_color_hex(C_GREEN), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t *t1 = lv_tabview_add_tab(tv, "Today");
    lv_obj_t *t2 = lv_tabview_add_tab(tv, "Calendar");
    lv_obj_t *t3 = lv_tabview_add_tab(tv, "Settings");
    lv_obj_t *tabs[] = { t1, t2, t3 };
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_pad_all(tabs[i], 0, 0);
        lv_obj_clear_flag(tabs[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    build_today(t1);
    build_calendar(t2);
    build_settings(t3);
    refresh();
}

void ui_tick(void)
{
    static time_t last_sec;
    static int last_yday = -1;

    time_t now = time(NULL);
    if (now == last_sec) return;             /* the loop runs at ~200Hz; this needs 1Hz */
    last_sec = now;

    struct tm t = *localtime(&now);
    char buf[64];
    snprintf(buf, sizeof buf, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    if (lbl_clock) lv_label_set_text(lbl_clock, buf);

    if (ui_backlight_slider)
        lv_label_set_text_fmt(lbl_bl_val, "%d%%",
                              (int)lv_slider_get_value(ui_backlight_slider));

    if (t.tm_yday != last_yday) {            /* midnight rollover: today's status changed */
        last_yday = t.tm_yday;
        refresh();
    }
}
