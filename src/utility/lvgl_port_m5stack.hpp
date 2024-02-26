#ifndef __LVGL_PORT_M5STACK_HPP__
#define __LVGL_PORT_M5STACK_HPP__

#if defined(ARDUINO)
#include <Arduino.h>
#endif
#include <M5GFX.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_port_init(M5GFX &gfx);
bool lvgl_port_lock(void);
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif

#endif  // __LVGL_PORT_M5STACK_HPP__