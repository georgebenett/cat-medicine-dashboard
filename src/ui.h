/* Cat medicine dashboard - hand-written LVGL 8.3 UI. */
#pragma once
#include <lvgl/lvgl.h>

/* Implemented in main.c, which owns the sysfs backlight. Raw units, not
 * percent: percentages cannot express the panel's bottom step (1% of 255
 * truncates to 2), and they quantise the fade most coarsely exactly where
 * it should be smoothest. src/ui.c converts for the slider and the labels.
 * Raw 0 is clamped to 1, so this can never blank the panel. */
void ui_backlight_set_raw(int raw);

/* Full-scale raw value, so src/ui.c can convert percentages. */
int  ui_backlight_raw_max(void);

/* The level to fade up to after ui_init, in raw units. */
int  ui_backlight_raw(void);

void ui_init(void);
void ui_tick(void);
