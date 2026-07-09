/**
 * @file    eye_renderer.h
 * @brief   RobotEyes 参数化几何眼型引擎 v5
 *          v5: 基于 esp32-sh1106-oled-emojis-web 的 EyeDrawer 算法
 *              眼型 = 8参数几何体，眨眼 = 遮罩覆盖，表情 = 参数平滑过渡
 */

#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include <stdint.h>
#include <U8g2lib.h>

/* ================================================================
 *  通用常量
 * ================================================================ */
#define EYE_CX            64
#define EYE_CY            32
#define FRAME_INTERVAL_MS 33

/* ================================================================
 *  EyeShape_t — 参数化眼型 (8 个几何参数)
 *
 *  参考 esp32-sh1106 EyeConfig:
 *    Width/Height  → 眼型基准宽高
 *    Radius_Top/Bot → 上下圆角半径 (0=直角, 大=圆眼)
 *    Slope_Top/Bot  → 上下边缘斜率 (-1~+1)
 *      +Slope_Top: 内眼角高于外眼角 (怒/专注)
 *      -Slope_Top: 外眼角高于内眼角 (委屈/困)
 *    Offset_X/Y     → 微调位置 (视线偏移用)
 * ================================================================ */
typedef struct {
    int16_t width;
    int16_t height;
    int16_t radius_top;
    int16_t radius_bottom;
    float   slope_top;
    float   slope_bottom;
    int16_t offset_x;
    int16_t offset_y;
} EyeShape_t;

/* ================================================================
 *  EyeConfig_t — 眼球运行时状态
 * ================================================================ */
typedef struct {
    uint8_t   cx;
    uint8_t   cy;
    float     lid;          /* 眨眼遮罩系数: 0.0=全睁 1.0=全闭 */
    EyeShape_t shape;       /* 当前眼型参数 */
    EyeShape_t target;      /* 目标眼型 (平滑过渡) */
    float     morph_t;      /* 过渡进度 0.0→1.0 */
} EyeConfig_t;

/* ================================================================
 *  BlinkState_t — 眨眼状态机 (Trapezium: 闭→持→开)
 * ================================================================ */
typedef enum {
    BLINK_IDLE = 0,
    BLINK_CLOSING,
    BLINK_HOLD,
    BLINK_OPENING
} BlinkPhase_t;

typedef struct {
    BlinkPhase_t phase;
    uint32_t     phase_start_ms;
    uint16_t     phase_duration_ms;
    uint32_t     next_blink_ms;
} BlinkState_t;

/* ================================================================
 *  MicroState_t — 闲置微表情 (随机眯眼/瞪大)
 * ================================================================ */
typedef enum {
    MICRO_NONE = 0,
    MICRO_SQUINT,
    MICRO_WIDEN,
} MicroAnim_t;

typedef struct {
    MicroAnim_t anim;
    uint32_t    start_ms;
    uint16_t    duration_ms;
    uint32_t    next_trigger_ms;
} MicroState_t;

/* ---- 眨眼时序 (ms) ---- */
#define BLINK_CLOSING_MS   120
#define BLINK_HOLD_MS       35
#define BLINK_OPENING_MS   180
#define BLINK_INTERVAL_MIN 2000
#define BLINK_INTERVAL_MAX 6000

/* ---- 微表情时序 (ms) ---- */
#define MICRO_INTERVAL_MIN 3000
#define MICRO_INTERVAL_MAX 8000
#define MICRO_DURATION_MS   400

/* ---- 表情过渡 (ms) ---- */
#define MORPH_DURATION_MS  300

/* ================================================================
 *  预设表情定义
 *
 *  参考 esp32-sh1106 EyePresets, 适配 128x64 屏幕
 *  基准: Width=40, center=(64,32)
 * ================================================================ */

/* Normal: 标准圆角矩形眼 */
static const EyeShape_t PRESET_NORMAL = {
    40, 36,     /* w, h */
    8, 8,       /* radius_top, bot */
    0.0f, 0.0f, /* slope_top, bot */
    0, 0        /* offset */
};

/* Happy: 上弯月眼 (下边缘上拱) */
static const EyeShape_t PRESET_HAPPY = {
    40, 14,
    10, 4,
    0.0f, 0.0f,
    0, 0
};

/* Angry: 内眼角下压 (上边缘外低内高) */
static const EyeShape_t PRESET_ANGRY = {
    40, 22,
    2, 12,
    0.3f, 0.0f,
    -2, 0
};

/* Sad: 外眼角下垂 (上边缘内高外低) */
static const EyeShape_t PRESET_SAD = {
    40, 18,
    1, 10,
    -0.4f, 0.0f,
    0, 0
};

/* Sleepy: 半闭眼 (高度减半 + 上边缘下垂) */
static const EyeShape_t PRESET_SLEEPY = {
    40, 16,
    3, 4,
    -0.3f, -0.2f,
    0, -1
};

/* Surprised: 大圆眼 */
static const EyeShape_t PRESET_SURPRISED = {
    42, 44,
    14, 14,
    0.0f, 0.0f,
    0, 0
};

/* Squint: 眯眼 (用于微表情) */
static const EyeShape_t PRESET_SQUINT = {
    38, 20,
    4, 6,
    0.0f, 0.15f,
    0, 0
};

/* Widen: 瞪大 (用于微表情) */
static const EyeShape_t PRESET_WIDEN = {
    42, 40,
    10, 10,
    0.0f, 0.0f,
    0, 0
};

/* ================================================================
 *  updateDisplayArea() tile 坐标
 *
 *  最坏情况 (Surprised): H=44, cx=64 → y=[10,54]
 *  安全边距: x=[40,88] y=[8,56]
 *  tile(8px): tx=5, ty=1, w=7, h=6
 *  传输体量: 7*6*8 = 336 bytes
 * ================================================================ */
#define EYE_TILE_X    5
#define EYE_TILE_Y    1
#define EYE_TILE_W    7
#define EYE_TILE_H    6

/* ================================================================
 *  API
 * ================================================================ */
void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy);
void eye_set_shape(EyeConfig_t* cfg, const EyeShape_t* shape);

void blink_state_init(BlinkState_t* state);
void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms);

void micro_state_init(MicroState_t* ms);
void micro_state_update(MicroState_t* ms, EyeConfig_t* cfg, uint32_t now_ms);

void eye_morph_update(EyeConfig_t* cfg, uint32_t now_ms);
void eye_render(U8G2* disp, EyeConfig_t* cfg);

#endif
