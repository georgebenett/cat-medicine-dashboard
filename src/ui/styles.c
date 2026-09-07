#include "styles.h"
#include "images.h"
#include "fonts.h"

#include "ui.h"
#include "screens.h"

//
// Style: inactive_tab
//

void init_style_inactive_tab_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_shadow_width(style, 0);
    lv_style_set_radius(style, 0);
    lv_style_set_bg_color(style, lv_color_hex(0xffffffff));
};

lv_style_t *get_style_inactive_tab_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_inactive_tab_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_inactive_tab(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_inactive_tab_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_inactive_tab(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_inactive_tab_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: tab_panel
//

void init_style_tab_panel_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_border_width(style, 0);
    lv_style_set_radius(style, 0);
    lv_style_set_pad_top(style, 0);
    lv_style_set_pad_bottom(style, 0);
    lv_style_set_pad_left(style, 0);
    lv_style_set_pad_right(style, 0);
};

lv_style_t *get_style_tab_panel_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_tab_panel_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_tab_panel(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_tab_panel_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_tab_panel(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_tab_panel_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: active_tab
//

void init_style_active_tab_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_bg_color(style, lv_color_hex(0xfff5f5f5));
    lv_style_set_text_align(style, LV_TEXT_ALIGN_CENTER);
    lv_style_set_bg_opa(style, 255);
};

lv_style_t *get_style_active_tab_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_active_tab_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_active_tab(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_active_tab_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_active_tab(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_active_tab_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
// Style: widget_name_label
//

void init_style_widget_name_label_MAIN_DEFAULT(lv_style_t *style) {
    lv_style_set_text_color(style, lv_color_hex(0xff0078b5));
};

lv_style_t *get_style_widget_name_label_MAIN_DEFAULT() {
    static lv_style_t *style;
    if (!style) {
        style = lv_mem_alloc(sizeof(lv_style_t));
        lv_style_init(style);
        init_style_widget_name_label_MAIN_DEFAULT(style);
    }
    return style;
};

void add_style_widget_name_label(lv_obj_t *obj) {
    (void)obj;
    lv_obj_add_style(obj, get_style_widget_name_label_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

void remove_style_widget_name_label(lv_obj_t *obj) {
    (void)obj;
    lv_obj_remove_style(obj, get_style_widget_name_label_MAIN_DEFAULT(), LV_PART_MAIN | LV_STATE_DEFAULT);
};

//
//
//

void add_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*AddStyleFunc)(lv_obj_t *obj);
    static const AddStyleFunc add_style_funcs[] = {
        add_style_inactive_tab,
        add_style_tab_panel,
        add_style_active_tab,
        add_style_widget_name_label,
    };
    add_style_funcs[styleIndex](obj);
}

void remove_style(lv_obj_t *obj, int32_t styleIndex) {
    typedef void (*RemoveStyleFunc)(lv_obj_t *obj);
    static const RemoveStyleFunc remove_style_funcs[] = {
        remove_style_inactive_tab,
        remove_style_tab_panel,
        remove_style_active_tab,
        remove_style_widget_name_label,
    };
    remove_style_funcs[styleIndex](obj);
}

