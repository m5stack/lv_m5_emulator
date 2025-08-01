#include "lvgl.h"
#include "lvgl_port_m5stack.hpp"
#include "demos/lv_demos.h"

#ifdef USE_EEZ_STUDIO
#include "ui/ui.h"

// EEZ Studio specific actions
/*
extern "C" void action_example_for_eez(lv_event_t* e)
{
    // Example action for EEZ Studio
    // You can add your own logic here
    // The following code demonstrates how to change the UI
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello EEZ Studio!");
}
*/
#endif


void user_app(void)
{
#ifndef USE_EEZ_STUDIO
    // You can test the lvgl default demo
    if (lvgl_port_lock()) {
        lv_demo_benchmark();
        // lv_demo_widgets();
        // lv_demo_music();
        // lv_demo_stress();
        lvgl_port_unlock();
    }

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
#else
    // Or you can initialize the UI for EEZ Studio if you are using it
    if (lvgl_port_lock()) {
        ui_init();
        lvgl_port_unlock();
    }
#endif
}
