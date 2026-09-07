#include "images.h"

const ext_img_desc_t images[7] = {
    { "checked pressed", &img_checked_pressed },
    { "checked disabled", &img_checked_disabled },
    { "released", &img_released },
    { "pressed", &img_pressed },
    { "disabled", &img_disabled },
    { "checked released", &img_checked_released },
    { "background", &img_background },
};
