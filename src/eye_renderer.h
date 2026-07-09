/**
 * @file    eye_renderer.h
 * @brief   RobotEyes 双眼几何渲染器 + 眨眼 + 微表情 v3
 */

#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include <stdint.h>
#include <U8g2lib.h>

/* ================================================================
 *  眼型参数
 * ================================================================ */
#define EYE_RX            20    /* 椭圆半宽 */
#define EYE_RY            20    /* 椭圆半高 (全睁) */
#define PUPIL_RADIUS      10    /* 瞳孔半径 */
#define SHINE1_DX         -5
#define SHINE1_DY         -5
#define SHINE1_R           3
#define SHINE2_DX          7
#define SHINE2_DY          3
#define SHINE2_R           2

/* ================================================================
 *  updateDisplayArea() tile 坐标 — 眼球包围盒
 *
 *  眼球: cx=64 cy=32, max_rx=20, max_ry=20*1.15≈23 (含 widen)
 *  包围盒: x=[44,84] y=[9,55]
 *  tile(8px单位): tx=44/8=5, ty=9/8=1, 宽=6, 高=6
 *  传输体量: 6×6×8 = 288 bytes (vs 全屏 1024 bytes, 缩减 72%)
 * ================================================================ */
#define EYE_TILE_X    5
#define EYE_TILE_Y    1
#define EYE_TILE_W    6
#define EYE_TILE_H    6

/* ================================================================
 *  EyeConfig_t
 * ================================================================ */
typedef struct {
    uint8_t cx;
    uint8_t cy;
    float   lid;
    float   squint;
    float   widen;
} EyeConfig_t;

typedef enum {
    BLINK_IDLE,
    BLINK_CLOSING,
    BLINK_HOLD,
    BLINK_OPENING
} BlinkPhase_t;

typedef struct {
    BlinkPhase_t phase;
    uint32_t     phase_start_ms;
    float        start_lid;
    float        target_lid;
    uint16_t     phase_duration_ms;
    uint32_t     next_blink_ms;
} BlinkState_t;

typedef enum {
    MICRO_NONE,
    MICRO_SQUINT,
    MICRO_WIDEN,
    MICRO_FLICKER
} MicroAnim_t;

typedef struct {
    MicroAnim_t anim;
    uint32_t    start_ms;
    uint16_t    duration_ms;
    uint32_t    next_trigger_ms;
} MicroState_t;

#define BLINK_CLOSING_MS   160
#define BLINK_HOLD_MS       50
#define BLINK_OPENING_MS   240
#define BLINK_INTERVAL_MIN 2000
#define BLINK_INTERVAL_MAX 6000

#define MICRO_INTERVAL_MIN 3000
#define MICRO_INTERVAL_MAX 8000
#define MICRO_SQUINT_MS     200
#define MICRO_WIDEN_MS      300
#define MICRO_FLICKER_MS    120

#define FRAME_INTERVAL_MS  33

void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy);
void blink_state_init(BlinkState_t* state);
void micro_state_init(MicroState_t* ms);
void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms);
void micro_state_update(MicroState_t* ms, EyeConfig_t* cfg, uint32_t now_ms);
void eye_render(U8G2* disp, const EyeConfig_t* cfg);

#endif
