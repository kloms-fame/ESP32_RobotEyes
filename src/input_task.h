/**
 * @file    input_task.h
 * @brief   RobotEyes 输入采集 Task — 摇杆 + SW 按键
 */

#ifndef INPUT_TASK_H
#define INPUT_TASK_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* ---- 调参宏 ---- */
#define INPUT_RAW_SAMPLES     10
#define INPUT_DISCARD_EXTREME  2
#define INPUT_STABLE_COUNT     3
#define INPUT_DEADZONE         6
#define INPUT_ADC_RANGE      1550

/* ---- 任务句柄 ---- */
extern TaskHandle_t g_inputTaskHandle;

void input_task_init(void);
void input_task_run(void* pvParameters);

#endif
