/* Cat medicine dashboard - hand-written LVGL 8.3 UI. */
#pragma once
#include <lvgl/lvgl.h>

/* main.c owns the sysfs backlight code and wires it to this slider. */
extern lv_obj_t *ui_backlight_slider;

/* Implemented in main.c, which owns the sysfs backlight. Used for idle
 * dimming; the slider is wired up there too. */
void ui_backlight_apply(int pct);

void ui_init(void);
void ui_tick(void);
