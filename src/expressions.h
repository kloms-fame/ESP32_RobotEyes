/**
 * @file    expressions.h
 * @brief   RobotEyes 表情定义表 — 8种预设表情 (眼型 + 眉毛 + 动画)
 * @note    基于 2026-07-09 ADC 键盘标定: S1(3600-4095) → S8(450-800)
 *
 * 表情切换机制:
 *   - 眼型参数走 lerp 平滑过渡 (复用 eye_renderer 的 LOOK_SMOOTH_FACTOR)
 *   - 眉毛角度通过 servo_set_target() 推送, ServoTask 自行非阻塞步进
 *   - 特殊动画 (anim_peak > 0): pupil_scale 先跳到峰值再回落到目标值
 *
 * 时序设计 (防止"长按才响应"):
 *   - ADC 边沿检测: NONE→KEY 跳变后 30ms 去抖确认, 立即触发
 *   - 不积累稳定周期, 不要求持续按住
 *   - 长按判定是独立计时器, 不与触发判定混合
 */

#ifndef EXPRESSIONS_H
#define EXPRESSIONS_H

#include <stdint.h>
/* 眉毛基准角度 (SERVO_CENTER_DEG=90) */

/* ================================================================
 *  ExpressionDef_t — 单条表情定义
 *
 *  字段说明:
 *    name         : 表情名称 (调试用)
 *    lid_top      : 上眼皮遮挡比例 0.0(全开) ~ 1.0(全闭)
 *    lid_bottom   : 下眼皮遮挡比例 0.0(正常) ~ 1.0(全抬)
 *    pupil_scale  : 瞳孔缩放倍数 (1.0=默认, <1.0=小瞳孔/愤怒, >1.0=大瞳孔/惊讶)
 *    brow_left    : 左眉毛舵机角度 (45-135)
 *    brow_right   : 右眉毛舵机角度 (45-135)
 *    anim_peak    : 特殊动画峰值 pupil_scale, 0=无动画
 *    anim_ms      : 特殊动画持续时间 (ms)
 * ================================================================ */
typedef struct {
    const char* name;
    float   lid_top;
    float   lid_bottom;
    float   pupil_scale;
    int8_t  brow_left;
    int8_t  brow_right;
    float   anim_peak;
    uint16_t anim_ms;
} ExpressionDef_t;

/* ================================================================
 *  EXPRESSIONS[8] — 表情定义表
 *
 *  索引 0-7 对应 ADC 按键 S1-S8
 * ================================================================ */
static const ExpressionDef_t EXPRESSIONS[8] = {
    /* [0] S1 — Normal 默认圆眼 */
    {
        "Normal",
        0.00f,   /* lid_top: 全开 */
        0.00f,   /* lid_bottom: 正常 */
        1.00f,   /* pupil_scale: 默认 */
        90, 90,  /* 眉毛居中 */
        0.00f, 0  /* 无特殊动画 */
    },
    /* [1] S2 — Happy 开心 ^_^ (下眼皮上抬) */
    {
        "Happy",
        0.00f,   /* lid_top: 全开 */
        0.22f,   /* lid_bottom: 下眼皮上抬, 形成弯月眼 */
        1.00f,   /* pupil_scale: 正常 */
        100, 100,  /* 眉毛上扬 */
        0.00f, 0
    },
    /* [2] S3 — Angry 生气 (上眼皮下压 + 小瞳孔 + 眉毛下压) */
    {
        "Angry",
        0.18f,   /* lid_top: 上眼皮下压 */
        0.00f,   /* lid_bottom: 正常 */
        0.65f,   /* pupil_scale: 小瞳孔 */
        75, 75,  /* 眉毛下压 */
        0.00f, 0
    },
    /* [3] S4 — Sad 伤心 (上眼皮微垂 + 眉毛下压) */
    {
        "Sad",
        0.12f,   /* lid_top: 上眼皮微垂 */
        0.00f,   /* lid_bottom: 正常 */
        0.80f,   /* pupil_scale: 略小 */
        80, 80,  /* 眉毛微垂 */
        0.00f, 0
    },
    /* [4] S5 — Surprised 惊讶 (大瞳孔 + 眉毛高抬 + 动画: 瞳孔先放大再回缩) */
    {
        "Surprised",
        0.00f,   /* lid_top: 全开 */
        0.00f,   /* lid_bottom: 正常 */
        1.40f,   /* pupil_scale: 大瞳孔 (持续) */
        105, 105,  /* 眉毛高抬 */
        1.80f,   /* anim_peak: 动画峰值瞳孔 1.8x */
        400      /* anim_ms: 动画持续 400ms */
    },
    /* [5] S6 — Sleepy 困倦 (上眼皮半闭 + 眉毛放松) */
    {
        "Sleepy",
        0.45f,   /* lid_top: 半闭眼 */
        0.00f,   /* lid_bottom: 正常 */
        0.75f,   /* pupil_scale: 略小 */
        85, 85,  /* 眉毛略垂 */
        0.00f, 0
    },
    /* [6] S7 — Squint 眯眼 (上下眼皮同时内收 + 眉毛微垂) */
    {
        "Squint",
        0.25f,   /* lid_top: 上眼皮微压 */
        0.15f,   /* lid_bottom: 下眼皮微抬 */
        0.60f,   /* pupil_scale: 小瞳孔 */
        85, 85,  /* 眉毛微垂 */
        0.00f, 0
    },
    /* [7] S8 — Curious 好奇 (大瞳孔 + 不对称眉毛: 一高一低) */
    {
        "Curious",
        0.00f,   /* lid_top: 全开 */
        0.00f,   /* lid_bottom: 正常 */
        1.20f,   /* pupil_scale: 略大 */
        95, 85,  /* 不对称眉毛 */
        0.00f, 0
    },
};

/* ================================================================
 *  ADC_KEY_MAP — 标定区间 (基于 2026-07-09 串口实测, 已二次验证正确)
 *
 *  分压结构: 上拉 → S1(最靠近VCC) → S2 → ... → S8(最靠近GND) → GND
 *  按下时 ADC 值升高, S1 最高 ~3900, S8 最低 ~580
 *  各区间之间留 ≥200 安全间距
 *
 *  ⚠️ 硬件红线 — 如需修改按键映射, 必须先重新标定 ADC 区间!
 * ================================================================ */
typedef struct {
    uint16_t min;
    uint16_t max;
    uint8_t  expr_index;  /* EXPRESSIONS[] 索引 (0-7) */
} AdcKeyMap_t;

static const AdcKeyMap_t ADC_KEY_MAP[] = {
    { 3600, 4095, 0 },  /* S1 → Normal  (实测 ~3833-3934) */
    { 3000, 3600, 1 },  /* S2 → Happy   (实测 ~3159-3255) */
    { 2550, 3000, 2 },  /* S3 → Angry   (实测 ~2700-2805) */
    { 2150, 2550, 3 },  /* S4 → Sad     (实测 ~2288-2353) */
    { 1750, 2150, 4 },  /* S5 → Surprised (实测 ~1890-1932) */
    { 1300, 1750, 5 },  /* S6 → Sleepy  (实测 ~1448-1468) */
    {  800, 1300, 6 },  /* S7 → Squint  (实测 ~992-1015) */
    {  450,  800, 7 },  /* S8 → Curious (实测 ~558-594) */
};
#define ADC_KEY_MAP_COUNT (sizeof(ADC_KEY_MAP) / sizeof(ADC_KEY_MAP[0]))

/* ---- 无按键区间 ---- */
#define ADC_KEY_NONE_MIN  0
#define ADC_KEY_NONE_MAX  350

#endif /* EXPRESSIONS_H */
