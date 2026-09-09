/* Cat medicine dashboard - hand-written LVGL 8.3 UI. */
#pragma once
#include <lvgl/lvgl.h>

/* Implemented in main.c, which owns the sysfs backlight. src/ui.c owns the
 * slider and the remembered brightness. Percentage only - it is clamped to
 * BL_MIN_PCT, so it can never blank the panel. */
void ui_backlight_apply(int pct);

/* The remembered brightness, for main.c to fade up to after ui_init. */
int  ui_backlight_pct(void);

void ui_init(void);
void ui_tick(void);
