/**
 * @file    eye_renderer.h
 * @brief   RobotEyes 眼型渲染 v9.0 — OCP 解耦管线
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-10
 *
 *  架构:
 *    eye_geom_compute() → EyeGeom_t (纯几何, 无副作用)
 *      → eye_draw_body()    (RBox 眼眶)
 *      → eye_draw_pupil()   (函数指针表分派, OCP)
 *      → eye_draw_shine()   (高光)
 *      → eye_draw_lid_mask()(眼皮遮罩 + slope)
 *
 *  OCP: 新增瞳孔类型 = 新增 draw 函数 + 注册分派表, 核心管线零修改
 *  OCP: 新增风格 = 新增 EyeStyle_t 常量 (宏切换)
 *
 *  v9.0 新增:
 *    - EyeConfig_t 新增 happy_wink / panic_scan / surprised 字段
 *    - eye_draw_sweat() 恐慌冷汗特效
 */

#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include <stdint.h>
#include <U8g2lib.h>

/* ================================================================
 *  风格选择 (OCP: 宏切换, 不改管线)
 * ================================================================ */
#define EYE_STYLE_A   /* 动漫星瞳: 56x44 */
// #define EYE_STYLE_B   /* 委屈修勾: 46x32 */
// #define EYE_STYLE_C   /* 傲娇小兽: 40x36 */

#if !defined(EYE_STYLE_A) && !defined(EYE_STYLE_B) && !defined(EYE_STYLE_C)
#define EYE_STYLE_A
#endif

/* ================================================================
 *  PupilType_t — 瞳孔变异类型 (OCP: 新增类型仅需加枚举 + 绘制函数)
 * ================================================================ */
typedef enum {
    PUPIL_NORMAL = 0,   /* 标准圆形瞳孔 */
    PUPIL_HEART,        /* 爱心瞳孔 (期待/狂喜) */
    PUPIL_SLIT,         /* 竖缝瞳孔 (愤怒/野兽) */
    PUPIL_NONE,         /* 无瞳孔 (翻白眼) */
    PUPIL_SHOCK,        /* 中空圆环 + 电波线 (震惊) */
    PUPIL_HAPPY,        /* 笑形 >< 眯眼 */
    PUPIL_STAR,         /* 四角星星 sparkle (兴奋) */
    PUPIL_COUNT          /* 类型总数 (用于分派表) */
} PupilType_t;

/* ================================================================
 *  通用常量
 * ================================================================ */
#define EYE_CX            64
#define EYE_CY            32
#define FRAME_INTERVAL_MS 33
#define LOOK_SMOOTH_FACTOR 0.22f

/* ================================================================
 *  EyeStyle_t — 风格参数 (OCP: 新增风格 = 新增此结构体常量)
 * ================================================================ */
typedef struct {
    uint8_t  eye_w;           /* 眼宽 (px) */
    uint8_t  eye_h;           /* 眼高 (px) */
    uint8_t  eye_radius;      /* 圆角半径 */
    uint8_t  pupil_r;         /* 瞳孔基础半径 */
    uint8_t  look_max;        /* 视线最大偏移 (px) */
    float    shine_parallax;  /* 高光视差系数 (0~1) */

    /* 高光 1 */
    int8_t   s1_dx, s1_dy;
    uint8_t  s1_r;
    /* 高光 2 */
    int8_t   s2_dx, s2_dy;
    uint8_t  s2_r;
    /* 高光 3 */
    int8_t   s3_dx, s3_dy;
    uint8_t  s3_r;
} EyeStyle_t;

/* ================================================================
 *  EyeGeom_t — 单帧几何计算结果 (纯数据, 无副作用)
 * ================================================================ */
typedef struct {
    /* 眼眶边界 */
    int16_t  eye_l, eye_t;    /* 左上角 */
    int16_t  eye_r, eye_b;    /* 右下角 */
    int16_t  hw, hh;          /* 半宽/半高 (便捷) */

    /* 瞳孔位置 (已 clamp) */
    int16_t  pupil_cx, pupil_cy;
    int16_t  pupil_r;         /* 缩放后半径 */

    /* 瞳孔类型 (绘制分派用) */
    PupilType_t pupil_type;

    /* 高光位置 (已 clamp, 含视差) */
    int16_t  s1_x, s1_y;
    int16_t  s2_x, s2_y;
    int16_t  s3_x, s3_y;

    /* 眼皮遮罩几何 */
    int16_t  lid_top_base_y;   /* 上眼皮基线 Y */
    int16_t  lid_slope_px;     /* 斜率偏移量 (px) */
    int16_t  lid_y_inner;      /* 内眼角上眼皮 Y */
    int16_t  lid_y_outer;      /* 外眼角上眼皮 Y */
    int16_t  lid_y_left;       /* 左角上眼皮 Y (is_left 调整后) */
    int16_t  lid_y_right;      /* 右角上眼皮 Y (is_left 调整后) */
    int16_t  lid_bottom_h;     /* 下眼皮高度 (0=无) */

    /* 当前是否为左眼 */
    bool     is_left;
} EyeGeom_t;

/* ================================================================
 *  EyeConfig_t — 运行时状态 v9.0
 * ================================================================ */
typedef struct {
    uint8_t cx, cy;           /* 眼睛中心坐标 */
    float   lid;              /* 眨眼遮挡比例 (0.0=全开, 1.0=全闭) */

    /* 视线 */
    int8_t  target_look_x, target_look_y;
    float   cur_look_x, cur_look_y;

    /* 表情参数 */
    uint8_t      active_expr;
    float        target_lid_top;
    float        target_lid_top_l;
    float        target_lid_top_r;
    float        target_lid_bottom;
    float        target_lid_slope;
    float        target_pupil_scale;
    PupilType_t  target_pupil_type;
    PupilType_t  cur_pupil_type;

    float   cur_lid_top;
    float   cur_lid_top_l;
    float   cur_lid_top_r;
    float   cur_lid_bottom;
    float   cur_lid_slope;
    float   cur_pupil_scale;

    /* 特殊动画 */
    float    anim_peak_scale;
    uint32_t anim_start_ms;
    uint16_t anim_duration_ms;

    /* Sleepy 瞌睡引擎 */
    uint32_t sleepy_phase_ms;
    float    sleepy_lid;

    /* 眉毛微动引擎 (v7.0: 参数化) */
    float    brow_phase;           /* 通用动画相位 (弧度) */
    float    brow_angry_phase;     /* Angry 震颤相位 (保留兼容) */
    float    brow_burst_timer;     /* Angry 爆发计时器 (保留兼容) */
    float    brow_anim_phase;      /* 表情特定动画相位 (v7.0) */
    int8_t   brow_offset_l;        /* 左眉当前微动偏移 */
    int8_t   brow_offset_r;        /* 右眉当前微动偏移 */

    /* 泪滴动画 (v7.0) */
    uint32_t tear_phase_ms;        /* 泪滴动画计时器 */
    uint32_t tear_phase2_ms;       /* 第二滴泪相位偏移 */

    /* ---- v8.0 分层动画架构 ---- */

    /* 注意力层: 自主视线漂移 */
    uint32_t attention_next_ms;    /* 下次注意力移动时刻 */
    int8_t   attention_target_x;   /* 注意力目标 X */
    int8_t   attention_target_y;   /* 注意力目标 Y */
    int8_t   attention_prev_x;     /* 注意力起点 X (用于平滑) */
    int8_t   attention_prev_y;     /* 注意力起点 Y */
    uint8_t  attention_phase;      /* 0=idle, 1=moving, 2=holding, 3=returning */

    /* 二级运动: overshoot/decay */
    float    overdrive_decay;      /* 过冲衰减系数 (0=无) */
    float    overdrive_amount;     /* 过冲幅度 */

    /* 随机怠速微动作 */
    uint32_t idle_micro_next_ms;   /* 下次微动作时刻 */
    uint8_t  idle_micro_type;      /* 0=none, 1=pupil_scale, 2=brow_twitch, 3=lid_flutter */
    float    idle_micro_lid_delta; /* 眼皮微动幅度 */
    float    idle_micro_pupil_delta;/* 瞳孔缩放幅度 */

    /* ---- v9.0 新增字段 ---- */

    /* Happy 单眼快眨 */
    uint32_t happy_wink_next_ms;   /* 下次快眨时刻 */
    uint8_t  happy_wink_eye;       /* 0=无, 1=左眼, 2=右眼 */
    uint32_t happy_wink_start_ms;  /* 快眨开始时刻 */

    /* Panic 恐慌眼球扫视 */
    uint32_t panic_scan_next_ms;   /* 下次扫视时刻 */

    /* Excited 心跳缩放 (通过 millis() 直接计算, 不依赖 anim_peak) */
    /* (无额外字段, 在 eye_expr_update 中直接使用 millis()) */

    /* Surprised 四阶段大小眼 (通过 millis() 在 eye_expr_update 中计算) */
    /* (无额外字段, 在 eye_expr_update 中直接计算) */
} EyeConfig_t;

/* ================================================================
 *  BlinkState_t — 眨眼状态机
 * ================================================================ */
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

/* ================================================================
 *  updateDisplayArea() tile 坐标 (Style A 默认)
 * ================================================================ */
#define EYE_TILE_X    4
#define EYE_TILE_Y    1
#define EYE_TILE_W    8
#define EYE_TILE_H    6

/* ================================================================
 *  公开 API
 * ================================================================ */

/* ---- 获取当前激活的风格 ---- */
const EyeStyle_t* eye_style_get(void);

/* ---- 几何计算 (纯函数, 无副作用) ---- */
void eye_geom_compute(EyeGeom_t* geom,
                      const EyeConfig_t* cfg,
                      const EyeStyle_t* style,
                      bool is_left);

/* ---- 分阶段绘制 (只读 geom, 只写 disp) ---- */
void eye_draw_body(U8G2* disp, const EyeGeom_t* geom);
void eye_draw_pupil(U8G2* disp, const EyeGeom_t* geom);
void eye_draw_shine(U8G2* disp, const EyeGeom_t* geom);
void eye_draw_lid_mask(U8G2* disp, const EyeGeom_t* geom);
void eye_draw_sweat(U8G2* disp, const EyeGeom_t* geom);  /* v9.0: Panic 冷汗 */

/* ---- 渲染入口 (组装管线) ---- */
void eye_render(U8G2* disp, EyeConfig_t* cfg, bool is_left);

/* ---- 配置 & 状态 API (保持兼容) ---- */
void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy);
void eye_set_look(EyeConfig_t* cfg, int8_t x, int8_t y);
void eye_look_update(EyeConfig_t* cfg);
void eye_look_reset(EyeConfig_t* cfg);
void eye_set_expression(EyeConfig_t* cfg, uint8_t expr_index);
void eye_expr_update(EyeConfig_t* cfg, uint32_t now_ms);
void blink_state_init(BlinkState_t* state);
void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms);

/* ---- v8.0 分层动画 API ---- */
void eye_attention_update(EyeConfig_t* cfg, uint32_t now_ms);
void eye_idle_micro_update(EyeConfig_t* cfg, uint32_t now_ms);

#endif /* EYE_RENDERER_H */
