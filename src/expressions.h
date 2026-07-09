/**
 * @file    expressions.h
 * @brief   RobotEyes 8 大极限夸张微表情 v6.0 — 爱心/竖瞳/倾斜切角
 * @note    基于 2026-07-09 ADC 键盘标定: S1(3600-4095) → S8(450-800)
 *
 *  瞳孔变异系统:
 *    PUPIL_NORMAL - 标准圆形
 *    PUPIL_HEART  - 爱心 ♥ (期待/狂喜)
 *    PUPIL_SLIT   - 竖缝 | (极度愤怒/野兽)
 *    PUPIL_NONE   - 消失 (震惊)
 *
 *  倾斜切角引擎:
 *    lid_slope > 0: 内侧眼角下压 → 倒八字 (◣ ‸ ◢) 怒火
 *    lid_slope < 0: 外侧眼角下垂 → 八字眉 (T_T) 委屈
 *
 *  交互逻辑 (v6.0):
 *    短按 (<500ms): 表情瞬间触发 → 动画播放 → 1.5s 后自动回弹 Normal
 *    长按 (≥500ms): 表情触发并锁定，松手后维持
 *    摇杆: 全程可控制视线 + 眉毛相对微调
 */

#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include <stdint.h>
#include "eye_renderer.h"

/* ================================================================
 *  ExpressionDef_t — 单条表情定义
 * ================================================================ */
typedef struct {
    const char*     name;
    float           lid_top;        /* 上眼皮遮挡 0.0 ~ 1.0 */
    float           lid_bottom;     /* 下眼皮遮挡 0.0 ~ 1.0 */
    float           lid_slope;      /* 眼皮斜率: >0 内压(怒), <0 外垂(委屈) */
    PupilType_t     pupil_type;     /* 瞳孔形状! */
    float           pupil_scale;    /* 瞳孔稳态缩放 */
    float           anim_peak;      /* 动画峰值瞳孔 (制造回弹感) */
    uint16_t        anim_ms;        /* 动画时长 ms */
    int8_t          brow_left;      /* 左眉绝对角度 */
    int8_t          brow_right;     /* 右眉绝对角度 */
} ExpressionDef_t;

/* ---- 辅助宏 ---- */
#define BROW_CENTER  90
#define SYM_L(offset)  ((int8_t)(BROW_CENTER + (offset)))
#define SYM_R(offset)  ((int8_t)(BROW_CENTER - (offset)))

/* ================================================================
 *  EXPRESSIONS[8] — 8 种极限夸张微表情
 * ================================================================ */
static const ExpressionDef_t EXPRESSIONS[8] = {

    /* [0] S1 — Normal 乖巧平视
     *   标准圆瞳, 眉毛平直, 无倾斜, 无动画 */
    {
        "Normal",
        0.0f, 0.0f, 0.0f,     /* 全开 */
        PUPIL_NORMAL, 1.0f,    /* 标准圆瞳 */
        0.0f, 0,               /* 无动画 */
        SYM_L(0), SYM_R(0)     /* 眉毛平直 90°/90° */
    },

    /* [1] S2 — Happy 开心到飞起 ^_^
     *   下眼皮大幅上抬 (笑眼弯弯), 瞳孔放大变萌,
     *   眉毛飞起, 动画: 瞳孔弹到 1.4x → 回弹 1.1x */
    {
        "Happy",
        0.0f, 0.50f, 0.0f,    /* 下眼皮半抬 → ^_^ */
        PUPIL_NORMAL, 1.1f,    /* 瞳孔微大 */
        1.4f, 350,             /* 弹到 1.4x 再回弹 */
        SYM_L(25), SYM_R(25)   /* 眉毛高飞 115°/65° */
    },

    /* [2] S3 — Angry 暴怒 ◣_◢
     *   极高斜率切出锐角倒八字, 竖缝瞳孔 (野兽模式),
     *   眉毛狠狠向内下压 (非对称), 极其有攻击性
     *   动画: 瞳孔瞬间缩到 0.3x → 恢复 0.7x */
    {
        "Angry",
        0.18f, 0.10f, 0.70f,  /* 上眼皮微压 + 极高内眼角下压 */
        PUPIL_SLIT, 0.7f,      /* 竖缝瞳孔! */
        0.3f, 250,             /* 瞬间缩小 → 恢复 */
        SYM_L(-35), SYM_R(-35) /* 眉毛死命下压 55°/125° */
    },

    /* [3] S4 — Sad 委屈大哭 T_T
     *   眼皮向外八字形下垂 (lid_slope <0),
     *   瞳孔放极大 (水汪汪无辜), 眉毛微微上扬
     *   动画: 瞳孔弹到 1.5x → 回落 1.3x */
    {
        "Sad",
        0.12f, 0.20f, -0.45f, /* 外眼角下垂 → 八字眉 T_T */
        PUPIL_NORMAL, 1.3f,    /* 水汪汪大瞳孔 */
        1.5f, 400,             /* 弹到 1.5x → 回落 */
        SYM_L(15), SYM_R(15)   /* 眉毛微挑 105°/75° */
    },

    /* [4] S5 — Surprised 震惊瞳孔地震 O_O
     *   眼皮全开, 瞳孔瞬间消失 (PUPIL_NONE 翻白眼),
     *   然后弹出一个极小瞳孔 → 恢复正常
     *   眉毛飞出天际, 动画: 0 → 0.1x → 0.6x */
    {
        "Surprised",
        0.0f, 0.0f, 0.0f,     /* 全开 */
        PUPIL_NORMAL, 0.6f,    /* 稳态瞳孔略小 (震撼) */
        0.0f, 250,             /* 动画瞳孔为0 (消失→弹出) */
        SYM_L(45), SYM_R(45)   /* 眉毛飞最高 135°/45° */
    },

    /* [5] S6 — Sleepy 困到昏迷 -_-
     *   上眼皮几乎完全闭合 (70%), 眉毛无力下垂,
     *   瞳孔缩小, 无动画 (困了没力气弹) */
    {
        "Sleepy",
        0.70f, 0.12f, 0.0f,   /* 上眼皮几乎闭合 */
        PUPIL_NORMAL, 0.8f,    /* 瞳孔缩小 */
        0.0f, 0,               /* 无动画 */
        SYM_L(-10), SYM_R(-10) /* 眉毛无力 80°/100° */
    },

    /* [6] S7 — Skeptic 极度怀疑 ¬_¬
     *   上下眼皮半闭, 瞳孔缩小, 非对称眉毛 (一高一低),
     *   微倾斜眼皮, 动画: 瞳孔瞬间缩小 → 审视 */
    {
        "Skeptic",
        0.32f, 0.22f, 0.12f,  /* 半闭 + 微内压 */
        PUPIL_NORMAL, 0.7f,    /* 审视小瞳孔 */
        0.4f, 200,             /* 瞬间缩小 → 锐利 */
        SYM_L(30), SYM_R(-15)  /* 非对称 120°/105° */
    },

    /* [7] S8 — Excited 狂喜星星眼 ♥_♥
     *   瞳孔瞬间变成爱心! 跳动到巨大的 2.2 倍!
     *   眉毛飞起, 500ms 慢弹性回弹 */
    {
        "Excited",
        0.0f, 0.0f, 0.0f,     /* 全开 */
        PUPIL_HEART, 1.5f,     /* 爱心瞳孔! 稳态 1.5x */
        2.2f, 500,             /* 弹跳到 2.2x → 慢回 1.5x */
        SYM_L(30), SYM_R(30)   /* 眉毛高飞 120°/60° */
    },
};

/* ================================================================
 *  ADC_KEY_MAP — 标定区间 (⚠️ 硬件红线, 实测确认)
 * ================================================================ */
typedef struct {
    uint16_t min;
    uint16_t max;
    uint8_t  expr_index;
} AdcKeyMap_t;

static const AdcKeyMap_t ADC_KEY_MAP[] = {
    { 3600, 4095, 0 },  /* S1 → Normal    (实测 ~3833-3934) */
    { 3000, 3600, 1 },  /* S2 → Happy     (实测 ~3159-3255) */
    { 2550, 3000, 2 },  /* S3 → Angry     (实测 ~2700-2805) */
    { 2150, 2550, 3 },  /* S4 → Sad       (实测 ~2288-2353) */
    { 1750, 2150, 4 },  /* S5 → Surprised (实测 ~1890-1932) */
    { 1300, 1750, 5 },  /* S6 → Sleepy    (实测 ~1448-1468) */
    {  800, 1300, 6 },  /* S7 → Skeptic   (实测 ~992-1015) */
    {  450,  800, 7 },  /* S8 → Excited   (实测 ~558-594) */
};
#define ADC_KEY_MAP_COUNT (sizeof(ADC_KEY_MAP) / sizeof(ADC_KEY_MAP[0]))

#define ADC_KEY_NONE_MIN  0
#define ADC_KEY_NONE_MAX  350

/* ---- 短按/长按阈值 ---- */
#define ADC_LONG_PRESS_MS   500   /* ≥500ms 判定为长按 */

#endif /* EXPRESSIONS_H */
