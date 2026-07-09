/**
 * @file    force_return.cpp
 * @brief   RobotEyes 安全归位机制实现
 *
 *  状态图说明:
 *    IDLE:         等待按键按下
 *    PRESSED:      按键按住中, 累计时间
 *    LONG_TRIGGERED: 已触发长按, 等待释放 (防止重复触发)
 *
 *  互斥规则:
 *    - 长按触发后, 不再产生短按事件
 *    - 释放后回到 IDLE 才会重置一切
 *
 *  GPIO 中断: 上升/下降沿均触发, 通过 digitalRead() 判断当前电平
 */

#include "force_return.h"
#include "event_bus.h"
#include "pin_config.h"
#include <Arduino.h>

/* ---- 状态 ---- */
typedef enum {
    FR_IDLE = 0,
    FR_PRESSED,
    FR_LONG_TRIGGERED
} FRState_t;

static FRState_t  g_state     = FR_IDLE;
static uint32_t   g_press_ms  = 0;
static volatile bool g_isr_fired = false;

/* ---- GPIO 中断回调 ---- */
static void IRAM_ATTR fr_isr(void) {
    g_isr_fired = true;
}

/* ================================================================
 *  force_return_init()
 * ================================================================ */
void force_return_init(void) {
    pinMode(PIN_JOY_SW, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_JOY_SW), fr_isr, CHANGE);
    g_state    = FR_IDLE;
    g_press_ms = 0;
}

/* ================================================================
 *  force_return_poll() — 每周期调用 (由 InputTask 驱动 ~30Hz)
 * ================================================================ */
void force_return_poll(uint32_t now_ms) {
    if (!g_isr_fired) return;
    g_isr_fired = false;

    bool pressed = (digitalRead(PIN_JOY_SW) == LOW);  /* 低电平有效 */

    switch (g_state) {

    case FR_IDLE:
        if (pressed) {
            g_state    = FR_PRESSED;
            g_press_ms = now_ms;
        }
        break;

    case FR_PRESSED:
        if (!pressed) {
            /* 短按: 释放 (< 2s) */
            EventMsg_t msg = { EVT_BUTTON_SHORT, 0, 0, {0} };
            event_bus_push(&msg);
            g_state = FR_IDLE;
        } else if (now_ms - g_press_ms >= FORCE_RETURN_HOLD_MS) {
            /* 长按触发 */
            EventMsg_t msg = { EVT_BUTTON_LONG, 0, 0, {0} };
            event_bus_push(&msg);
            g_state = FR_LONG_TRIGGERED;
        }
        break;

    case FR_LONG_TRIGGERED:
        if (!pressed) {
            /* 释放, 回到空闲 */
            g_state = FR_IDLE;
        }
        /* 按住中: 不再产生任何事件, 防止重复触发 */
        break;
    }
}
