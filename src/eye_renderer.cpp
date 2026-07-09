/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes 眼型渲染 v7.0 — OCP 解耦管线实现
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-09
 *
 *  渲染管线 (每个函数职责单一):
 *    eye_geom_compute() → EyeGeom_t
 *      → eye_draw_body()     // 眼眶 RBox
 *      → eye_draw_pupil()    // 瞳孔 (函数指针表分派)
 *      → eye_draw_shine()    // 高光
 *      → eye_draw_lid_mask() // 眼皮遮罩
 *
 *  OCP 体现:
 *    - 新风格: 新增 EyeStyle_t 常量 + #ifdef 注册
 *    - 新瞳孔: 新增绘制函数 + 注册到 s_pupil_draw[PUPIL_COUNT]
 *    - 管线函数 (geom/draw_body/draw_shine/draw_lid) 永远不变
 */

#include "eye_renderer.h"
#include "expressions.h"
#include <math.h>

/* ================================================================
 *  缓动工具
 * ================================================================ */
static inline float ease_in(float t)  { return t * t; }
static inline float ease_out(float t) { float i = 1.0f - t; return 1.0f - i * i; }

static inline int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ================================================================
 *  EyeStyle_t 常量 — OCP: 宏切换, 不改管线
 * ================================================================ */

#ifdef EYE_STYLE_A
/* 动漫星瞳: 56×44 超大宽眼 */
static const EyeStyle_t g_style = {
    56, 44,          /* eye_w, eye_h */
    20,              /* eye_radius */
    10,              /* pupil_r */
    16,              /* look_max */
    0.55f,           /* shine_parallax */
    -6, -7, 4,       /* s1_dx, s1_dy, s1_r */
     7,  5, 2,       /* s2_dx, s2_dy, s2_r */
    -2,  3, 1        /* s3_dx, s3_dy, s3_r */
};
#endif

#ifdef EYE_STYLE_B
/* 委屈修勾: 46×32 */
static const EyeStyle_t g_style = {
    46, 32, 16, 0, 13, 0.30f,
    -5, -4, 3, 5, -3, 2, 0, 0, 0
};
#endif

#ifdef EYE_STYLE_C
/* 傲娇小兽: 40×36 */
static const EyeStyle_t g_style = {
    40, 36, 12, 0, 12, 0.30f,
    -3, -7, 3, 0, 0, 0, 0, 0, 0
};
#endif

/* ================================================================
 *  瞳孔绘制函数表 (OCP 核心)
 *
 *  新增瞳孔类型步骤:
 *    1. 在 eye_renderer.h 中 PupilType_t 枚举新增类型 (PUPIL_COUNT 之前)
 *    2. 编写 static void draw_pupil_xxx(U8G2*, const EyeGeom_t*)
 *    3. 在 s_pupil_draw 数组中注册 (PUPIL_COUNT 自动扩容, 编译期检查)
 *    4. 管线代码无需任何修改
 * ================================================================ */
typedef void (*PupilDrawFunc)(U8G2* disp, const EyeGeom_t* geom);

/* ---- 各瞳孔类型绘制实现 ---- */

static void draw_pupil_normal(U8G2* disp, const EyeGeom_t* g) {
    if (g->pupil_r > 0) {
        disp->drawDisc(g->pupil_cx, g->pupil_cy, g->pupil_r, U8G2_DRAW_ALL);
    }
}

static void draw_pupil_heart(U8G2* disp, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, r = g->pupil_r;
    if (r < 3) { draw_pupil_normal(disp, g); return; }

    int16_t hr = r;
    /* 两个上半圆 */
    disp->drawDisc(cx - r/2, cy - r/4, r/2 + 1, U8G2_DRAW_ALL);
    disp->drawDisc(cx + r/2, cy - r/4, r/2 + 1, U8G2_DRAW_ALL);
    /* 填充中间 */
    disp->drawBox(cx - r/2, cy - r/4, r, r/2 + 1);
    /* 倒三角 */
    disp->drawTriangle(cx - hr, cy, cx + hr, cy, cx, cy + hr);
    disp->drawTriangle(cx - hr + 1, cy, cx + hr - 1, cy, cx, cy + hr - 1);
}

static void draw_pupil_slit(U8G2* disp, const EyeGeom_t* g) {
    int16_t sw = g->pupil_r / 4;
    if (sw < 1) sw = 1;
    disp->drawBox(g->pupil_cx - sw, g->pupil_cy - g->pupil_r - 2,
                  sw * 2, g->pupil_r * 2 + 4);
}

static void draw_pupil_none(U8G2* /*disp*/, const EyeGeom_t* /*g*/) {
    /* 无瞳孔 */
}

static void draw_pupil_shock(U8G2* disp, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, r = g->pupil_r;
    /* 中空圆环 */
    disp->drawCircle(cx, cy, r, U8G2_DRAW_ALL);
    if (r > 3) {
        disp->setDrawColor(1);
        disp->drawCircle(cx, cy, r - 2, U8G2_DRAW_ALL);
        disp->setDrawColor(0);
    }
    /* 八向电波线交替闪烁 */
    if ((millis() / 40) % 2 == 0) {
        disp->drawLine(cx-6, cy-6, cx-15, cy-15);
        disp->drawLine(cx+6, cy-6, cx+15, cy-15);
        disp->drawLine(cx-6, cy+6, cx-15, cy+15);
        disp->drawLine(cx+6, cy+6, cx+15, cy+15);
        disp->drawLine(cx-14, cy, cx-8, cy);
        disp->drawLine(cx+8, cy, cx+14, cy);
    } else {
        disp->drawLine(cx-4, cy-7, cx-10, cy-17);
        disp->drawLine(cx+4, cy-7, cx+10, cy-17);
        disp->drawLine(cx-4, cy+7, cx-10, cy+17);
        disp->drawLine(cx+4, cy+7, cx+10, cy+17);
        disp->drawLine(cx, cy-16, cx, cy-8);
        disp->drawLine(cx, cy+8, cx, cy+16);
    }
}

static void draw_pupil_happy(U8G2* disp, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, r = g->pupil_r;
    /* 弯弯笑眼 > < : 用两条弧线 */
    int16_t hr = r * 2 / 3;
    if (hr < 2) hr = 2;
    /* 上半弧: > 形状 */
    for (int16_t dy = -hr; dy <= 0; dy++) {
        int16_t dx = (int16_t)(r * 0.6f * (float)(dy + hr) / (float)hr);
        disp->drawHLine(cx - dx, cy + dy, dx * 2);
    }
    /* 下半弧: 外扩 */
    int16_t lr = r * 3 / 4;
    if (lr < 2) lr = 2;
    for (int16_t dy = 1; dy <= lr; dy++) {
        int16_t dx = (int16_t)(r * 0.6f * (float)(lr - dy) / (float)lr);
        disp->drawHLine(cx - dx, cy + dy, dx * 2);
    }
}

/* ---- 分派表 (编译期验证: 条目数 == PUPIL_COUNT) ---- */
static const PupilDrawFunc s_pupil_draw[PUPIL_COUNT] = {
    [PUPIL_NORMAL] = draw_pupil_normal,
    [PUPIL_HEART]  = draw_pupil_heart,
    [PUPIL_SLIT]   = draw_pupil_slit,
    [PUPIL_NONE]   = draw_pupil_none,
    [PUPIL_SHOCK]  = draw_pupil_shock,
    [PUPIL_HAPPY]  = draw_pupil_happy,
};

/* ================================================================
 *  eye_style_get()
 * ================================================================ */
const EyeStyle_t* eye_style_get(void) {
    return &g_style;
}

/* ================================================================
 *  eye_geom_compute() — 单帧几何计算 (纯函数)
 *
 *  所有位置、clamp、斜率计算在此一次性完成。
 *  后续 draw 函数只读 EyeGeom_t, 不重复计算。
 * ================================================================ */
void eye_geom_compute(EyeGeom_t* geom,
                      const EyeConfig_t* cfg,
                      const EyeStyle_t* style,
                      bool is_left)
{
    const int16_t hw = style->eye_w / 2;
    const int16_t hh = style->eye_h / 2;

    geom->hw = hw;
    geom->hh = hh;
    geom->is_left = is_left;

    /* 眼眶边界 */
    geom->eye_l = cfg->cx - hw;
    geom->eye_t = cfg->cy - hh;
    geom->eye_r = cfg->cx + hw;
    geom->eye_b = cfg->cy + hh;

    /* 瞳孔缩放半径 */
    geom->pupil_r = (int16_t)((float)style->pupil_r * cfg->cur_pupil_scale);
    if (geom->pupil_r < 1) geom->pupil_r = 1;

    /* 瞳孔位置 (视线偏移 + clamp 在眼眶内) */
    int16_t ppx = (int16_t)(cfg->cur_look_x * (float)style->look_max / 127.0f);
    int16_t ppy = (int16_t)(cfg->cur_look_y * (float)style->look_max / 127.0f);
    geom->pupil_cx = clamp_i16(cfg->cx + ppx,
                                geom->eye_l + geom->pupil_r + 1,
                                geom->eye_r - geom->pupil_r - 1);
    geom->pupil_cy = clamp_i16(cfg->cy + ppy,
                                geom->eye_t + geom->pupil_r + 1,
                                geom->eye_b - geom->pupil_r - 1);

    geom->pupil_type = cfg->cur_pupil_type;

    /* 高光位置 (含视差) */
    float sp = style->shine_parallax;
    int16_t sx = (int16_t)(cfg->cur_look_x * sp * (float)style->look_max / 127.0f);
    int16_t sy = (int16_t)(cfg->cur_look_y * sp * (float)style->look_max / 127.0f);

    if (style->s1_r > 0) {
        geom->s1_x = clamp_i16(geom->pupil_cx + style->s1_dx + sx,
                                geom->eye_l + style->s1_r + 1,
                                geom->eye_r - style->s1_r - 1);
        geom->s1_y = clamp_i16(geom->pupil_cy + style->s1_dy + sy,
                                geom->eye_t + style->s1_r + 1,
                                geom->eye_b - style->s1_r - 1);
    } else {
        geom->s1_x = 0; geom->s1_y = 0;
    }
    if (style->s2_r > 0) {
        geom->s2_x = clamp_i16(geom->pupil_cx + style->s2_dx + sx,
                                geom->eye_l + style->s2_r + 1,
                                geom->eye_r - style->s2_r - 1);
        geom->s2_y = clamp_i16(geom->pupil_cy + style->s2_dy + sy,
                                geom->eye_t + style->s2_r + 1,
                                geom->eye_b - style->s2_r - 1);
    } else {
        geom->s2_x = 0; geom->s2_y = 0;
    }
    if (style->s3_r > 0) {
        geom->s3_x = clamp_i16(geom->pupil_cx + style->s3_dx + sx,
                                geom->eye_l + style->s3_r + 1,
                                geom->eye_r - style->s3_r - 1);
        geom->s3_y = clamp_i16(geom->pupil_cy + style->s3_dy + sy,
                                geom->eye_t + style->s3_r + 1,
                                geom->eye_b - style->s3_r - 1);
    } else {
        geom->s3_x = 0; geom->s3_y = 0;
    }

    /* 眼皮遮罩几何 */
    float lid_top, lid_bottom;
    if (cfg->lid > 0.001f) {
        /* 眨眼模式: 对称闭合 */
        lid_top    = cfg->lid;
        lid_bottom = cfg->lid * 0.5f;
    } else {
        /* 表情模式: 支持非对称 (Skeptic 大小眼) */
        lid_top    = is_left ? cfg->cur_lid_top_l : cfg->cur_lid_top_r;
        lid_bottom = cfg->cur_lid_bottom;
    }

    geom->lid_top_base_y = geom->eye_t + (int16_t)((float)(hh * 2 + 4) * lid_top);
    geom->lid_slope_px   = (int16_t)(cfg->cur_lid_slope * (float)hh);
    int16_t y_inner      = geom->lid_top_base_y + geom->lid_slope_px;
    int16_t y_outer      = geom->lid_top_base_y - geom->lid_slope_px;

    /* 根据左右眼交换内外眼角: 左眼外角=左侧, 右眼外角=右侧 */
    geom->lid_y_left  = is_left ? y_outer : y_inner;
    geom->lid_y_right = is_left ? y_inner : y_outer;

    /* 下眼皮 */
    if (lid_bottom > 0.001f) {
        geom->lid_bottom_h = (int16_t)((float)(hh + 2) * lid_bottom);
    } else {
        geom->lid_bottom_h = 0;
    }
}

/* ================================================================
 *  eye_draw_body() — 眼眶 RBox
 * ================================================================ */
void eye_draw_body(U8G2* disp, const EyeGeom_t* geom) {
    const EyeStyle_t* s = eye_style_get();
    disp->drawRBox(geom->eye_l, geom->eye_t,
                   geom->eye_r - geom->eye_l + 1,
                   geom->eye_b - geom->eye_t + 1,
                   s->eye_radius);
}

/* ================================================================
 *  eye_draw_pupil() — 瞳孔分派 (OCP: 函数指针表)
 *
 *  瞳孔在眼白上用黑色 (setDrawColor=0) 绘制,
 *  形成暗色瞳孔效果。各子函数内部可按需切换颜色
 *  (如 PUPIL_SHOCK 的中空圆环)。
 * ================================================================ */
void eye_draw_pupil(U8G2* disp, const EyeGeom_t* geom) {
    disp->setDrawColor(0);  /* 瞳孔基础色: 黑色 (眼白上的暗区) */
    if (geom->pupil_type < PUPIL_COUNT && s_pupil_draw[geom->pupil_type]) {
        s_pupil_draw[geom->pupil_type](disp, geom);
    }
}

/* ================================================================
 *  eye_draw_shine() — 高光
 * ================================================================ */
void eye_draw_shine(U8G2* disp, const EyeGeom_t* geom) {
    const EyeStyle_t* s = eye_style_get();

    /* 某些瞳孔类型不需要高光 (爱心/震惊已有自己的视觉效果) */
    if (geom->pupil_type == PUPIL_HEART ||
        geom->pupil_type == PUPIL_SHOCK ||
        geom->pupil_type == PUPIL_HAPPY) {
        return;
    }

    disp->setDrawColor(1);
    if (s->s1_r > 0) {
        disp->drawDisc(geom->s1_x, geom->s1_y, s->s1_r, U8G2_DRAW_ALL);
    }
    if (s->s2_r > 0) {
        disp->drawDisc(geom->s2_x, geom->s2_y, s->s2_r, U8G2_DRAW_ALL);
    }
    if (s->s3_r > 0) {
        disp->drawDisc(geom->s3_x, geom->s3_y, s->s3_r, U8G2_DRAW_ALL);
    }
}

/* ================================================================
 *  eye_draw_lid_mask() — 眼皮遮罩 (支持 slope 斜率)
 *
 *  上眼皮: 三角形遮罩, 从屏幕顶部盖下, 支持 slope 倾斜
 *          用于实现: Angry ◣◢, Sad T_T, Skeptic 大小眼
 *  下眼皮: 矩形遮罩, 从底部向上盖
 * ================================================================ */
void eye_draw_lid_mask(U8G2* disp, const EyeGeom_t* geom) {
    disp->setDrawColor(0);

    /* 上眼皮遮罩 (三角形) */
    if (geom->lid_y_left != geom->eye_t || geom->lid_y_right != geom->eye_t) {
        int16_t top = geom->eye_t - 20;
        if (top < 0) top = 0;

        /* 两个三角形拼成梯形遮罩:
         *   (eye_l-4, top) ───────────────── (eye_r+4, top)
         *         ╲                                  ╱
         *          ╲                                ╱
         *    (eye_l-4, lid_y_left) ──── (eye_r+4, lid_y_right)
         */
        disp->drawTriangle(geom->eye_l - 4, top,
                           geom->eye_r + 4, top,
                           geom->eye_l - 4, geom->lid_y_left);
        disp->drawTriangle(geom->eye_r + 4, top,
                           geom->eye_l - 4, geom->lid_y_left,
                           geom->eye_r + 4, geom->lid_y_right);
    }

    /* 下眼皮遮罩 (矩形) */
    if (geom->lid_bottom_h > 0) {
        disp->drawBox(geom->eye_l - 2,
                      geom->eye_b + 2 - geom->lid_bottom_h,
                      geom->eye_r - geom->eye_l + 5,
                      geom->lid_bottom_h + 2);
    }

    disp->setDrawColor(1);
}

/* ================================================================
 *  eye_draw_tears() — Sad 泪海特效 (仅在 active_expr==3 时调用)
 *
 *  从渲染管线中分离, 不污染核心绘制逻辑。
 * ================================================================ */
static void eye_draw_tears(U8G2* disp, const EyeGeom_t* geom) {
    int16_t pcx = geom->pupil_cx;
    int16_t pcy = geom->pupil_cy;
    int16_t pr  = geom->pupil_r;

    /* 眼底积水反光 */
    disp->setDrawColor(1);
    int16_t water_y = pcy + pr - 2;
    disp->drawBox(pcx - pr + 2, water_y, pr * 2 - 4, 3);

    /* 水光闪烁 */
    if (millis() % 600 > 300) {
        disp->drawBox(pcx - pr/2 - 1, pcy + pr/3, pr + 2, 2);
    }

    /* 双泪滴错落滑落 */
    disp->setDrawColor(0);
    uint32_t t1 = millis() % 1800;
    int16_t y1 = pcy + pr + (int16_t)(t1 * 18 / 1800);
    if (y1 < geom->eye_b - 2) {
        int16_t x1 = pcx + (geom->is_left ? -8 : 8);
        disp->drawDisc(x1, y1, 3, U8G2_DRAW_ALL);
        disp->drawTriangle(x1-3, y1, x1+3, y1, x1, y1-5);
    }
    uint32_t t2 = (millis() + 800) % 2000;
    int16_t y2 = pcy + pr + (int16_t)(t2 * 16 / 2000);
    if (y2 < geom->eye_b - 2) {
        int16_t x2 = pcx + (geom->is_left ? 4 : -4);
        disp->drawDisc(x2, y2, 2, U8G2_DRAW_ALL);
        disp->drawTriangle(x2-2, y2, x2+2, y2, x2, y2-4);
    }
}

/* ================================================================
 *  eye_render() — 渲染入口 (组装管线)
 *
 *  管线顺序:
 *    1. geom_compute  (纯计算, 无副作用)
 *    2. draw_body      (眼眶 RBox)
 *    3. draw_pupil     (瞳孔, 函数指针表分派)
 *    4. draw_tears     (Sad 特效, 仅 active_expr==3)
 *    5. draw_shine     (高光)
 *    6. draw_lid_mask  (眼皮遮罩, 必须在最后以覆盖瞳孔)
 * ================================================================ */
void eye_render(U8G2* disp, EyeConfig_t* cfg, bool is_left) {
    const EyeStyle_t* style = eye_style_get();
    EyeGeom_t geom;

    /* Phase 1: 几何计算 */
    eye_geom_compute(&geom, cfg, style, is_left);

    /* Phase 2: 眼眶 */
    disp->setDrawColor(1);
    eye_draw_body(disp, &geom);

    /* Phase 3: 瞳孔 */
    eye_draw_pupil(disp, &geom);

    /* Phase 4: Sad 泪海特效 (仅悲伤表情, 在眼皮遮罩之前) */
    if (cfg->active_expr == 3) {
        eye_draw_tears(disp, &geom);
    }

    /* Phase 5: 高光 (在瞳孔之上, 眼皮之下) */
    eye_draw_shine(disp, &geom);

    /* Phase 6: 眼皮遮罩 (最后, 覆盖一切) */
    eye_draw_lid_mask(disp, &geom);
}

/* ================================================================
 *  以下为状态管理函数 (保持与 v6.2 兼容)
 * ================================================================ */

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

    /* Sleepy 锯齿缓动瞌睡引擎 */
    if (cfg->active_expr == 5) {
        cfg->sleepy_phase_ms += 33;
        uint32_t cycle = cfg->sleepy_phase_ms % 2500;
        if (cycle < 2200) {
            float t = (float)cycle / 2200.0f;
            cfg->sleepy_lid = 0.30f + t * 0.65f;
        } else {
            float t = (float)(cycle - 2200) / 300.0f;
            cfg->sleepy_lid = 0.95f - t * 0.65f;
        }
        cfg->target_lid_top = cfg->sleepy_lid;
    }

    /* 眉毛微动引擎 */
    cfg->brow_phase += 0.02f;
    float breathe = sin(cfg->brow_phase) * 2.0f;

    if (cfg->active_expr == 2) {
        cfg->brow_angry_phase += 0.15f;
        cfg->brow_burst_timer += 0.033f;
        float burst = 0.0f;
        float bt = fmod(cfg->brow_burst_timer, 0.8f);
        if (bt < 0.1f) {
            burst = sin(bt / 0.1f * M_PI) * 5.0f;
        }
        cfg->brow_offset_l = (int8_t)(sin(cfg->brow_angry_phase) * 3.0f + burst + breathe);
        cfg->brow_offset_r = (int8_t)(sin(cfg->brow_angry_phase + 0.5f) * 3.0f - burst + breathe);
    } else if (cfg->active_expr == 3) {
        float sob = sin(cfg->brow_phase * 0.7f + 2.0f) * 1.5f;
        cfg->brow_offset_l = (int8_t)(breathe + sob);
        cfg->brow_offset_r = (int8_t)(breathe - sob);
    } else {
        cfg->brow_offset_l = (int8_t)breathe;
        cfg->brow_offset_r = (int8_t)breathe;
    }

    /* 表达式参数 lerp */
    cfg->cur_lid_top    += (cfg->target_lid_top    - cfg->cur_lid_top)    * 0.18f;
    cfg->cur_lid_top_l  += (cfg->target_lid_top_l  - cfg->cur_lid_top_l)  * 0.18f;
    cfg->cur_lid_top_r  += (cfg->target_lid_top_r  - cfg->cur_lid_top_r)  * 0.18f;
    cfg->cur_lid_bottom += (cfg->target_lid_bottom - cfg->cur_lid_bottom) * 0.18f;
    cfg->cur_lid_slope  += (cfg->target_lid_slope  - cfg->cur_lid_slope)  * 0.18f;
    cfg->cur_pupil_scale += (cfg->target_pupil_scale - cfg->cur_pupil_scale) * 0.18f;
    cfg->cur_pupil_type  = cfg->target_pupil_type;
}

void blink_state_init(BlinkState_t* state) {
    state->phase = BLINK_IDLE;
    state->phase_start_ms = 0;
    state->phase_duration_ms = 0;
    state->next_blink_ms = millis() + BLINK_INTERVAL_MIN +
                           (rand() % (BLINK_INTERVAL_MAX - BLINK_INTERVAL_MIN));
}

void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms) {
    switch (state->phase) {
    case BLINK_IDLE:
        if (now_ms >= state->next_blink_ms) {
            state->phase = BLINK_CLOSING;
            state->phase_start_ms = now_ms;
            state->phase_duration_ms = BLINK_CLOSING_MS;
        }
        break;
    case BLINK_CLOSING: {
        float t = (float)(now_ms - state->phase_start_ms) / state->phase_duration_ms;
        if (t >= 1.0f) {
            cfg->lid = 1.0f;
            state->phase = BLINK_HOLD;
            state->phase_start_ms = now_ms;
            state->phase_duration_ms = BLINK_HOLD_MS;
        } else {
            cfg->lid = ease_in(t);
        }
        break;
    }
    case BLINK_HOLD:
        if (now_ms - state->phase_start_ms >= state->phase_duration_ms) {
            state->phase = BLINK_OPENING;
            state->phase_start_ms = now_ms;
            state->phase_duration_ms = BLINK_OPENING_MS;
        }
        break;
    case BLINK_OPENING: {
        float t = (float)(now_ms - state->phase_start_ms) / state->phase_duration_ms;
        if (t >= 1.0f) {
            cfg->lid = 0.0f;
            state->phase = BLINK_IDLE;
            state->next_blink_ms = now_ms + BLINK_INTERVAL_MIN +
                                   (rand() % (BLINK_INTERVAL_MAX - BLINK_INTERVAL_MIN));
        } else {
            cfg->lid = 1.0f - ease_out(t);
        }
        break;
    }
    }
}
