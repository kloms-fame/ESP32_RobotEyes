/**
 * @file    expressions.h
 * @brief   RobotEyes 8 大灵魂微表情 v10.0 �?参数化眉毛动画引�?+ 泪滴坐标映射
 *
 *  v10.0 关键升级:
 *    - 【核心修复】brow_left/right �?int8_t 升级�?int16_t
 *      int8_t 最�?27, Angry SYM_L(-45)=135 溢出�?121 �?镜像崩塌!
 *    - SYM_L/SYM_R 宏同步升�?int16_t
 *    - 表情参数大规模调�?(视觉冲击力重�?
 *    - Happy: 弯月笑眼 + 高频星星粒子 + 弹跳眉毛
 *    - Angry: int16_t修复�?\ / 镜像正确 + 高频颤抖
 *    - Sad: 汪汪泪眼 大泪�?+ 眼角抽泣 + 水光反射
 *    - Surprised: 高频大小眼交替跳�?+ 眉毛跷跷板摇�? *    - Sleepy: 抗拒困意状态机 (闭眼皱眉→惊醒弹开 眉毛联动)
 *    - Panic: 极度慌张 无规律乱�?+ 急促扫视 + 大汗�? *    - Excited: 超大爱心瞳孔 + 双相心跳(lub-dub)
 */

#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include <stdint.h>
#include "eye_renderer.h"

/* ================================================================
 *  BrowAnimation_t �?眉毛动画类型 (OCP: 新增 = 加枚�?+ 引擎 case)
 * ================================================================ */
typedef enum {
    BROW_ANIM_NONE = 0,     /* 静�? 无动�?*/
    BROW_ANIM_BREATHE,      /* sin 慢呼�?(~0.3Hz, ±2deg) */
    BROW_ANIM_TREMBLE,      /* 高频震颤 + 爆发 burst (愤�?恐慌) */
    BROW_ANIM_SOB,          /* 抽泣�?+ 左右错相 (悲伤) */
    BROW_ANIM_RAISE_BOUNCE, /* 上扬弹跳 (开�?惊讶: 峰值后回弹) */
    BROW_ANIM_SAG_DRIFT,    /* 无力下垂 + 微漂�?(困�? */
    BROW_ANIM_TWITCH,       /* 随机单侧抽动 (怠速微动作) */
    BROW_ANIM_SWAY,         /* 跷跷板式左右反相摇摆 (Surprised) */
    BROW_ANIM_PANIC,        /* 高频恐慌颤抖 (Panic) */
} BrowAnimation_t;

/* ================================================================
 *  ExpressionDef_t �?表情定义 (v10: int16_t 防溢�?
 * ================================================================ */
typedef struct {
    const char*     name;

    /* ---- 眼皮 ---- */
    float           lid_top;        /* 对称上眼�?(0=全开, 1=全闭) */
    float           lid_top_l;      /* 左眼上眼�?(非对称用) */
    float           lid_top_r;      /* 右眼上眼�?(非对称用) */
    float           lid_bottom;     /* 下眼�?*/
    float           lid_slope;      /* 倾斜斜率 (-1=外角下垂, +1=内角下垂) */

    /* ---- 瞳孔 ---- */
    PupilType_t     pupil_type;     /* 瞳孔变异类型 */
    float           pupil_scale;    /* 瞳孔基础缩放 (1.0=正常) */
    float           anim_peak;      /* 动画峰值缩�?(0=无动�? */
    uint16_t        anim_ms;        /* 动画持续时间 (ms) */

    /* ---- 眉毛静态角�?(v10: int16_t 防溢�? 支持0-180全范�? ---- */
    int16_t         brow_left;      /* 左眉基础角度 */
    int16_t         brow_right;     /* 右眉基础角度 */

    /* ---- 眉毛动画参数 ---- */
    BrowAnimation_t brow_anim;          /* 动画类型 */
    float           brow_freq;          /* 基频 (弧度/�? */
    float           brow_amp;           /* 振幅 (�? */
    float           brow_asymmetry;     /* 左右非对称系�?*/
    float           brow_burst_amp;     /* 爆发振幅 (TREMBLE) */
    uint16_t        brow_burst_intv;    /* 爆发间隔 ms (TREMBLE) */

    /* ---- 泪滴参数 ---- */
    bool            tear_enabled;       /* 启用泪滴 */
    float           tear_rate;          /* 滑落速率 (px/ms) */
    uint8_t         tear_spacing;       /* 双泪滴初始间�?(px) */
} ExpressionDef_t;

#define BROW_CENTER  90
/* v10: int16_t 强制转换, 防溢�? int8_t最�?27, 135会溢出为-121 */
#define SYM_L(offset)  ((int16_t)(BROW_CENTER - (offset)))
#define SYM_R(offset)  ((int16_t)(BROW_CENTER + (offset)))

/* ================================================================
 *  8 大灵魂微表情�?v10.0 (视觉冲击力极致重�?
 * ================================================================ */
static const ExpressionDef_t EXPRESSIONS[8] = {

    /* [0] Normal �?自然灵动, 微呼�?+ 注意力漂�?*/
    { "Normal",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_NORMAL, 1.0f,  0.0f, 0,
      SYM_L(0), SYM_R(0),
      BROW_ANIM_BREATHE, 0.018f, 2.5f, 0.25f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [1] Comical Smile �� ��������Ц: ������Ƥ�л����� + ͫ��300ms���� */
    { "Smile",  0.50f, 0.50f, 0.50f,  0.28f, 0.12f, PUPIL_NORMAL, 0.65f,  0.0f, 0, SYM_L(35), SYM_R(35), BROW_ANIM_NONE, 0.0f, 0.0f, 0.0f, 0.0f, 0, false, 0.0f, 0 },

    /* [2] Angry �?倒八字眉 \ / 高频颤抖 + 竖缝瞳孔 (v10: int16_t修复镜像) */
    { "Angry",
      0.28f, 0.0f, 0.0f,  0.10f, 0.85f,
      PUPIL_SLIT, 0.50f,  0.35f, 220,
      SYM_L(-45), SYM_R(-45),
      BROW_ANIM_TREMBLE, 0.22f, 4.5f, 0.55f, 12.0f, 500,
      false, 0.0f, 0 },

    /* [3] Sad �?汪汪泪眼 大泪�?+ 眼角抽泣 + 水光反射 */
    { "Sad",
      0.12f, 0.0f, 0.0f,  0.35f, -0.85f,
      PUPIL_NORMAL, 1.6f,  2.5f, 600,
      SYM_L(28), SYM_R(28),
      BROW_ANIM_SOB, 0.008f, 3.5f, 1.4f, 0.0f, 0,
      true, 0.018f, 20 },

    /* [4] Surprised �?高频大小眼交替跳�?+ 眉毛跷跷板摇�?*/
    { "Surprised",0.0f, 0.0f, 0.0f,  -0.18f, 0.0f,
      PUPIL_SHOCK, 1.00f,  0.0f, 0,
      SYM_L(58), SYM_R(58),
      BROW_ANIM_SWAY, 0.08f, 12.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [5] Sleepy �?抗拒困意状态机: 闭眼皱眉→惊醒弹开 (v10: 眉毛联动) */
    { "Sleepy", 0.55f, 0.55f, 0.55f,  0.05f, 0.0f, PUPIL_NORMAL, 0.60f,  0.0f, 0, SYM_L(-15), SYM_R(-15), BROW_ANIM_NONE, 0.0f, 0.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [6] Panic �?极度慌张: 无规律乱�?+ 急促扫视 + 大汗�?*/
    { "Panic",
      0.02f, 0.0f, 0.0f,  0.08f, 0.0f,
      PUPIL_NORMAL, 0.55f,  0.0f, 0,
      SYM_L(42), SYM_R(42),
      BROW_ANIM_TREMBLE, 0.28f, 4.0f, 0.45f, 5.0f, 300,
      false, 0.0f, 0 },

    /* [7] Excited �?超大爱心瞳孔 + 双相心跳(lub-dub) + 弹跳眉毛 */
    { "Excited",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_HEART, 1.15f,  0.0f, 0,
      SYM_L(45), SYM_R(45),
      BROW_ANIM_RAISE_BOUNCE, 0.10f, 12.0f, 0.0f, 0.0f, 0,
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
