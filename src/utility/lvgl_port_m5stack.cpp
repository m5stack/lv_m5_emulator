#include "lvgl_port_m5stack.hpp"
#include <cstdlib>  // for aligned_alloc
#include <cstring>  // for memset

#if defined(ARDUINO) && defined(ESP_PLATFORM)
static SemaphoreHandle_t xGuiSemaphore;
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
static SDL_mutex *xGuiMutex;
#endif

#ifndef LV_BUFFER_LINE
#define LV_BUFFER_LINE 120
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ARDUINO) && defined(ESP_PLATFORM)
static void lvgl_tick_timer(void *arg) {
    (void)arg;
    lv_tick_inc(10);
}
static void lvgl_rtos_task(void *pvParameter) {
    (void)pvParameter;
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

#if LVGL_USE_V8 == 1
static lv_disp_draw_buf_t draw_buf;
static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    M5GFX &gfx      = *(M5GFX *)disp->user_data;
    int w           = (area->x2 - area->x1 + 1);
    int h           = (area->y2 - area->y1 + 1);
    uint32_t pixels = w * h;

    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);

    // Critical fix: Use safe pixel writing method to avoid M5GFX SIMD optimizations
    // Break large transfers into small chunks to avoid problematic copy_rgb_fast function
    const uint32_t SAFE_CHUNK_SIZE = 8192;  // 8K pixels per chunk, suitable for small buffer settings

    if (pixels > SAFE_CHUNK_SIZE) {
        // Chunked transmission for large data
        const lgfx::rgb565_t *src = (const lgfx::rgb565_t *)color_p;
        uint32_t remaining        = pixels;
        uint32_t offset           = 0;

        while (remaining > 0) {
            uint32_t chunk_size = (remaining > SAFE_CHUNK_SIZE) ? SAFE_CHUNK_SIZE : remaining;
            gfx.writePixels(src + offset, chunk_size);
            offset += chunk_size;
            remaining -= chunk_size;
        }
    } else {
        // Direct transmission for small data
        gfx.writePixels((lgfx::rgb565_t *)color_p, pixels);
    }

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

void lvgl_port_init(M5GFX &gfx) {
    lv_init();

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
    const uint32_t buf_pixels = gfx.width() * LV_BUFFER_LINE;
    // Moderate safety margin, combined with chunked transmission strategy, suitable for small buffers
    const uint32_t extra_pixels = 1024;        // 1024 pixels safety margin (2KB)
    const uint32_t safe_pixels  = buf_pixels;  // LVGL can use all allocated pixels

    // Use aligned allocation to ensure memory boundary alignment, reducing SIMD access issues
    const size_t alignment    = 64;  // 64-byte alignment
    const size_t total_bytes  = (buf_pixels + extra_pixels) * sizeof(lv_color_t);
    const size_t aligned_size = (total_bytes + alignment - 1) & ~(alignment - 1);

    static lv_color_t *buf1 = static_cast<lv_color_t *>(aligned_alloc(alignment, aligned_size));
    static lv_color_t *buf2 = static_cast<lv_color_t *>(aligned_alloc(alignment, aligned_size));

    if (!buf1 || !buf2) {
        printf("ERROR: Failed to allocate aligned memory buffers\n");
        return;
    }

    // Clear buffers to avoid issues caused by random data
    memset(buf1, 0, aligned_size);
    memset(buf2, 0, aligned_size);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, safe_pixels);
#endif

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = gfx.width();
    disp_drv.ver_res   = gfx.height();
    disp_drv.flush_cb  = lvgl_flush_cb;
    disp_drv.draw_buf  = &draw_buf;
    disp_drv.user_data = &gfx;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type      = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb   = lvgl_read_cb;
    indev_drv.user_data = &gfx;
    lv_indev_t *indev   = lv_indev_drv_register(&indev_drv);

#if defined(ARDUINO) && defined(ESP_PLATFORM)
    xGuiSemaphore                                     = xSemaphoreCreateMutex();
    const esp_timer_create_args_t periodic_timer_args = {.callback = &lvgl_tick_timer, .name = "lvgl_tick_timer"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000));
    xTaskCreate(lvgl_rtos_task, "lvgl_rtos_task", 4096, NULL, 1, NULL);
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    xGuiMutex = SDL_CreateMutex();
    SDL_AddTimer(10, lvgl_tick_timer, NULL);
    SDL_CreateThread(lvgl_sdl_thread, "lvgl_sdl_thread", NULL);
#endif
}
#elif LVGL_USE_V9 == 1
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    M5GFX &gfx = *(M5GFX *)lv_display_get_driver_data(disp);

    uint32_t w      = (area->x2 - area->x1 + 1);
    uint32_t h      = (area->y2 - area->y1 + 1);
    uint32_t pixels = w * h;

    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);

    // Critical fix: Use safe pixel writing method to avoid M5GFX SIMD optimizations
    // Break large transfers into small chunks to avoid problematic copy_rgb_fast function
    const uint32_t SAFE_CHUNK_SIZE = 8192;  // 8K pixels per chunk, suitable for small buffer settings

    if (pixels > SAFE_CHUNK_SIZE) {
        // Chunked transmission for large data
        const lgfx::rgb565_t *src = (const lgfx::rgb565_t *)px_map;
        uint32_t remaining        = pixels;
        uint32_t offset           = 0;

        while (remaining > 0) {
            uint32_t chunk_size = (remaining > SAFE_CHUNK_SIZE) ? SAFE_CHUNK_SIZE : remaining;
            gfx.writePixels(src + offset, chunk_size);
            offset += chunk_size;
            remaining -= chunk_size;
        }
    } else {
        // Direct transmission for small data
        gfx.writePixels((lgfx::rgb565_t *)px_map, pixels);
    }

    gfx.endWrite();

    lv_display_flush_ready(disp);
}

static void lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    M5GFX &gfx = *(M5GFX *)lv_indev_get_driver_data(indev);
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

void lvgl_port_init(M5GFX &gfx) {
    lv_init();

    static lv_display_t *disp = lv_display_create(gfx.width(), gfx.height());
    if (disp == NULL) {
        LV_LOG_ERROR("lv_display_create failed");
        return;
    }

    lv_display_set_driver_data(disp, &gfx);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
#if defined(ARDUINO) && defined(ESP_PLATFORM)
#if defined(BOARD_HAS_PSRAM)
    static uint8_t *buf1 = (uint8_t *)heap_caps_malloc(gfx.width() * LV_BUFFER_LINE, MALLOC_CAP_SPIRAM);
    static uint8_t *buf2 = (uint8_t *)heap_caps_malloc(gfx.width() * LV_BUFFER_LINE, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(disp, (void *)buf1, (void *)buf2, gfx.width() * LV_BUFFER_LINE,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
#else
    static uint8_t *buf1 = (uint8_t *)malloc(gfx.width() * LV_BUFFER_LINE);
    lv_display_set_buffers(disp, (void *)buf1, NULL, gfx.width() * LV_BUFFER_LINE, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    const uint32_t buf_pixels = gfx.width() * LV_BUFFER_LINE;
    const uint32_t buf_bytes  = buf_pixels * 2;  // LVGL v9 uses bytes (2 bytes per pixel for RGB565)
    // Moderate safety margin, combined with chunked transmission strategy, suitable for small buffers
    const uint32_t extra_bytes = 1024 * 2;  // 1024 pixels safety margin (2KB)

    // Use aligned allocation to ensure memory boundary alignment, reducing SIMD access issues
    const size_t alignment    = 64;  // 64-byte alignment
    const size_t total_bytes  = buf_bytes + extra_bytes;
    const size_t aligned_size = (total_bytes + alignment - 1) & ~(alignment - 1);

    static uint8_t *buf1 = static_cast<uint8_t *>(aligned_alloc(alignment, aligned_size));
    static uint8_t *buf2 = static_cast<uint8_t *>(aligned_alloc(alignment, aligned_size));

    if (!buf1 || !buf2) {
        printf("ERROR: Failed to allocate aligned memory buffers\n");
        return;
    }

    // Clear buffers to avoid issues caused by random data
    memset(buf1, 0, aligned_size);
    memset(buf2, 0, aligned_size);

    lv_display_set_buffers(disp, (void *)buf1, (void *)buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    static lv_indev_t *indev = lv_indev_create();
    LV_ASSERT_MALLOC(indev);
    if (indev == NULL) {
        LV_LOG_ERROR("lv_indev_create failed");
        return;
    }
    lv_indev_set_driver_data(indev, &gfx);
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_read_cb);
    lv_indev_set_display(indev, disp);

#if defined(ARDUINO) && defined(ESP_PLATFORM)
    xGuiSemaphore                                     = xSemaphoreCreateMutex();
    const esp_timer_create_args_t periodic_timer_args = {.callback = &lvgl_tick_timer, .name = "lvgl_tick_timer"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000));
    xTaskCreate(lvgl_rtos_task, "lvgl_rtos_task", 4096, NULL, 1, NULL);
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    xGuiMutex = SDL_CreateMutex();
    SDL_AddTimer(10, lvgl_tick_timer, NULL);
    SDL_CreateThread(lvgl_sdl_thread, "lvgl_sdl_thread", NULL);
#endif
}
#endif

bool lvgl_port_lock(void) {
#if defined(ARDUINO) && defined(ESP_PLATFORM)
    return xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE ? true : false;
#elif !defined(ARDUINO) && (__has_include(<SDL2/SDL.h>) || __has_include(<SDL.h>))
    return SDL_LockMutex(xGuiMutex) == 0 ? true : false;
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
