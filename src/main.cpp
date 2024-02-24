#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lvgl_port_m5stack.hpp"

extern void user_app(void);

M5GFX gfx;

void setup(void) {
    gfx.init();

    lvgl_port_init(gfx);

    user_app();
}

void loop(void) {
#if defined(ARDUINO) && defined(ESP_PLATFORM)
    delay(10);
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    usleep(10 * 000);
#endif
}
