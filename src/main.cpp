/**
 * @file    main.cpp
 * @brief   RobotEyes v5 阶段3 — 摇杆视线 + 舵机眉毛 + Force Return
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 *
 *  架构:
 *    InputTask (生产者) ──→ EventBus (FreeRTOS Queue) ──→ loop() (唯一消费者)
 *                                                              │
 *                                                         ServoTask (执行器)
 *
 *  角色: 3 类 (InputTask, ServoTask, main loop) — 不新增 Task
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "pin_config.h"
#include "event_bus.h"
#include "eye_renderer.h"
#include "input_task.h"
#include "servo_task.h"
#include "expressions.h"

/* ---- 双屏 ---- */
U8G2_SSD1306_128X64_NONAME_F_HW_I2C g_leftDisp(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C g_rightDisp(U8G2_R0,
                                                  PIN_SW_I2C_SCL,
                                                  PIN_SW_I2C_SDA,
                                                  U8X8_PIN_NONE);
bool g_leftReady  = false;
bool g_rightReady = false;

/* ---- 运行时状态 ---- */
static EyeConfig_t  g_eyeCfg;
static BlinkState_t g_blinkState;

/* ---- 帧节拍 ---- */
static uint32_t g_last_frame_ms = 0;

/* ---- 调试日志节拍 ---- */
static uint32_t g_last_beat_ms  = 0;

/* ================================================================
 *  esp32_fast_sw_i2c_gpio_cb() v5.1 — 标准开漏 I2C
 * ================================================================ */
extern "C" uint8_t esp32_fast_sw_i2c_gpio_cb(u8x8_t *u8x8, uint8_t msg,
                                              uint8_t arg_int, void *arg_ptr) {
    switch (msg) {

    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        pinMode(PIN_SW_I2C_SCL, INPUT_PULLUP);
        pinMode(PIN_SW_I2C_SDA, INPUT_PULLUP);
        break;

    case U8X8_MSG_DELAY_MILLI:
        delay(arg_int);
        break;

    case U8X8_MSG_DELAY_10MICRO:
        delayMicroseconds(arg_int * 10);
        break;

    case U8X8_MSG_DELAY_100NANO:
        delayMicroseconds((arg_int + 9) / 10);
        break;

    case U8X8_MSG_DELAY_NANO: {
        uint32_t ns = *(uint32_t *)arg_ptr;
        delayMicroseconds((ns + 999) / 1000);
        break;
    }

    case U8X8_MSG_GPIO_I2C_CLOCK:
        if (arg_int) {
            GPIO.enable_w1tc.val = (1UL << PIN_SW_I2C_SCL);
        } else {
            GPIO.out_w1tc.val    = (1UL << PIN_SW_I2C_SCL);
            GPIO.enable_w1ts.val = (1UL << PIN_SW_I2C_SCL);
        }
        break;

    case U8X8_MSG_GPIO_I2C_DATA:
        if (arg_int) {
            GPIO.enable_w1tc.val = (1UL << PIN_SW_I2C_SDA);
        } else {
            GPIO.out_w1tc.val    = (1UL << PIN_SW_I2C_SDA);
            GPIO.enable_w1ts.val = (1UL << PIN_SW_I2C_SDA);
        }
        break;

    default:
        return 0;
    }
    return 1;
}

/* ================================================================
 *  initLeftDisplay()
 * ================================================================ */
bool initLeftDisplay() {
    Serial.print(F("[LEFT]  I2C probe @0x"));
    Serial.print(I2C_ADDR_LEFT, HEX);
    Serial.print(F(" ... "));
    Wire.beginTransmission(I2C_ADDR_LEFT);
    if (Wire.endTransmission() != 0) {
        Serial.println(F("[FAIL] skip")); return false;
    }
    Serial.println(F("[OK]"));
    Serial.print(F("[LEFT]  begin() ... "));
    g_leftDisp.setI2CAddress(I2C_ADDR_LEFT << 1);
    if (!g_leftDisp.begin()) { Serial.println(F("[FAIL]")); return false; }
    Serial.println(F("[OK]"));
    g_leftDisp.setBusClock(400000);
    g_leftDisp.setPowerSave(0);
    return true;
}

/* ================================================================
 *  initRightDisplay()
 * ================================================================ */
bool initRightDisplay() {
    Serial.print(F("[RIGHT] SW-I2C begin() (GPIO"));
    Serial.print(PIN_SW_I2C_SDA);
    Serial.print(F("=SDA, GPIO"));
    Serial.print(PIN_SW_I2C_SCL);
    Serial.print(F("=SCL) ... "));

    u8x8_t *u8x8 = g_rightDisp.getU8x8();
    u8x8->gpio_and_delay_cb = esp32_fast_sw_i2c_gpio_cb;

    if (!g_rightDisp.begin()) { Serial.println(F("[FAIL]")); return false; }
    Serial.println(F("[OK]"));
    Serial.print(F("[RIGHT] open-drain GPIO callback [OK]"));
    Serial.println();
    g_rightDisp.setPowerSave(0);
    return true;
}

/* ================================================================
 *  screenBlackOut()
 * ================================================================ */
static void screenBlackOut(U8G2* disp) {
    disp->clearBuffer(); disp->sendBuffer();
}

/* ================================================================
 *  do_force_return() — 安全归位
 *
 *  清空 EventBus → 视线归中 → 舵机归中 → 眼型恢复正常
 * ================================================================ */
static void do_force_return(void) {
    Serial.println(F("[FORCE-RETURN] Triggered!"));
    event_bus_flush();
    eye_look_reset(&g_eyeCfg);
    servo_set_target(SERVO_CENTER_DEG, SERVO_CENTER_DEG);
    Serial.println(F("[FORCE-RETURN] Done."));
}

/* ================================================================
 *  process_event() — 消费单个事件
 * ================================================================ */
static void process_event(const EventMsg_t* msg) {
    switch (msg->type) {

    case EVT_JOYSTICK_MOVE: {
        /* 更新视线目标 */
        eye_set_look(&g_eyeCfg, msg->value_x, msg->value_y);

        /* 摇杆 Y 轴 → 眉毛舵机角度
         *  上推 (Y>0): 眉毛上扬 (角度增大) → 俏皮感
         *  下推 (Y<0): 眉毛下压 (角度减小)
         *  映射: Y=[-127,127] → 角度=[75,105] (±15度)
         */
        int8_t brow_angle = SERVO_CENTER_DEG + (msg->value_y * 15 / 127);
        servo_set_target(brow_angle, brow_angle);
        break;
    }

    case EVT_EXPR_SET: {
        /* 表情切换: 眼型 + 眉毛同步发起
         *  眼型参数走 lerp 过渡 (eye_expr_update)
         *  眉毛角度推给 ServoTask 非阻塞步进
         *  两者在同一时刻设置目标值, 视觉上同步变化
         */
        uint8_t idx = msg->value_x;
        if (idx < 8) {
            eye_set_expression(&g_eyeCfg, idx);

            /* 眉毛联动: 推目标角度给 ServoTask */
            servo_set_target(EXPRESSIONS[idx].brow_left,
                             EXPRESSIONS[idx].brow_right);

            Serial.print(F("[EXPR] Switched to "));
            Serial.println(EXPRESSIONS[idx].name);
        }
        break;
    }

    case EVT_BUTTON_SHORT:
        Serial.println(F("[BTN] Short press (reserved)"));
        break;

    case EVT_BUTTON_LONG:
        do_force_return();
        break;

    default:
        break;
    }
}

/* ================================================================
 *  render_frame() — 渲染一帧
 * ================================================================ */
static void render_frame(void) {
    uint32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0;

    if (g_leftReady && g_rightReady) {
        t0 = micros();
        g_leftDisp.clearBuffer();
        eye_render(&g_leftDisp, &g_eyeCfg);
        t1 = micros();
        g_leftDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y, EYE_TILE_W, EYE_TILE_H);
        t2 = micros();

        g_rightDisp.clearBuffer();
        eye_render(&g_rightDisp, &g_eyeCfg);
        t3 = micros();
        g_rightDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y, EYE_TILE_W, EYE_TILE_H);
        t4 = micros();
    }

    /* 每 2 秒打印一次性能日志 */
    uint32_t now = millis();
    if (now - g_last_beat_ms >= 2000) {
        g_last_beat_ms = now;
        Serial.print(F("[BEAT] "));
        if (g_leftReady && g_rightReady) {
            Serial.print(F("Lren=")); Serial.print(t1 - t0);
            Serial.print(F("us Lsend=")); Serial.print(t2 - t1);
            Serial.print(F("us | Rren=")); Serial.print(t3 - t2);
            Serial.print(F("us Rsend=")); Serial.print(t4 - t3);
            Serial.print(F("us | "));
        }
        Serial.print(F("blink=")); Serial.print(g_blinkState.phase);
        Serial.print(F(" lid=")); Serial.print(g_eyeCfg.lid, 2);
        Serial.print(F(" look=(")); Serial.print((int)g_eyeCfg.cur_look_x);
        Serial.print(F(",")); Serial.print((int)g_eyeCfg.cur_look_y);
        Serial.println(F(")"));
    }
}

/* ================================================================
 *  setup()
 * ================================================================ */
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  RobotEyes v5.6 — Expression Switch"));
    Serial.println(F("========================================"));
    Serial.println();

    /* OLED 初始化 */
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    g_leftReady  = initLeftDisplay();
    Serial.println();
    g_rightReady = initRightDisplay();
    Serial.println();
    if (g_leftReady)  { screenBlackOut(&g_leftDisp); }
    if (g_rightReady) { screenBlackOut(&g_rightDisp); }
    Serial.println();

    /* EventBus */
    event_bus_init();
    Serial.println(F("[EVENT] EventBus init OK"));

    /* Eye 渲染器 */
    eye_config_init(&g_eyeCfg, 64, 32);
    blink_state_init(&g_blinkState);
    Serial.println(F("[EYE]   Renderer init OK"));

    /* 舵机 */
    servo_task_init();

    /* InputTask 初始化 (校准在 task 内部) */
    input_task_init();

    /* 创建 FreeRTOS Tasks */
    xTaskCreate(input_task_run,  "InputTask",  2048, NULL, 2, &g_inputTaskHandle);
    xTaskCreate(servo_task_run,  "ServoTask",  1536, NULL, 2, &g_servoTaskHandle);

    Serial.println(F("[TASK]  InputTask + ServoTask created"));
    Serial.print(F("[SUMMARY] L="));
    Serial.print(g_leftReady ? F("OK") : F("FAIL"));
    Serial.print(F(" R="));
    Serial.print(g_rightReady ? F("OK") : F("FAIL"));
    Serial.println(F(" ready"));
    Serial.println(F("========================================"));
}

/* ================================================================
 *  loop() — 唯一消费者
 * ================================================================ */
void loop() {
    uint32_t now = millis();

    /* ---- 事件消费 ---- */
    EventMsg_t msg;
    while (event_bus_pop(&msg, 0)) {
        process_event(&msg);
    }

    /* ---- 帧节拍 (33ms ≈ 30fps) ---- */
    if (now - g_last_frame_ms < FRAME_INTERVAL_MS) {
        vTaskDelay(1);
        return;
    }
    g_last_frame_ms = now;

    /* ---- 状态更新 ---- */
    eye_look_update(&g_eyeCfg);
    eye_expr_update(&g_eyeCfg, now);
    blink_state_update(&g_blinkState, &g_eyeCfg, now);

    /* ---- 渲染 ---- */
    render_frame();
}

