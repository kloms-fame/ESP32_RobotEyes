/**
 * @file    expressions.h
 * @brief   RobotEyes 8 大灵魂微表情 v6.2 — 笑眼/眉颤/泪海/中空瞳孔/锯齿瞌睡
 *
 *  v6.2 关键升级:
 *    - PUPIL_HAPPY: 笑形瞳孔 (> < 弯弯眯眼)
 *    - 眉毛微动引擎: sin呼吸 + Angry爆发式眉颤
 *    - Sad 双泪滴: 错落滑落 + 眼底积水反光 + 抽泣抖动
 *    - Surprised 中空圆环瞳孔 + 八向电波
 *    - Sleepy 锯齿瞌睡: 缓慢合→惊醒→再合
 *    - Skeptic 极致非对称: 左眼全开/右眼75%眯起
 *    - Excited 完美爱心: 上半圆+填充+饱满倒三角
 */

#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include <stdint.h>
#include "eye_renderer.h"

typedef struct {
    const char*     name;
    float           lid_top;        /* 对称上眼皮 */
    float           lid_top_l;      /* 左眼上眼皮 */
    float           lid_top_r;      /* 右眼上眼皮 */
    float           lid_bottom;     /* 下眼皮 */
    float           lid_slope;      /* 倾斜斜率 */
    PupilType_t     pupil_type;
    float           pupil_scale;
    float           anim_peak;
    uint16_t        anim_ms;
    int8_t          brow_left;
    int8_t          brow_right;
} ExpressionDef_t;

#define BROW_CENTER  90
#define SYM_L(offset)  ((int8_t)(BROW_CENTER - (offset)))
#define SYM_R(offset)  ((int8_t)(BROW_CENTER + (offset)))

static const ExpressionDef_t EXPRESSIONS[8] = {

    /* [0] Normal */
    { "Normal",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_NORMAL, 1.0f,  0.0f, 0,
      SYM_L(0), SYM_R(0) },

    /* [1] Happy — 极致开心 > <
     *   笑形瞳孔 PUPIL_HAPPY, 下眼皮 0.65 大弯眼
     *   瞳孔弹跳 1.2x→1.8x, 眉毛高扬 30° */
    { "Happy",
      0.0f, 0.0f, 0.0f,  0.65f, 0.15f,
      PUPIL_HAPPY, 1.2f,  1.8f, 300,
      SYM_L(30), SYM_R(30) },

    /* [2] Angry — 极致凶狠 ◣◢
     *   竖缝瞳孔更细, 斜率 0.9 极陡, 上下眼皮同时压
     *   眉毛深压 -40° + 爆发式眉颤 */
    { "Angry",
      0.28f, 0.0f, 0.0f,  0.18f, 0.90f,
      PUPIL_SLIT, 0.6f,  0.35f, 200,
      SYM_L(-40), SYM_R(-40) },

    /* [3] Sad — 汪洋泪海 T_T
     *   外眼角大幅下垂 -0.65, 瞳孔 1.4x→1.8x 泪眼朦胧
     *   双泪滴错落滑落 + 眼底积水 + 抽泣微抖 */
    { "Sad",
      0.18f, 0.0f, 0.0f,  0.28f, -0.65f,
      PUPIL_NORMAL, 1.4f,  1.8f, 450,
      SYM_L(20), SYM_R(20) },

    /* [4] Surprised — 中空瞳孔地震 !!
     *   PUPIL_SHOCK: 中空圆环 + 八向电波线交替闪烁
     *   眉毛飞出天际 50°, 动画 0.3x→1.0x */
    { "Surprised",
      0.0f, 0.0f, 0.0f,  -0.10f, 0.0f,
      PUPIL_SHOCK, 1.0f,  0.3f, 300,
      SYM_L(50), SYM_R(50) },

    /* [5] Sleepy — 锯齿缓动瞌睡
     *   锯齿波: 缓慢合→惊醒→再合 (eye_expr_update 驱动)
     *   lid_top 基础值由引擎动态覆盖, 眉毛无力微垂 */
    { "Sleepy",
      0.60f, 0.0f, 0.0f,  0.06f, 0.0f,
      PUPIL_NORMAL, 0.7f,  0.0f, 0,
      SYM_L(-12), SYM_R(-12) },

    /* [6] Skeptic — 极致大小眼 ¬_¬
     *   左眼全开(0.0f), 右眼 0.75 狠狠眯起
     *   非对称眉毛: 左 35°高挑, 右 -20°微垂 */
    { "Skeptic",
      0.0f, 0.0f, 0.75f,  0.25f, 0.15f,
      PUPIL_NORMAL, 0.65f,  0.5f, 250,
      SYM_L(35), SYM_R(-20) },

    /* [7] Excited — 完美跃动爱心 ♥
     *   绝美像素爱心: 上半圆 + 填充 + 饱满倒三角
     *   弹跳 1.3x→2.5x 超大幅弹性, 眉毛高飞 35° */
    { "Excited",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_HEART, 1.3f,  2.5f, 450,
      SYM_L(35), SYM_R(35) },
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

#endif
