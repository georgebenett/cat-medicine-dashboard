/* Cat medicine dashboard - hand-written LVGL 8.3 UI. */
#pragma once
#include <lvgl/lvgl.h>

/* main.c owns the sysfs backlight code and wires it to this slider. */
extern lv_obj_t *ui_backlight_slider;

void ui_init(void);
void ui_tick(void);
