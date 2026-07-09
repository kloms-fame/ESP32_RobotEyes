/**
 * @file    main.cpp
 * @brief   RobotEyes 阶段2 v4 — updateDisplayArea 局部刷新
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 *
 * 核心改进:
 *   - 用 updateDisplayArea(tx,ty,tw,th) 替代 sendBuffer()
 *   - 只发送眼球包围盒对应的 6×6=36 tile (288 bytes), 而非全屏 1024 bytes
 *   - 首次初始化时 clearBuffer+sendBuffer 做全屏黑色打底
 *   - 诊断日志分别显示 I2C 传输耗时和渲染耗时
 *
 *  预期: 右眼 SW I2C 从 ~450ms → ~120ms (288/1024=28% 数据量)
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "pin_config.h"
#include "eye_renderer.h"

/* ================================================================
 *  全局显示对象 — _F 全缓冲
 *  右眼改回 400kHz (800kHz 已被实测证明无效)
 * ================================================================ */

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
 *  initLeftDisplay()
 * ================================================================ */
bool initLeftDisplay() {
    Serial.print(F("[LEFT]  I2C probe @0x"));
    Serial.print(I2C_ADDR_LEFT, HEX);
    Serial.print(F(" ... "));
    Wire.beginTransmission(I2C_ADDR_LEFT);
    uint8_t error = Wire.endTransmission();
    if (error != 0) {
        Serial.print(F("[FAIL] err="));
        Serial.print(error);
        Serial.println(F(", skip"));
        return false;
    }
    Serial.println(F("[OK]"));
    Serial.print(F("[LEFT]  U8g2 begin() ... "));
    g_leftDisp.setI2CAddress(I2C_ADDR_LEFT << 1);
    if (!g_leftDisp.begin()) { Serial.println(F("[FAIL]")); return false; }
    Serial.println(F("[OK]"));
    Serial.print(F("[LEFT]  setBusClock(400000) ... "));
    g_leftDisp.setBusClock(400000);
    Serial.println(F("[OK]"));
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
    if (!g_rightDisp.begin()) { Serial.println(F("[FAIL]")); return false; }
    Serial.println(F("[OK]"));
    Serial.print(F("[RIGHT] setBusClock(400000) ... "));
    g_rightDisp.setBusClock(400000);
    Serial.println(F("[OK]"));
    g_rightDisp.setPowerSave(0);
    return true;
}

/* ================================================================
 *  screenBlackOut() — 全屏发送黑色帧, 一次性打底
 *
 *  之后每一帧只 updateDisplayArea 眼球包围盒的 tile,
 *  包围盒外区域永远是黑底, 不需要再传。
 * ================================================================ */
static void screenBlackOut(U8G2* disp) {
    disp->clearBuffer();
    disp->sendBuffer();  /* 仅此一次全屏发送 */
}

/* ================================================================
 *  setup()
 * ================================================================ */
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  RobotEyes Phase 2 v4 — Partial Refresh"));
    Serial.println(F("========================================"));
    Serial.println();

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Serial.print(F("[WIRE] HW I2C (SDA=GPIO"));
    Serial.print(PIN_I2C_SDA);
    Serial.print(F(", SCL=GPIO"));
    Serial.print(PIN_I2C_SCL);
    Serial.println(F(") [OK]"));
    Serial.println();

    g_leftReady  = initLeftDisplay();
    Serial.println();
    g_rightReady = initRightDisplay();
    Serial.println();

    /* 全屏黑色打底 (一次性) */
    if (g_leftReady) {
        Serial.print(F("[BLANK] Left  full-screen ... "));
        screenBlackOut(&g_leftDisp);
        Serial.println(F("[OK]"));
    }
    if (g_rightReady) {
        Serial.print(F("[BLANK] Right full-screen ... "));
        screenBlackOut(&g_rightDisp);
        Serial.println(F("[OK]"));
    }
    Serial.println();

    eye_config_init(&g_eyeCfg, 64, 32);
    blink_state_init(&g_blinkState);
    micro_state_init(&g_microState);

    Serial.print(F("[ANIM] rx="));
    Serial.print(EYE_RX);
    Serial.print(F(" ry="));
    Serial.print(EYE_RY);
    Serial.print(F(" pupil="));
    Serial.print(PUPIL_RADIUS);
    Serial.print(F(" +shine"));
    Serial.println();

    Serial.print(F("[TILE] tx="));
    Serial.print(EYE_TILE_X);
    Serial.print(F(" ty="));
    Serial.print(EYE_TILE_Y);
    Serial.print(F(" tw="));
    Serial.print(EYE_TILE_W);
    Serial.print(F(" th="));
    Serial.print(EYE_TILE_H);
    Serial.print(F(" ("));
    Serial.print(EYE_TILE_W * EYE_TILE_H * 8);
    Serial.println(F(" bytes/frame)"));
    Serial.println();

    Serial.print(F("[SUMMARY] Left="));
    Serial.print(g_leftReady ? F("OK") : F("FAIL"));
    Serial.print(F("  Right="));
    Serial.print(g_rightReady ? F("OK") : F("FAIL"));
    Serial.println();
    Serial.println(F("========================================"));
}

/* ================================================================
 *  loop() — 30fps + 局部刷新 + 诊断计时
 *
 *  诊断输出:
 *    Lsend=左屏I2C耗时  Rsend=右屏I2C耗时  Lrender=左屏渲染耗时
 *    (渲染=clearBuffer+eye_render  I2C=updateDisplayArea)
 * ================================================================ */
void loop() {
    static uint32_t lastFrameMs  = 0;
    static uint32_t lastBeatMs   = 0;
    static uint32_t lastLSendUs  = 0;
    static uint32_t lastRSendUs  = 0;
    static uint32_t lastLRenderUs= 0;
    static uint32_t lastRRenderUs= 0;

    uint32_t now = millis();
    if (now - lastFrameMs < FRAME_INTERVAL_MS) return;
    lastFrameMs = now;

    /* Step 1: 状态更新 */
    blink_state_update(&g_blinkState, &g_eyeCfg, now);
    micro_state_update(&g_microState, &g_eyeCfg, now);

    /* Step 2: 渲染 + 局部 I2C 发送 (带 micros 计时) */
    if (g_leftReady && g_rightReady) {
        uint32_t t0, t1, t2, t3, t4;

        /* ---- 左屏 ---- */
        t0 = micros();
        g_leftDisp.clearBuffer();
        eye_render(&g_leftDisp, &g_eyeCfg);
        t1 = micros();
        g_leftDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y,
                                      EYE_TILE_W, EYE_TILE_H);
        t2 = micros();

        /* ---- 右屏 ---- */
        g_rightDisp.clearBuffer();
        eye_render(&g_rightDisp, &g_eyeCfg);
        t3 = micros();
        g_rightDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y,
                                       EYE_TILE_W, EYE_TILE_H);
        t4 = micros();

        lastLRenderUs = t1 - t0;
        lastLSendUs   = t2 - t1;
        lastRRenderUs = t3 - t2;
        lastRSendUs   = t4 - t3;
    } else {
        if (g_leftReady) {
            g_leftDisp.clearBuffer();
            eye_render(&g_leftDisp, &g_eyeCfg);
            g_leftDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y,
                                          EYE_TILE_W, EYE_TILE_H);
        }
        if (g_rightReady) {
            g_rightDisp.clearBuffer();
            eye_render(&g_rightDisp, &g_eyeCfg);
            g_rightDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y,
                                           EYE_TILE_W, EYE_TILE_H);
        }
    }

    /* 心跳 + 诊断 */
    if (now - lastBeatMs >= 2000) {
        lastBeatMs = now;
        Serial.print(F("[BEAT] "));
        if (g_leftReady && g_rightReady) {
            Serial.print(F("Lren="));
            Serial.print(lastLRenderUs);
            Serial.print(F("us Lsend="));
            Serial.print(lastLSendUs);
            Serial.print(F("us | Rren="));
            Serial.print(lastRRenderUs);
            Serial.print(F("us Rsend="));
            Serial.print(lastRSendUs);
            Serial.print(F("us | "));
        }
        Serial.print(F("blink="));
        Serial.print(g_blinkState.phase);
        Serial.print(F(" lid="));
        Serial.print(g_eyeCfg.lid, 2);
        Serial.print(F(" micro="));
        Serial.println(g_microState.anim);
    }
}
