/**
 * @file    event_bus.h
 * @brief   RobotEyes 事件总线 — FreeRTOS Queue 封装 (v6.1 修复容量)
 *
 *  单向搬运: InputTask (生产者) → EventBus → main loop (唯一消费者)
 *  参考 Pixel-Box-ESP32 (MIT) 的模式
 *
 *  v6.1 修复: value_x/value_y 从 int8_t 扩展为 int16_t
 *    - int8_t 最大 127, 无法存储毫秒级持有时长 → 长按判定瘫痪
 *    - int16_t 最大 32767, 可存储最长 32 秒的按键时长
 */

#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>

/* ================================================================
 *  事件类型
 * ================================================================ */
typedef enum {
    EVT_NONE = 0,          /* 空事件 */
    EVT_JOYSTICK_MOVE,     /* 摇杆移动 */
    EVT_BUTTON_SHORT,      /* SW 短按 */
    EVT_BUTTON_LONG,       /* SW 长按 (Force Return) */
    EVT_FORCE_RETURN,      /* 强制归位 */
    EVT_SERVO_MOVE,        /* 舵机移动 */
    EVT_EXPR_SET,          /* 表情切换: value_x=索引(0-7) */
    EVT_EXPR_RELEASE,      /* 按键释放: value_x=索引, value_y=持有时长ms */
} EventType_t;

/* ================================================================
 *  事件消息体 (紧凑布局)
 * ================================================================ */
#pragma pack(push, 1)
typedef struct {
    uint8_t     type;       /* EventType_t */
    int16_t     value_x;    /* int16_t: 视线X/表情索引 */
    int16_t     value_y;    /* int16_t: 视线Y/持有时长(ms) */
    uint8_t     _pad[11];   /* 保持结构体大小不变 */
} EventMsg_t;
#pragma pack(pop)

/* ================================================================
 *  API
 * ================================================================ */
void event_bus_init(void);
bool event_bus_push(const EventMsg_t* msg);
bool event_bus_pop(EventMsg_t* msg, uint32_t timeout_ms);
void event_bus_flush(void);

#endif
