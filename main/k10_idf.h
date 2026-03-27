#ifndef K10_IDF_H
#define K10_IDF_H

#include <stdint.h>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t k10_millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static inline uint64_t k10_micros(void) {
    return (uint64_t)esp_timer_get_time();
}

static inline void k10_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void k10_start_app_task(void);

#ifdef __cplusplus
}
#endif

#endif