/**
 * @file    input_task.cpp
 * @brief   RobotEyes 输入采集 Task 实现
 *
 *  摇杆去毛刺算法:
 *    1. 启动时自动校准中心点 (多次采样取平均)
 *    2. 每次采集: 10次 ADC → 排序 → 丢弃2个极值 → 平均8个
 *    3. 归一化到 [-127, 127], 加死区
 *    4. 稳定性确认: 连续3次读数一致才推送事件
 *
 *  SW 按键: 委托 force_return 模块处理 (GPIO中断 + 状态机)
 */

#include "input_task.h"
#include "event_bus.h"
#include "force_return.h"
#include "pin_config.h"
#include <Arduino.h>
#include <algorithm>

TaskHandle_t g_inputTaskHandle = NULL;

/* ---- 校准值 ---- */
static int16_t g_joy_center_x = 2048;
static int16_t g_joy_center_y = 2048;

/* ---- 上次推送的归一化值 ---- */
static int8_t g_last_sent_x = 0;
static int8_t g_last_sent_y = 0;

/* ---- 待确认值 + 稳定性计数器 ---- */
static int8_t  g_pending_x = 0;
static int8_t  g_pending_y = 0;
static uint8_t g_stable_cnt = 0;

/* ---- 稳定阈值 (归一化单位) ---- */
#define STABLE_THRESHOLD  5

/* ================================================================
 *  辅助: 快速排序 (冒泡, 10元素足够快)
 * ================================================================ */
static void sort_samples(int16_t* arr, uint8_t n) {
    for (uint8_t i = 0; i < n - 1; i++) {
        for (uint8_t j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int16_t tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

/* ================================================================
 *  辅助: 去极值平均
 * ================================================================ */
static int16_t filtered_average(int16_t* samples, uint8_t n, uint8_t discard) {
    sort_samples(samples, n);
    int32_t sum = 0;
    uint8_t count = n - discard * 2;
    for (uint8_t i = discard; i < n - discard; i++) {
        sum += samples[i];
    }
    return (int16_t)(sum / count);
}

/* ================================================================
 *  calibrate_center() — 启动时自动校准摇杆中心
 *
 *  采样 200 次 (约2秒), 取平均作为 center_x/center_y
 * ================================================================ */
static void calibrate_center(void) {
    Serial.println(F("[INPUT] Calibrating joystick center (2s)..."));
    int32_t sum_x = 0, sum_y = 0;
    const int cal_samples = 200;

    for (int i = 0; i < cal_samples; i++) {
        sum_x += analogRead(PIN_JOY_X);
        sum_y += analogRead(PIN_JOY_Y);
        delay(10);
    }

    g_joy_center_x = (int16_t)(sum_x / cal_samples);
    g_joy_center_y = (int16_t)(sum_y / cal_samples);

    Serial.print(F("[INPUT] Calibrated center: X="));
    Serial.print(g_joy_center_x);
    Serial.print(F(" Y="));
    Serial.println(g_joy_center_y);
}

/* ================================================================
 *  normalize_adc() — ADC原始值 → 归一化 [-127, 127]
 * ================================================================ */
static int8_t normalize_adc(int16_t raw, int16_t center) {
    int32_t diff = (int32_t)raw - (int32_t)center;
    int32_t norm = diff * 127 / INPUT_ADC_RANGE;
    if (norm > 127)  norm = 127;
    if (norm < -127) norm = -127;
    return (int8_t)norm;
}

/* ================================================================
 *  apply_deadzone() — 死区归零
 * ================================================================ */
static int8_t apply_deadzone(int8_t val) {
    if (val >= -INPUT_DEADZONE && val <= INPUT_DEADZONE) return 0;
    return val;
}

/* ================================================================
 *  input_task_init() — 初始化 ADC 引脚 + 自动校准
 * ================================================================ */
void input_task_init(void) {
    analogReadResolution(12);
    pinMode(PIN_JOY_X, INPUT);
    pinMode(PIN_JOY_Y, INPUT);

    /* 自动校准中心 */
    calibrate_center();

    /* 初始化 Force Return (GPIO中断) */
    force_return_init();

    g_last_sent_x = 0;
    g_last_sent_y = 0;
    g_pending_x   = 0;
    g_pending_y   = 0;
    g_stable_cnt  = 0;

    Serial.println(F("[INPUT] Init done."));
}

/* ================================================================
 *  input_task_run() — FreeRTOS Task 主循环
 *
 *  周期: ~10ms (vTaskDelay(10))
 *  流程:
 *    1. 采集 10 次 ADC → 去极值平均 → 归一化 → 死区
 *    2. 与 pending 值比较 → 稳定性计数
 *    3. 连续稳定 → 推送 EVT_JOYSTICK_MOVE
 *    4. 调用 force_return_poll() 处理 SW 按键
 * ================================================================ */
void input_task_run(void* pvParameters) {
    (void)pvParameters;
    int16_t x_samples[INPUT_RAW_SAMPLES];
    int16_t y_samples[INPUT_RAW_SAMPLES];

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        /* ---- 1. 采集 ADC ---- */
        for (uint8_t i = 0; i < INPUT_RAW_SAMPLES; i++) {
            x_samples[i] = analogRead(PIN_JOY_X);
            y_samples[i] = analogRead(PIN_JOY_Y);
        }

        /* ---- 2. 去极值 + 平均 ---- */
        int16_t raw_x = filtered_average(x_samples, INPUT_RAW_SAMPLES, INPUT_DISCARD_EXTREME);
        int16_t raw_y = filtered_average(y_samples, INPUT_RAW_SAMPLES, INPUT_DISCARD_EXTREME);

        /* ---- 3. 归一化 + 死区 ---- */
        int8_t norm_x = apply_deadzone(normalize_adc(raw_x, g_joy_center_x));
        int8_t norm_y = apply_deadzone(normalize_adc(raw_y, g_joy_center_y));

        /* ---- 4. 稳定性确认 ---- */
        if (abs(norm_x - g_pending_x) <= STABLE_THRESHOLD &&
            abs(norm_y - g_pending_y) <= STABLE_THRESHOLD) {
            g_stable_cnt++;
        } else {
            g_pending_x  = norm_x;
            g_pending_y  = norm_y;
            g_stable_cnt = 0;
        }

        /* 连续稳定 → 推送 */
        if (g_stable_cnt >= INPUT_STABLE_COUNT) {
            if (g_pending_x != g_last_sent_x || g_pending_y != g_last_sent_y) {
                EventMsg_t msg;
                msg.type    = EVT_JOYSTICK_MOVE;
                msg.value_x = g_pending_x;
                msg.value_y = g_pending_y;
                event_bus_push(&msg);

                g_last_sent_x = g_pending_x;
                g_last_sent_y = g_pending_y;
            }
            g_stable_cnt = 0;  /* 重置, 等待下一轮变化 */
        }

        /* ---- 5. SW 按键检测 (Force Return) ---- */
        force_return_poll(millis());

        /* ---- 6. 周期延迟 ---- */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}
