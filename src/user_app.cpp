#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "demos/lv_demos.h"

void user_app(void)
{
    // You can test the lvgl default demo
    // /*
    if (lvgl_port_lock()) {
#if LVGL_USE_V8 == 1
        lv_demo_widgets();
#elif LVGL_USE_V9 == 1
        // lv_demo_widgets not work proerly with v9 for now :(
#if defined(ARDUINO) && defined(ESP_PLATFORM)
        lv_demo_benchmark();
#else
        // lv_demo_stress not work properly with actual devices for now :(
        lv_demo_stress();
#endif
#endif
        lvgl_port_unlock();
    }
    // */

    // Or you can create your own lvgl app here
    /*
    if (lvgl_port_lock()) {
        static lv_obj_t* btn1 = lv_btn_create(lv_scr_act());
        lv_obj_align(btn1, LV_ALIGN_CENTER, 0, 0);

        static lv_obj_t* label = lv_label_create(btn1);
        lv_label_set_text(label, "Button");
        lv_obj_center(label);

        lvgl_port_unlock();
    }
    */
}
