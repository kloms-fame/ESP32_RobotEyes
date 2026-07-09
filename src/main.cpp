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

/* ---- 自动回弹计时器 (短按后 1.5s 回归 Normal) ---- */
static uint32_t g_revert_deadline_ms = 0;  /* 0=不自动回弹 */

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
        /* 1. 更新视线目标 (绝对运动) */
        eye_set_look(&g_eyeCfg, msg->value_x, msg->value_y);

        /* 2. 摇杆 Y 轴 → 眉毛相对微调 (在表情基础上叠加)
         *    上推 (Y>0): 眉毛在当前表情基础上额外上挑 → 俏皮灵动
         *    下推 (Y<0): 眉毛在当前表情基础上额外下压
         *    偏移量: Y / 8 → 最大 ±15° 相对偏移
         *
         *    镜像舵机: 左 +offset, 右 -offset
         *    (左舵机 angle↑=上扬, 右舵机 angle↓=上扬)
         */
        int8_t brow_offset = msg->value_y / 8;

        /* v6.2: 摇杆偏移 + 眉毛微动引擎叠加 */
        if (g_eyeCfg.active_expr < 8) {
            int8_t base_l = EXPRESSIONS[g_eyeCfg.active_expr].brow_left;
            int8_t base_r = EXPRESSIONS[g_eyeCfg.active_expr].brow_right;
            servo_set_target(base_l - brow_offset + g_eyeCfg.brow_offset_l,
                             base_r + brow_offset + g_eyeCfg.brow_offset_r);
        } else {
            servo_set_target(SERVO_CENTER_DEG - brow_offset + g_eyeCfg.brow_offset_l,
                             SERVO_CENTER_DEG + brow_offset + g_eyeCfg.brow_offset_r);
        }
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

            /* ---- 自动回弹计时器 ---- */
            if (idx == 0) {
                /* Normal: 不收束, 不设回弹 */
                g_revert_deadline_ms = 0;
            } else {
                /* 非 Normal: 1.5s 后自动回弹 Normal
                 * 每次按键都重置计时器, 防止多次点击状态混乱 */
                g_revert_deadline_ms = millis() + 1500;
            }

            Serial.print(F("[EXPR] Switched to "));
            Serial.print(EXPRESSIONS[idx].name);
            if (idx > 0) {
                Serial.print(F(" (revert in 1.5s)"));
            }
            Serial.println();
        }
        break;
    }

    case EVT_EXPR_RELEASE: {
        /* 按键释放: 根据持有时长决定是否锁定
         *   msg->value_y = 持有时长 (ms)
         *   自动回弹计时器已在 EVT_EXPR_SET 中设置,
         *   此处仅处理长按锁定 (清零计时器实现永久锁死)
         */
        uint16_t held_ms = (uint16_t)msg->value_y;
        if (held_ms >= ADC_LONG_PRESS_MS) {
            /* 长按 ≥500ms: 清零计时器 → 永久锁定当前表情 */
            g_revert_deadline_ms = 0;
            Serial.print(F("[EXPR] Long press ("));
            Serial.print(held_ms);
            Serial.println(F("ms) — expression locked"));
        }
        /* 短按: 不干预, 计时器已在 EVT_EXPR_SET 中设好 */
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
        eye_render(&g_leftDisp, &g_eyeCfg, true);   /* 左眼 */
        t1 = micros();
        g_leftDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y, EYE_TILE_W, EYE_TILE_H);
        t2 = micros();

        g_rightDisp.clearBuffer();
        eye_render(&g_rightDisp, &g_eyeCfg, false);  /* 右眼 */
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
    Serial.println(F("  RobotEyes v6.2 — Brow Engine + Happy/Shock/Tear/Sawtooth"));
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

    /* ---- 自动回弹: 短按后 1.5s 回归 Normal ---- */
    if (g_revert_deadline_ms > 0 && now >= g_revert_deadline_ms) {
        g_revert_deadline_ms = 0;
        if (g_eyeCfg.active_expr != 0) {
            eye_set_expression(&g_eyeCfg, 0);  /* 回归 Normal */
            servo_set_target(SYM_L(0), SYM_R(0));
            Serial.println(F("[EXPR] Auto-revert → Normal"));
        }
    }

    /* ---- 状态更新 ---- */
    eye_look_update(&g_eyeCfg);
    eye_expr_update(&g_eyeCfg, now);
    blink_state_update(&g_blinkState, &g_eyeCfg, now);

    /* ---- 渲染 ---- */
    render_frame();
}

