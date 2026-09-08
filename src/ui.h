/* Cat medicine dashboard - hand-written LVGL 8.3 UI. */
#pragma once
#include <lvgl/lvgl.h>

/* Implemented in main.c, which owns the sysfs backlight. src/ui.c owns the
 * slider and the remembered brightness. pct <= 0 means the dimmest the
 * panel goes while still lit, used for idle dimming. */
void ui_backlight_apply(int pct);

void ui_init(void);
void ui_tick(void);
