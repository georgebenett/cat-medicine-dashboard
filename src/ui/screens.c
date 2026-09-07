#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;

static lv_meter_scale_t * scale0;
static lv_meter_indicator_t * indicator1;

static void event_handler_cb_basic_widgets_demo_obj0(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 6, 0, e);
    }
}

static void event_handler_cb_basic_widgets_demo_obj1(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 8, 0, e);
    }
}

static void event_handler_cb_basic_widgets_demo_obj2(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 10, 0, e);
    }
}

static void event_handler_cb_input1_widgets_demo_obj3(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 5, 0, e);
    }
}

static void event_handler_cb_input1_widgets_demo_obj4(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 8, 0, e);
    }
}

static void event_handler_cb_input1_widgets_demo_obj5(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 10, 0, e);
    }
}

static void event_handler_cb_input1_widgets_demo_obj6(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        if (tick_value_change_obj != ta) {
            bool value = lv_obj_has_state(ta, LV_STATE_CHECKED);
            assignBooleanProperty(flowState, 13, 3, value, "Failed to assign Checked state");
        }
    }
}

static void event_handler_cb_input1_widgets_demo_obj7(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target(e);
        if (tick_value_change_obj != ta) {
            bool value = lv_obj_has_state(ta, LV_STATE_CHECKED);
            assignBooleanProperty(flowState, 15, 3, value, "Failed to assign Checked state");
        }
    }
}

static void event_handler_cb_input2_widgets_demo_obj8(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 5, 0, e);
    }
}

static void event_handler_cb_input2_widgets_demo_obj9(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 7, 0, e);
    }
}

static void event_handler_cb_input2_widgets_demo_obj10(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 10, 0, e);
    }
}

static void event_handler_cb_visualiser_widgets_demo_obj11(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 10, 0, e);
    }
}

static void event_handler_cb_visualiser_widgets_demo_obj12(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 12, 0, e);
    }
}

static void event_handler_cb_visualiser_widgets_demo_obj13(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_CLICKED) {
        e->user_data = (void *)0;
        flowPropagateValueLVGLEvent(flowState, 14, 0, e);
    }
}

void create_screen_basic_widgets_demo() {
    void *flowState = getFlowState(0, 0);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.basic_widgets_demo = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 800, 50);
            add_style_tab_panel(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 100, 50);
                    add_style_active_tab(obj);
                    lv_obj_set_style_pad_top(obj, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Basic");
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj0 = obj;
                    lv_obj_set_pos(obj, 100, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_basic_widgets_demo_obj0, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj14 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Input #1");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj1 = obj;
                    lv_obj_set_pos(obj, 200, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_basic_widgets_demo_obj1, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj15 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Input #2");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj2 = obj;
                    lv_obj_set_pos(obj, 300, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_basic_widgets_demo_obj2, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj16 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Visualiser");
                        }
                    }
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 14, 127);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Arc");
        }
        {
            lv_obj_t *obj = lv_arc_create(parent_obj);
            lv_obj_set_pos(obj, 95, 60);
            lv_obj_set_size(obj, 150, 150);
            lv_arc_set_value(obj, 25);
            lv_arc_set_bg_start_angle(obj, 120);
            lv_arc_set_bg_end_angle(obj, 60);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 9, 279);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Button");
        }
        {
            lv_obj_t *obj = lv_btn_create(parent_obj);
            lv_obj_set_pos(obj, 95, 262);
            lv_obj_set_size(obj, 100, 50);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 327, 405);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Image");
        }
        {
            lv_obj_t *obj = lv_img_create(parent_obj);
            lv_obj_set_pos(obj, 404, 380);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_background);
            lv_img_set_pivot(obj, 0, 0);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 14, 405);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Panel");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 95, 363);
            lv_obj_set_size(obj, 205, 100);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 327, 206);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Textarea");
        }
        {
            lv_obj_t *obj = lv_textarea_create(parent_obj);
            lv_obj_set_pos(obj, 404, 97);
            lv_obj_set_size(obj, 387, 233);
            lv_textarea_set_max_length(obj, 1024);
            lv_textarea_set_text(obj, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Pellentesque eget ex risus. Sed hendrerit bibendum mi eget maximus. Cras in risus eget ex interdum imperdiet vitae posuere dolor. Mauris dui justo, ultricies id rutrum eu, laoreet sed metus. Suspendisse id tempor sem. Vestibulum sollicitudin nulla consectetur, sodales ipsum vel, sagittis lacus. Nullam sed vehicula nibh. Praesent ac arcu et velit mattis semper.");
            lv_textarea_set_one_line(obj, false);
            lv_textarea_set_password_mode(obj, false);
        }
    }
    
    tick_screen_basic_widgets_demo();
}

void tick_screen_basic_widgets_demo() {
    void *flowState = getFlowState(0, 0);
    (void)flowState;
}

void create_screen_input1_widgets_demo() {
    void *flowState = getFlowState(0, 1);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.input1_widgets_demo = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 800, 50);
            add_style_tab_panel(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj3 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_input1_widgets_demo_obj3, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj17 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Basic");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 100, 0);
                    lv_obj_set_size(obj, 100, 50);
                    add_style_active_tab(obj);
                    lv_obj_set_style_pad_top(obj, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Input #1");
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj4 = obj;
                    lv_obj_set_pos(obj, 200, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_input1_widgets_demo_obj4, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj18 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Input #2");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj5 = obj;
                    lv_obj_set_pos(obj, 300, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_input1_widgets_demo_obj5, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj19 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Visualiser");
                        }
                    }
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 11, 76);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Checkbox");
        }
        {
            lv_obj_t *obj = lv_checkbox_create(parent_obj);
            objects.obj6 = obj;
            lv_obj_set_pos(obj, 113, 74);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_checkbox_set_text(obj, "Label");
            lv_obj_add_event_cb(obj, event_handler_cb_input1_widgets_demo_obj6, LV_EVENT_ALL, flowState);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 11, 136);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Switch");
        }
        {
            lv_obj_t *obj = lv_switch_create(parent_obj);
            objects.obj7 = obj;
            lv_obj_set_pos(obj, 113, 130);
            lv_obj_set_size(obj, 50, 25);
            lv_obj_add_event_cb(obj, event_handler_cb_input1_widgets_demo_obj7, LV_EVENT_ALL, flowState);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 11, 204);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Dropdown");
        }
        {
            lv_obj_t *obj = lv_dropdown_create(parent_obj);
            lv_obj_set_pos(obj, 113, 191);
            lv_obj_set_size(obj, 150, LV_SIZE_CONTENT);
            lv_dropdown_set_options(obj, "Option 1\nOption 2\nOption 3");
            lv_dropdown_set_selected(obj, 0);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 11, 311);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Roller");
        }
        {
            lv_obj_t *obj = lv_roller_create(parent_obj);
            lv_obj_set_pos(obj, 113, 269);
            lv_obj_set_size(obj, 150, 100);
            lv_roller_set_options(obj, "Option 1\nOption 2\nOption 3", LV_ROLLER_MODE_NORMAL);
        }
        {
            lv_obj_t *obj = lv_slider_create(parent_obj);
            lv_obj_set_pos(obj, 113, 405);
            lv_obj_set_size(obj, 150, 10);
            lv_slider_set_value(obj, 25, LV_ANIM_OFF);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 11, 402);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Slider");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 385, 327);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Colorwheel");
        }
        {
            lv_obj_t *obj = lv_colorwheel_create(parent_obj, false);
            lv_obj_set_pos(obj, 488, 260);
            lv_obj_set_size(obj, 150, 150);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 385, 110);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Imgbutton");
        }
        {
            lv_obj_t *obj = lv_imgbtn_create(parent_obj);
            lv_obj_set_pos(obj, 488, 84);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, 60);
            lv_imgbtn_set_src(obj, LV_IMGBTN_STATE_RELEASED, NULL, &img_released, NULL);
            lv_imgbtn_set_src(obj, LV_IMGBTN_STATE_PRESSED, NULL, &img_pressed, NULL);
            lv_imgbtn_set_src(obj, LV_IMGBTN_STATE_DISABLED, NULL, &img_disabled, NULL);
            lv_imgbtn_set_src(obj, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL, &img_checked_released, NULL);
            lv_imgbtn_set_src(obj, LV_IMGBTN_STATE_CHECKED_PRESSED, NULL, &img_checked_pressed, NULL);
            lv_imgbtn_set_src(obj, LV_IMGBTN_STATE_CHECKED_DISABLED, NULL, &img_checked_disabled, NULL);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN|LV_OBJ_FLAG_CHECKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        }
    }
    
    tick_screen_input1_widgets_demo();
}

void tick_screen_input1_widgets_demo() {
    void *flowState = getFlowState(0, 1);
    (void)flowState;
    {
        bool new_val = evalBooleanProperty(flowState, 13, 3, "Failed to evaluate Checked state");
        bool cur_val = lv_obj_has_state(objects.obj6, LV_STATE_CHECKED);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.obj6;
            if (new_val) lv_obj_add_state(objects.obj6, LV_STATE_CHECKED);
            else lv_obj_clear_state(objects.obj6, LV_STATE_CHECKED);
            tick_value_change_obj = NULL;
        }
    }
    {
        bool new_val = evalBooleanProperty(flowState, 15, 3, "Failed to evaluate Checked state");
        bool cur_val = lv_obj_has_state(objects.obj7, LV_STATE_CHECKED);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.obj7;
            if (new_val) lv_obj_add_state(objects.obj7, LV_STATE_CHECKED);
            else lv_obj_clear_state(objects.obj7, LV_STATE_CHECKED);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_input2_widgets_demo() {
    void *flowState = getFlowState(0, 2);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.input2_widgets_demo = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 800, 50);
            add_style_tab_panel(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj8 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_input2_widgets_demo_obj8, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj20 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Basic");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj9 = obj;
                    lv_obj_set_pos(obj, 100, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_input2_widgets_demo_obj9, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj21 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Input #1");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 200, 0);
                    lv_obj_set_size(obj, 100, 50);
                    add_style_active_tab(obj);
                    lv_obj_set_style_pad_top(obj, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Input #2");
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj10 = obj;
                    lv_obj_set_pos(obj, 300, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_input2_widgets_demo_obj10, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj22 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Visualiser");
                        }
                    }
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 12, 61);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Calendar");
        }
        {
            lv_obj_t *obj = lv_calendar_create(parent_obj);
            lv_obj_set_pos(obj, 12, 85);
            lv_obj_set_size(obj, 304, 379);
            lv_calendar_header_arrow_create(obj);
            lv_calendar_set_today_date(obj, 2022, 11, 1);
            lv_calendar_set_showed_date(obj, 2022, 11);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 332, 61);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Keyboard");
        }
        {
            lv_obj_t *obj = lv_keyboard_create(parent_obj);
            objects.obj23 = obj;
            lv_obj_set_pos(obj, 160, -205);
            lv_obj_set_size(obj, 457, 190);
            lv_obj_set_style_pad_column(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // textarea1
            lv_obj_t *obj = lv_textarea_create(parent_obj);
            objects.textarea1 = obj;
            lv_obj_set_pos(obj, 332, 288);
            lv_obj_set_size(obj, 457, 176);
            lv_textarea_set_max_length(obj, 128);
            lv_textarea_set_one_line(obj, false);
            lv_textarea_set_password_mode(obj, false);
        }
    }
    lv_keyboard_set_textarea(objects.obj23, objects.textarea1);
    
    tick_screen_input2_widgets_demo();
}

void tick_screen_input2_widgets_demo() {
    void *flowState = getFlowState(0, 2);
    (void)flowState;
}

void create_screen_visualiser_widgets_demo() {
    void *flowState = getFlowState(0, 3);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.visualiser_widgets_demo = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, 800, 50);
            add_style_tab_panel(obj);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj11 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_visualiser_widgets_demo_obj11, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj24 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Basic");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj12 = obj;
                    lv_obj_set_pos(obj, 100, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_visualiser_widgets_demo_obj12, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj25 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Input #1");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_btn_create(parent_obj);
                    objects.obj13 = obj;
                    lv_obj_set_pos(obj, 200, 0);
                    lv_obj_set_size(obj, 100, 50);
                    lv_obj_add_event_cb(obj, event_handler_cb_visualiser_widgets_demo_obj13, LV_EVENT_ALL, flowState);
                    add_style_inactive_tab(obj);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj26 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(0xff303030), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "Input #2");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 300, 0);
                    lv_obj_set_size(obj, 100, 50);
                    add_style_active_tab(obj);
                    lv_obj_set_style_pad_top(obj, 17, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Visualiser");
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 10, 70);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Bar");
        }
        {
            lv_obj_t *obj = lv_bar_create(parent_obj);
            objects.obj27 = obj;
            lv_obj_set_pos(obj, 86, 73);
            lv_obj_set_size(obj, 180, 10);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 387, 70);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Chart");
        }
        {
            lv_obj_t *obj = lv_chart_create(parent_obj);
            lv_obj_set_pos(obj, 386, 91);
            lv_obj_set_size(obj, 401, 229);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 10, 212);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Meter");
        }
        {
            lv_obj_t *obj = lv_meter_create(parent_obj);
            objects.obj28 = obj;
            lv_obj_set_pos(obj, 86, 130);
            lv_obj_set_size(obj, 180, 180);
            {
                lv_meter_scale_t *scale = lv_meter_add_scale(obj);
                scale0 = scale;
                lv_meter_set_scale_ticks(obj, scale, 41, 1, 5, lv_color_hex(0xffa0a0a0));
                lv_meter_set_scale_major_ticks(obj, scale, 8, 3, 10, lv_color_hex(0xff000000), 10);
                lv_meter_set_scale_range(obj, scale, 0, 100, 300, 120);
                {
                    lv_meter_indicator_t *indicator = lv_meter_add_needle_line(obj, scale, 3, lv_color_hex(0xff0000ff), -28);
                    indicator1 = indicator;
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 10, 379);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            add_style_widget_name_label(obj);
            lv_label_set_text(obj, "Spinner");
        }
        {
            lv_obj_t *obj = lv_spinner_create(parent_obj, 1000, 60);
            lv_obj_set_pos(obj, 136, 347);
            lv_obj_set_size(obj, 80, 80);
        }
    }
    
    tick_screen_visualiser_widgets_demo();
}

void tick_screen_visualiser_widgets_demo() {
    void *flowState = getFlowState(0, 3);
    (void)flowState;
    {
        int32_t new_val = evalIntegerProperty(flowState, 18, 3, "Failed to evaluate Value in Bar widget");
        int32_t cur_val = lv_bar_get_value(objects.obj27);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.obj27;
            lv_bar_set_value(objects.obj27, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        lv_meter_indicator_t *indicator;
        
        lv_ll_t *indicators = &((lv_meter_t *)objects.obj28)->indicator_ll;
        int index = 0;
        for (indicator = _lv_ll_get_tail(indicators); index > 0 && indicator != NULL; indicator = _lv_ll_get_prev(indicators, indicator), index--);
        
        if (indicator) {
            int32_t new_val = evalIntegerProperty(flowState, 22, 3, "Failed to evaluate Value in Meter widget");
            int32_t cur_val = indicator->start_value;
            if (new_val != cur_val) {
                tick_value_change_obj = objects.obj28;
                lv_meter_set_indicator_value(objects.obj28, indicator, new_val);
                tick_value_change_obj = NULL;
            }
        }
    }
}


extern void add_style(lv_obj_t *obj, int32_t styleIndex);
extern void remove_style(lv_obj_t *obj, int32_t styleIndex);

static const char *screen_names[] = { "basic_widgets_demo", "input1_widgets_demo", "input2_widgets_demo", "visualiser_widgets_demo" };
static const char *object_names[] = { "basic_widgets_demo", "input1_widgets_demo", "input2_widgets_demo", "visualiser_widgets_demo", "obj0", "obj1", "obj2", "obj3", "obj4", "obj5", "obj6", "obj7", "obj8", "obj9", "obj10", "obj11", "obj12", "obj13", "obj14", "obj15", "obj16", "obj17", "obj18", "obj19", "obj20", "obj21", "obj22", "textarea1", "obj23", "obj24", "obj25", "obj26", "obj27", "obj28" };
static const char *style_names[] = { "inactive_tab", "tab_panel", "active_tab", "widget_name_label" };


typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_basic_widgets_demo,
    tick_screen_input1_widgets_demo,
    tick_screen_input2_widgets_demo,
    tick_screen_visualiser_widgets_demo,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    eez_flow_init_styles(add_style, remove_style);
    
    eez_flow_init_screen_names(screen_names, sizeof(screen_names) / sizeof(const char *));
    eez_flow_init_object_names(object_names, sizeof(object_names) / sizeof(const char *));
    eez_flow_init_style_names(style_names, sizeof(style_names) / sizeof(const char *));
    
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_basic_widgets_demo();
    create_screen_input1_widgets_demo();
    create_screen_input2_widgets_demo();
    create_screen_visualiser_widgets_demo();
}
