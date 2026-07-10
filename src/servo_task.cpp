/**
 * @file    servo_task.cpp
 * @brief   RobotEyes 舵机控制 Task 实现 v9.0 — 非阻塞步进 + 独立抖动通道
 *
 *  使用 ESP32Servo 库 (LGPL-2.1)
 *  非阻塞步进: 每 20ms 向目标移动 SERVO_STEP_DEG 度
 *  抖动通道: servo_set_jitter() 直接叠加到 write() 输出, 绕过缓动
 */

#include "servo_task.h"
#include "pin_config.h"
#include <Arduino.h>
#include <ESP32Servo.h>

TaskHandle_t g_servoTaskHandle = NULL;

static Servo g_servo_left;
static Servo g_servo_right;

static volatile int8_t g_target_left  = SERVO_CENTER_DEG;
static volatile int8_t g_target_right = SERVO_CENTER_DEG;

static volatile int8_t g_jitter_left  = 0;
static volatile int8_t g_jitter_right = 0;

static int8_t g_current_left  = SERVO_CENTER_DEG;
static int8_t g_current_right = SERVO_CENTER_DEG;

/* ================================================================
 *  servo_task_init()
 * ================================================================ */
void servo_task_init(void) {
    ESP32PWM::allocateTimer(0);
    g_servo_left.setPeriodHertz(50);
    g_servo_right.setPeriodHertz(50);

    g_servo_left.attach(PIN_SERVO_LEFT, 500, 2500);
    g_servo_right.attach(PIN_SERVO_RIGHT, 500, 2500);

    g_servo_left.write(SERVO_CENTER_DEG);
    g_servo_right.write(SERVO_CENTER_DEG);

    g_current_left  = SERVO_CENTER_DEG;
    g_current_right = SERVO_CENTER_DEG;
    g_target_left   = SERVO_CENTER_DEG;
    g_target_right  = SERVO_CENTER_DEG;
    g_jitter_left   = 0;
    g_jitter_right  = 0;

    Serial.println(F("[SERVO] Init done. Center=90 deg, jitter=0"));
}

/* ================================================================
 *  servo_set_target() — 线程安全设置目标角度
 * ================================================================ */
void servo_set_target(int8_t left_deg, int8_t right_deg) {
    /* 钳位 */
    if (left_deg  < SERVO_MIN_DEG) left_deg  = SERVO_MIN_DEG;
    if (left_deg  > SERVO_MAX_DEG) left_deg  = SERVO_MAX_DEG;
    if (right_deg < SERVO_MIN_DEG) right_deg = SERVO_MIN_DEG;
    if (right_deg > SERVO_MAX_DEG) right_deg = SERVO_MAX_DEG;

    g_target_left  = left_deg;
    g_target_right = right_deg;
}

/* ================================================================
 *  servo_get_target() — 读取当前目标角度
 * ================================================================ */
void servo_get_target(int8_t* left_deg, int8_t* right_deg) {
    *left_deg  = g_target_left;
    *right_deg = g_target_right;
}

/* ================================================================
 *  servo_add_relative() — 相对当前目标角度偏移
 * ================================================================ */
void servo_add_relative(int8_t left_offset, int8_t right_offset) {
    servo_set_target(g_target_left + left_offset,
                     g_target_right + right_offset);
}

/* ================================================================
 *  servo_set_jitter() — v9.0: 直接注频抖动
 *
 *  调用后下一帧 servo_task_run 直接叠加到 write() 输出,
 *  不经过 SERVO_STEP_DEG 缓动限速。
 *  传 (0, 0) 关闭抖动。
 * ================================================================ */
void servo_set_jitter(int8_t left_jitter, int8_t right_jitter) {
    g_jitter_left  = left_jitter;
    g_jitter_right = right_jitter;
}

/* ================================================================
 *  servo_task_run() — FreeRTOS Task 主循环
 *
 *  每 20ms 执行一次:
 *    1. 按 SERVO_STEP_DEG 步进追目标
 *    2. 叠加 g_jitter_left/right (绕过缓动)
 *    3. 钳位后 write() 到舵机
 * ================================================================ */
void servo_task_run(void* pvParameters) {
    (void)pvParameters;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        int8_t cur_l  = g_current_left;
        int8_t cur_r  = g_current_right;
        int8_t tgt_l  = g_target_left;
        int8_t tgt_r  = g_target_right;

        /* 左舵机步进 */
        if (cur_l < tgt_l) {
            cur_l += SERVO_STEP_DEG;
            if (cur_l > tgt_l) cur_l = tgt_l;
        } else if (cur_l > tgt_l) {
            cur_l -= SERVO_STEP_DEG;
            if (cur_l < tgt_l) cur_l = tgt_l;
        }

        /* 右舵机步进 */
        if (cur_r < tgt_r) {
            cur_r += SERVO_STEP_DEG;
            if (cur_r > tgt_r) cur_r = tgt_r;
        } else if (cur_r > tgt_r) {
            cur_r -= SERVO_STEP_DEG;
            if (cur_r < tgt_r) cur_r = tgt_r;
        }

        /* v9.0: 叠加抖动 (绕过缓动, 直接加到输出) */
        int8_t out_l = cur_l + g_jitter_left;
        int8_t out_r = cur_r + g_jitter_right;

        /* 钳位保护硬件 */
        if (out_l < SERVO_MIN_DEG) out_l = SERVO_MIN_DEG;
        if (out_l > SERVO_MAX_DEG) out_l = SERVO_MAX_DEG;
        if (out_r < SERVO_MIN_DEG) out_r = SERVO_MIN_DEG;
        if (out_r > SERVO_MAX_DEG) out_r = SERVO_MAX_DEG;

        /* 写入舵机 */
        g_servo_left.write(out_l);
        g_servo_right.write(out_r);

        g_current_left  = cur_l;
        g_current_right = cur_r;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SERVO_UPDATE_MS));
    }
}
