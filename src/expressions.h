/**
 * @file    expressions.h
 * @brief   RobotEyes 8 大灵魂微表情 v7.0 — 参数化眉毛动画引擎 + 泪滴坐标映射
 *
 *  v7.0 关键升级:
 *    - BrowAnimation_t: 6 种眉毛动画类型 (参数化, OCP)
 *    - ExpressionDef_t 扩展: brow_anim / brow_freq / brow_amp 等动画参数
 *    - Angry 高频震颤: sin() 载波 + 周期性爆发 burst
 *    - Sad 泪滴: 眼睑边缘 clamp + 相位错落
 *    - eye_expr_update 动画引擎: 硬编码 if/else → switch 分派 (参数驱动)
 */

#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include <stdint.h>
#include "eye_renderer.h"

/* ================================================================
 *  BrowAnimation_t — 眉毛动画类型 (OCP: 新增 = 加枚举 + 引擎 case)
 * ================================================================ */
typedef enum {
    BROW_ANIM_NONE = 0,     /* 静态, 无动画 */
    BROW_ANIM_BREATHE,      /* sin 慢呼吸 (~0.3Hz, ±2deg) */
    BROW_ANIM_TREMBLE,      /* 高频震颤 + 爆发 burst (愤怒) */
    BROW_ANIM_SOB,          /* 抽泣波 + 左右错相 (悲伤) */
    BROW_ANIM_RAISE_BOUNCE, /* 上扬弹跳 (开心/惊讶: 峰值后回弹) */
    BROW_ANIM_SAG_DRIFT,    /* 无力下垂 + 微漂移 (困倦) */
} BrowAnimation_t;

/* ================================================================
 *  ExpressionDef_t — 表情定义 (参数化眉毛动画)
 *
 *  新增字段 (v7.0):
 *    brow_anim        动画类型 (BrowAnimation_t)
 *    brow_freq        基频 (弧度/帧, 典型 0.02~0.20)
 *    brow_amp         振幅 (度, 典型 1~8)
 *    brow_asymmetry   左右非对称系数 (-1.0~1.0, 0=对称)
 *    brow_burst_amp   爆发振幅 (仅 TREMBLE 使用)
 *    brow_burst_intv  爆发间隔 (ms, 仅 TREMBLE 使用)
 *    tear_enabled     是否启用泪滴渲染 (Sad)
 *    tear_rate        泪滴滑落速率 (px/ms)
 *    tear_spacing     双泪滴初始间距 (px)
 * ================================================================ */
typedef struct {
    const char*     name;

    /* ---- 眼皮 ---- */
    float           lid_top;        /* 对称上眼皮 (0=全开, 1=全闭) */
    float           lid_top_l;      /* 左眼上眼皮 (非对称用) */
    float           lid_top_r;      /* 右眼上眼皮 (非对称用) */
    float           lid_bottom;     /* 下眼皮 */
    float           lid_slope;      /* 倾斜斜率 (-1=外角下垂, +1=内角下垂) */

    /* ---- 瞳孔 ---- */
    PupilType_t     pupil_type;     /* 瞳孔变异类型 */
    float           pupil_scale;    /* 瞳孔基础缩放 (1.0=正常) */
    float           anim_peak;      /* 动画峰值缩放 (0=无动画) */
    uint16_t        anim_ms;        /* 动画持续时间 (ms) */

    /* ---- 眉毛静态角度 ---- */
    int8_t          brow_left;      /* 左眉基础角度 */
    int8_t          brow_right;     /* 右眉基础角度 */

    /* ---- 眉毛动画参数 (v7.0) ---- */
    BrowAnimation_t brow_anim;          /* 动画类型 */
    float           brow_freq;          /* 基频 (弧度/帧) */
    float           brow_amp;           /* 振幅 (度) */
    float           brow_asymmetry;     /* 左右非对称系数 */
    float           brow_burst_amp;     /* 爆发振幅 (TREMBLE) */
    uint16_t        brow_burst_intv;    /* 爆发间隔 ms (TREMBLE) */

    /* ---- 泪滴参数 (v7.0) ---- */
    bool            tear_enabled;       /* 启用泪滴 */
    float           tear_rate;          /* 滑落速率 (px/ms) */
    uint8_t         tear_spacing;       /* 双泪滴初始间距 (px) */
} ExpressionDef_t;

#define BROW_CENTER  90
#define SYM_L(offset)  ((int8_t)(BROW_CENTER - (offset)))
#define SYM_R(offset)  ((int8_t)(BROW_CENTER + (offset)))

/* ================================================================
 *  8 大灵魂微表情表
 *
 *  每个表情携带完整的瞳孔类型 + 眉毛动画参数,
 *  eye_expr_update 引擎通过 brow_anim 字段分派,
 *  无需硬编码 if/else。
 * ================================================================ */
static const ExpressionDef_t EXPRESSIONS[8] = {

    /* [0] Normal — 平静注视
     *   瞳孔: 标准圆, 眉毛: sin 慢呼吸 ±2deg */
    { "Normal",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_NORMAL, 1.0f,  0.0f, 0,
      SYM_L(0), SYM_R(0),
      BROW_ANIM_BREATHE, 0.02f, 2.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [1] Happy — 极致开心 > <
     *   瞳孔: PUPIL_HAPPY 笑眼弯弯, 弹跳 1.2x→1.8x
     *   眉毛: 上扬弹跳 30° → 回弹至 +10° */
    { "Happy",
      0.0f, 0.0f, 0.0f,  0.65f, 0.15f,
      PUPIL_HAPPY, 1.2f,  1.8f, 300,
      SYM_L(30), SYM_R(30),
      BROW_ANIM_RAISE_BOUNCE, 0.03f, 6.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [2] Angry — 极致凶狠 ◣◢
     *   瞳孔: PUPIL_SLIT 竖缝, 缩放 0.6x→0.35x
     *   眉毛: BROW_ANIM_TREMBLE 高频震颤
     *     sin 载波 f=0.15rad/帧 (~2.4Hz), ±3deg 基础颤
     *     每 800ms 爆发一次 burst: 正弦包络, ±8deg 尖峰
     *     左右眉反相 (asymmetry=0.5) → 交替颤抖 */
    { "Angry",
      0.28f, 0.0f, 0.0f,  0.18f, 0.90f,
      PUPIL_SLIT, 0.6f,  0.35f, 200,
      SYM_L(-40), SYM_R(-40),
      BROW_ANIM_TREMBLE, 0.15f, 3.0f, 0.5f, 8.0f, 800,
      false, 0.0f, 0 },

    /* [3] Sad — 汪洋泪海 T_T
     *   瞳孔: PUPIL_NORMAL, 1.4x→1.8x 泪眼朦胧
     *   眉毛: BROW_ANIM_SOB 抽泣波
     *     sin 基频 0.014rad/帧 (~0.22Hz 慢抽泣)
     *     左右交错: 左眉 sin(phase), 右眉 sin(phase+2.0)
     *   泪滴: tear_enabled=true, 双泪滴错落滑落
     *     速率 0.010 px/ms, 间距 12px */
    { "Sad",
      0.18f, 0.0f, 0.0f,  0.28f, -0.65f,
      PUPIL_NORMAL, 1.4f,  1.8f, 450,
      SYM_L(20), SYM_R(20),
      BROW_ANIM_SOB, 0.014f, 2.5f, 1.0f, 0.0f, 0,
      true, 0.010f, 12 },

    /* [4] Surprised — 中空瞳孔地震 !!
     *   瞳孔: PUPIL_SHOCK 中空圆环 + 八向电波
     *   眉毛: 上扬弹跳 50° → 回弹至 +25° */
    { "Surprised",
      0.0f, 0.0f, 0.0f,  -0.10f, 0.0f,
      PUPIL_SHOCK, 1.0f,  0.3f, 300,
      SYM_L(50), SYM_R(50),
      BROW_ANIM_RAISE_BOUNCE, 0.04f, 8.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [5] Sleepy — 锯齿缓动瞌睡
     *   瞳孔: PUPIL_NORMAL, 缩放 0.7x
     *   眉毛: BROW_ANIM_SAG_DRIFT 无力下垂 + 漂移 */
    { "Sleepy",
      0.60f, 0.0f, 0.0f,  0.06f, 0.0f,
      PUPIL_NORMAL, 0.7f,  0.0f, 0,
      SYM_L(-12), SYM_R(-12),
      BROW_ANIM_SAG_DRIFT, 0.01f, 3.0f, 0.3f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [6] Skeptic — 极致大小眼
     *   瞳孔: PUPIL_NORMAL 缩放 0.65x
     *   眉毛: 静态非对称 (左 35°高挑, 右 -20°微垂)
     *   动画: BROW_ANIM_BREATHE 微弱呼吸叠加 */
    { "Skeptic",
      0.0f, 0.0f, 0.75f,  0.25f, 0.15f,
      PUPIL_NORMAL, 0.65f,  0.5f, 250,
      SYM_L(35), SYM_R(-20),
      BROW_ANIM_BREATHE, 0.015f, 1.0f, 0.8f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [7] Excited — 完美跃动爱心
     *   瞳孔: PUPIL_HEART, 弹跳 1.3x→2.5x
     *   眉毛: 上扬弹跳 35° */
    { "Excited",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_HEART, 1.3f,  2.5f, 450,
      SYM_L(35), SYM_R(35),
      BROW_ANIM_RAISE_BOUNCE, 0.05f, 5.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },
};

/* ---- ADC_KEY_MAP ---- */
typedef struct { uint16_t min; uint16_t max; uint8_t expr_index; } AdcKeyMap_t;
static const AdcKeyMap_t ADC_KEY_MAP[] = {
    { 3600, 4095, 0 }, { 3000, 3600, 1 }, { 2550, 3000, 2 },
    { 2150, 2550, 3 }, { 1750, 2150, 4 }, { 1300, 1750, 5 },
    {  800, 1300, 6 }, {  450,  800, 7 },
};
#define ADC_KEY_MAP_COUNT (sizeof(ADC_KEY_MAP) / sizeof(ADC_KEY_MAP[0]))
#define ADC_KEY_NONE_MIN  0
#define ADC_KEY_NONE_MAX  350
#define ADC_LONG_PRESS_MS 500

#endif /* EXPRESSIONS_H */
