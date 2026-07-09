/**
 * @file    eye_renderer.h
 * @brief   RobotEyes 眼型渲染 v5.5 — Style A 超宽眼 + 高光防穿模
 */

#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include <stdint.h>
#include <U8g2lib.h>

/* ================================================================
 *  风格选择
 * ================================================================ */
#define EYE_STYLE_A   /* 动漫星瞳: 56×44 超大宽眼 */
// #define EYE_STYLE_B   /* 委屈修勾 */
// #define EYE_STYLE_C   /* 傲娇小兽 */

#if !defined(EYE_STYLE_A) && !defined(EYE_STYLE_B) && !defined(EYE_STYLE_C)
#define EYE_STYLE_A
#endif

/* ================================================================
 *  通用
 * ================================================================ */
#define EYE_CX            64
#define EYE_CY            32
#define FRAME_INTERVAL_MS 33
#define LOOK_SMOOTH_FACTOR 0.22f

/* ================================================================
 *  Style A: 动漫星瞳 — 超宽 56×44
 * ================================================================ */
#ifdef EYE_STYLE_A
#define EYE_W              56
#define EYE_H              44
#define EYE_RADIUS         20
#define PUPIL_SHAPE         0
#define PUPIL_R            10
#define PUPIL_RX           10
#define PUPIL_RY           10
#define LOOK_MAX           16
#define SHINE_PARALLAX     0.55f
#define SHINE1_DX          -6
#define SHINE1_DY          -7
#define SHINE1_R            4
#define SHINE2_DX           7
#define SHINE2_DY           5
#define SHINE2_R            2
#define SHINE3_DX          -2
#define SHINE3_DY           3
#define SHINE3_R            1
#endif

/* ================================================================
 *  Style B: 委屈修勾
 * ================================================================ */
#ifdef EYE_STYLE_B
#define EYE_W              46
#define EYE_H              32
#define EYE_RADIUS         16
#define PUPIL_SHAPE         2
#define PUPIL_R             0
#define PUPIL_RX           10
#define PUPIL_RY            9
#define LOOK_MAX           13
#define SHINE_PARALLAX     0.30f
#define SHINE1_DX          -5
#define SHINE1_DY          -4
#define SHINE1_R            3
#define SHINE2_DX           5
#define SHINE2_DY          -3
#define SHINE2_R            2
#define SHINE3_DX           0
#define SHINE3_DY           0
#define SHINE3_R            0
#endif

/* ================================================================
 *  Style C: 傲娇小兽
 * ================================================================ */
#ifdef EYE_STYLE_C
#define EYE_W              40
#define EYE_H              36
#define EYE_RADIUS         12
#define PUPIL_SHAPE         1
#define PUPIL_R             0
#define PUPIL_RX            6
#define PUPIL_RY           13
#define LOOK_MAX           12
#define SHINE_PARALLAX     0.30f
#define SHINE1_DX          -3
#define SHINE1_DY          -7
#define SHINE1_R            3
#define SHINE2_DX           0
#define SHINE2_DY           0
#define SHINE2_R            0
#define SHINE3_DX           0
#define SHINE3_DY           0
#define SHINE3_R            0
#endif

/* ================================================================
 *  updateDisplayArea() tile 坐标
 *
 *  Style A: W=56, H=44, cx=64, cy=32
 *  x: [36, 92]  y: [10, 54]
 *  tile: tx=4, ty=1, w=8, h=6 → 384 bytes
 * ================================================================ */
#define EYE_TILE_X    4
#define EYE_TILE_Y    1
#define EYE_TILE_W    8
#define EYE_TILE_H    6

/* ================================================================
 *  EyeConfig_t / BlinkState_t
 * ================================================================ */
typedef struct {
    uint8_t cx, cy;        /* 眼睛中心坐标 */
    float   lid;           /* 眨眼遮挡比例 (0.0=全开, 1.0=全闭) */

    /* ---- 视线 ---- */
    int8_t  target_look_x, target_look_y;
    float   cur_look_x, cur_look_y;

    /* ---- 表情参数 (v5.6) ---- */
    uint8_t active_expr;       /* 当前表情索引 (0-7), 255=未设置 */
    float   target_lid_top;    /* 上眼皮目标 */
    float   target_lid_bottom; /* 下眼皮目标 */
    float   target_pupil_scale;/* 瞳孔缩放目标 */

    float   cur_lid_top;       /* 上眼皮当前值 (lerp) */
    float   cur_lid_bottom;    /* 下眼皮当前值 (lerp) */
    float   cur_pupil_scale;   /* 瞳孔缩放当前值 (lerp) */

    /* ---- 特殊动画 (v5.6) ---- */
    float    anim_peak_scale;  /* 动画峰值瞳孔 */
    uint32_t anim_start_ms;    /* 动画开始时间 */
    uint16_t anim_duration_ms; /* 动画持续时间 */
} EyeConfig_t;

typedef enum { BLINK_IDLE = 0, BLINK_CLOSING, BLINK_HOLD, BLINK_OPENING } BlinkPhase_t;

typedef struct {
    BlinkPhase_t phase;
    uint32_t phase_start_ms;
    uint16_t phase_duration_ms;
    uint32_t next_blink_ms;
} BlinkState_t;

#define BLINK_CLOSING_MS   120
#define BLINK_HOLD_MS       35
#define BLINK_OPENING_MS   180
#define BLINK_INTERVAL_MIN 2000
#define BLINK_INTERVAL_MAX 6000

void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy);
void eye_set_look(EyeConfig_t* cfg, int8_t x, int8_t y);
void eye_look_update(EyeConfig_t* cfg);
void eye_look_reset(EyeConfig_t* cfg);

/* ---- 表情切换 (v5.6) ---- */
void eye_set_expression(EyeConfig_t* cfg, uint8_t expr_index);
void eye_expr_update(EyeConfig_t* cfg, uint32_t now_ms);
void blink_state_init(BlinkState_t* state);
void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms);
void eye_render(U8G2* disp, EyeConfig_t* cfg);

#endif
