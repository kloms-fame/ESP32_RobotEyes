/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes 眼型渲染 v8.0 — OCP 解耦管线实现
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
    if (r < 2) { draw_pupil_normal(disp, g); return; }

    /* v8.0: 圆润弯月笑眼 ^_^
     *   上方: 倒 U 弧线 (上眼皮挤压形成的弯弧)
     *   下方: 浅 U 弧线 (下眼皮上抬形成的笑弧)
     *   使用 drawCircle 部分弧 + HLine 填充, 避免锯齿 */

    /* 上弧: 在 cy-H 处画一个扁平椭圆的上半部 */
    int16_t uy = cy - r / 3;
    int16_t uw = r + 1;
    /* 用逐行水平线填充形成平滑弧顶 */
    for (int16_t dy = -r; dy <= 0; dy++) {
        float norm = (float)(dy + r) / (float)r;  /* 0→1 从上到下 */
        float curve = 1.0f - norm * norm;          /* 二次曲线 */
        int16_t dx = (int16_t)((float)uw * curve * 0.55f);
        if (dx < 1) dx = 1;
        disp->drawHLine(cx - dx, cy + dy, dx * 2);
    }

    /* 下弧: 笑弧向上弯曲 (下眼皮抬起) */
    for (int16_t dy = 1; dy <= r; dy++) {
        float norm = (float)dy / (float)r;
        float curve = 1.0f - norm * norm;
        int16_t dx = (int16_t)((float)uw * curve * 0.45f);
        if (dx < 1) dx = 1;
        disp->drawHLine(cx - dx, cy + dy, dx * 2);
    }
}

static void draw_pupil_star(U8G2* disp, const EyeGeom_t* g) {
    /* v8.0: 四角星星 sparkle ★
     *   中心圆 + 四条辐射线 + 旋转的 secondary spokes
     *   在 128x64 单色屏上半透明效果不可用, 改用几何叠加 */
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, r = g->pupil_r;
    if (r < 3) { draw_pupil_normal(disp, g); return; }

    /* 中心圆 (星核) */
    int16_t cr = r / 3;
    if (cr < 2) cr = 2;
    disp->drawDisc(cx, cy, cr, U8G2_DRAW_ALL);

    /* 四条主辐射线 (十字星芒) */
    int16_t spoke_len = r + 2;
    disp->drawLine(cx, cy - cr, cx, cy - spoke_len);
    disp->drawLine(cx, cy + cr, cx, cy + spoke_len);
    disp->drawLine(cx - cr, cy, cx - spoke_len, cy);
    disp->drawLine(cx + cr, cy, cx + spoke_len, cy);

    /* 对角辅助线 (45度旋转闪烁) */
    if ((millis() / 60) % 2 == 0) {
        int16_t diag = (int16_t)((float)spoke_len * 0.7f);
        disp->drawLine(cx - cr, cy - cr, cx - diag, cy - diag);
        disp->drawLine(cx + cr, cy - cr, cx + diag, cy - diag);
        disp->drawLine(cx - cr, cy + cr, cx - diag, cy + diag);
        disp->drawLine(cx + cr, cy + cr, cx + diag, cy + diag);
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
    [PUPIL_STAR]   = draw_pupil_star,
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
 *  eye_draw_tears() — Sad 泪滴特效 (仅在 active_expr==3 时调用)
 *
 *  v8.0: 眼睑边缘坐标映射
 *
 *  泪滴从下眼睑边缘滑落, 而非从瞳孔下方固定位置。
 *  坐标计算:
 *    tear_origin_y = eye_b - lid_bottom_h  (下眼睑上缘)
 *    tear_y = tear_origin_y + phase * rate  (滑落距离)
 *    tear_y clamp: [tear_origin_y, eye_b - 2]  (不超出眼眶)
 *
 *  双泪滴相位错落: 泪1 从 0 开始, 泪2 延迟 800ms
 *  泪滴形状: drawDisc + drawTriangle 拼成水滴形
 * ================================================================ */
static void eye_draw_tears(U8G2* disp, const EyeGeom_t* geom) {
    int16_t pcx = geom->pupil_cx;
    int16_t pcy = geom->pupil_cy;
    int16_t pr  = geom->pupil_r;

    /* 下眼睑上缘 Y 坐标 (泪滴起点) */
    int16_t tear_origin_y = geom->eye_b - geom->lid_bottom_h;
    if (tear_origin_y < pcy + pr) {
        tear_origin_y = pcy + pr;  /* 至少从瞳孔下缘开始 */
    }

    /* 眼底积水反光 */
    disp->setDrawColor(1);
    int16_t water_y = tear_origin_y - 1;
    disp->drawBox(pcx - pr + 2, water_y, pr * 2 - 4, 2);

    /* 水光闪烁 (抽泣感) */
    if (millis() % 600 > 300) {
        disp->drawBox(pcx - pr/2 - 1, pcy + pr/3, pr + 2, 2);
    }

    /* 双泪滴错落滑落 */
    disp->setDrawColor(0);
    const uint32_t cycle_ms = 2200;  /* 完整滑落周期 */

    /* 泪滴 1: phase 从 press 时刻开始 */
    uint32_t t1 = millis() % cycle_ms;
    int16_t y1 = tear_origin_y + (int16_t)((float)t1 * 0.010f);  /* rate=0.010 px/ms */

    /* clamp 到眼睑边缘内 */
    int16_t tear_max_y = geom->eye_b - 3;
    if (y1 > tear_max_y) y1 = tear_max_y;
    if (y1 < tear_origin_y) y1 = tear_origin_y;

    if (y1 < tear_max_y) {
        /* 泪滴位置: 眼角外侧 (左眼=左外角, 右眼=右外角) */
        int16_t x1 = geom->is_left ? (geom->eye_l + 10) : (geom->eye_r - 10);
        /* 水滴形: 圆 + 倒三角尖 */
        disp->drawDisc(x1, y1, 3, U8G2_DRAW_ALL);
        disp->drawTriangle(x1-3, y1, x1+3, y1, x1, y1-5);
    }

    /* 泪滴 2: 延迟 800ms 错落 */
    uint32_t t2 = (millis() + 800) % cycle_ms;
    int16_t y2 = tear_origin_y + (int16_t)((float)t2 * 0.008f);  /* 稍慢 */

    if (y2 > tear_max_y) y2 = tear_max_y;
    if (y2 < tear_origin_y) y2 = tear_origin_y;

    if (y2 < tear_max_y) {
        /* 泪滴位置: 眼角内侧, 比泪1更小 */
        int16_t x2 = geom->is_left ? (geom->eye_l + 18) : (geom->eye_r - 18);
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
    cfg->brow_anim_phase    = 0.0f;
    cfg->brow_offset_l      = 0;
    cfg->brow_offset_r      = 0;
    cfg->tear_phase_ms      = 0;
    cfg->tear_phase2_ms     = 0;
    /* v8.0 分层动画初始化 */
    cfg->attention_next_ms   = millis() + 3000;
    cfg->attention_target_x  = 0;
    cfg->attention_target_y  = 0;
    cfg->attention_prev_x    = 0;
    cfg->attention_prev_y    = 0;
    cfg->attention_phase     = 0;
    cfg->overdrive_decay     = 0.0f;
    cfg->overdrive_amount    = 0.0f;
    cfg->idle_micro_next_ms  = millis() + 2000;
    cfg->idle_micro_type     = 0;
    cfg->idle_micro_lid_delta = 0.0f;
    cfg->idle_micro_pupil_delta = 0.0f;
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
    cfg->brow_anim_phase  = 0.0f;
    cfg->brow_offset_l   = 0;
    cfg->brow_offset_r   = 0;
    cfg->tear_phase_ms   = 0;
    cfg->tear_phase2_ms  = 800;  /* 第二滴泪延迟 800ms */

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

    /* Sleepy 四秒打瞌睡循环 (v8.0 全新节律)
     *
     *  Phase 0 (0-2200ms): 缓慢闭眼 0.60→0.95
     *  Phase 1 (2200-2500ms): 突然惊醒, 弹回 0.20
     *  Phase 2 (2500-3000ms): 半睁眼微晃, 瞳孔无意识漂移
     *  Phase 3 (3000-4000ms): 再次犯困 0.20→0.60
     */
    if (cfg->active_expr == 5) {
        cfg->sleepy_phase_ms += 33;
        uint32_t cycle = cfg->sleepy_phase_ms % 4000;

        if (cycle < 2200) {
            /* Phase 0: 缓慢闭眼 */
            float t = (float)cycle / 2200.0f;
            cfg->sleepy_lid = 0.60f + t * 0.35f;
            /* 瞳孔无意识漂移 */
            float drift = sin((float)cycle * 0.002f) * 15.0f;
            cfg->target_look_x = (int8_t)drift;
            cfg->target_look_y = (int8_t)(cos((float)cycle * 0.003f) * 8.0f);
        } else if (cycle < 2500) {
            /* Phase 1: 突然惊醒 */
            float t = (float)(cycle - 2200) / 300.0f;
            float snap = 1.0f - t;
            cfg->sleepy_lid = 0.95f - snap * 0.75f;
            cfg->target_look_x = 0;
            cfg->target_look_y = 0;
        } else if (cycle < 3000) {
            /* Phase 2: 半睁眼微晃 */
            float t = (float)(cycle - 2500) / 500.0f;
            float wobble = sin(t * M_PI * 2.0f) * 0.08f;
            cfg->sleepy_lid = 0.20f + wobble;
            /* 瞳孔缓慢扫视 */
            cfg->target_look_x = (int8_t)(sin(t * M_PI) * 20.0f);
            cfg->target_look_y = (int8_t)(cos(t * M_PI * 0.7f) * 6.0f);
        } else {
            /* Phase 3: 再次犯困 */
            float t = (float)(cycle - 3000) / 1000.0f;
            cfg->sleepy_lid = 0.20f + t * 0.40f;
            cfg->target_look_x = 0;
            cfg->target_look_y = 0;
        }
        cfg->target_lid_top = cfg->sleepy_lid;
    }

    /* ============================================================
     *  眉毛动画引擎 v8.0 — 参数驱动, OCP
     *
     *  通过 ExpressionDef_t.brow_anim 分派动画类型,
     *  所有参数从表情表读取 (brow_freq / brow_amp / brow_asymmetry),
     *  新增动画类型只需: 加枚举 + 加 case, 无需改表达式表结构。
     * ============================================================ */
    if (cfg->active_expr < 8) {
        const ExpressionDef_t* expr = &EXPRESSIONS[cfg->active_expr];
        float freq = expr->brow_freq;
        float amp  = expr->brow_amp;
        float asym = expr->brow_asymmetry;
        float off_l = 0.0f, off_r = 0.0f;

        switch (expr->brow_anim) {

        case BROW_ANIM_BREATHE: {
            /* sin 慢呼吸: 左右同步, 小幅度 */
            cfg->brow_anim_phase += freq;
            off_l = sin(cfg->brow_anim_phase) * amp;
            off_r = off_l;
            break;
        }

        case BROW_ANIM_TREMBLE: {
            /* 高频震颤 + 周期性爆发 burst
             *
             *  载波:   sin(phase) * amp                 → 基础震颤
             *  爆发:   每 brow_burst_intv ms 触发一次
             *          正弦包络 sin(t/T*pi) * burst_amp  → 0→峰值→0
             *          宽度 = burst_intv * 0.125 (12.5% 占空比)
             *  叠加:   off = carrier + burst
             *  左右:   off_l = off, off_r = -off * asymmetry → 反相颤抖
             */
            cfg->brow_anim_phase += freq;
            float carrier = sin(cfg->brow_anim_phase) * amp;

            /* 爆发计算 */
            float burst = 0.0f;
            uint32_t cycle = millis() % (uint32_t)expr->brow_burst_intv;
            uint32_t burst_width = (uint32_t)expr->brow_burst_intv / 8;
            if (cycle < burst_width) {
                /* 正弦包络: 0 → 1 → 0 */
                float bt = (float)cycle / (float)burst_width;
                burst = sin(bt * M_PI) * expr->brow_burst_amp;
            }

            off_l = carrier + burst;
            off_r = -(carrier * asym) - burst * asym;
            break;
        }

        case BROW_ANIM_SOB: {
            /* 抽泣波: 慢频率 + 左右错相
             *
             *  左眉: sin(phase) * amp
             *  右眉: sin(phase + pi*asymmetry) * amp
             *  左右异步产生抽泣感
             */
            cfg->brow_anim_phase += freq;
            off_l = sin(cfg->brow_anim_phase) * amp;
            off_r = sin(cfg->brow_anim_phase + M_PI * asym) * amp;
            break;
        }

        case BROW_ANIM_RAISE_BOUNCE: {
            /* 上扬弹跳: 先快升 → 阻尼回落
             *
             *  phase 从 0 单调递增, 用 exp 衰减模拟弹跳:
             *    off = amp * exp(-phase * 3) * sin(phase * 6)
             *  初始: ~amp (快速上扬)
             *  稳定: → 0 (回落到基础角度)
             */
            cfg->brow_anim_phase += freq;
            float decay = expf(-cfg->brow_anim_phase * 3.0f);
            float bounce = sin(cfg->brow_anim_phase * 6.0f) * decay * amp;
            off_l = bounce;
            off_r = bounce;
            break;
        }

        case BROW_ANIM_SAG_DRIFT: {
            /* 无力下垂 + 微漂移
             *
             *  基准: -amp (下垂偏移)
             *  叠加: 超低频 sin 漂移 (±amp*0.5)
             *  左右: 略带非对称 (asymmetry)
             */
            cfg->brow_anim_phase += freq;
            float drift  = sin(cfg->brow_anim_phase * 0.3f) * amp * 0.5f;
            float drift2 = sin(cfg->brow_anim_phase * 0.3f + 1.5f) * amp * 0.5f;
            off_l = -amp + drift;
            off_r = -amp + drift2 * asym;
            break;
        }

        case BROW_ANIM_TWITCH: {
            /* v8.0: 随机单侧抽动
             *   每 800-2000ms 触发一次, 单侧眉毛快速抽动 10-15deg
             *   持续 80ms, 然后回落
             *   模拟真实人类无意识的单侧挑眉毛动作
             */
            uint32_t twitch_cycle = (uint32_t)(cfg->brow_anim_phase * 1000.0f);
            uint32_t interval = 1200 + (twitch_cycle * 7 % 800);
            uint32_t pos = twitch_cycle % interval;

            if (pos < 40) {
                /* 上升沿 */
                float t = (float)pos / 40.0f;
                off_l = amp * t;
                off_r = 0.0f;
            } else if (pos < 80) {
                /* 下降沿 */
                float t = (float)(pos - 40) / 40.0f;
                off_l = amp * (1.0f - t);
                off_r = 0.0f;
            } else {
                off_l = 0.0f;
                off_r = 0.0f;
            }
            cfg->brow_anim_phase += freq;
            break;
        }

        case BROW_ANIM_NONE:
        default:
            cfg->brow_anim_phase += freq;
            off_l = 0.0f;
            off_r = 0.0f;
            break;
        }

        cfg->brow_offset_l = (int8_t)off_l;
        cfg->brow_offset_r = (int8_t)off_r;
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

/* ================================================================
 *  v8.0 分层动画引擎
 * ================================================================ */

/* ---- 注意力层: 自主视线漂移 ---- */
void eye_attention_update(EyeConfig_t* cfg, uint32_t now_ms) {
    /* 仅在 Normal 表情下启用 (非表情状态) */
    if (cfg->active_expr != 0 && cfg->active_expr != 255) return;
    if (cfg->lid > 0.1f) return;  /* 眨眼时不移动 */

    if (cfg->attention_phase == 0) {
        /* 等待下次移动 */
        if (now_ms >= cfg->attention_next_ms) {
            /* 选中随机方向 */
            cfg->attention_prev_x = cfg->target_look_x;
            cfg->attention_prev_y = cfg->target_look_y;
            cfg->attention_target_x = (int8_t)((rand() % 61) - 30);  /* -30..+30 */
            cfg->attention_target_y = (int8_t)((rand() % 41) - 20);  /* -20..+20 */
            cfg->attention_phase = 1;
        }
    } else if (cfg->attention_phase == 1) {
        /* 平滑移向目标 */
        float t = 0.05f;  /* 缓慢移动 */
        float dx = (float)(cfg->attention_target_x - cfg->target_look_x) * t;
        float dy = (float)(cfg->attention_target_y - cfg->target_look_y) * t;
        cfg->target_look_x += (int8_t)dx;
        cfg->target_look_y += (int8_t)dy;

        if (abs(cfg->attention_target_x - cfg->target_look_x) <= 1 &&
            abs(cfg->attention_target_y - cfg->target_look_y) <= 1) {
            cfg->attention_phase = 2;
            cfg->attention_next_ms = now_ms + 800 + (rand() % 1500);  /* 停留 0.8-2.3s */
        }
    } else if (cfg->attention_phase == 2) {
        /* 停留 */
        if (now_ms >= cfg->attention_next_ms) {
            cfg->attention_phase = 3;
        }
    } else {
        /* 回归中心 */
        float t = 0.03f;
        cfg->target_look_x += (int8_t)((float)(-cfg->target_look_x) * t);
        cfg->target_look_y += (int8_t)((float)(-cfg->target_look_y) * t);

        if (abs(cfg->target_look_x) <= 1 && abs(cfg->target_look_y) <= 1) {
            cfg->target_look_x = 0;
            cfg->target_look_y = 0;
            cfg->attention_phase = 0;
            cfg->attention_next_ms = now_ms + 2000 + (rand() % 4000);  /* 间隔 2-6s */
        }
    }
}

/* ---- 随机怠速微动作 ---- */
void eye_idle_micro_update(EyeConfig_t* cfg, uint32_t now_ms) {
    if (cfg->lid > 0.2f) return;  /* 眨眼时不触发 */

    if (cfg->idle_micro_type == 0) {
        /* 等待下次微动作 */
        if (now_ms >= cfg->idle_micro_next_ms) {
            uint8_t r = rand() % 6;
            if (r == 0) {
                /* 瞳孔缩放 ±5% */
                cfg->idle_micro_type = 1;
                cfg->idle_micro_pupil_delta = (rand() % 2) ? 0.05f : -0.05f;
            } else if (r == 1) {
                /* 单侧眉毛轻挑 (通过 brow_offset 临时叠加) */
                cfg->idle_micro_type = 2;
            } else if (r == 2) {
                /* 眼皮微颤 */
                cfg->idle_micro_type = 3;
                cfg->idle_micro_lid_delta = 0.04f;
            } else {
                /* 什么都不做, 等下次 */
                cfg->idle_micro_next_ms = now_ms + 1500 + (rand() % 3000);
            }
        }
    } else {
        /* 执行中的微动作 */
        uint32_t elapsed = now_ms - (cfg->idle_micro_next_ms - 500);
        if (elapsed > 120) {
            /* 动作结束, 恢复 */
            if (cfg->idle_micro_type == 1) {
                cfg->target_pupil_scale -= cfg->idle_micro_pupil_delta;
                cfg->idle_micro_pupil_delta = 0.0f;
            } else if (cfg->idle_micro_type == 3) {
                cfg->target_lid_top -= cfg->idle_micro_lid_delta;
                cfg->idle_micro_lid_delta = 0.0f;
            }
            cfg->idle_micro_type = 0;
            cfg->idle_micro_next_ms = now_ms + 2000 + (rand() % 4000);
        } else {
            /* 动作进行中 */
            if (cfg->idle_micro_type == 1) {
                cfg->target_pupil_scale += cfg->idle_micro_pupil_delta * 0.05f;
            } else if (cfg->idle_micro_type == 2 && elapsed < 60) {
                cfg->brow_offset_l = (int8_t)((rand() % 5) - 2);
            } else if (cfg->idle_micro_type == 3) {
                cfg->target_lid_top += cfg->idle_micro_lid_delta * 0.1f;
            }
        }
    }
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
