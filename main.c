/* Minimal LVGL 8.3 harness: Waveshare 9" DSI panel (fbdev) + Goodix touch (evdev).
 *
 * The panel is physically 720x1280 portrait. The EEZ UI is 1280x720 landscape,
 * so LVGL rotates in software: we hand LVGL the NATIVE size and set ROT_90,
 * and lv_disp_get_hor_res() then reports 1280x720 to the UI code.
 *
 * ponytail: software rotation costs a full-frame rotate per refresh (~1.8MB on
 * a Pi 3A+, so expect low double-digit fps). If that's too slow, the upgrade
 * path is the DRM backend with a hardware plane-rotation property.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <glob.h>
#include <lvgl/lvgl.h>
#include "ui/ui.h"
#include "ui/screens.h"

#define FB_DEV     "/dev/fb0"

#define PANEL_W 720
#define PANEL_H 1280

/* Touch calibration knobs. The panel controller reports in native portrait
 * coords; flip these if the pointer moves the wrong way after rotation. */
/* Runtime-overridable: TOUCH_SWAP / TOUCH_INVX / TOUCH_INVY env vars (0|1). */
/* Pass NATIVE (unrotated) panel coords to LVGL: lv_indev.c's
 * indev_pointer_proc() already applies disp_drv.rotated itself. Rotating
 * here as well double-rotates and puts every touch 90 degrees off.
 * These stay as calibration knobs for a differently-mounted panel. */
static int t_swap = 0, t_invx = 0, t_invy = 0, t_debug = 0;
static void touch_cfg_from_env(void)
{
    const char *e;
    if ((e = getenv("TOUCH_SWAP"))) t_swap = atoi(e);
    if ((e = getenv("TOUCH_INVX"))) t_invx = atoi(e);
    if ((e = getenv("TOUCH_INVY"))) t_invy = atoi(e);
    if ((e = getenv("TOUCH_DEBUG"))) t_debug = atoi(e);
    printf("touch cfg: swap=%d invx=%d invy=%d\n", t_swap, t_invx, t_invy);
}

static int      fbfd = -1, touch_fd = -1;
static uint8_t *fbmem;
static int      fb_stride, fb_bytes_pp;
/* Touch raw ranges, read from the driver rather than assumed. */
static int      t_max_x = 0, t_max_y = 0;

/* Find the touchscreen by NAME, not by event index: /dev/input/eventN
 * numbering shifts with USB enumeration order across reboots. */
static int open_touch(void)
{
    char path[32], name[256];
    for (int i = 0; i < 32; i++) {
        snprintf(path, sizeof path, "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        name[0] = 0;
        if (ioctl(fd, EVIOCGNAME(sizeof name), name) >= 0 && strcasestr(name, "goodix")) {
            struct input_absinfo ai;
            if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ai) == 0 && ai.maximum > 0) t_max_x = ai.maximum;
            else if (ioctl(fd, EVIOCGABS(ABS_X), &ai) == 0 && ai.maximum > 0) t_max_x = ai.maximum;
            if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ai) == 0 && ai.maximum > 0) t_max_y = ai.maximum;
            else if (ioctl(fd, EVIOCGABS(ABS_Y), &ai) == 0 && ai.maximum > 0) t_max_y = ai.maximum;
            if (t_max_x <= 0) t_max_x = PANEL_W - 1;
            if (t_max_y <= 0) t_max_y = PANEL_H - 1;
            printf("touch: %s \"%s\" raw range %dx%d\n", path, name, t_max_x, t_max_y);
            return fd;
        }
        close(fd);
    }
    fprintf(stderr, "touch: no Goodix device found\n");
    return -1;
}

static void fb_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    int w = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(fbmem + y * fb_stride + area->x1 * fb_bytes_pp,
               color_p, w * fb_bytes_pp);
        color_p += w;
    }
    lv_disp_flush_ready(drv);
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    static int raw_x = 0, raw_y = 0, pressed = 0;
    struct input_event ev;

    while (read(touch_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) raw_x = ev.value;
            else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) raw_y = ev.value;
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            pressed = ev.value;
        }
    }

    /* raw -> panel-native pixels */
    int nx = (int)((long)raw_x * (PANEL_W - 1) / t_max_x);
    int ny = (int)((long)raw_y * (PANEL_H - 1) / t_max_y);

    /* native portrait -> logical landscape, matching LV_DISP_ROT_90 */
    int x, y;
    if (t_swap) { x = ny; y = nx; } else { x = nx; y = ny; }
    if (t_invx) x = (PANEL_H - 1) - x;
    if (t_invy) y = (PANEL_W - 1) - y;

    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    /* Opt-in only: stdout draws over the UI on the panel console. */
    if (t_debug && pressed) fprintf(stderr, "touch raw=%d,%d -> %d,%d\n", raw_x, raw_y, x, y);
}

/* --- panel backlight -------------------------------------------------
 * The Waveshare panel MCU exposes a standard sysfs backlight. Found by
 * glob, not hardcoded: the i2c address shows up in the device name. */
#define BL_MIN_PCT 5        /* never fully dark - you'd lose sight of the slider */

static char bl_path[512];
static int  bl_max;

static void backlight_find(void)
{
    glob_t g;
    if (glob("/sys/class/backlight/*/brightness", 0, NULL, &g) == 0 && g.gl_pathc > 0) {
        snprintf(bl_path, sizeof bl_path, "%s", g.gl_pathv[0]);
        char mp[512];
        snprintf(mp, sizeof mp, "%s", g.gl_pathv[0]);
        char *slash = strrchr(mp, '/');
        if (slash) snprintf(slash, sizeof mp - (slash - mp), "/max_brightness");
        FILE *f = fopen(mp, "r");
        if (f) { if (fscanf(f, "%d", &bl_max) != 1) bl_max = 0; fclose(f); }
        printf("backlight: %s (max %d)\n", bl_path, bl_max);
    } else {
        printf("backlight: none found\n");
    }
    globfree(&g);
}

static void backlight_set_pct(int pct)
{
    if (!bl_path[0] || bl_max <= 0) return;
    if (pct < BL_MIN_PCT) pct = BL_MIN_PCT;
    if (pct > 100) pct = 100;
    FILE *f = fopen(bl_path, "w");
    if (!f) { perror("backlight write"); return; }
    fprintf(f, "%d\n", pct * bl_max / 100);
    fclose(f);
}

static int backlight_get_pct(void)
{
    if (!bl_path[0] || bl_max <= 0) return 100;
    int v = bl_max;
    FILE *f = fopen(bl_path, "r");
    if (f) { if (fscanf(f, "%d", &v) != 1) v = bl_max; fclose(f); }
    return v * 100 / bl_max;
}

static void backlight_slider_cb(lv_event_t *e)
{
    backlight_set_pct((int)lv_slider_get_value(lv_event_get_target(e)));
}

static uint32_t millis(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/* Partial buffers, sized in LOGICAL (post-rotation) width.
 * NOT full_refresh: LVGL 8.3 refuses to software-rotate a full-refresh
 * display ("cannot rotate a full refreshed display!") and silently skips
 * flush_cb entirely, leaving the panel showing whatever was there before. */
#define DRAW_LINES 40
static lv_color_t draw_buf_1[PANEL_H * DRAW_LINES];
static lv_color_t draw_buf_2[PANEL_H * DRAW_LINES];

int main(void)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    fbfd = open(FB_DEV, O_RDWR);
    if (fbfd < 0) { perror("open " FB_DEV); return 1; }
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) < 0) { perror("VSCREENINFO"); return 1; }
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) < 0) { perror("FSCREENINFO"); return 1; }

    fb_stride   = finfo.line_length;
    fb_bytes_pp = vinfo.bits_per_pixel / 8;
    printf("fb: %ux%u %ubpp stride=%d\n", vinfo.xres, vinfo.yres,
           vinfo.bits_per_pixel, fb_stride);

    if (vinfo.bits_per_pixel != LV_COLOR_DEPTH) {
        fprintf(stderr, "WARNING: fb is %u bpp but LV_COLOR_DEPTH is %d\n",
                vinfo.bits_per_pixel, LV_COLOR_DEPTH);
    }

    fbmem = mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmem == MAP_FAILED) { perror("mmap"); return 1; }

    touch_cfg_from_env();
    touch_fd = open_touch();

    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, draw_buf_1, draw_buf_2, PANEL_H * DRAW_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf     = &draw_buf;
    disp_drv.hor_res      = PANEL_W;   /* native; LVGL swaps these for the UI */
    disp_drv.ver_res      = PANEL_H;
    disp_drv.flush_cb     = fb_flush;
    disp_drv.rotated      = LV_DISP_ROT_90;
    disp_drv.sw_rotate    = 1;
    lv_disp_drv_register(&disp_drv);

    lv_indev_t *indev = NULL;
    if (touch_fd >= 0) {
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type    = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = touch_read;
        indev = lv_indev_drv_register(&indev_drv);
    }

    printf("lvgl reports %dx%d\n", (int)lv_disp_get_hor_res(NULL),
           (int)lv_disp_get_ver_res(NULL));

    ui_init();

    /* Wire the EEZ slider to the backlight HERE, not in src/ui/: EEZ Studio
     * regenerates screens.c and would overwrite anything added there. */
    backlight_find();
    if (objects.backlight_slider) {
        lv_slider_set_value(objects.backlight_slider, backlight_get_pct(), LV_ANIM_OFF);
        lv_obj_add_event_cb(objects.backlight_slider, backlight_slider_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
        printf("backlight slider wired (currently %d%%)\n", backlight_get_pct());
    }

    /* Debug cursor on the system layer: shows where LVGL believes the
     * pointer is. Set TOUCH_CURSOR=0 to hide it once calibration is done. */
    const char *cur_env = getenv("TOUCH_CURSOR");   /* set to 1 to re-enable */
    if (indev && cur_env && atoi(cur_env)) {
        lv_obj_t *cur = lv_obj_create(lv_layer_sys());
        lv_obj_set_size(cur, 28, 28);
        lv_obj_set_style_radius(cur, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(cur, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_bg_opa(cur, LV_OPA_60, 0);
        lv_obj_set_style_border_width(cur, 2, 0);
        lv_obj_set_style_border_color(cur, lv_color_hex(0xFFFFFF), 0);
        lv_obj_clear_flag(cur, LV_OBJ_FLAG_CLICKABLE);
        lv_indev_set_cursor(indev, cur);
        printf("debug cursor enabled\n");
    }

    uint32_t prev = millis();
    for (;;) {
        uint32_t now = millis();
        lv_tick_inc(now - prev);
        prev = now;
        lv_timer_handler();
        ui_tick();
        usleep(5000);
    }
    return 0;
}
