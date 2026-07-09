/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes 眼型渲染 v5.6 — 表情切换 + lerp过渡
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 */

#include "eye_renderer.h"
#include "expressions.h"

static inline float ease_in(float t)  { return t * t; }
static inline float ease_out(float t) {
    float i = 1.0f - t; return 1.0f - i * i;
}

void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy) {
    cfg->cx = cx; cfg->cy = cy; cfg->lid = 0.0f;
    cfg->target_look_x = 0; cfg->target_look_y = 0;
    cfg->cur_look_x = 0.0f; cfg->cur_look_y = 0.0f;

    /* 表情字段初始化 */
    cfg->active_expr       = 255;
    cfg->target_lid_top    = 0.0f;
    cfg->target_lid_bottom = 0.0f;
    cfg->target_pupil_scale = 1.0f;
    cfg->cur_lid_top       = 0.0f;
    cfg->cur_lid_bottom    = 0.0f;
    cfg->cur_pupil_scale   = 1.0f;
    cfg->anim_peak_scale   = 0.0f;
    cfg->anim_start_ms     = 0;
    cfg->anim_duration_ms  = 0;
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

/* ================================================================
 *  eye_set_expression() — 切换表情 (v5.6)
 *
 *  设置眼型参数目标值, 触发特殊动画 (如有)
 *  眼型和眉毛的 lerp 过渡在各自的 update 函数中独立进行
 *  调用时机: main loop 收到 EVT_EXPR_SET 事件时
 * ================================================================ */
void eye_set_expression(EyeConfig_t* cfg, uint8_t expr_index) {
    if (expr_index >= 8) return;

    const ExpressionDef_t* expr = &EXPRESSIONS[expr_index];

    cfg->active_expr = expr_index;

    /* 设置眼型目标值 (由 eye_expr_update lerp 过渡) */
    cfg->target_lid_top    = expr->lid_top;
    cfg->target_lid_bottom = expr->lid_bottom;

    /* 特殊动画: 先跳到峰值, 再回落到目标值 */
    if (expr->anim_peak > 0.0f) {
        cfg->target_pupil_scale = expr->anim_peak;
        cfg->anim_peak_scale    = expr->anim_peak;
        cfg->anim_start_ms      = millis();
        cfg->anim_duration_ms   = expr->anim_ms;
    } else {
        cfg->target_pupil_scale = expr->pupil_scale;
        cfg->anim_peak_scale    = 0.0f;
        cfg->anim_start_ms      = 0;
        cfg->anim_duration_ms   = 0;
    }
}

/* ================================================================
 *  eye_expr_update() — 表情参数 lerp 更新 (v5.6)
 *
 *  每帧调用, 将 cur_lid_top/bottom/pupil_scale 向 target 插值
 *  使用与视线相同的 LOOK_SMOOTH_FACTOR, 保证切换丝滑
 *  特殊动画: 峰值结束后自动回落到表情定义的 pupil_scale
 * ================================================================ */
void eye_expr_update(EyeConfig_t* cfg, uint32_t now_ms) {
    /* 瞳孔特殊动画: 峰值到期后回落到持续值 */
    if (cfg->anim_peak_scale > 0.0f) {
        uint32_t elapsed = now_ms - cfg->anim_start_ms;
        if (elapsed >= cfg->anim_duration_ms) {
            /* 动画结束, 回落到表情定义的持续值 */
            cfg->anim_peak_scale = 0.0f;
            if (cfg->active_expr < 8) {
                cfg->target_pupil_scale = EXPRESSIONS[cfg->active_expr].pupil_scale;
            }
        }
    }

    /* lerp: 眼型参数 + 瞳孔缩放 */
    cfg->cur_lid_top    += (cfg->target_lid_top    - cfg->cur_lid_top)    * LOOK_SMOOTH_FACTOR;
    cfg->cur_lid_bottom += (cfg->target_lid_bottom - cfg->cur_lid_bottom) * LOOK_SMOOTH_FACTOR;
    cfg->cur_pupil_scale += (cfg->target_pupil_scale - cfg->cur_pupil_scale) * LOOK_SMOOTH_FACTOR;
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
 *  eye_render() — v5.6 表情切换 + 眨眼遮罩
 *
 *  管线:
 *    1. 白色眼型
 *    2. 瞳孔 (限位在眼眶内, 大小受 pupil_scale 控制)
 *    3. 高光 (限位在眼眶内, 视差晃动)
 *    4. 表情眼皮遮罩 (lid_top / lid_bottom)
 *    5. 眨眼遮罩 (覆盖在表情眼皮之上)
 *
 *  眼皮优先级: 眨眼(lid) > 表情眼皮
 *    - 眨眼进行中: lid_top = lid, lid_bottom = lid*0.5
 *    - 否则: 使用表情的 lid_top / lid_bottom
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

    /* ---- 2. 瞳孔 (运行时缩放) ---- */
    int16_t pupil_r = (int16_t)((float)PUPIL_R * cfg->cur_pupil_scale);
    if (pupil_r < 2) pupil_r = 2;  /* 最小 2px, 防止瞳孔消失 */

    int16_t ppx = (int16_t)(cfg->cur_look_x * LOOK_MAX / 127.0f);
    int16_t ppy = (int16_t)(cfg->cur_look_y * LOOK_MAX / 127.0f);

    int16_t max_dx = hw - pupil_r - 2;
    int16_t max_dy = hh - pupil_r - 2;
    if (max_dx < 0) max_dx = 0; if (max_dy < 0) max_dy = 0;
    ppx = clamp_i16(ppx, -max_dx, max_dx);
    ppy = clamp_i16(ppy, -max_dy, max_dy);

    int16_t pcx = cfg->cx + ppx;
    int16_t pcy = cfg->cy + ppy;

    /* ---- 3. 瞳孔绘制 ---- */
    disp->setDrawColor(0);
    disp->drawDisc(pcx, pcy, pupil_r, U8G2_DRAW_ALL);

    /* ---- 4. 高光 (视差 + 防穿模限位) ---- */
    disp->setDrawColor(1);
    int16_t sx = (int16_t)(cfg->cur_look_x * SHINE_PARALLAX * LOOK_MAX / 127.0f);
    int16_t sy = (int16_t)(cfg->cur_look_y * SHINE_PARALLAX * LOOK_MAX / 127.0f);

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

    /* ---- 5. 眼皮遮罩 (表情 + 眨眼合并) ---- */
    float lid_top, lid_bottom;

    if (cfg->lid > 0.001f) {
        /* 眨眼进行中: 眨眼覆盖表情 */
        lid_top    = cfg->lid;
        lid_bottom = cfg->lid * 0.5f;
    } else {
        /* 无眨眼: 使用表情眼皮 */
        lid_top    = cfg->cur_lid_top;
        lid_bottom = cfg->cur_lid_bottom;
    }

    if (lid_top < 0.001f && lid_bottom < 0.001f) { disp->setDrawColor(1); return; }

    disp->setDrawColor(0);

    /* 上眼皮遮罩 */
    if (lid_top > 0.001f) {
        int16_t upper_h = (int16_t)((float)(hh + 2) * lid_top);
        if (upper_h > 0) {
            int16_t my = eye_t - 2;
            if (my < 0) my = 0;
            disp->drawBox(eye_l - 2, my, EYE_W + 4, upper_h);
        }
    }

    /* 下眼皮遮罩 */
    if (lid_bottom > 0.001f) {
        int16_t lower_h = (int16_t)((float)(hh + 2) * lid_bottom);
        if (lower_h > 0) {
            disp->drawBox(eye_l - 2, eye_b + 2 - lower_h, EYE_W + 4, lower_h + 2);
        }
    }

    disp->setDrawColor(1);
}
