/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes 双眼渲染 + 眨眼 + 微表情 v3
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 *
 * 综合参考:
 *   RoboEyes         — cur=(cur+next)/2 指数衰减平滑, 50fps, 微颤动画
 *   esp32-smooth-eye — 椭圆动态高度 + 非对称 easing
 *   esp32-sh1106-emoji — Trapezium 升-持-降, 多变眼型
 *   BitStream_12864  — 800kHz I2C 超频思路
 *
 * 渲染管线:
 *   1. 状态更新 (blink + micro)
 *   2. 左屏: clearBuffer + 椭圆眼球 + 瞳孔 + 高光
 *   3. 右屏: clearBuffer + 椭圆眼球 + 瞳孔 + 高光
 *   4. 左屏: sendBuffer (HW I2C, ~3ms)
 *   5. 右屏: sendBuffer (SW I2C, ~23ms)
 *   ↑ 先画后发 = 同步差距从 22ms 缩至 ~3ms
 */

#include "eye_renderer.h"

/* ================================================================
 *  缓动函数
 * ================================================================ */
static inline float ease_in(float t)  { return t * t; }
static inline float ease_out(float t) { float i = 1.0f - t; return 1.0f - i * i; }

/* ================================================================
 *  eye_config_init()
 * ================================================================ */
void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy) {
    cfg->cx     = cx;
    cfg->cy     = cy;
    cfg->lid    = 0.0f;
    cfg->squint = 0.0f;
    cfg->widen  = 0.0f;
}

/* ================================================================
 *  blink_state_init()
 * ================================================================ */
void blink_state_init(BlinkState_t* state) {
    state->phase             = BLINK_IDLE;
    state->start_lid         = 0.0f;
    state->target_lid        = 0.0f;
    state->phase_duration_ms = 0;
    state->phase_start_ms    = 0;
    state->next_blink_ms     = millis() + random(1500, 3001);
}

/* ================================================================
 *  micro_state_init()
 * ================================================================ */
void micro_state_init(MicroState_t* ms) {
    ms->anim             = MICRO_NONE;
    ms->start_ms         = 0;
    ms->duration_ms      = 0;
    ms->next_trigger_ms  = millis() + random(MICRO_INTERVAL_MIN, MICRO_INTERVAL_MAX + 1);
}

/* ================================================================
 *  blink_state_update() — Trapezium 状态机 + 非对称缓动
 * ================================================================ */
void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms) {
    switch (state->phase) {

    case BLINK_IDLE:
        cfg->lid = 0.0f;
        if (now_ms >= state->next_blink_ms) {
            state->phase             = BLINK_CLOSING;
            state->phase_start_ms    = now_ms;
            state->phase_duration_ms = BLINK_CLOSING_MS;
            state->start_lid         = 0.0f;
            state->target_lid        = 1.0f;
        }
        break;

    case BLINK_CLOSING: {
        float t = (float)(now_ms - state->phase_start_ms) / state->phase_duration_ms;
        if (t > 1.0f) t = 1.0f;
        cfg->lid = state->start_lid + (state->target_lid - state->start_lid) * ease_in(t);

        if (t >= 1.0f) {
            state->phase             = BLINK_HOLD;
            state->phase_start_ms    = now_ms;
            state->phase_duration_ms = BLINK_HOLD_MS;
        }
        break;
    }

    case BLINK_HOLD:
        cfg->lid = 1.0f;
        if (now_ms - state->phase_start_ms >= state->phase_duration_ms) {
            state->phase             = BLINK_OPENING;
            state->phase_start_ms    = now_ms;
            state->phase_duration_ms = BLINK_OPENING_MS;
        }
        break;

    case BLINK_OPENING: {
        float t = (float)(now_ms - state->phase_start_ms) / state->phase_duration_ms;
        if (t > 1.0f) t = 1.0f;
        cfg->lid = 1.0f - 1.0f * ease_out(t);  /* lid: 1→0, ease-out */

        if (t >= 1.0f) {
            cfg->lid = 0.0f;
            /* 10% 双连眨 */
            if (random(10) == 0) {
                state->phase             = BLINK_CLOSING;
                state->phase_start_ms    = now_ms;
                state->phase_duration_ms = BLINK_CLOSING_MS;
                state->start_lid         = 0.0f;
                state->target_lid        = 1.0f;
            } else {
                state->phase         = BLINK_IDLE;
                state->next_blink_ms = now_ms + random(BLINK_INTERVAL_MIN, BLINK_INTERVAL_MAX + 1);
            }
        }
        break;
    }
    }
}

/* ================================================================
 *  micro_state_update() — 随机微表情
 *
 *  每 3~8 秒随机触发: 眯眼 / 瞪大 / 微颤
 *  用指数衰减 cur=(cur+target)/2 平滑过渡 (参考 RoboEyes)
 * ================================================================ */
void micro_state_update(MicroState_t* ms, EyeConfig_t* cfg, uint32_t now_ms) {

    /* ---- 检查是否触发新动画 ---- */
    if (ms->anim == MICRO_NONE && now_ms >= ms->next_trigger_ms) {
        uint8_t r = random(10);
        if (r < 4) {
            ms->anim        = MICRO_SQUINT;
            ms->duration_ms = MICRO_SQUINT_MS;
        } else if (r < 7) {
            ms->anim        = MICRO_WIDEN;
            ms->duration_ms = MICRO_WIDEN_MS;
        } else {
            ms->anim        = MICRO_FLICKER;
            ms->duration_ms = MICRO_FLICKER_MS;
        }
        ms->start_ms = now_ms;
    }

    /* ---- 执行当前动画 ---- */
    float target_squint = 0.0f;
    float target_widen  = 0.0f;

    if (ms->anim != MICRO_NONE) {
        uint32_t elapsed = now_ms - ms->start_ms;

        if (elapsed < ms->duration_ms / 2) {
            /* 前半段: 渐入 */
            float t = (float)elapsed / (ms->duration_ms / 2);
            if (ms->anim == MICRO_SQUINT)  target_squint = 0.25f * t;
            if (ms->anim == MICRO_WIDEN)   target_widen  = 0.15f * t;
            if (ms->anim == MICRO_FLICKER) target_squint = 0.10f * t;
        } else if (elapsed < ms->duration_ms) {
            /* 后半段: 渐出 */
            float t = 1.0f - (float)(elapsed - ms->duration_ms / 2) / (ms->duration_ms / 2);
            if (ms->anim == MICRO_SQUINT)  target_squint = 0.25f * t;
            if (ms->anim == MICRO_WIDEN)   target_widen  = 0.15f * t;
            if (ms->anim == MICRO_FLICKER) target_squint = 0.10f * t;
        } else {
            /* 结束 */
            ms->anim            = MICRO_NONE;
            ms->next_trigger_ms = now_ms + random(MICRO_INTERVAL_MIN, MICRO_INTERVAL_MAX + 1);
        }
    }

    /* 指数衰减平滑 (RoboEyes 风格) */
    cfg->squint = (cfg->squint + target_squint) / 2.0f;
    cfg->widen  = (cfg->widen  + target_widen)  / 2.0f;
}

/* ================================================================
 *  eye_render() v3 — 可爱眼: 椭圆 + 瞳孔 + 双高光
 *
 *  绘制顺序:
 *    1. 白色椭圆 (眼球), 高度受 lid/squint/widen 影响
 *    2. 黑色瞳孔 (椭圆睁开 >30% 才画)
 *    3. 两个白色高光圆 (漫画眼灵魂)
 * ================================================================ */
void eye_render(U8G2* disp, const EyeConfig_t* cfg) {
    /* 计算有效睁开度 */
    float eye_open = 1.0f - cfg->lid;
    eye_open = eye_open * (1.0f - cfg->squint + cfg->widen);
    if (eye_open > 1.0f) eye_open = 1.0f;
    if (eye_open < 0.0f) eye_open = 0.0f;

    if (eye_open < 0.02f) return;  /* 全闭 */

    uint8_t rx = EYE_RX;
    uint8_t ry = (uint8_t)((float)EYE_RY * eye_open);
    if (ry < 1) ry = 1;

    /* 1. 白色眼球 */
    disp->setDrawColor(1);
    disp->drawEllipse(cfg->cx, cfg->cy, rx, ry, U8G2_DRAW_ALL);

    /* 2. 黑色瞳孔 */
    if (eye_open > 0.3f) {
        disp->setDrawColor(0);
        disp->drawDisc(cfg->cx, cfg->cy, PUPIL_RADIUS, U8G2_DRAW_ALL);

        /* 3. 双高光 (白色, 漫画眼灵魂!) */
        disp->setDrawColor(1);
        disp->drawDisc(cfg->cx + SHINE1_DX, cfg->cy + SHINE1_DY,
                       SHINE1_R, U8G2_DRAW_ALL);
        disp->drawDisc(cfg->cx + SHINE2_DX, cfg->cy + SHINE2_DY,
                       SHINE2_R, U8G2_DRAW_ALL);
    }

    disp->setDrawColor(1);
}
