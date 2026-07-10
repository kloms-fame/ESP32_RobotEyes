/**
 * @file    servo_task.h
 * @brief   RobotEyes 舵机控制 Task v10 — 平滑非阻塞插值 + 抖动通道 (int16_t 全链路)
 */

#ifndef SERVO_TASK_H
#define SERVO_TASK_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define SERVO_UPDATE_MS     20
#define SERVO_STEP_DEG       2
#define SERVO_CENTER_DEG    90
#define SERVO_MIN_DEG       45
#define SERVO_MAX_DEG      135

extern TaskHandle_t g_servoTaskHandle;

/* v10: 所有角度参数从 int8_t 升级为 int16_t, 防溢出 */
void servo_task_init(void);
void servo_set_target(int16_t left_deg, int16_t right_deg);
void servo_get_target(int16_t* left_deg, int16_t* right_deg);
void servo_add_relative(int16_t left_offset, int16_t right_offset);
void servo_set_jitter(int16_t left_jitter, int16_t right_jitter);
void servo_task_run(void* pvParameters);

#endif
