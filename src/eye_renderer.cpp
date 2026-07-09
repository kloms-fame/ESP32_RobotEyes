/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes 眼型渲染 v6.2 — 笑眼/中空瞳孔/泪海/锯齿瞌睡/完美爱心
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 */

#include "eye_renderer.h"
#include "expressions.h"
#include <math.h>

static inline float ease_in(float t)  { return t * t; }
static inline float ease_out(float t) { float i = 1.0f - t; return 1.0f - i * i; }

void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy) {
    cfg->cx = cx; cfg->cy = cy; cfg->lid = 0.0f;
    cfg->target_look_x = 0; cfg->target_look_y = 0;
    cfg->cur_look_x = 0.0f; cfg->cur_look_y = 0.0f;

    cfg->active_expr        = 255;
    cfg->target_lid_top     = 0.0f;
    cfg->target_lid_top_l   = 0.0f;
    cfg->target_lid_top_r   = 0.0f;
    cfg->target_lid_bottom  = 0.0f;
    cfg->target_lid_slope   = 0.0f;
    cfg->target_pupil_scale = 1.0f;
    cfg->target_pupil_type  = PUPIL_NORMAL;
    cfg->cur_pupil_type     = PUPIL_NORMAL;
    cfg->cur_lid_top        = 0.0f;
    cfg->cur_lid_top_l      = 0.0f;
    cfg->cur_lid_top_r      = 0.0f;
    cfg->cur_lid_bottom     = 0.0f;
    cfg->cur_lid_slope      = 0.0f;
    cfg->cur_pupil_scale    = 1.0f;
    cfg->anim_peak_scale    = 0.0f;
    cfg->anim_start_ms      = 0;
    cfg->anim_duration_ms   = 0;
    cfg->sleepy_phase_ms    = 0;
    cfg->sleepy_lid         = 0.0f;
    cfg->brow_phase         = 0.0f;
    cfg->brow_angry_phase   = 0.0f;
    cfg->brow_burst_timer   = 0.0f;
    cfg->brow_offset_l      = 0;
    cfg->brow_offset_r      = 0;
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

void eye_set_expression(EyeConfig_t* cfg, uint8_t expr_index) {
    if (expr_index >= 8) return;
    const ExpressionDef_t* expr = &EXPRESSIONS[expr_index];
    cfg->active_expr = expr_index;

    cfg->target_lid_top     = expr->lid_top;
    cfg->target_lid_top_l   = expr->lid_top_l;
    cfg->target_lid_top_r   = expr->lid_top_r;
    cfg->target_lid_bottom  = expr->lid_bottom;
    cfg->target_lid_slope   = expr->lid_slope;
    cfg->target_pupil_type  = expr->pupil_type;

    cfg->sleepy_phase_ms = 0;
    cfg->sleepy_lid      = expr->lid_top;
    cfg->brow_phase      = 0.0f;
    cfg->brow_angry_phase = 0.0f;
    cfg->brow_burst_timer = 0.0f;
    cfg->brow_offset_l   = 0;
    cfg->brow_offset_r   = 0;

    if (expr->anim_peak > 0.001f || expr->anim_peak < -0.001f) {
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
 *  eye_expr_update() — 表情参数 lerp + 眉毛微动引擎 + Sleepy 锯齿瞌睡
 * ================================================================ */
void eye_expr_update(EyeConfig_t* cfg, uint32_t now_ms) {
    (void)now_ms;

    /* 瞳孔动画: 峰值到期后回落 */
    if (cfg->anim_peak_scale > 0.001f || cfg->anim_peak_scale < -0.001f) {
        uint32_t elapsed = now_ms - cfg->anim_start_ms;
        if (elapsed >= cfg->anim_duration_ms) {
            cfg->anim_peak_scale = 0.0f;
            if (cfg->active_expr < 8) {
                cfg->target_pupil_scale = EXPRESSIONS[cfg->active_expr].pupil_scale;
            }
        }
    }

    /* === Sleepy 锯齿缓动瞌睡引擎 ===
     *   锯齿波: 缓慢合 2s → 惊醒(快速开) 0.2s → 再缓慢合
     *   瞌睡 lid 范围: 0.30 ~ 0.95 */
    if (cfg->active_expr == 5) {
        cfg->sleepy_phase_ms += 33;
        uint32_t cycle = cfg->sleepy_phase_ms % 2500;  /* 2.5s 一个周期 */
        if (cycle < 2200) {
            /* 0~2200ms: 缓慢合上 (0.30 → 0.95) */
            float t = (float)cycle / 2200.0f;
            cfg->sleepy_lid = 0.30f + t * 0.65f;
        } else {
            /* 2200~2500ms: 惊醒挣开 (0.95 → 0.30, 快速) */
            float t = (float)(cycle - 2200) / 300.0f;
            cfg->sleepy_lid = 0.95f - t * 0.65f;
        }
        cfg->target_lid_top = cfg->sleepy_lid;
    }

    /* === 眉毛微动引擎 === */
    cfg->brow_phase += 0.02f;  /* ~0.3Hz 慢呼吸 */
    float breathe = sin(cfg->brow_phase) * 2.0f;  /* ±2° 呼吸 */

    if (cfg->active_expr == 2) {
        /* Angry: 爆发式眉颤 */
        cfg->brow_angry_phase += 0.15f;
        cfg->brow_burst_timer += 0.033f;
        /* 每 800ms 左右爆发一次 */
        float burst = 0.0f;
        float bt = fmod(cfg->brow_burst_timer, 0.8f);
        if (bt < 0.15f) {
            burst = sin(bt / 0.15f * 3.14159f) * 6.0f;  /* 爆发 ±6° */
        }
        cfg->brow_offset_l = (int8_t)(breathe + burst + sin(cfg->brow_angry_phase) * 3.0f);
        cfg->brow_offset_r = (int8_t)(breathe + burst + sin(cfg->brow_angry_phase + 1.5f) * 3.0f);
    } else if (cfg->active_expr == 3) {
        /* Sad: 极慢 ±1.5° 抽泣微动 */
        cfg->brow_phase += 0.01f;  /* 更慢 */
        float sob = sin(cfg->brow_phase * 0.7f + 2.0f) * 1.5f;
        cfg->brow_offset_l = (int8_t)(breathe * 0.5f + sob);
        cfg->brow_offset_r = (int8_t)(breathe * 0.5f + sob);
    } else if (cfg->active_expr < 8 && cfg->active_expr != 255) {
        /* 其他表情: 轻柔 ±2° 呼吸 */
        cfg->brow_offset_l = (int8_t)breathe;
        cfg->brow_offset_r = (int8_t)breathe;
    } else {
        cfg->brow_offset_l = 0;
        cfg->brow_offset_r = 0;
    }

    /* lerp: 眼型参数 */
    cfg->cur_lid_top     += (cfg->target_lid_top     - cfg->cur_lid_top)     * LOOK_SMOOTH_FACTOR;
    cfg->cur_lid_top_l   += (cfg->target_lid_top_l   - cfg->cur_lid_top_l)   * LOOK_SMOOTH_FACTOR;
    cfg->cur_lid_top_r   += (cfg->target_lid_top_r   - cfg->cur_lid_top_r)   * LOOK_SMOOTH_FACTOR;
    cfg->cur_lid_bottom  += (cfg->target_lid_bottom  - cfg->cur_lid_bottom)  * LOOK_SMOOTH_FACTOR;
    cfg->cur_lid_slope   += (cfg->target_lid_slope   - cfg->cur_lid_slope)   * LOOK_SMOOTH_FACTOR;
    cfg->cur_pupil_scale += (cfg->target_pupil_scale - cfg->cur_pupil_scale) * LOOK_SMOOTH_FACTOR;
    cfg->cur_pupil_type   = cfg->target_pupil_type;
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

static inline int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}

/* ================================================================
 *  eye_render() — v6.2 完整管线
 *
 *  PUPIL_HAPPY:  弯弯笑眼 > <  (双下半圆)
 *  PUPIL_SHOCK:  中空圆环 + 八向电波
 *  PUPIL_HEART:  完美爱心 (双上半圆+填充+饱满倒三角)
 *  PUPIL_SLIT:   极细竖缝
 *  Sad:          双泪滴错落 + 眼底积水反光
 * ================================================================ */
void eye_render(U8G2* disp, EyeConfig_t* cfg, bool is_left) {
    int16_t hw = EYE_W / 2; int16_t hh = EYE_H / 2;
    int16_t eye_l = cfg->cx - hw; int16_t eye_t = cfg->cy - hh;
    int16_t eye_r = cfg->cx + hw; int16_t eye_b = cfg->cy + hh;

    /* 1. 眼型 */
    disp->setDrawColor(1);
    disp->drawRBox(eye_l, eye_t, EYE_W, EYE_H, EYE_RADIUS);

    /* 2. 瞳孔 */
    int16_t pupil_r = (int16_t)((float)PUPIL_R * cfg->cur_pupil_scale);
    if (pupil_r < 1) pupil_r = 1;
    int16_t ppx = clamp_i16((int16_t)(cfg->cur_look_x * LOOK_MAX / 127.0f),
                            -(hw - pupil_r - 2), (hw - pupil_r - 2));
    int16_t ppy = clamp_i16((int16_t)(cfg->cur_look_y * LOOK_MAX / 127.0f),
                            -(hh - pupil_r - 2), (hh - pupil_r - 2));
    int16_t pcx = cfg->cx + ppx; int16_t pcy = cfg->cy + ppy;

    disp->setDrawColor(0);

    /* === 多态瞳孔引擎 === */
    if (cfg->cur_pupil_type == PUPIL_HAPPY) {
        /* 笑形瞳孔 > < : 两个下半圆拼出弯弯笑眼 */
        int16_t hw_p = pupil_r;
        disp->drawDisc(pcx - hw_p/2, pcy + 1, hw_p/2,
                       U8G2_DRAW_UPPER_RIGHT);
        disp->drawDisc(pcx + hw_p/2, pcy + 1, hw_p/2,
                       U8G2_DRAW_UPPER_LEFT);
    }
    else if (cfg->cur_pupil_type == PUPIL_HEART) {
        /* 完美爱心: 双上半圆 + 填充中间缝隙 + 饱满倒三角 */
        int16_t hr = pupil_r;
        disp->drawDisc(pcx - hr/2 - 1, pcy - hr/2, hr/2 + 1, U8G2_DRAW_ALL);
        disp->drawDisc(pcx + hr/2 + 1, pcy - hr/2, hr/2 + 1, U8G2_DRAW_ALL);
        disp->drawBox(pcx - hr/2, pcy - hr/2, hr, hr/2 + 1);  /* 填充缝隙 */
        disp->drawTriangle(pcx - hr,     pcy - hr/3,
                           pcx + hr,     pcy - hr/3,
                           pcx,          pcy + hr);
    }
    else if (cfg->cur_pupil_type == PUPIL_SHOCK) {
        /* 中空圆环瞳孔: 外圆 + 内圆掏空 */
        disp->drawCircle(pcx, pcy, pupil_r, U8G2_DRAW_ALL);
        if (pupil_r > 3) {
            disp->setDrawColor(1);
            disp->drawCircle(pcx, pcy, pupil_r - 2, U8G2_DRAW_ALL);
            disp->setDrawColor(0);
        }

        /* 八向电波线交替闪烁 */
        if ((millis() / 40) % 2 == 0) {
            disp->drawLine(pcx-6, pcy-6, pcx-15, pcy-15);
            disp->drawLine(pcx+6, pcy-6, pcx+15, pcy-15);
            disp->drawLine(pcx-6, pcy+6, pcx-15, pcy+15);
            disp->drawLine(pcx+6, pcy+6, pcx+15, pcy+15);
            disp->drawLine(pcx-14, pcy, pcx-8, pcy);
            disp->drawLine(pcx+8, pcy, pcx+14, pcy);
        } else {
            disp->drawLine(pcx-4, pcy-7, pcx-10, pcy-17);
            disp->drawLine(pcx+4, pcy-7, pcx+10, pcy-17);
            disp->drawLine(pcx-4, pcy+7, pcx-10, pcy+17);
            disp->drawLine(pcx+4, pcy+7, pcx+10, pcy+17);
            disp->drawLine(pcx, pcy-16, pcx, pcy-8);
            disp->drawLine(pcx, pcy+8, pcx, pcy+16);
        }
    }
    else if (cfg->cur_pupil_type == PUPIL_SLIT) {
        /* 极细竖缝: 宽=半径1/4 */
        int16_t sw = pupil_r / 4; if (sw < 1) sw = 1;
        disp->drawBox(pcx - sw, pcy - pupil_r - 2, sw * 2, pupil_r * 2 + 4);
    }
    else if (cfg->cur_pupil_type == PUPIL_NONE) {
        /* 无瞳孔 */
    }
    else {
        /* PUPIL_NORMAL */
        disp->drawDisc(pcx, pcy, pupil_r, U8G2_DRAW_ALL);
    }

    /* === Sad 汪洋泪海引擎 === */
    if (cfg->active_expr == 3) {
        /* 眼底积水反光 */
        disp->setDrawColor(1);
        int16_t water_y = pcy + pupil_r - 2;
        disp->drawBox(pcx - pupil_r + 2, water_y, pupil_r * 2 - 4, 3);

        /* 水光闪烁 */
        if (millis() % 600 > 300) {
            disp->drawBox(pcx - pupil_r/2 - 1, pcy + pupil_r/3, pupil_r + 2, 2);
        }

        /* 双泪滴错落滑落 */
        disp->setDrawColor(0);
        uint32_t t1 = millis() % 1800;
        int16_t y1 = pcy + pupil_r + (int16_t)(t1 * 18 / 1800);
        if (y1 < eye_b - 2) {
            int16_t x1 = pcx + (is_left ? -8 : 8);
            disp->drawDisc(x1, y1, 3, U8G2_DRAW_ALL);
            disp->drawTriangle(x1-3, y1, x1+3, y1, x1, y1-5);
        }
        uint32_t t2 = (millis() + 800) % 2000;
        int16_t y2 = pcy + pupil_r + (int16_t)(t2 * 16 / 2000);
        if (y2 < eye_b - 2) {
            int16_t x2 = pcx + (is_left ? 4 : -4);
            disp->drawDisc(x2, y2, 2, U8G2_DRAW_ALL);
            disp->drawTriangle(x2-2, y2, x2+2, y2, x2, y2-4);
        }
    }

    /* 3. 高光 */
    disp->setDrawColor(1);
    int16_t sx = (int16_t)(cfg->cur_look_x * SHINE_PARALLAX * LOOK_MAX / 127.0f);
    int16_t sy = (int16_t)(cfg->cur_look_y * SHINE_PARALLAX * LOOK_MAX / 127.0f);

    if (SHINE1_R > 0 && cfg->cur_pupil_type != PUPIL_HEART && cfg->cur_pupil_type != PUPIL_SHOCK && cfg->cur_pupil_type != PUPIL_HAPPY) {
        int16_t shx = clamp_i16(pcx + SHINE1_DX + sx, eye_l + SHINE1_R + 1, eye_r - SHINE1_R - 1);
        int16_t shy = clamp_i16(pcy + SHINE1_DY + sy, eye_t + SHINE1_R + 1, eye_b - SHINE1_R - 1);
        disp->drawDisc(shx, shy, SHINE1_R, U8G2_DRAW_ALL);
    }
    if (SHINE2_R > 0 && cfg->cur_pupil_type != PUPIL_SHOCK) {
        int16_t shx = clamp_i16(pcx + SHINE2_DX + sx, eye_l + SHINE2_R + 1, eye_r - SHINE2_R - 1);
        int16_t shy = clamp_i16(pcy + SHINE2_DY + sy, eye_t + SHINE2_R + 1, eye_b - SHINE2_R - 1);
        disp->drawDisc(shx, shy, SHINE2_R, U8G2_DRAW_ALL);
    }

    /* 4. 眼皮遮罩 */
    float lid_top, lid_bottom;
    if (cfg->lid > 0.001f) {
        lid_top = cfg->lid; lid_bottom = cfg->lid * 0.5f;
    } else {
        lid_top    = is_left ? cfg->cur_lid_top_l : cfg->cur_lid_top_r;
        lid_bottom = cfg->cur_lid_bottom;
    }

    disp->setDrawColor(0);

    if (lid_top > 0.001f || cfg->cur_lid_slope > 0.01f || cfg->cur_lid_slope < -0.01f) {
        int16_t base_y = eye_t + (int16_t)((float)(hh * 2 + 4) * lid_top);
        int16_t slope_px = (int16_t)(cfg->cur_lid_slope * (float)hh);
        int16_t y_inner = base_y + slope_px;
        int16_t y_outer = base_y - slope_px;
        int16_t y_left  = is_left ? y_outer : y_inner;
        int16_t y_right = is_left ? y_inner : y_outer;

        int16_t top = eye_t - 20; if (top < 0) top = 0;
        disp->drawTriangle(eye_l - 4, top, eye_r + 4, top, eye_l - 4, y_left);
        disp->drawTriangle(eye_r + 4, top, eye_l - 4, y_left, eye_r + 4, y_right);
    }

    if (lid_bottom > 0.001f) {
        int16_t lower_h = (int16_t)((float)(hh + 2) * lid_bottom);
        if (lower_h > 0) {
            disp->drawBox(eye_l - 2, eye_b + 2 - lower_h, EYE_W + 4, lower_h + 2);
        }
    }
    disp->setDrawColor(1);
}
