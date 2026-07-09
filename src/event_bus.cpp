/**
 * @file    event_bus.cpp
 * @brief   RobotEyes 事件总线实现
 */

#include "event_bus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define EVENT_QUEUE_DEPTH 32

static QueueHandle_t g_queue = NULL;

/* ================================================================
 *  event_bus_init()
 * ================================================================ */
void event_bus_init(void) {
    g_queue = xQueueCreate(EVENT_QUEUE_DEPTH, sizeof(EventMsg_t));
}

/* ================================================================
 *  event_bus_push() — 非阻塞入队
 * ================================================================ */
bool event_bus_push(const EventMsg_t* msg) {
    if (!g_queue || !msg) return false;
    return xQueueSend(g_queue, msg, 0) == pdTRUE;
}

/* ================================================================
 *  event_bus_pop() — 阻塞等待
 * ================================================================ */
bool event_bus_pop(EventMsg_t* msg, uint32_t timeout_ms) {
    if (!g_queue || !msg) return false;
    return xQueueReceive(g_queue, msg, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

/* ================================================================
 *  event_bus_flush() — 清空队列 (Force Return 用)
 * ================================================================ */
void event_bus_flush(void) {
    if (!g_queue) return;
    xQueueReset(g_queue);
}
