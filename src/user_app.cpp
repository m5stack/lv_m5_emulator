#include "lvgl.h"
#include "demos/lv_demos.h"

void user_app(void) {
#if LVGL_USE_V8 == 1
    lv_demo_widgets();
#elif LVGL_USE_V9 == 1
    // lv_demo_widgets();  // lv_demo_widgets not work with v9 now
    lv_demo_stress();
#endif
}
