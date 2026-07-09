/**
 * @file    servo_task.h
 * @brief   RobotEyes 舵机控制 Task — 平滑非阻塞插值
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

void servo_task_init(void);
void servo_set_target(int8_t left_deg, int8_t right_deg);

/* 获取当前舵机目标角度 */
void servo_get_target(int8_t* left_deg, int8_t* right_deg);

/* 相对当前目标角度进行偏移 (用于摇杆微调) */
void servo_add_relative(int8_t left_offset, int8_t right_offset);
void servo_task_run(void* pvParameters);

#endif
