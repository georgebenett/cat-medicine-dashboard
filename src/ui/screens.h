#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *basic_widgets_demo;
    lv_obj_t *input1_widgets_demo;
    lv_obj_t *input2_widgets_demo;
    lv_obj_t *visualiser_widgets_demo;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *obj10;
    lv_obj_t *obj11;
    lv_obj_t *obj12;
    lv_obj_t *obj13;
    lv_obj_t *obj14;
    lv_obj_t *obj15;
    lv_obj_t *obj16;
    lv_obj_t *obj17;
    lv_obj_t *obj18;
    lv_obj_t *obj19;
    lv_obj_t *obj20;
    lv_obj_t *obj21;
    lv_obj_t *obj22;
    lv_obj_t *textarea1;
    lv_obj_t *obj23;
    lv_obj_t *obj24;
    lv_obj_t *obj25;
    lv_obj_t *obj26;
    lv_obj_t *obj27;
    lv_obj_t *obj28;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_BASIC_WIDGETS_DEMO = 1,
    SCREEN_ID_INPUT1_WIDGETS_DEMO = 2,
    SCREEN_ID_INPUT2_WIDGETS_DEMO = 3,
    SCREEN_ID_VISUALISER_WIDGETS_DEMO = 4,
};

void create_screen_basic_widgets_demo();
void tick_screen_basic_widgets_demo();

void create_screen_input1_widgets_demo();
void tick_screen_input1_widgets_demo();

void create_screen_input2_widgets_demo();
void tick_screen_input2_widgets_demo();

void create_screen_visualiser_widgets_demo();
void tick_screen_visualiser_widgets_demo();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/