/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes 参数化几何眼型引擎 v5
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 *
 * v5: 基于 esp32-sh1106-oled-emojis-web 的 EyeDrawer 算法
 *     - 8参数几何体渲染 (Width/Height/Slope/Radius)
 *     - 遮罩引擎眨眼 (黑色矩形遮挡, 眼型不变)
 *     - 表情平滑过渡 (lerp 8参数 + ease_out)
 *
 * 参考:
 *   esp32-sh1106-oled-emojis-web (MIT) — EyeDrawer 核心算法
 *   Anki Vector / Cozmo            — 遮罩动画理念
 */

#include "eye_renderer.h"
#include <algorithm>

/* ================================================================
 *  缓动函数
 * ================================================================ */
static inline float ease_in(float t)  { return t * t; }
static inline float ease_out(float t) {
    float i = 1.0f - t;
    return 1.0f - i * i;
}

/* ================================================================
 *  线性插值
 * ================================================================ */
static inline int16_t lerp_i16(int16_t a, int16_t b, float t) {
    return (int16_t)((float)a + (float)(b - a) * t);
}
static inline float lerp_f(float a, float b, float t) {
    return a + (b - a) * t;
}

/* ================================================================
 *  eye_config_init()
 * ================================================================ */
void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy) {
    cfg->cx      = cx;
    cfg->cy      = cy;
    cfg->lid     = 0.0f;
    cfg->morph_t = 1.0f;
    cfg->shape   = PRESET_NORMAL;
    cfg->target  = PRESET_NORMAL;
}

/* ================================================================
 *  eye_set_shape() — 触发平滑过渡到目标眼型
 * ================================================================ */
void eye_set_shape(EyeConfig_t* cfg, const EyeShape_t* shape) {
    cfg->target  = *shape;
    cfg->morph_t = 0.0f;
}

/* ================================================================
 *  eye_morph_update() — 每帧推进过渡动画
 * ================================================================ */
void eye_morph_update(EyeConfig_t* cfg, uint32_t now_ms) {
    if (cfg->morph_t >= 1.0f) return;

    (void)now_ms; /* 固定帧率推进, 不依赖时间戳 */
    cfg->morph_t += 1.0f / ((float)MORPH_DURATION_MS / (float)FRAME_INTERVAL_MS);
    if (cfg->morph_t > 1.0f) cfg->morph_t = 1.0f;

    float t = ease_out(cfg->morph_t);

    cfg->shape.width         = lerp_i16(cfg->shape.width,         cfg->target.width,         t);
    cfg->shape.height        = lerp_i16(cfg->shape.height,        cfg->target.height,        t);
    cfg->shape.radius_top    = lerp_i16(cfg->shape.radius_top,    cfg->target.radius_top,    t);
    cfg->shape.radius_bottom = lerp_i16(cfg->shape.radius_bottom, cfg->target.radius_bottom, t);
    cfg->shape.slope_top     = lerp_f( cfg->shape.slope_top,      cfg->target.slope_top,     t);
    cfg->shape.slope_bottom  = lerp_f( cfg->shape.slope_bottom,   cfg->target.slope_bottom,  t);
    cfg->shape.offset_x      = lerp_i16(cfg->shape.offset_x,      cfg->target.offset_x,      t);
    cfg->shape.offset_y      = lerp_i16(cfg->shape.offset_y,      cfg->target.offset_y,      t);
}

/* ================================================================
 *  blink_state_init()
 * ================================================================ */
void blink_state_init(BlinkState_t* state) {
    state->phase             = BLINK_IDLE;
    state->phase_duration_ms = 0;
    state->phase_start_ms    = 0;
    state->next_blink_ms     = millis() + random(1500, 3001);
}

/* ================================================================
 *  blink_state_update() — Trapezium 状态机
 * ================================================================ */
void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms) {
    switch (state->phase) {

    case BLINK_IDLE:
        cfg->lid = 0.0f;
        if (now_ms >= state->next_blink_ms) {
            state->phase             = BLINK_CLOSING;
            state->phase_start_ms    = now_ms;
            state->phase_duration_ms = BLINK_CLOSING_MS;
        }
        break;

    case BLINK_CLOSING: {
        float t = (float)(now_ms - state->phase_start_ms) / state->phase_duration_ms;
        if (t > 1.0f) t = 1.0f;
        cfg->lid = ease_in(t);  /* lid: 0→1, ease-in (先慢后快) */

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
        cfg->lid = 1.0f - ease_out(t);  /* lid: 1→0, ease-out (先快后慢) */

        if (t >= 1.0f) {
            cfg->lid = 0.0f;
            if (random(10) == 0) {
                /* 10% 双连眨 */
                state->phase             = BLINK_CLOSING;
                state->phase_start_ms    = now_ms;
                state->phase_duration_ms = BLINK_CLOSING_MS;
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
 *  micro_state_init()
 * ================================================================ */
void micro_state_init(MicroState_t* ms) {
    ms->anim             = MICRO_NONE;
    ms->start_ms         = 0;
    ms->duration_ms      = 0;
    ms->next_trigger_ms  = millis() + random(MICRO_INTERVAL_MIN, MICRO_INTERVAL_MAX + 1);
}

/* ================================================================
 *  micro_state_update() — 随机微表情
 *
 *  通过修改 cfg->target 触发 morph 过渡来实现微表情
 *  眯眼 → 目标设为 PRESET_SQUINT
 *  瞪大 → 目标设为 PRESET_WIDEN
 *  结束后恢复 PRESET_NORMAL
 * ================================================================ */
void micro_state_update(MicroState_t* ms, EyeConfig_t* cfg, uint32_t now_ms) {

    if (ms->anim == MICRO_NONE && now_ms >= ms->next_trigger_ms) {
        uint8_t r = random(10);
        if (r < 5) {
            ms->anim        = MICRO_SQUINT;
            ms->duration_ms = MICRO_DURATION_MS;
            eye_set_shape(cfg, &PRESET_SQUINT);
        } else {
            ms->anim        = MICRO_WIDEN;
            ms->duration_ms = MICRO_DURATION_MS;
            eye_set_shape(cfg, &PRESET_WIDEN);
        }
        ms->start_ms = now_ms;
    }

    if (ms->anim != MICRO_NONE) {
        if (now_ms - ms->start_ms >= ms->duration_ms) {
            /* 恢复默认眼型 */
            eye_set_shape(cfg, &PRESET_NORMAL);
            ms->anim            = MICRO_NONE;
            ms->next_trigger_ms = now_ms + random(MICRO_INTERVAL_MIN, MICRO_INTERVAL_MAX + 1);
        }
    }
}

/* ================================================================
 *  底层几何绘制函数 (移植自 esp32-sh1106 EyeDrawer)
 * ================================================================ */

/* 填充矩形 */
static void fill_rect(U8G2* disp, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int16_t l = (x0 < x1) ? x0 : x1;
    int16_t r = (x0 > x1) ? x0 : x1;
    int16_t t = (y0 < y1) ? y0 : y1;
    int16_t b = (y0 > y1) ? y0 : y1;
    disp->drawBox(l, t, r - l, b - t);
}

/* 填充直角三角形 */
static void fill_rtri(U8G2* disp,
                      int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2) {
    disp->drawTriangle(x0, y0, x1, y1, x2, y2);
}

/* 填充椭圆角 (四分之一椭圆弧)
 *   角类型: 0=TR 1=TL 2=BL 3=BR
 *   x0,y0 = 角顶点坐标, rx,ry = 半径
 *   绘制方式: 从角顶点向眼内部画水平线填充
 */
static void fill_ellipse_corner(U8G2* disp,
                                 uint8_t corner,
                                 int16_t x0, int16_t y0,
                                 int16_t rx, int16_t ry) {
    if (rx < 2 || ry < 2) return;

    int32_t x, y;
    int32_t rx2 = (int32_t)rx * rx;
    int32_t ry2 = (int32_t)ry * ry;
    int32_t fx2 = 4 * rx2;
    int32_t fy2 = 4 * ry2;
    int32_t s;

    switch (corner) {
    case 0: /* T_R: 右上角, 画到 (x0, y0-y) 的右上象限 */
        for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry);
             ry2 * x <= rx2 * y; x++) {
            disp->drawHLine(x0, y0 - y, x);
            if (s >= 0) { s += fx2 * (1 - y); y--; }
            s += ry2 * ((4 * x) + 6);
        }
        for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx);
             rx2 * y <= ry2 * x; y++) {
            disp->drawHLine(x0, y0 - y, x);
            if (s >= 0) { s += fy2 * (1 - x); x--; }
            s += rx2 * ((4 * y) + 6);
        }
        break;

    case 1: /* T_L: 左上角, 画到 (x0-x, y0-y) */
        for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry);
             ry2 * x <= rx2 * y; x++) {
            disp->drawHLine(x0 - x, y0 - y, x);
            if (s >= 0) { s += fx2 * (1 - y); y--; }
            s += ry2 * ((4 * x) + 6);
        }
        for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx);
             rx2 * y <= ry2 * x; y++) {
            disp->drawHLine(x0 - x, y0 - y, x);
            if (s >= 0) { s += fy2 * (1 - x); x--; }
            s += rx2 * ((4 * y) + 6);
        }
        break;

    case 2: /* B_L: 左下角, 画到 (x0-x, y0+y) */
        for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry);
             ry2 * x <= rx2 * y; x++) {
            disp->drawHLine(x0 - x, y0 + y - 1, x);
            if (s >= 0) { s += fx2 * (1 - y); y--; }
            s += ry2 * ((4 * x) + 6);
        }
        for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx);
             rx2 * y <= ry2 * x; y++) {
            disp->drawHLine(x0 - x, y0 + y - 1, x);
            if (s >= 0) { s += fy2 * (1 - x); x--; }
            s += rx2 * ((4 * y) + 6);
        }
        break;

    case 3: /* B_R: 右下角, 画到 (x0, y0+y) */
        for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry);
             ry2 * x <= rx2 * y; x++) {
            disp->drawHLine(x0, y0 + y - 1, x);
            if (s >= 0) { s += fx2 * (1 - y); y--; }
            s += ry2 * ((4 * x) + 6);
        }
        for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx);
             rx2 * y <= ry2 * x; y++) {
            disp->drawHLine(x0, y0 + y - 1, x);
            if (s >= 0) { s += fy2 * (1 - x); x--; }
            s += rx2 * ((4 * y) + 6);
        }
        break;
    }
}

/* ================================================================
 *  render_eye_shape() — 参数化几何眼型渲染
 *
 *  移植自 esp32-sh1106 EyeDrawer::Draw() 算法:
 *    1. 根据 Slope 计算角点偏移量
 *    2. 计算 4 个内角坐标 (TL/TR/BL/BR)
 *    3. 填充中心矩形 + 外扩矩形
 *    4. 用三角形绘制倾斜边缘
 *    5. 用椭圆弧绘制 4 个圆角
 *
 *  注意: 此函数只绘制眼型本体，不处理眨眼遮罩
 * ================================================================ */
static void render_eye_shape(U8G2* disp,
                              int16_t cx, int16_t cy,
                              const EyeShape_t* s) {
    /* 1. 计算斜率偏移量 */
    int16_t delta_yt = (int16_t)((float)s->height * s->slope_top / 2.0f);
    int16_t delta_yb = (int16_t)((float)s->height * s->slope_bottom / 2.0f);

    /* 半径自适应: 如果上下圆角之和超过总高度, 按比例缩小 */
    int16_t rt = s->radius_top;
    int16_t rb = s->radius_bottom;
    int16_t total_h = s->height + delta_yt - delta_yb;
    if (rt > 0 && rb > 0 && total_h - 1 < rt + rb) {
        rt = (int32_t)rt * (total_h - 1) / (rt + rb);
        rb = (int32_t)rb * (total_h - 1) / (rt + rb);
    }

    /* 2. 计算 4 个内角坐标 */
    int16_t ox = s->offset_x;
    int16_t oy = s->offset_y;
    int16_t hw = s->width / 2;
    int16_t hh = s->height / 2;

    int16_t TLc_x = cx + ox - hw + rt;
    int16_t TLc_y = cy + oy - hh + rt - delta_yt;
    int16_t TRc_x = cx + ox + hw - rt;
    int16_t TRc_y = cy + oy - hh + rt + delta_yt;
    int16_t BLc_x = cx + ox - hw + rb;
    int16_t BLc_y = cy + oy + hh - rb - delta_yb;
    int16_t BRc_x = cx + ox + hw - rb;
    int16_t BRc_y = cy + oy + hh - rb + delta_yb;

    /* 3. 填充中心矩形 */
    int16_t min_x = (TLc_x < BLc_x) ? TLc_x : BLc_x;
    int16_t max_x = (TRc_x > BRc_x) ? TRc_x : BRc_x;
    int16_t min_y = (TLc_y < TRc_y) ? TLc_y : TRc_y;
    int16_t max_y = (BLc_y > BRc_y) ? BLc_y : BRc_y;
    fill_rect(disp, min_x, min_y, max_x, max_y);

    /* 4. 外扩矩形 (填充到圆角范围) */
    fill_rect(disp, TRc_x, TRc_y, BRc_x + rb, BRc_y);       /* 右侧 */
    fill_rect(disp, TLc_x - rt, TLc_y, BLc_x, BLc_y);       /* 左侧 */
    fill_rect(disp, TLc_x, TLc_y - rt, TRc_x, TRc_y);       /* 顶部 */
    fill_rect(disp, BLc_x, BLc_y, BRc_x, BRc_y + rb);       /* 底部 */

    /* 5. 倾斜边缘 (三角形) */
    if (s->slope_top > 0.001f) {
        /* 内眼角高于外眼角 → 左上角填白, 右上角切黑 */
        fill_rtri(disp,
                  TLc_x, TLc_y - rt,
                  TRc_x, TRc_y - rt,
                  TLc_x, TLc_y - rt);   /* 左上白色补全 */
        disp->setDrawColor(0);
        fill_rtri(disp,
                  TRc_x, TRc_y - rt,
                  TLc_x, TLc_y - rt,
                  TRc_x, TRc_y - rt);   /* 右上黑色切角 */
        disp->setDrawColor(1);
    } else if (s->slope_top < -0.001f) {
        /* 外眼角高于内眼角 → 右上角填白, 左上角切黑 */
        fill_rtri(disp,
                  TRc_x, TRc_y - rt,
                  TLc_x, TLc_y - rt,
                  TRc_x, TRc_y - rt);   /* 右上白色补全 */
        disp->setDrawColor(0);
        fill_rtri(disp,
                  TLc_x, TLc_y - rt,
                  TRc_x, TRc_y - rt,
                  TLc_x, TLc_y - rt);   /* 左上黑色切角 */
        disp->setDrawColor(1);
    }

    if (s->slope_bottom > 0.001f) {
        fill_rtri(disp,
                  BRc_x + rb, BRc_y + rb,
                  BLc_x - rb, BLc_y + rb,
                  BRc_x + rb, BRc_y + rb);
        disp->setDrawColor(0);
        fill_rtri(disp,
                  BLc_x - rb, BLc_y + rb,
                  BRc_x + rb, BRc_y + rb,
                  BLc_x - rb, BLc_y + rb);
        disp->setDrawColor(1);
    } else if (s->slope_bottom < -0.001f) {
        fill_rtri(disp,
                  BLc_x - rb, BLc_y + rb,
                  BRc_x + rb, BRc_y + rb,
                  BLc_x - rb, BLc_y + rb);
        disp->setDrawColor(0);
        fill_rtri(disp,
                  BRc_x + rb, BRc_y + rb,
                  BLc_x - rb, BLc_y + rb,
                  BRc_x + rb, BRc_y + rb);
        disp->setDrawColor(1);
    }

    /* 6. 绘制 4 个圆角 */
    if (rt > 0) {
        fill_ellipse_corner(disp, 0, TRc_x, TRc_y, rt, rt);  /* 右上 */
        fill_ellipse_corner(disp, 1, TLc_x, TLc_y, rt, rt);  /* 左上 */
    }
    if (rb > 0) {
        fill_ellipse_corner(disp, 2, BLc_x, BLc_y, rb, rb);  /* 左下 */
        fill_ellipse_corner(disp, 3, BRc_x, BRc_y, rb, rb);  /* 右下 */
    }
}

/* ================================================================
 *  eye_render() — 主渲染: 参数化眼型 + 遮罩眨眼
 *
 *  渲染管线:
 *    1. 绘制眼型本体 (参数化几何, 形状永远不变)
 *    2. 叠加黑色遮罩矩形 (上下眼皮) 模拟眨眼
 *
 *  遮罩原理:
 *    上眼皮: 从眼球顶部向下遮挡
 *    下眼皮: 从眼球底部向上遮挡
 *    lid=0.0 → 无遮罩 (全睁)
 *    lid=1.0 → 遮罩合拢 (全闭)
 *
 *  眼型越界保底: 坐标超出屏幕边界会被限制
 * ================================================================ */
void eye_render(U8G2* disp, EyeConfig_t* cfg) {
    const EyeShape_t* s = &cfg->shape;

    /* ---- 1. 绘制眼睛本体 (白色, 形状永远固定) ---- */
    disp->setDrawColor(1);
    render_eye_shape(disp, cfg->cx, cfg->cy, s);

    /* ---- 2. 计算遮罩参数 ---- */
    float lid = cfg->lid;
    if (lid < 0.001f) return;  /* 全睁, 无需遮罩 */

    int16_t cx = cfg->cx + s->offset_x;
    int16_t cy = cfg->cy + s->offset_y;
    int16_t hw = s->width / 2 + 3;   /* 稍宽于眼球, 确保完全覆盖 */
    int16_t hh = s->height / 2 + 2;

    /* ---- 3. 绘制黑色遮罩 (眼皮) ---- */
    disp->setDrawColor(0);

    /* 上眼皮: 从顶部向下压 */
    int16_t upper_h = (int16_t)((float)hh * lid);
    if (upper_h > 0) {
        int16_t mask_y = cy - hh;
        if (mask_y < 0) mask_y = 0;
        disp->drawBox(cx - hw, mask_y, hw * 2, upper_h);
    }

    /* 下眼皮: 从底部向上抬 */
    int16_t lower_h = (int16_t)((float)hh * lid * 0.6f);  /* 下眼皮幅度略小, 更自然 */
    if (lower_h > 0) {
        int16_t mask_y = cy + hh - lower_h;
        disp->drawBox(cx - hw, mask_y, hw * 2, lower_h + 2);
    }

    disp->setDrawColor(1);
}
