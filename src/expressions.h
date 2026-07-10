/**
 * @file    expressions.h
 * @brief   RobotEyes 8 大灵魂微表情 v9.0 — 参数化眉毛动画引擎 + 泪滴坐标映射
 *
 *  v9.0 关键升级:
 *    - 新增 BROW_ANIM_SWAY (Surprised 左右反相摇摆)
 *    - 新增 BROW_ANIM_PANIC (Panic 高频颤抖)
 *    - Skeptic[6] 替换为 Panic (恐慌)
 *    - Excited[7] pupil_type → PUPIL_HEART (双相心跳)
 *    - Sleepy[5] brow_anim → NONE (眉毛由专属节律控制)
 *    - Surprised[4] brow_anim → SWAY (左右反相摇摆)
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
    BROW_ANIM_TREMBLE,      /* 高频震颤 + 爆发 burst (愤怒/恐慌) */
    BROW_ANIM_SOB,          /* 抽泣波 + 左右错相 (悲伤) */
    BROW_ANIM_RAISE_BOUNCE, /* 上扬弹跳 (开心/惊讶: 峰值后回弹) */
    BROW_ANIM_SAG_DRIFT,    /* 无力下垂 + 微漂移 (困倦) */
    BROW_ANIM_TWITCH,       /* 随机单侧抽动 (怠速微动作) */
    BROW_ANIM_SWAY,         /* 跷跷板式左右反相摇摆 (Surprised) */
    BROW_ANIM_PANIC,        /* 高频恐慌颤抖 (Panic) */
} BrowAnimation_t;

/* ================================================================
 *  ExpressionDef_t — 表情定义 (参数化眉毛动画)
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

    /* ---- 眉毛动画参数 (v8.0) ---- */
    BrowAnimation_t brow_anim;          /* 动画类型 */
    float           brow_freq;          /* 基频 (弧度/帧) */
    float           brow_amp;           /* 振幅 (度) */
    float           brow_asymmetry;     /* 左右非对称系数 */
    float           brow_burst_amp;     /* 爆发振幅 (TREMBLE) */
    uint16_t        brow_burst_intv;    /* 爆发间隔 ms (TREMBLE) */

    /* ---- 泪滴参数 (v8.0) ---- */
    bool            tear_enabled;       /* 启用泪滴 */
    float           tear_rate;          /* 滑落速率 (px/ms) */
    uint8_t         tear_spacing;       /* 双泪滴初始间距 (px) */
} ExpressionDef_t;

#define BROW_CENTER  90
#define SYM_L(offset)  ((int8_t)(BROW_CENTER - (offset)))
#define SYM_R(offset)  ((int8_t)(BROW_CENTER + (offset)))

/* ================================================================
 *  8 大灵魂微表情表 v9.0
 * ================================================================ */
static const ExpressionDef_t EXPRESSIONS[8] = {

    /* [0] Normal — 自然灵动, 微呼吸 + 注意力漂移 */
    { "Normal",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_NORMAL, 1.0f,  0.0f, 0,
      SYM_L(0), SYM_R(0),
      BROW_ANIM_BREATHE, 0.018f, 2.5f, 0.25f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [1] Happy — 极致开心 弯月笑眼 + 弹跳眉毛 */
    { "Happy",
      0.22f, 0.0f, 0.0f,  0.85f, 0.25f,
      PUPIL_HAPPY, 1.15f,  2.5f, 400,
      SYM_L(45), SYM_R(45),
      BROW_ANIM_RAISE_BOUNCE, 0.05f, 12.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [2] Angry — 极致凶狠 吊梢眼 + 抽动式震颤 */
    { "Angry",
      0.35f, 0.0f, 0.0f,  0.12f, 1.15f,
      PUPIL_SLIT, 0.55f,  0.30f, 250,
      SYM_L(-45), SYM_R(-45),
      BROW_ANIM_TREMBLE, 0.18f, 3.5f, 0.6f, 10.0f, 600,
      false, 0.0f, 0 },

    /* [3] Sad — 汪洋泪海 T_T 多层泪水 */
    { "Sad",
      0.15f, 0.0f, 0.0f,  0.32f, -0.75f,
      PUPIL_NORMAL, 1.5f,  2.2f, 500,
      SYM_L(25), SYM_R(25),
      BROW_ANIM_SOB, 0.010f, 3.0f, 1.2f, 0.0f, 0,
      true, 0.012f, 16 },

    /* [4] Surprised — 四阶段大小眼交替 O_O (v9.0: SWAY 摇摆) */
    { "Surprised",
      0.0f, 0.0f, 0.0f,  -0.12f, 0.0f,
      PUPIL_SHOCK, 1.0f,  0.25f, 350,
      SYM_L(55), SYM_R(55),
      BROW_ANIM_SWAY, 0.05f, 8.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [5] Sleepy — 四秒打瞌睡循环 (v9.0: 眉毛专属节律, NONE避开通用引擎) */
    { "Sleepy",
      0.60f, 0.0f, 0.0f,  0.05f, 0.0f,
      PUPIL_NORMAL, 0.65f,  0.0f, 0,
      SYM_L(-15), SYM_R(-15),
      BROW_ANIM_NONE, 0.0f, 0.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [6] Panic — 恐慌 (v9.0: 替换 Skeptic) 急促呼吸式瞳孔 + 高频扫视 */
    { "Panic",
      0.05f, 0.0f, 0.0f,  0.10f, 0.0f,
      PUPIL_NORMAL, 0.75f,  0.0f, 0,
      SYM_L(35), SYM_R(35),
      BROW_ANIM_TREMBLE, 0.22f, 2.5f, 0.5f, 4.0f, 350,
      false, 0.0f, 0 },

    /* [7] Excited — 爱心眼 双相心跳 (v9.0: HEART 代替 STAR) */
    { "Excited",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_HEART, 1.0f,  0.0f, 0,
      SYM_L(40), SYM_R(40),
      BROW_ANIM_RAISE_BOUNCE, 0.07f, 8.0f, 0.0f, 0.0f, 0,
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
