/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes 眼型渲染 v5.5 — 高光限位防穿模
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 */

#include "eye_renderer.h"

static inline float ease_in(float t)  { return t * t; }
static inline float ease_out(float t) {
    float i = 1.0f - t; return 1.0f - i * i;
}

void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy) {
    cfg->cx = cx; cfg->cy = cy; cfg->lid = 0.0f;
    cfg->target_look_x = 0; cfg->target_look_y = 0;
    cfg->cur_look_x = 0.0f; cfg->cur_look_y = 0.0f;
}

void eye_set_look(EyeConfig_t* cfg, int8_t x, int8_t y) {
    cfg->target_look_x = x; cfg->target_look_y = y;
}

void eye_look_update(EyeConfig_t* cfg) {
    cfg->cur_look_x += ((float)cfg->target_look_x - cfg->cur_look_x) * LOOK_SMOOTH_FACTOR;
    cfg->cur_look_y += ((float)cfg->target_look_y - cfg->cur_look_y) * LOOK_SMOOTH_FACTOR;
}

void eye_look_reset(EyeConfig_t* cfg) {
    cfg->target_look_x = 0; cfg->target_look_y = 0;
}

void blink_state_init(BlinkState_t* state) {
    state->phase = BLINK_IDLE; state->phase_duration_ms = 0;
    state->phase_start_ms = 0; state->next_blink_ms = millis() + random(1500, 3001);
}

void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms) {
    switch (state->phase) {
    case BLINK_IDLE:
        cfg->lid = 0.0f;
        if (now_ms >= state->next_blink_ms) {
            state->phase = BLINK_CLOSING; state->phase_start_ms = now_ms;
            state->phase_duration_ms = BLINK_CLOSING_MS;
        }
        break;
    case BLINK_CLOSING: {
        float t = (float)(now_ms - state->phase_start_ms) / state->phase_duration_ms;
        if (t > 1.0f) t = 1.0f;
        cfg->lid = ease_in(t);
        if (t >= 1.0f) { state->phase = BLINK_HOLD; state->phase_start_ms = now_ms; state->phase_duration_ms = BLINK_HOLD_MS; }
        break;
    }
    case BLINK_HOLD:
        cfg->lid = 1.0f;
        if (now_ms - state->phase_start_ms >= state->phase_duration_ms) {
            state->phase = BLINK_OPENING; state->phase_start_ms = now_ms; state->phase_duration_ms = BLINK_OPENING_MS;
        }
        break;
    case BLINK_OPENING: {
        float t = (float)(now_ms - state->phase_start_ms) / state->phase_duration_ms;
        if (t > 1.0f) t = 1.0f;
        cfg->lid = 1.0f - ease_out(t);
        if (t >= 1.0f) {
            cfg->lid = 0.0f;
            if (random(12) == 0) {
                state->phase = BLINK_CLOSING; state->phase_start_ms = now_ms; state->phase_duration_ms = BLINK_CLOSING_MS;
            } else {
                state->phase = BLINK_IDLE; state->next_blink_ms = now_ms + random(BLINK_INTERVAL_MIN, BLINK_INTERVAL_MAX + 1);
            }
        }
        break;
    }
    }
}

/* ---- 辅助: 钳位 ---- */
static inline int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}

/* ================================================================
 *  eye_render() — v5.5 高光防穿模
 *
 *  管线:
 *    1. 白色眼型
 *    2. 瞳孔 (限位在眼眶内)
 *    3. 高光 (限位在眼眶内, 视差晃动)
 *    4. 遮罩眨眼
 * ================================================================ */
void eye_render(U8G2* disp, EyeConfig_t* cfg) {
    int16_t hw = EYE_W / 2;
    int16_t hh = EYE_H / 2;
    int16_t eye_l = cfg->cx - hw;
    int16_t eye_t = cfg->cy - hh;
    int16_t eye_r = cfg->cx + hw;
    int16_t eye_b = cfg->cy + hh;

    /* ---- 1. 眼型 ---- */
    disp->setDrawColor(1);
    disp->drawRBox(eye_l, eye_t, EYE_W, EYE_H, EYE_RADIUS);

    /* ---- 2. 瞳孔偏移 ---- */
    int16_t ppx = (int16_t)(cfg->cur_look_x * LOOK_MAX / 127.0f);
    int16_t ppy = (int16_t)(cfg->cur_look_y * LOOK_MAX / 127.0f);

#if PUPIL_SHAPE == 0
    int16_t max_dx = hw - PUPIL_R  - 2;
    int16_t max_dy = hh - PUPIL_R  - 2;
#else
    int16_t max_dx = hw - PUPIL_RX - 2;
    int16_t max_dy = hh - PUPIL_RY - 2;
#endif
    if (max_dx < 0) max_dx = 0; if (max_dy < 0) max_dy = 0;
    ppx = clamp_i16(ppx, -max_dx, max_dx);
    ppy = clamp_i16(ppy, -max_dy, max_dy);

    int16_t pcx = cfg->cx + ppx;
    int16_t pcy = cfg->cy + ppy;

    /* ---- 3. 瞳孔 ---- */
    disp->setDrawColor(0);
#if PUPIL_SHAPE == 0
    disp->drawDisc(pcx, pcy, PUPIL_R, U8G2_DRAW_ALL);
#elif PUPIL_SHAPE == 1
    disp->drawEllipse(pcx, pcy, PUPIL_RX, PUPIL_RY, U8G2_DRAW_ALL);
#else
    disp->drawEllipse(pcx, pcy, PUPIL_RX, PUPIL_RY, U8G2_DRAW_ALL);
#endif

    /* ---- 4. 高光 (视差 + 防穿模限位) ---- */
    disp->setDrawColor(1);
    int16_t sx = (int16_t)(cfg->cur_look_x * SHINE_PARALLAX * LOOK_MAX / 127.0f);
    int16_t sy = (int16_t)(cfg->cur_look_y * SHINE_PARALLAX * LOOK_MAX / 127.0f);

    /* 每个高光独立限位: 高光中心 ± 高光半径 必须在眼框内 */
    if (SHINE1_R > 0) {
        int16_t shx = clamp_i16(pcx + SHINE1_DX + sx, eye_l + SHINE1_R + 1, eye_r - SHINE1_R - 1);
        int16_t shy = clamp_i16(pcy + SHINE1_DY + sy, eye_t + SHINE1_R + 1, eye_b - SHINE1_R - 1);
        disp->drawDisc(shx, shy, SHINE1_R, U8G2_DRAW_ALL);
    }
    if (SHINE2_R > 0) {
        int16_t shx = clamp_i16(pcx + SHINE2_DX + sx, eye_l + SHINE2_R + 1, eye_r - SHINE2_R - 1);
        int16_t shy = clamp_i16(pcy + SHINE2_DY + sy, eye_t + SHINE2_R + 1, eye_b - SHINE2_R - 1);
        disp->drawDisc(shx, shy, SHINE2_R, U8G2_DRAW_ALL);
    }
    if (SHINE3_R > 0) {
        int16_t shx = clamp_i16(pcx + SHINE3_DX + sx, eye_l + SHINE3_R + 1, eye_r - SHINE3_R - 1);
        int16_t shy = clamp_i16(pcy + SHINE3_DY + sy, eye_t + SHINE3_R + 1, eye_b - SHINE3_R - 1);
        disp->drawDisc(shx, shy, SHINE3_R, U8G2_DRAW_ALL);
    }

    /* ---- 5. 遮罩眨眼 ---- */
    float lid = cfg->lid;
    if (lid < 0.001f) { disp->setDrawColor(1); return; }

    disp->setDrawColor(0);

    int16_t upper_h = (int16_t)((float)(hh + 2) * lid);
    if (upper_h > 0) {
        int16_t my = eye_t - 2;
        if (my < 0) my = 0;
        disp->drawBox(eye_l - 2, my, EYE_W + 4, upper_h);
    }

    int16_t lower_h = (int16_t)((float)(hh + 2) * lid * 0.5f);
    if (lower_h > 0) {
        disp->drawBox(eye_l - 2, eye_b + 2 - lower_h, EYE_W + 4, lower_h + 2);
    }

    disp->setDrawColor(1);
}
