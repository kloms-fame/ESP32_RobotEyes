/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes v11.0 - OCP + 二次元夸张化重构 (Comical Smile + 极限表情)
 * @author  Rennick
 * @date    2026-07-10
 */
#include "eye_renderer.h"
#include "expressions.h"
#include <math.h>

static inline float ease_in(float t)  { return t * t; }
static inline float ease_out(float t) { float i = 1.0f - t; return 1.0f - i * i; }
static inline int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}

#ifdef EYE_STYLE_A
static const EyeStyle_t g_style = { 56, 44, 20, 10, 16, 0.55f, -6, -7, 4, 7, 5, 2, -2, 3, 1 };
#endif

typedef void (*PupilDrawFunc)(U8G2*, const EyeGeom_t*);

/* ================================================================
 *  Pupil 绘制函数表 (OCP 分派)
 * ================================================================ */

static void draw_pupil_normal(U8G2* d, const EyeGeom_t* g) {
    if (g->pupil_r > 0) d->drawDisc(g->pupil_cx, g->pupil_cy, g->pupil_r, U8G2_DRAW_ALL);
}

static void draw_pupil_heart(U8G2* d, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, br = g->pupil_r;
    if (br < 3) { draw_pupil_normal(d, g); return; }

    /* v11: 极限双相心跳 "Boom-Ba-Boom" 节律, 峰值 1.65x, 几乎撑满眼眶 */
    uint32_t bt = millis() % 800;
    float s = 1.0f;
    if (bt < 80) {           /* "Boom" 急速放大至 1.65x */
        float t = (float)bt / 80.0f;
        s = 1.0f + 0.65f * sinf(t * M_PI);
    } else if (bt < 190) {   /* 回弹 1.40x */
        float t = (float)(bt - 80) / 110.0f;
        s = 1.0f + 0.40f * sinf(t * M_PI);
    } else if (bt < 290) {   /* "Ba" 二次冲击 1.55x */
        float t = (float)(bt - 190) / 100.0f;
        s = 1.0f + 0.55f * sinf(t * M_PI);
    }
    /* 290~800ms: 静止期 (s=1.0) */

    float sw = 1.0f + (s - 1.0f) * 0.88f;
    float sh = s;
    int16_t rw = (int16_t)((float)br * sw), rh = (int16_t)((float)br * sh);
    if (rw < 5) rw = 5; if (rh < 5) rh = 5;
    int16_t lr = rw / 2;

    /* 爱心上半两个圆弧 */
    d->drawDisc(cx - lr, cy - lr / 2, lr, U8G2_DRAW_ALL);
    d->drawDisc(cx + lr, cy - lr / 2, lr, U8G2_DRAW_ALL);
    /* 爱心中间方形填充 */
    d->drawBox(cx - lr, cy - lr / 2 + 1, rw, rw / 2);
    /* 爱心尖角 */
    d->drawTriangle(cx - rw, cy, cx + rw, cy, cx, cy + rh);
    if (rw > 5) d->drawTriangle(cx - rw + 2, cy, cx + rw - 2, cy, cx, cy + rh - 2);

    /* 心跳高潮闪光: s > 1.30 时白色反光 */
    if (s > 1.30f) {
        d->setDrawColor(1);
        d->drawDisc(cx - lr + 2, cy - lr / 2 - 2, 4, U8G2_DRAW_ALL);
        d->drawDisc(cx + lr - 2, cy - lr / 2 - 2, 3, U8G2_DRAW_ALL);
        d->setDrawColor(0);
    }
}

static void draw_pupil_slit(U8G2* d, const EyeGeom_t* g) {
    int16_t sw = g->pupil_r / 3; if (sw < 1) sw = 1;
    int16_t h = g->pupil_r + 3;
    d->drawBox(g->pupil_cx - sw, g->pupil_cy - h, sw * 2, h * 2);
    if (g->pupil_r > 4) { d->setDrawColor(1); d->drawFrame(g->pupil_cx - sw - 1, g->pupil_cy - h - 1, sw * 2 + 2, h * 2 + 2); d->setDrawColor(0); }
}

static void draw_pupil_none(U8G2*, const EyeGeom_t*) {}

/* ================================================================
 *  draw_pupil_shock — v11 极限夸张: 200ms 黑白空心交替大小眼
 *
 *  利用 gm->is_left + millis() 实现:
 *    前100ms: 左眼=巨大实心黑瞳(1.5x) + 放射线, 右眼=极小白色空心瞳(0.5x)
 *    后100ms: 瞬间反转 (左小空心, 右大实心)
 *
 *  安全保证: 仅使用 U8g2 原生 drawDisc / drawCircle / drawLine,
 *            绝不使用未支持的填充算法, 绝对不会导致黑屏。
 * ================================================================ */
static void draw_pupil_shock(U8G2* d, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, r = g->pupil_r;
    if (r < 3) { draw_pupil_normal(d, g); return; }

    uint32_t phase = millis() % 200;
    bool is_big = (phase < 100) ? g->is_left : !g->is_left;

    if (is_big) {
        /* 巨大实心黑瞳 (1.5x) */
        int16_t bg_r = r * 3 / 2;
        if (bg_r > 24) bg_r = 24;
        d->drawDisc(cx, cy, bg_r, U8G2_DRAW_ALL);

        /* 放射状震惊电波线 (仅在变大时绘制, 增强夸张感) */
        int16_t sl = bg_r + 6;
        if (sl > 30) sl = 30;
        /* 四方向主放射线 */
        d->drawLine(cx - bg_r, cy, cx - sl, cy);
        d->drawLine(cx + bg_r, cy, cx + sl, cy);
        d->drawLine(cx, cy - bg_r, cx, cy - sl);
        d->drawLine(cx, cy + bg_r, cx, cy + sl);
        /* 对角副线 */
        d->drawLine(cx - bg_r * 3 / 4, cy - bg_r * 3 / 4, cx - sl * 3 / 4, cy - sl * 3 / 4);
        d->drawLine(cx + bg_r * 3 / 4, cy - bg_r * 3 / 4, cx + sl * 3 / 4, cy - sl * 3 / 4);
        d->drawLine(cx - bg_r * 3 / 4, cy + bg_r * 3 / 4, cx - sl * 3 / 4, cy + sl * 3 / 4);
        d->drawLine(cx + bg_r * 3 / 4, cy + bg_r * 3 / 4, cx + sl * 3 / 4, cy + sl * 3 / 4);
    } else {
        /* 极小白色空心瞳 (0.5x, 仅轮廓) */
        int16_t sm_r = r / 2;
        if (sm_r < 2) sm_r = 2;
        d->drawCircle(cx, cy, sm_r, U8G2_DRAW_ALL);
        /* 空心填充: 用白色 disc 清除内部, 形成只有外环的"白眼"效果 */
        if (sm_r > 2) {
            d->setDrawColor(1);
            d->drawDisc(cx, cy, sm_r - 1, U8G2_DRAW_ALL);
            d->setDrawColor(0);
        }
    }
}

/* ================================================================
 *  draw_pupil_happy → v11 已废弃, 改为 no-op
 *  Comical Smile 通过眼皮遮罩 (lid_top + lid_bottom) 实现弯月笑眼
 * ================================================================ */
static void draw_pupil_happy(U8G2*, const EyeGeom_t*) {}

static void draw_pupil_star(U8G2* d, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, r = g->pupil_r;
    if (r < 3) { draw_pupil_normal(d, g); return; }
    int16_t cr = r / 3; if (cr < 2) cr = 2; d->drawDisc(cx, cy, cr, U8G2_DRAW_ALL);
    int16_t sl = r + 2;
    d->drawLine(cx, cy - cr, cx, cy - sl); d->drawLine(cx, cy + cr, cx, cy + sl);
    d->drawLine(cx - cr, cy, cx - sl, cy); d->drawLine(cx + cr, cy, cx + sl, cy);
    if ((millis() / 60) % 2 == 0) { int16_t dg = (int16_t)((float)sl * 0.7f); d->drawLine(cx - cr, cy - cr, cx - dg, cy - dg); d->drawLine(cx + cr, cy - cr, cx + dg, cy - dg); d->drawLine(cx - cr, cy + cr, cx - dg, cy + dg); d->drawLine(cx + cr, cy + cr, cx + dg, cy + dg); }
}

static const PupilDrawFunc s_pupil_draw[PUPIL_COUNT] = {
    [PUPIL_NORMAL] = draw_pupil_normal, [PUPIL_HEART] = draw_pupil_heart,
    [PUPIL_SLIT] = draw_pupil_slit, [PUPIL_NONE] = draw_pupil_none,
    [PUPIL_SHOCK] = draw_pupil_shock, [PUPIL_HAPPY] = draw_pupil_happy,
    [PUPIL_STAR] = draw_pupil_star,
};

/* ================================================================
 *  eye_geom_compute — v11: 支持 per-eye pupil_scale_r (右眼独立缩放)
 * ================================================================ */
const EyeStyle_t* eye_style_get(void) { return &g_style; }
void eye_geom_compute(EyeGeom_t* gm, const EyeConfig_t* cfg,
                      const EyeStyle_t* s, bool is_left) {
    const int16_t hw = s->eye_w / 2, hh = s->eye_h / 2;
    gm->hw = hw; gm->hh = hh; gm->is_left = is_left;
    gm->eye_l = cfg->cx - hw; gm->eye_t = cfg->cy - hh;
    gm->eye_r = cfg->cx + hw; gm->eye_b = cfg->cy + hh;

    /* v11: 右眼使用独立的 pupil_scale_r (Surprised 大小眼) */
    float ps = cfg->cur_pupil_scale;
    if (!is_left && cfg->cur_pupil_scale_r > 0.001f)
        ps = cfg->cur_pupil_scale_r;
    gm->pupil_r = (int16_t)((float)s->pupil_r * ps);
    if (gm->pupil_r < 1) gm->pupil_r = 1;

    int16_t ppx = (int16_t)(cfg->cur_look_x * (float)s->look_max / 127.0f);
    int16_t ppy = (int16_t)(cfg->cur_look_y * (float)s->look_max / 127.0f);
    gm->pupil_cx = clamp_i16(cfg->cx + ppx, gm->eye_l + gm->pupil_r + 1, gm->eye_r - gm->pupil_r - 1);
    gm->pupil_cy = clamp_i16(cfg->cy + ppy, gm->eye_t + gm->pupil_r + 1, gm->eye_b - gm->pupil_r - 1);
    gm->pupil_type = cfg->cur_pupil_type;
    int16_t sx = (int16_t)(ppx * s->shine_parallax), sy = (int16_t)(ppy * s->shine_parallax);
    if (s->s1_r > 0) { gm->s1_x = clamp_i16(gm->pupil_cx + s->s1_dx + sx, gm->eye_l + s->s1_r + 1, gm->eye_r - s->s1_r - 1); gm->s1_y = clamp_i16(gm->pupil_cy + s->s1_dy + sy, gm->eye_t + s->s1_r + 1, gm->eye_b - s->s1_r - 1); } else { gm->s1_x = 0; gm->s1_y = 0; }
    if (s->s2_r > 0) { gm->s2_x = clamp_i16(gm->pupil_cx + s->s2_dx + sx, gm->eye_l + s->s2_r + 1, gm->eye_r - s->s2_r - 1); gm->s2_y = clamp_i16(gm->pupil_cy + s->s2_dy + sy, gm->eye_t + s->s2_r + 1, gm->eye_b - s->s2_r - 1); } else { gm->s2_x = 0; gm->s2_y = 0; }
    if (s->s3_r > 0) { gm->s3_x = clamp_i16(gm->pupil_cx + s->s3_dx + sx, gm->eye_l + s->s3_r + 1, gm->eye_r - s->s3_r - 1); gm->s3_y = clamp_i16(gm->pupil_cy + s->s3_dy + sy, gm->eye_t + s->s3_r + 1, gm->eye_b - s->s3_r - 1); } else { gm->s3_x = 0; gm->s3_y = 0; }
    float lt, lb;
    if (cfg->lid > 0.001f) { lt = cfg->lid; lb = cfg->lid * 0.5f; }
    else { lt = is_left ? cfg->cur_lid_top_l : cfg->cur_lid_top_r; lb = cfg->cur_lid_bottom; }
    gm->lid_top_base_y = gm->eye_t + (int16_t)((float)(hh * 2 + 4) * lt);
    gm->lid_slope_px = (int16_t)(cfg->cur_lid_slope * (float)hh);
    int16_t yi = gm->lid_top_base_y + gm->lid_slope_px, yo = gm->lid_top_base_y - gm->lid_slope_px;
    gm->lid_y_left = is_left ? yo : yi; gm->lid_y_right = is_left ? yi : yo;
    if (lb > 0.001f) gm->lid_bottom_h = (int16_t)((float)(hh + 2) * lb); else gm->lid_bottom_h = 0;
}

/* ================================================================
 *  绘制管线: body → pupil → sweat/tears → shine → lid_mask
 * ================================================================ */
void eye_draw_body(U8G2* d, const EyeGeom_t* gm) {
    const EyeStyle_t* s = eye_style_get();
    d->drawRBox(gm->eye_l, gm->eye_t, gm->eye_r - gm->eye_l + 1, gm->eye_b - gm->eye_t + 1, s->eye_radius);
}

void eye_draw_pupil(U8G2* d, const EyeGeom_t* gm) {
    d->setDrawColor(0);
    if (gm->pupil_type < PUPIL_COUNT && s_pupil_draw[gm->pupil_type])
        s_pupil_draw[gm->pupil_type](d, gm);
}

void eye_draw_shine(U8G2* d, const EyeGeom_t* gm) {
    const EyeStyle_t* s = eye_style_get();
    /* v11: SHOCK 类型不画高光 (已在瞳孔渲染中内置) */
    if (gm->pupil_type == PUPIL_HEART || gm->pupil_type == PUPIL_SHOCK) return;
    d->setDrawColor(1);
    if (s->s1_r > 0) d->drawDisc(gm->s1_x, gm->s1_y, s->s1_r, U8G2_DRAW_ALL);
    if (s->s2_r > 0) d->drawDisc(gm->s2_x, gm->s2_y, s->s2_r, U8G2_DRAW_ALL);
    if (s->s3_r > 0) d->drawDisc(gm->s3_x, gm->s3_y, s->s3_r, U8G2_DRAW_ALL);
}

void eye_draw_lid_mask(U8G2* d, const EyeGeom_t* gm) {
    d->setDrawColor(0);
    if (gm->lid_y_left != gm->eye_t || gm->lid_y_right != gm->eye_t) {
        int16_t top = gm->eye_t - 20; if (top < 0) top = 0;
        d->drawTriangle(gm->eye_l - 4, top, gm->eye_r + 4, top, gm->eye_l - 4, gm->lid_y_left);
        d->drawTriangle(gm->eye_r + 4, top, gm->eye_l - 4, gm->lid_y_left, gm->eye_r + 4, gm->lid_y_right);
    }
    if (gm->lid_bottom_h > 0)
        d->drawBox(gm->eye_l - 2, gm->eye_b + 2 - gm->lid_bottom_h, gm->eye_r - gm->eye_l + 5, gm->lid_bottom_h + 2);
    d->setDrawColor(1);
}

/* ================================================================
 *  eye_draw_happy_arc — v11: Comical Smile 已废弃此函数
 *  (弯月笑眼改为 lid_top + lid_bottom 几何遮罩实现)
 * ================================================================ */
void eye_draw_happy_arc(U8G2*, const EyeGeom_t*) {}

static void eye_draw_tears(U8G2* d, const EyeGeom_t* gm) {
    int16_t pcx = gm->pupil_cx, pcy = gm->pupil_cy, pr = gm->pupil_r;
    int16_t toy = gm->eye_b - gm->lid_bottom_h; if (toy < pcy + pr) toy = pcy + pr;
    d->setDrawColor(1);
    uint32_t wt = millis() % 800;
    if (wt < 400) { int16_t wy = toy - 1; d->drawHLine(pcx - pr + 1, wy, pr * 2 - 2); if (pr > 5) d->drawHLine(pcx - pr / 2, toy - 3, pr); }
    d->setDrawColor(0);
    const uint32_t cm = 2000; uint32_t t1 = millis() % cm;
    int16_t y1 = toy + (int16_t)((float)t1 * 0.015f), tmy = gm->eye_b - 2;
    if (y1 > tmy) y1 = tmy; if (y1 < toy) y1 = toy;
    if (y1 < tmy) { int16_t x1 = gm->is_left ? (gm->eye_l + 9) : (gm->eye_r - 9); d->drawDisc(x1, y1, 4, U8G2_DRAW_ALL); d->drawTriangle(x1 - 4, y1, x1 + 4, y1, x1, y1 - 6); }
    uint32_t t2 = (millis() + 700) % cm; int16_t y2 = toy + (int16_t)((float)t2 * 0.010f);
    if (y2 > tmy) y2 = tmy; if (y2 < toy) y2 = toy;
    if (y2 < tmy) { int16_t x2 = gm->is_left ? (gm->eye_l + 19) : (gm->eye_r - 19); d->drawDisc(x2, y2, 3, U8G2_DRAW_ALL); d->drawTriangle(x2 - 3, y2, x2 + 3, y2, x2, y2 - 5); }
}
void eye_draw_sad_water(U8G2* d, const EyeGeom_t* gm) {
    d->setDrawColor(1); int16_t bx = gm->eye_l + (gm->eye_r - gm->eye_l) / 2;
    int16_t by = gm->eye_b - gm->lid_bottom_h - 1; uint32_t phase = millis() % 1200;
    if (phase < 400) { float alpha = (phase < 200) ? (float)phase / 200.0f : 1.0f - (float)(phase - 200) / 200.0f;
        if (alpha > 0.3f) { int16_t w = (int16_t)(alpha * (float)(gm->eye_r - gm->eye_l) * 0.4f); d->drawHLine(bx - w, by, w * 2); d->drawHLine(bx - w + 2, by - 1, (w - 2) * 2); } }
    else if (phase < 600) { d->drawPixel(bx - 6, by); d->drawPixel(bx + 6, by); }
    d->setDrawColor(0);
}

/* ================================================================
 *  eye_draw_sweat — v11 Panic 冷汗: r=4 大汗珠 + 快速滑落 + 飞溅
 * ================================================================ */
void eye_draw_sweat(U8G2* d, const EyeGeom_t* gm) {
    int16_t soy = gm->eye_t + 3;
    d->setDrawColor(0);
    const uint32_t cm = 500;  /* v11: 加快循环 (原600ms → 500ms) */
    uint32_t t = millis() % cm;

    /* 大汗珠 #1: r=4, 高速滑落 */
    int16_t sx1 = gm->is_left ? (gm->eye_l + 4) : (gm->eye_r - 4);
    int16_t y1 = soy + (int16_t)((float)t * 0.055f);
    int16_t smy = gm->eye_b - 1;
    if (y1 > smy) y1 = smy; if (y1 < soy) y1 = soy;
    if (y1 < smy) {
        d->drawDisc(sx1, y1 - 1, 4, U8G2_DRAW_ALL);
        d->drawTriangle(sx1 - 4, y1, sx1 + 4, y1, sx1, y1 + 5);
    }

    /* 大汗珠 #2: r=3, 错相滑落 */
    int16_t sx2 = gm->is_left ? (gm->eye_l + 16) : (gm->eye_r - 16);
    uint32_t t2 = (millis() + 250) % cm;
    int16_t y2 = soy + (int16_t)((float)t2 * 0.045f);
    if (y2 > smy) y2 = smy; if (y2 < soy) y2 = soy;
    if (y2 < smy) {
        d->drawDisc(sx2, y2 - 1, 3, U8G2_DRAW_ALL);
        d->drawTriangle(sx2 - 3, y2, sx2 + 3, y2, sx2, y2 + 4);
    }

    /* 汗珠溅落飞溅效果 (循环末尾) */
    if (t > 475 && t < 500) {
        d->setDrawColor(1);
        d->drawPixel(sx1 - 2, smy - 1); d->drawPixel(sx1 + 2, smy - 1);
        d->drawPixel(sx1, smy - 2); d->drawPixel(sx1 - 1, smy - 2); d->drawPixel(sx1 + 1, smy - 2);
        d->setDrawColor(0);
    }
}

/* ================================================================
 *  eye_render — v11: 移除 Smile 的 happy_arc 调用
 * ================================================================ */
void eye_render(U8G2* d, EyeConfig_t* cfg, bool is_left) {
    const EyeStyle_t* s = eye_style_get(); EyeGeom_t gm;
    eye_geom_compute(&gm, cfg, s, is_left);
    d->setDrawColor(1); eye_draw_body(d, &gm);
    eye_draw_pupil(d, &gm);
    if (cfg->active_expr == 3) { eye_draw_sad_water(d, &gm); eye_draw_tears(d, &gm); }
    if (cfg->active_expr == 6) eye_draw_sweat(d, &gm);
    eye_draw_shine(d, &gm);
    eye_draw_lid_mask(d, &gm);
    /* v11: Comical Smile (expr 1) 不再调用 happy_arc, 弯月效果由 lid 参数实现 */
}

/* ================================================================
 *  eye_config_init — v11: 初始化 target_pupil_scale_r
 * ================================================================ */
void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy) {
    cfg->cx = cx; cfg->cy = cy; cfg->lid = 0.0f;
    cfg->target_look_x = 0; cfg->target_look_y = 0; cfg->cur_look_x = 0.0f; cfg->cur_look_y = 0.0f;
    cfg->active_expr = 255;
    cfg->target_lid_top = 0.0f; cfg->target_lid_top_l = 0.0f; cfg->target_lid_top_r = 0.0f;
    cfg->target_lid_bottom = 0.0f; cfg->target_lid_slope = 0.0f;
    cfg->target_pupil_scale = 1.0f; cfg->target_pupil_type = PUPIL_NORMAL; cfg->cur_pupil_type = PUPIL_NORMAL;
    cfg->cur_lid_top = 0.0f; cfg->cur_lid_top_l = 0.0f; cfg->cur_lid_top_r = 0.0f;
    cfg->cur_lid_bottom = 0.0f; cfg->cur_lid_slope = 0.0f; cfg->cur_pupil_scale = 1.0f;
    cfg->target_pupil_scale_r = 0.0f; cfg->cur_pupil_scale_r = 0.0f; /* v11 */
    cfg->anim_peak_scale = 0.0f; cfg->anim_start_ms = 0; cfg->anim_duration_ms = 0;
    cfg->sleepy_phase_ms = 0; cfg->sleepy_lid = 0.0f; cfg->sleepy_struggle_sub = 0;
    cfg->brow_phase = 0.0f; cfg->brow_angry_phase = 0.0f; cfg->brow_burst_timer = 0.0f;
    cfg->brow_anim_phase = 0.0f; cfg->brow_offset_l = 0; cfg->brow_offset_r = 0;
    cfg->tear_phase_ms = 0; cfg->tear_phase2_ms = 0; cfg->sad_water_phase_ms = 0;
    cfg->attention_next_ms = millis() + 3000; cfg->attention_target_x = 0; cfg->attention_target_y = 0;
    cfg->attention_prev_x = 0; cfg->attention_prev_y = 0; cfg->attention_phase = 0;
    cfg->overdrive_decay = 0.0f; cfg->overdrive_amount = 0.0f;
    cfg->idle_micro_next_ms = millis() + 2000; cfg->idle_micro_type = 0;
    cfg->idle_micro_lid_delta = 0.0f; cfg->idle_micro_pupil_delta = 0.0f;
    cfg->happy_wink_next_ms = millis() + 3500; cfg->happy_wink_eye = 0; cfg->happy_wink_start_ms = 0;
    cfg->panic_scan_next_ms = millis() + 50; cfg->panic_sweat_seed = 0;
    cfg->excited_heartbeat_ms = millis();
}

void eye_set_look(EyeConfig_t* cfg, int8_t x, int8_t y) { cfg->target_look_x = x; cfg->target_look_y = y; }

void eye_look_update(EyeConfig_t* cfg) {
    cfg->cur_look_x += ((float)cfg->target_look_x - cfg->cur_look_x) * LOOK_SMOOTH_FACTOR;
    cfg->cur_look_y += ((float)cfg->target_look_y - cfg->cur_look_y) * LOOK_SMOOTH_FACTOR;
}

void eye_look_reset(EyeConfig_t* cfg) { cfg->target_look_x = 0; cfg->target_look_y = 0; }

void eye_set_expression(EyeConfig_t* cfg, uint8_t ei) {
    if (ei >= 8) return; const ExpressionDef_t* e = &EXPRESSIONS[ei]; cfg->active_expr = ei;
    cfg->target_lid_top = e->lid_top; cfg->target_lid_top_l = e->lid_top_l; cfg->target_lid_top_r = e->lid_top_r;
    cfg->target_lid_bottom = e->lid_bottom; cfg->target_lid_slope = e->lid_slope; cfg->target_pupil_type = e->pupil_type;
    cfg->sleepy_phase_ms = 0; cfg->sleepy_lid = e->lid_top; cfg->sleepy_struggle_sub = 0;
    cfg->brow_phase = 0.0f; cfg->brow_angry_phase = 0.0f; cfg->brow_burst_timer = 0.0f;
    cfg->brow_anim_phase = 0.0f; cfg->brow_offset_l = 0; cfg->brow_offset_r = 0;
    cfg->tear_phase_ms = 0; cfg->tear_phase2_ms = 800; cfg->sad_water_phase_ms = 0;
    cfg->happy_wink_next_ms = millis() + 3500; cfg->happy_wink_eye = 0; cfg->happy_wink_start_ms = 0;
    cfg->panic_scan_next_ms = millis() + 50; cfg->panic_sweat_seed = (uint8_t)(rand() & 0xFF);
    cfg->excited_heartbeat_ms = millis();

    if (ei == 7) { cfg->target_pupil_scale = e->pupil_scale; cfg->anim_peak_scale = 0.0f; cfg->anim_start_ms = 0; cfg->anim_duration_ms = 0; }
    else if (e->anim_peak > 0.001f || e->anim_peak < -0.001f) { cfg->target_pupil_scale = e->anim_peak; cfg->anim_peak_scale = e->anim_peak; cfg->anim_start_ms = millis(); cfg->anim_duration_ms = e->anim_ms; }
    else { cfg->target_pupil_scale = e->pupil_scale; cfg->anim_peak_scale = 0.0f; cfg->anim_start_ms = 0; cfg->anim_duration_ms = 0; }
    if (e->brow_anim == BROW_ANIM_RAISE_BOUNCE) { cfg->overdrive_amount = e->brow_amp * 1.8f; cfg->overdrive_decay = 0.85f; }
    else { cfg->overdrive_amount = 0.0f; cfg->overdrive_decay = 0.0f; }

    /* v11: 非 Surprised(4) 表情必须清零右眼独立 pupil scale, 否则残留导致所有表情大小眼 */
    if (ei != 4) { cfg->target_pupil_scale_r = 0.0f; cfg->cur_pupil_scale_r = 0.0f; }
}

/* ================================================================
 *  eye_expr_update — v11 二次元夸张化重构
 *  重写 5 个表情分支:
 *    [1] Comical Smile — 300ms 瞳孔±25横跳 + 弯月眼皮
 *    [4] Surprised   — 200ms 极限大小眼交替 + 黑白空心
 *    [5] Sleepy      — 犯困(-15)↔惊醒(+15) 极限眉毛状态机
 *    [6] Panic       — 50ms 高频随机扫视
 *    [7] Excited     — 大爱心 + 心跳节律
 * ================================================================ */
void eye_expr_update(EyeConfig_t* cfg, uint32_t now_ms) {
    /* Peak animation decay */
    if (cfg->anim_peak_scale > 0.001f || cfg->anim_peak_scale < -0.001f) {
        uint32_t el = now_ms - cfg->anim_start_ms;
        if (el >= cfg->anim_duration_ms) {
            cfg->anim_peak_scale = 0.0f;
            if (cfg->active_expr < 8) cfg->target_pupil_scale = EXPRESSIONS[cfg->active_expr].pupil_scale;
        }
    }

    /* ---- [1] Comical Smile: 300ms 瞳孔±25 直接突变横跳 (不平滑) ---- */
    if (cfg->active_expr == 1) {
        uint32_t cs_phase = (now_ms / 300) % 2;
        cfg->target_look_x = cs_phase ? (int8_t)(-25) : (int8_t)25;
        cfg->target_look_y = cs_phase ? (int8_t)2 : (int8_t)(-2);
        /* 直接突变: 跳过平滑插值, 实现"疯狂试探"的滑稽感 */
        cfg->cur_look_x = (float)cfg->target_look_x;
        cfg->cur_look_y = (float)cfg->target_look_y;
    }

    /* ---- [4] Surprised: 200ms 极限大小眼交替 ---- */
    if (cfg->active_expr == 4) {
        uint32_t st = now_ms % 200;
        /* pupil_scale 由 eye_geom_compute 根据 cur_pupil_scale_r 分眼计算 */
        /* 此处设置 target 值, 通过高速插值 (0.35x) 实现接近瞬时切换 */
        if (st < 100) {
            /* 前半: 左大右小 */
            cfg->target_lid_top_l  = -0.35f;
            cfg->target_lid_top_r  =  0.65f;
            cfg->target_pupil_scale   = 1.55f;   /* 左眼巨大 */
            cfg->target_pupil_scale_r = 0.40f;   /* 右眼极小 */
        } else {
            /* 后半: 左小右大 */
            cfg->target_lid_top_l  =  0.65f;
            cfg->target_lid_top_r  = -0.35f;
            cfg->target_pupil_scale   = 0.40f;   /* 左眼极小 */
            cfg->target_pupil_scale_r = 1.55f;   /* 右眼巨大 */
        }
    }

    /* ---- [5] Sleepy: 极限抗拒困意状态机 ---- */
    if (cfg->active_expr == 5) {
        cfg->sleepy_phase_ms += 33;
        uint32_t cy = cfg->sleepy_phase_ms % 4500;

        if (cy < 2200) {
            /* 犯困挣扎: lid 逼近 0.95, 眉毛夸张向内收 (用力皱眉 -15) */
            float t = (float)cy / 2200.0f;
            cfg->sleepy_lid = 0.45f + t * 0.50f;
            float dr = sinf((float)cy * 0.0025f) * 12.0f;
            cfg->target_look_x = (int8_t)dr;
            cfg->target_look_y = (int8_t)(cosf((float)cy * 0.003f) * 5.0f);
            int16_t bs = (int16_t)(t * t * 15.0f);
            cfg->brow_offset_l = -bs; cfg->brow_offset_r = -bs;
            cfg->sleepy_struggle_sub = 0;
        } else if (cy < 2600) {
            /* 惊醒瞬间: lid 跳到 0.05, 眉毛瞬间弹开高挑到 +15 */
            float t = (float)(cy - 2200) / 400.0f;
            float sn = 1.0f - t;
            cfg->sleepy_lid = 0.95f - sn * 0.90f;
            cfg->target_look_x = 0; cfg->target_look_y = 0;
            int16_t bp;
            if (t < 0.3f) bp = (int16_t)(expf(-t * 5.0f) * 15.0f);
            else bp = (int16_t)(5.0f * (1.0f - t));
            cfg->brow_offset_l = bp; cfg->brow_offset_r = bp;
            cfg->sleepy_struggle_sub = 1;
        } else if (cy < 3400) {
            /* 眼球不受控乱晃 + 眉毛微颤 */
            float t = (float)(cy - 2600) / 800.0f;
            float wb = sinf(t * M_PI * 3.0f) * 0.06f;
            cfg->sleepy_lid = 0.05f + wb;
            cfg->target_look_x = (int8_t)(sinf(t * M_PI * 3.5f) * 24.0f);
            cfg->target_look_y = (int8_t)(cosf(t * M_PI * 1.3f) * 10.0f);
            cfg->brow_offset_l = (int16_t)(sinf(t * M_PI * 2.0f) * 7.0f);
            cfg->brow_offset_r = (int16_t)(cosf(t * M_PI * 2.5f) * 6.0f);
            cfg->sleepy_struggle_sub = 2;
        } else {
            /* 重新犯困, 眉毛再次内收 */
            float t = (float)(cy - 3400) / 1100.0f;
            cfg->sleepy_lid = 0.05f + t * 0.45f;
            cfg->target_look_x = 0; cfg->target_look_y = 0;
            cfg->brow_offset_l = (int16_t)(-t * 15.0f);
            cfg->brow_offset_r = (int16_t)(-t * 15.0f);
            cfg->sleepy_struggle_sub = 3;
        }
        /* 将 sleepy 状态机的 lid 同步到 target 参数 */
        cfg->target_lid_top   = cfg->sleepy_lid;
        cfg->target_lid_top_l = cfg->sleepy_lid;
        cfg->target_lid_top_r = cfg->sleepy_lid;
    }

    /* ---- [6] Panic: 50ms 高频随机扫视 + 瞳孔缩小 ---- */
    if (cfg->active_expr == 6) {
        if (now_ms >= cfg->panic_scan_next_ms) {
            cfg->target_look_x = (int8_t)((rand() % 61) - 30);
            cfg->target_look_y = (int8_t)((rand() % 41) - 20);
            cfg->panic_scan_next_ms = now_ms + 50;
        }
    }

    /* ---- [7] Excited: 大爱心瞳孔 (pupil_scale=1.15) + 心跳 —— 无额外时序, 心跳由 draw_pupil_heart 内部 millis() 驱动 ---- */
    /* ---- [0] Normal & [2] Angry & [3] Sad — 保持原有逻辑不变 ---- */

    /* ================================================================
     *  眉毛动画引擎 (不变)
     * ================================================================ */
    if (cfg->active_expr < 8) {
        const ExpressionDef_t* e = &EXPRESSIONS[cfg->active_expr];
        float f = e->brow_freq, a = e->brow_amp;
        float ol = 0.0f, orr = 0.0f;
        switch (e->brow_anim) {
        case BROW_ANIM_BREATHE: cfg->brow_anim_phase += f; { float v = sinf(cfg->brow_anim_phase) * a, va = v * e->brow_asymmetry; ol = v + va; orr = v - va; } break;
        case BROW_ANIM_TREMBLE: cfg->brow_anim_phase += f; { float tv = sinf(cfg->brow_anim_phase) * a; if (e->brow_burst_intv > 0) { cfg->brow_burst_timer += 33.0f; if (cfg->brow_burst_timer >= (float)e->brow_burst_intv) { cfg->brow_burst_timer = 0.0f; } } float bv = (cfg->brow_burst_timer < 80.0f) ? (e->brow_burst_amp * sinf(cfg->brow_burst_timer / 80.0f * M_PI)) : 0.0f; ol = tv + bv; orr = tv + bv; } break;
        case BROW_ANIM_SOB: cfg->brow_anim_phase += f; { float w1 = sinf(cfg->brow_anim_phase) * a, w2 = sinf(cfg->brow_anim_phase + M_PI * 0.4f) * a * e->brow_asymmetry; ol = w1; orr = w2; } break;
        case BROW_ANIM_RAISE_BOUNCE: { cfg->brow_anim_phase += f; float bv = sinf(cfg->brow_anim_phase) * a; if (cfg->overdrive_amount > 0.01f) { float od = cfg->overdrive_amount * sinf(cfg->brow_anim_phase * 0.8f); ol = bv + od; orr = bv + od; cfg->overdrive_amount *= cfg->overdrive_decay; } else { ol = bv; orr = bv; } } break;
        case BROW_ANIM_SAG_DRIFT: cfg->brow_anim_phase += f; { float dv = sinf(cfg->brow_anim_phase) * a * 0.5f; ol = -a + dv; orr = -a - dv; } break;
        case BROW_ANIM_TWITCH: cfg->brow_anim_phase += f; { ol = (rand() % 2) ? a : -a; orr = (rand() % 2) ? a : -a; } break;
        case BROW_ANIM_SWAY: cfg->brow_anim_phase += f; { float dl = sinf(cfg->brow_anim_phase) * a; ol = dl; orr = -dl; } break;
        case BROW_ANIM_PANIC: cfg->brow_anim_phase += f; { float tr = sinf(cfg->brow_anim_phase) * a, no = ((float)(rand() % 100) / 100.0f - 0.5f) * a * 0.7f, dl = a + tr + no; ol = dl; orr = dl; } break;
        case BROW_ANIM_NONE: default: cfg->brow_anim_phase += f; ol = 0.0f; orr = 0.0f; break;
        }
        cfg->brow_offset_l = (int16_t)ol; cfg->brow_offset_r = (int16_t)orr;
    }

    /* ================================================================
     *  通用平滑插值 (v11: 新增 cur_pupil_scale_r 插值)
     *  Comical Smile 的 cur_look 已被直接突变跳过此处
     *  Surprised 使用 0.35 加速因子实现接近瞬时切换
     * ================================================================ */
    float smooth_factor = (cfg->active_expr == 4) ? 0.35f : 0.18f;
    cfg->cur_lid_top    += (cfg->target_lid_top    - cfg->cur_lid_top)    * smooth_factor;
    cfg->cur_lid_top_l  += (cfg->target_lid_top_l  - cfg->cur_lid_top_l)  * smooth_factor;
    cfg->cur_lid_top_r  += (cfg->target_lid_top_r  - cfg->cur_lid_top_r)  * smooth_factor;
    cfg->cur_lid_bottom += (cfg->target_lid_bottom - cfg->cur_lid_bottom) * smooth_factor;
    cfg->cur_lid_slope  += (cfg->target_lid_slope  - cfg->cur_lid_slope)  * smooth_factor;
    cfg->cur_pupil_scale   += (cfg->target_pupil_scale   - cfg->cur_pupil_scale)   * smooth_factor;
    cfg->cur_pupil_scale_r += (cfg->target_pupil_scale_r - cfg->cur_pupil_scale_r) * smooth_factor;
    cfg->cur_pupil_type = cfg->target_pupil_type;
}

/* ================================================================
 *  以下函数保持不变 (attention / idle_micro / blink)
 * ================================================================ */
void eye_attention_update(EyeConfig_t* cfg, uint32_t now_ms) {
    if (cfg->active_expr != 0 && cfg->active_expr != 255) return; if (cfg->lid > 0.1f) return;
    if (cfg->attention_phase == 0) { if (now_ms >= cfg->attention_next_ms) { cfg->attention_prev_x = cfg->target_look_x; cfg->attention_prev_y = cfg->target_look_y; cfg->attention_target_x = (int8_t)((rand() % 61) - 30); cfg->attention_target_y = (int8_t)((rand() % 41) - 20); cfg->attention_phase = 1; } }
    else if (cfg->attention_phase == 1) { float t = 0.05f; float dx = (float)(cfg->attention_target_x - cfg->target_look_x) * t, dy = (float)(cfg->attention_target_y - cfg->target_look_y) * t; cfg->target_look_x += (int8_t)dx; cfg->target_look_y += (int8_t)dy; if (abs(cfg->attention_target_x - cfg->target_look_x) <= 1 && abs(cfg->attention_target_y - cfg->target_look_y) <= 1) { cfg->attention_phase = 2; cfg->attention_next_ms = now_ms + 800 + (rand() % 1500); } }
    else if (cfg->attention_phase == 2) { if (now_ms >= cfg->attention_next_ms) cfg->attention_phase = 3; }
    else { float t = 0.03f; cfg->target_look_x += (int8_t)((float)(-cfg->target_look_x) * t); cfg->target_look_y += (int8_t)((float)(-cfg->target_look_y) * t); if (abs(cfg->target_look_x) <= 1 && abs(cfg->target_look_y) <= 1) { cfg->target_look_x = 0; cfg->target_look_y = 0; cfg->attention_phase = 0; cfg->attention_next_ms = now_ms + 2000 + (rand() % 4000); } }
}

void eye_idle_micro_update(EyeConfig_t* cfg, uint32_t now_ms) {
    if (cfg->lid > 0.2f) return;
    if (cfg->idle_micro_type == 0) { if (now_ms >= cfg->idle_micro_next_ms) { uint8_t r = rand() % 6; if (r == 0) { cfg->idle_micro_type = 1; cfg->idle_micro_pupil_delta = (rand() % 2) ? 0.05f : -0.05f; } else if (r == 1) { cfg->idle_micro_type = 2; } else if (r == 2) { cfg->idle_micro_type = 3; cfg->idle_micro_lid_delta = 0.04f; } else { cfg->idle_micro_next_ms = now_ms + 1500 + (rand() % 3000); } } }
    else { uint32_t el = now_ms - (cfg->idle_micro_next_ms - 500); if (el > 120) { if (cfg->idle_micro_type == 1) { cfg->target_pupil_scale -= cfg->idle_micro_pupil_delta; cfg->idle_micro_pupil_delta = 0.0f; } else if (cfg->idle_micro_type == 3) { cfg->target_lid_top -= cfg->idle_micro_lid_delta; cfg->idle_micro_lid_delta = 0.0f; } cfg->idle_micro_type = 0; cfg->idle_micro_next_ms = now_ms + 2000 + (rand() % 4000); } else { if (cfg->idle_micro_type == 1) cfg->target_pupil_scale += cfg->idle_micro_pupil_delta * 0.05f; else if (cfg->idle_micro_type == 2 && el < 60) cfg->brow_offset_l = (int16_t)((rand() % 5) - 2); else if (cfg->idle_micro_type == 3) cfg->target_lid_top += cfg->idle_micro_lid_delta * 0.1f; } }
}

void blink_state_init(BlinkState_t* st) {
    st->phase = BLINK_IDLE; st->phase_start_ms = 0; st->phase_duration_ms = 0;
    st->next_blink_ms = millis() + BLINK_INTERVAL_MIN + (rand() % (BLINK_INTERVAL_MAX - BLINK_INTERVAL_MIN));
}

void blink_state_update(BlinkState_t* st, EyeConfig_t* cfg, uint32_t now_ms) {
    switch (st->phase) {
    case BLINK_IDLE: if (now_ms >= st->next_blink_ms) { st->phase = BLINK_CLOSING; st->phase_start_ms = now_ms; st->phase_duration_ms = BLINK_CLOSING_MS; } break;
    case BLINK_CLOSING: { float t = (float)(now_ms - st->phase_start_ms) / st->phase_duration_ms; if (t >= 1.0f) { cfg->lid = 1.0f; st->phase = BLINK_HOLD; st->phase_start_ms = now_ms; st->phase_duration_ms = BLINK_HOLD_MS; } else cfg->lid = ease_in(t); break; }
    case BLINK_HOLD: if (now_ms - st->phase_start_ms >= st->phase_duration_ms) { st->phase = BLINK_OPENING; st->phase_start_ms = now_ms; st->phase_duration_ms = BLINK_OPENING_MS; } break;
    case BLINK_OPENING: { float t = (float)(now_ms - st->phase_start_ms) / st->phase_duration_ms; if (t >= 1.0f) { cfg->lid = 0.0f; st->phase = BLINK_IDLE; st->next_blink_ms = now_ms + BLINK_INTERVAL_MIN + (rand() % (BLINK_INTERVAL_MAX - BLINK_INTERVAL_MIN)); } else cfg->lid = 1.0f - ease_out(t); break; }
    }
}