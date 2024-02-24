#include "lvgl_port_m5stack.hpp"

#if defined(ARDUINO) && defined(ESP_PLATFORM)
static SemaphoreHandle_t xGuiSemaphore;
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
static SDL_mutex *xGuiMutex;
#endif

#ifndef LV_BUFFER_LINE
#define LV_BUFFER_LINE (gfx.height() / 2)
#endif

#ifdef __cplusplus
extern "C" {
#endif

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    M5GFX &gfx = *(M5GFX *)disp->user_data;
    int w      = (area->x2 - area->x1 + 1);
    int h      = (area->y2 - area->y1 + 1);

    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
    // gfx.writePixels((lgfx::swap565_t *)&color_p->full, w * h);  // swap red and blue
    gfx.endWrite();
    lv_disp_flush_ready(disp);
}

static void lvgl_read_cb(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    M5GFX &gfx = *(M5GFX *)indev_driver->user_data;
    uint16_t touchX, touchY;

    bool touched = gfx.getTouch(&touchX, &touchY);
    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

#if defined(ARDUINO) && defined(ESP_PLATFORM)
static void lvgl_tick_timer(void *arg) {
    (void)arg;
    lv_tick_inc(10);
}
static void lvgl_rtos_task(void *pvParameter) {
    (void)pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();
    while (1) {
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
static uint32_t lvgl_tick_timer(uint32_t interval, void *param) {
    (void)interval;
    (void)param;
    lv_tick_inc(10);
    return 10;
}

static int lvgl_sdl_thread(void *data) {
    (void)data;
    xGuiMutex = SDL_CreateMutex();
    while (1) {
        if (SDL_LockMutex(xGuiMutex) == 0) {
            lv_timer_handler();
            SDL_UnlockMutex(xGuiMutex);
        }
        SDL_Delay(10);
    }
    return 0;
}
#endif

void lvgl_port_init(M5GFX &gfx) {
    lv_init();

    static lv_disp_draw_buf_t *draw_buf = (lv_disp_draw_buf_t *)malloc(sizeof(lv_disp_draw_buf_t));
#if defined(ARDUINO) && defined(ESP_PLATFORM)
#if defined(BOARD_HAS_PSRAM)
    static lv_color_t *buf1 =
        (lv_color_t *)heap_caps_malloc(gfx.width() * LV_BUFFER_LINE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    static lv_color_t *buf2 =
        (lv_color_t *)heap_caps_malloc(gfx.width() * LV_BUFFER_LINE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, gfx.width() * LV_BUFFER_LINE);
#else
    static lv_color_t *buf1 = (lv_color_t *)malloc(gfx.width() * LV_BUFFER_LINE * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, gfx.width() * LV_BUFFER_LINE);
#endif
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    static lv_color_t *buf1 = (lv_color_t *)malloc(gfx.width() * LV_BUFFER_LINE * sizeof(lv_color_t));
    static lv_color_t *buf2 = (lv_color_t *)malloc(gfx.width() * LV_BUFFER_LINE * sizeof(lv_color_t));
    lv_disp_draw_buf_init(draw_buf, buf1, buf2, gfx.width() * LV_BUFFER_LINE);
#endif

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = gfx.width();
    disp_drv.ver_res   = gfx.height();
    disp_drv.flush_cb  = lvgl_flush_cb;
    disp_drv.draw_buf  = draw_buf;
    disp_drv.user_data = &gfx;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type      = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb   = lvgl_read_cb;
    indev_drv.user_data = &gfx;
    lv_indev_t *indev   = lv_indev_drv_register(&indev_drv);

#if defined(ARDUINO) && defined(ESP_PLATFORM)
    const esp_timer_create_args_t periodic_timer_args = {.callback = &lvgl_tick_timer, .name = "lvgl_tick_timer"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));
    xTaskCreate(lv_task_handler, "lv_task", 4096, NULL, 1, NULL);
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    SDL_AddTimer(10, lvgl_tick_timer, NULL);
    SDL_CreateThread(lvgl_sdl_thread, "lvgl_sdl_thread", NULL);
#endif
}

void lvgl_port_lock(void) {
#if defined(ARDUINO) && defined(ESP_PLATFORM)
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    SDL_LockMutex(xGuiMutex);
#endif
}

void lvgl_port_unlock(void) {
#if defined(ARDUINO) && defined(ESP_PLATFORM)
    xSemaphoreGive(xGuiSemaphore);
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    SDL_UnlockMutex(xGuiMutex);
#endif
}

#ifdef __cplusplus
}
#endif
