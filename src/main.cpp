/**
 * @file    main.cpp
 * @brief   RobotEyes v5 参数化几何眼型引擎 (输入模式释放)
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "pin_config.h"
#include "eye_renderer.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C g_leftDisp(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C g_rightDisp(U8G2_R0,
                                                  PIN_SW_I2C_SCL,
                                                  PIN_SW_I2C_SDA,
                                                  U8X8_PIN_NONE);
bool g_leftReady  = false;
bool g_rightReady = false;
static EyeConfig_t  g_eyeCfg;
static BlinkState_t g_blinkState;
static MicroState_t g_microState;

/* ================================================================
 *  esp32_fast_sw_i2c_gpio_cb() v5.1 — 标准开漏 I2C
 *
 *  拉低(arg=0): GPIO.out_w1tc + GPIO.enable_w1ts (输出, 驱动低)
 *  释放(arg=1): GPIO.enable_w1tc (输入, 高阻, 上拉电阻拉到高)
 *
 *  关键: 释放时必须切输入模式，让从机也能拉低 SDA (ACK/NACK)
 * ================================================================ */
extern "C" uint8_t esp32_fast_sw_i2c_gpio_cb(u8x8_t *u8x8, uint8_t msg,
                                              uint8_t arg_int, void *arg_ptr) {
    switch (msg) {

    case U8X8_MSG_GPIO_AND_DELAY_INIT: {  /* 40 */
        /* 初始态: 输入 + 内部上拉, 模拟 idle 高电平 */
        pinMode(PIN_SW_I2C_SCL, INPUT_PULLUP);
        pinMode(PIN_SW_I2C_SDA, INPUT_PULLUP);
        break;
    }

    case U8X8_MSG_DELAY_MILLI:          /* 41 */
        delay(arg_int);
        break;

    case U8X8_MSG_DELAY_10MICRO:        /* 42 */
        delayMicroseconds(arg_int * 10);
        break;

    case U8X8_MSG_DELAY_100NANO:        /* 43 */
        delayMicroseconds((arg_int + 9) / 10);
        break;

    case U8X8_MSG_DELAY_NANO: {         /* 44 */
        uint32_t ns = *(uint32_t *)arg_ptr;
        delayMicroseconds((ns + 999) / 1000);
        break;
    }

    case U8X8_MSG_GPIO_I2C_CLOCK: {     /* 76 */
        if (arg_int) {
            /* 释放 SCL: 切输入, 高阻 → 上拉拉到高 */
            GPIO.enable_w1tc.val = (1UL << PIN_SW_I2C_SCL);
        } else {
            /* 拉低 SCL: 写0 + 切输出 */
            GPIO.out_w1tc.val    = (1UL << PIN_SW_I2C_SCL);
            GPIO.enable_w1ts.val = (1UL << PIN_SW_I2C_SCL);
        }
        break;
    }

    case U8X8_MSG_GPIO_I2C_DATA: {      /* 77 */
        if (arg_int) {
            /* 释放 SDA: 切输入, 让从机也能拉低 */
            GPIO.enable_w1tc.val = (1UL << PIN_SW_I2C_SDA);
        } else {
            /* 拉低 SDA */
            GPIO.out_w1tc.val    = (1UL << PIN_SW_I2C_SDA);
            GPIO.enable_w1ts.val = (1UL << PIN_SW_I2C_SDA);
        }
        break;
    }

    default:
        return 0;
    }
    return 1;
}

/* ================================================================
 *  初始化
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

bool initRightDisplay() {
    Serial.print(F("[RIGHT] SW-I2C begin() (GPIO"));
    Serial.print(PIN_SW_I2C_SDA);
    Serial.print(F("=SDA, GPIO"));
    Serial.print(PIN_SW_I2C_SCL);
    Serial.print(F("=SCL) ... "));

    /* 替换回调 (begin 之前) */
    u8x8_t *u8x8 = g_rightDisp.getU8x8();
    u8x8->gpio_and_delay_cb = esp32_fast_sw_i2c_gpio_cb;

    if (!g_rightDisp.begin()) { Serial.println(F("[FAIL]")); return false; }
    Serial.println(F("[OK]"));
    Serial.print(F("[RIGHT] open-drain GPIO callback [OK]"));
    Serial.println();
    g_rightDisp.setPowerSave(0);
    return true;
}

static void screenBlackOut(U8G2* disp) {
    disp->clearBuffer(); disp->sendBuffer();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  RobotEyes v5 — Parameterized Eye Geometry"));
    Serial.println(F("========================================"));
    Serial.println();
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    g_leftReady  = initLeftDisplay();
    Serial.println();
    g_rightReady = initRightDisplay();
    Serial.println();
    if (g_leftReady)  { Serial.print(F("[BLANK] Left  [OK]")); screenBlackOut(&g_leftDisp); Serial.println(); }
    if (g_rightReady) { Serial.print(F("[BLANK] Right [OK]")); screenBlackOut(&g_rightDisp); Serial.println(); }
    Serial.println();
    eye_config_init(&g_eyeCfg, 64, 32);
    blink_state_init(&g_blinkState);
    micro_state_init(&g_microState);
    Serial.print(F("[SUMMARY] L="));
    Serial.print(g_leftReady ? F("OK") : F("FAIL"));
    Serial.print(F(" R="));
    Serial.print(g_rightReady ? F("OK") : F("FAIL"));
    Serial.println(F(" GPIO=open-drain"));
    Serial.println(F("========================================"));
}

void loop() {
    static uint32_t lf=0, lb=0, lsu=0, rsu=0, lru=0, rru=0;
    uint32_t n = millis();
    if (n - lf < FRAME_INTERVAL_MS) return;
    lf = n;
    blink_state_update(&g_blinkState, &g_eyeCfg, n);
    micro_state_update(&g_microState, &g_eyeCfg, n);
    eye_morph_update(&g_eyeCfg, n);
    if (g_leftReady && g_rightReady) {
        uint32_t t0=micros(), t1, t2, t3, t4;
        g_leftDisp.clearBuffer(); eye_render(&g_leftDisp, &g_eyeCfg); t1=micros();
        g_leftDisp.updateDisplayArea(EYE_TILE_X,EYE_TILE_Y,EYE_TILE_W,EYE_TILE_H); t2=micros();
        g_rightDisp.clearBuffer(); eye_render(&g_rightDisp, &g_eyeCfg); t3=micros();
        g_rightDisp.updateDisplayArea(EYE_TILE_X,EYE_TILE_Y,EYE_TILE_W,EYE_TILE_H); t4=micros();
        lru=t1-t0; lsu=t2-t1; rru=t3-t2; rsu=t4-t3;
    }
    if (n - lb >= 2000) {
        lb = n;
        Serial.print(F("[BEAT] "));
        if (g_leftReady&&g_rightReady) {
            Serial.print(F("Lren=")); Serial.print(lru);
            Serial.print(F("us Lsend=")); Serial.print(lsu);
            Serial.print(F("us | Rren=")); Serial.print(rru);
            Serial.print(F("us Rsend=")); Serial.print(rsu);
            Serial.print(F("us | "));
        }
        Serial.print(F("blink=")); Serial.print(g_blinkState.phase);
        Serial.print(F(" lid=")); Serial.println(g_eyeCfg.lid, 2);
    }
}

