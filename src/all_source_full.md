 
# RobotEyes v6.1 完整源码汇总 
导出时间：周五 2026/07/10 10:40:34.52 
包含文件：*.h + *.cpp 
 
## 完整目录树 
```tree 
Folder PATH listing
Volume serial number is 2607-239F
D:.
    all_source_full.md
    event_bus.cpp
    event_bus.h
    export_code.bat
    expressions.h
    eye_renderer.cpp
    eye_renderer.h
    eye_renderer_part1.txt
    force_return.cpp
    force_return.h
    input_task.cpp
    input_task.h
    main.cpp
    servo_task.cpp
    servo_task.h
    
No subfolders exist 

``` 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\event_bus.h 
```c 
/**
 * @file    event_bus.h
 * @brief   RobotEyes 事件总线 — FreeRTOS Queue 封装 (v6.1 修复容量)
 *
 *  单向搬运: InputTask (生产者) → EventBus → main loop (唯一消费者)
 *  参考 Pixel-Box-ESP32 (MIT) 的模式
 *
 *  v6.1 修复: value_x/value_y 从 int8_t 扩展为 int16_t
 *    - int8_t 最大 127, 无法存储毫秒级持有时长 → 长按判定瘫痪
 *    - int16_t 最大 32767, 可存储最长 32 秒的按键时长
 */

#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>

/* ================================================================
 *  事件类型
 * ================================================================ */
typedef enum {
    EVT_NONE = 0,          /* 空事件 */
    EVT_JOYSTICK_MOVE,     /* 摇杆移动 */
    EVT_BUTTON_SHORT,      /* SW 短按 */
    EVT_BUTTON_LONG,       /* SW 长按 (Force Return) */
    EVT_FORCE_RETURN,      /* 强制归位 */
    EVT_SERVO_MOVE,        /* 舵机移动 */
    EVT_EXPR_SET,          /* 表情切换: value_x=索引(0-7) */
    EVT_EXPR_RELEASE,      /* 按键释放: value_x=索引, value_y=持有时长ms */
} EventType_t;

/* ================================================================
 *  事件消息体 (紧凑布局)
 * ================================================================ */
#pragma pack(push, 1)
typedef struct {
    uint8_t     type;       /* EventType_t */
    int16_t     value_x;    /* int16_t: 视线X/表情索引 */
    int16_t     value_y;    /* int16_t: 视线Y/持有时长(ms) */
    uint8_t     _pad[11];   /* 保持结构体大小不变 */
} EventMsg_t;
#pragma pack(pop)

/* ================================================================
 *  API
 * ================================================================ */
void event_bus_init(void);
bool event_bus_push(const EventMsg_t* msg);
bool event_bus_pop(EventMsg_t* msg, uint32_t timeout_ms);
void event_bus_flush(void);

#endif
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\expressions.h 
```c 
/**
 * @file    expressions.h
 * @brief   RobotEyes 8 大灵魂微表情 v10.0 — 参数化眉毛动画引擎 + 泪滴坐标映射
 *
 *  v10.0 关键升级:
 *    - 【核心修复】brow_left/right 从 int8_t 升级为 int16_t
 *      int8_t 最大127, Angry SYM_L(-45)=135 溢出为-121 → 镜像崩塌!
 *    - SYM_L/SYM_R 宏同步升级 int16_t
 *    - 表情参数大规模调优 (视觉冲击力重构)
 *    - Happy: 弯月笑眼 + 高频星星粒子 + 弹跳眉毛
 *    - Angry: int16_t修复后 \ / 镜像正确 + 高频颤抖
 *    - Sad: 汪汪泪眼 大泪珠 + 眼角抽泣 + 水光反射
 *    - Surprised: 高频大小眼交替跳动 + 眉毛跷跷板摇摆
 *    - Sleepy: 抗拒困意状态机 (闭眼皱眉→惊醒弹开 眉毛联动)
 *    - Panic: 极度慌张 无规律乱颤 + 急促扫视 + 大汗珠
 *    - Excited: 超大爱心瞳孔 + 双相心跳(lub-dub)
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
 *  ExpressionDef_t — 表情定义 (v10: int16_t 防溢出)
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

    /* ---- 眉毛静态角度 (v10: int16_t 防溢出, 支持0-180全范围) ---- */
    int16_t         brow_left;      /* 左眉基础角度 */
    int16_t         brow_right;     /* 右眉基础角度 */

    /* ---- 眉毛动画参数 ---- */
    BrowAnimation_t brow_anim;          /* 动画类型 */
    float           brow_freq;          /* 基频 (弧度/帧) */
    float           brow_amp;           /* 振幅 (度) */
    float           brow_asymmetry;     /* 左右非对称系数 */
    float           brow_burst_amp;     /* 爆发振幅 (TREMBLE) */
    uint16_t        brow_burst_intv;    /* 爆发间隔 ms (TREMBLE) */

    /* ---- 泪滴参数 ---- */
    bool            tear_enabled;       /* 启用泪滴 */
    float           tear_rate;          /* 滑落速率 (px/ms) */
    uint8_t         tear_spacing;       /* 双泪滴初始间距 (px) */
} ExpressionDef_t;

#define BROW_CENTER  90
/* v10: int16_t 强制转换, 防溢出! int8_t最大127, 135会溢出为-121 */
#define SYM_L(offset)  ((int16_t)(BROW_CENTER - (offset)))
#define SYM_R(offset)  ((int16_t)(BROW_CENTER + (offset)))

/* ================================================================
 *  8 大灵魂微表情表 v10.0 (视觉冲击力极致重构)
 * ================================================================ */
static const ExpressionDef_t EXPRESSIONS[8] = {

    /* [0] Normal — 自然灵动, 微呼吸 + 注意力漂移 */
    { "Normal",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_NORMAL, 1.0f,  0.0f, 0,
      SYM_L(0), SYM_R(0),
      BROW_ANIM_BREATHE, 0.018f, 2.5f, 0.25f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [1] Smirk — 滑稽笑: 左眼半闭斜视 + 右眼狡黠大眼 + 眉毛不对称 */
    { "Smirk",  0.0f, 0.50f, 0.08f,  0.22f, -0.18f, PUPIL_NORMAL, 0.70f,  1.5f, 350, SYM_L(55), SYM_R(20), BROW_ANIM_TWITCH, 0.03f, 7.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [2] Angry — 倒八字眉 \ / 高频颤抖 + 竖缝瞳孔 (v10: int16_t修复镜像) */
    { "Angry",
      0.28f, 0.0f, 0.0f,  0.10f, 0.85f,
      PUPIL_SLIT, 0.50f,  0.35f, 220,
      SYM_L(-45), SYM_R(-45),
      BROW_ANIM_TREMBLE, 0.22f, 4.5f, 0.55f, 12.0f, 500,
      false, 0.0f, 0 },

    /* [3] Sad — 汪汪泪眼 大泪珠 + 眼角抽泣 + 水光反射 */
    { "Sad",
      0.12f, 0.0f, 0.0f,  0.35f, -0.85f,
      PUPIL_NORMAL, 1.6f,  2.5f, 600,
      SYM_L(28), SYM_R(28),
      BROW_ANIM_SOB, 0.008f, 3.5f, 1.4f, 0.0f, 0,
      true, 0.018f, 20 },

    /* [4] Surprised — 高频大小眼交替跳动 + 眉毛跷跷板摇摆 */
    { "Surprised",
      0.0f, 0.0f, 0.0f,  -0.15f, 0.0f,
      PUPIL_SHOCK, 1.30f,  0.30f, 300,
      SYM_L(58), SYM_R(58),
      BROW_ANIM_SWAY, 0.06f, 10.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [5] Sleepy — 抗拒困意状态机: 闭眼皱眉→惊醒弹开 (v10: 眉毛联动) */
    { "Sleepy", 0.55f, 0.55f, 0.55f,  0.05f, 0.0f, PUPIL_NORMAL, 0.60f,  0.0f, 0, SYM_L(-18), SYM_R(-18), BROW_ANIM_NONE, 0.0f, 0.0f, 0.0f, 0.0f, 0,
      false, 0.0f, 0 },

    /* [6] Panic — 极度慌张: 无规律乱颤 + 急促扫视 + 大汗珠 */
    { "Panic",
      0.02f, 0.0f, 0.0f,  0.08f, 0.0f,
      PUPIL_NORMAL, 0.65f,  0.0f, 0,
      SYM_L(38), SYM_R(38),
      BROW_ANIM_TREMBLE, 0.25f, 3.0f, 0.45f, 5.0f, 300,
      false, 0.0f, 0 },

    /* [7] Excited — 超大爱心瞳孔 + 双相心跳(lub-dub) + 弹跳眉毛 */
    { "Excited",
      0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
      PUPIL_HEART, 1.0f,  0.0f, 0,
      SYM_L(45), SYM_R(45),
      BROW_ANIM_RAISE_BOUNCE, 0.08f, 10.0f, 0.0f, 0.0f, 0,
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
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\eye_renderer.h 
```c 
/**
 * @file    eye_renderer.h
 * @brief   RobotEyes 眼型渲染 v10.0 — OCP 解耦管线 (int16_t 全链路)
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
 *  v10.0 关键升级:
 *    - brow_offset_l/r int8_t → int16_t (防溢出, 配合舵机全链路)
 *    - 新增 sad_water_phase (Sad 水光反射动画)
 *    - 新增 panic_sweat_seed (Panic 冷汗随机种子)
 *    - 新增 sleepy_struggle_phase (Sleepy 抗拒困意子状态)
 *    - Happy 弯月笑眼弧形遮罩 #define
 *    - Excited 双相心跳 lub-dub 节律
 */

#ifndef EYE_RENDERER_H
#define EYE_RENDERER_H

#include <stdint.h>
#include <U8g2lib.h>

/* ================================================================
 *  风格选择 (OCP: 宏切换, 不改管线)
 * ================================================================ */
#define EYE_STYLE_A   /* 动漫星瞳: 56x44 */

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
    PUPIL_HAPPY,        /* 弯月笑眼 >< 弧形 (v10: 重构) */
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
 *  EyeConfig_t — 运行时状态 v10.0
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
    PupilType_t  target_pupil_type;
    float        target_pupil_scale; float target_pupil_scale_r;
    /* 当前渐变值 */
    float   cur_lid_top;
    float   cur_lid_top_l;
    float   cur_lid_top_r;
    float   cur_lid_bottom;
    float   cur_lid_slope;
    float   cur_pupil_scale; float cur_pupil_scale_r;
    PupilType_t cur_pupil_type;

    /* 峰值动画回弹 */
    float    anim_peak_scale;
    uint32_t anim_start_ms;
    uint16_t anim_duration_ms;

    /* Sleepy 瞌睡引擎 */
    uint32_t sleepy_phase_ms;
    float    sleepy_lid;
    uint8_t  sleepy_struggle_sub;  /* v10: 抗拒困意子阶段 0=闭眼 1=惊醒 2=挣扎 */

    /* 眉毛微动引擎 (v10: int16_t 防溢出) */
    float    brow_phase;
    float    brow_angry_phase;
    float    brow_burst_timer;
    float    brow_anim_phase;
    int16_t  brow_offset_l;        /* v10: int8_t → int16_t */
    int16_t  brow_offset_r;        /* v10: int8_t → int16_t */

    /* 泪滴动画 */
    uint32_t tear_phase_ms;
    uint32_t tear_phase2_ms;
    uint32_t sad_water_phase_ms;   /* v10: Sad 水光反射相位 */

    /* ---- 分层动画架构 ---- */

    /* 注意力层 */
    uint32_t attention_next_ms;
    int8_t   attention_target_x;
    int8_t   attention_target_y;
    int8_t   attention_prev_x;
    int8_t   attention_prev_y;
    uint8_t  attention_phase;

    /* 二级运动 */
    float    overdrive_decay;
    float    overdrive_amount;

    /* 随机怠速微动作 */
    uint32_t idle_micro_next_ms;
    uint8_t  idle_micro_type;
    float    idle_micro_lid_delta;
    float    idle_micro_pupil_delta;

    /* ---- v10.0 专属动画字段 ---- */

    /* Happy 单眼快眨 */
    uint32_t happy_wink_next_ms;
    uint8_t  happy_wink_eye;
    uint32_t happy_wink_start_ms;

    /* Panic 恐慌眼球扫视 */
    uint32_t panic_scan_next_ms;
    uint8_t  panic_sweat_seed;     /* v10: 冷汗绘制随机种子 */

    /* Excited 心跳节律 */
    uint32_t excited_heartbeat_ms; /* v10: 心跳计时起点 */
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

const EyeStyle_t* eye_style_get(void);

void eye_geom_compute(EyeGeom_t* geom,
                      const EyeConfig_t* cfg,
                      const EyeStyle_t* style,
                      bool is_left);

void eye_draw_body(U8G2* disp, const EyeGeom_t* geom);
void eye_draw_pupil(U8G2* disp, const EyeGeom_t* geom);
void eye_draw_shine(U8G2* disp, const EyeGeom_t* geom);
void eye_draw_lid_mask(U8G2* disp, const EyeGeom_t* geom);
void eye_draw_sweat(U8G2* disp, const EyeGeom_t* geom);       /* Panic 冷汗 */
void eye_draw_happy_arc(U8G2* disp, const EyeGeom_t* geom);   /* v10: Happy 弯月弧形 */
void eye_draw_sad_water(U8G2* disp, const EyeGeom_t* geom);   /* v10: Sad 水光反射 */

void eye_render(U8G2* disp, EyeConfig_t* cfg, bool is_left);

void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy);
void eye_set_look(EyeConfig_t* cfg, int8_t x, int8_t y);
void eye_look_update(EyeConfig_t* cfg);
void eye_look_reset(EyeConfig_t* cfg);
void eye_set_expression(EyeConfig_t* cfg, uint8_t expr_index);
void eye_expr_update(EyeConfig_t* cfg, uint32_t now_ms);
void blink_state_init(BlinkState_t* state);
void blink_state_update(BlinkState_t* state, EyeConfig_t* cfg, uint32_t now_ms);

void eye_attention_update(EyeConfig_t* cfg, uint32_t now_ms);
void eye_idle_micro_update(EyeConfig_t* cfg, uint32_t now_ms);

#endif /* EYE_RENDERER_H */
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\force_return.h 
```c 
/**
 * @file    force_return.h
 * @brief   RobotEyes 安全归位机制 — SW 长按检测
 *
 *  状态图:
 *    IDLE ──(按下)──> PRESSED ──(2秒超时)──> LONG_TRIGGERED
 *      ↑                 │                        │
 *      │                 └──(<2秒释放)──> SHORT    │
 *      │                      │                   │
 *      └──────────────────────┴───────────────────┘(释放)
 *
 *  实现: GPIO 中断标记 + InputTask 轮询 + 软件计时
 *  长按判定和短按判定互斥: 一旦触发 LONG, 不再产生 SHORT
 */

#ifndef FORCE_RETURN_H
#define FORCE_RETURN_H

#include <stdint.h>

/* 长按阈值 (ms) */
#define FORCE_RETURN_HOLD_MS  2000

void force_return_init(void);
void force_return_poll(uint32_t now_ms);

#endif
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\input_task.h 
```c 
/**
 * @file    input_task.h
 * @brief   RobotEyes 输入采集 Task — 摇杆 + SW 按键
 */

#ifndef INPUT_TASK_H
#define INPUT_TASK_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* ---- 调参宏 ---- */
#define INPUT_RAW_SAMPLES     10
#define INPUT_DISCARD_EXTREME  2
#define INPUT_STABLE_COUNT     3
#define INPUT_DEADZONE         6
#define INPUT_ADC_RANGE      1550

/* ---- 任务句柄 ---- */
extern TaskHandle_t g_inputTaskHandle;

void input_task_init(void);
void input_task_run(void* pvParameters);

#endif
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\servo_task.h 
```c 
/**
 * @file    servo_task.h
 * @brief   RobotEyes 舵机控制 Task v10 — 平滑非阻塞插值 + 抖动通道 (int16_t 全链路)
 */

#ifndef SERVO_TASK_H
#define SERVO_TASK_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define SERVO_UPDATE_MS     20
#define SERVO_STEP_DEG       2
#define SERVO_CENTER_DEG    90
#define SERVO_MIN_DEG       45
#define SERVO_MAX_DEG      135

extern TaskHandle_t g_servoTaskHandle;

/* v10: 所有角度参数从 int8_t 升级为 int16_t, 防溢出 */
void servo_task_init(void);
void servo_set_target(int16_t left_deg, int16_t right_deg);
void servo_get_target(int16_t* left_deg, int16_t* right_deg);
void servo_add_relative(int16_t left_offset, int16_t right_offset);
void servo_set_jitter(int16_t left_jitter, int16_t right_jitter);
void servo_task_run(void* pvParameters);

#endif
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\event_bus.cpp 
```c 
/**
 * @file    event_bus.cpp
 * @brief   RobotEyes 事件总线实现
 */

#include "event_bus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define EVENT_QUEUE_DEPTH 32

static QueueHandle_t g_queue = NULL;

/* ================================================================
 *  event_bus_init()
 * ================================================================ */
void event_bus_init(void) {
    g_queue = xQueueCreate(EVENT_QUEUE_DEPTH, sizeof(EventMsg_t));
}

/* ================================================================
 *  event_bus_push() — 非阻塞入队
 * ================================================================ */
bool event_bus_push(const EventMsg_t* msg) {
    if (!g_queue || !msg) return false;
    return xQueueSend(g_queue, msg, 0) == pdTRUE;
}

/* ================================================================
 *  event_bus_pop() — 阻塞等待
 * ================================================================ */
bool event_bus_pop(EventMsg_t* msg, uint32_t timeout_ms) {
    if (!g_queue || !msg) return false;
    return xQueueReceive(g_queue, msg, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

/* ================================================================
 *  event_bus_flush() — 清空队列 (Force Return 用)
 * ================================================================ */
void event_bus_flush(void) {
    if (!g_queue) return;
    xQueueReset(g_queue);
}
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\eye_renderer.cpp 
```c 
/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes v10.0 - OCP + 7 expression visual overhaul
 * @author  Rennick
 * @date    2026-07-10
 */
#include "eye_renderer.h"
#include "expressions.h"
#include <math.h>

static inline float ease_in(float t)  { return t * t; }
static inline float ease_out(float t) { float i = 1.0f - t; return 1.0f - i * i; }
static inline int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}

#ifdef EYE_STYLE_A
static const EyeStyle_t g_style = { 56, 44, 20, 10, 16, 0.55f, -6, -7, 4, 7, 5, 2, -2, 3, 1 };
#endif

typedef void (*PupilDrawFunc)(U8G2*, const EyeGeom_t*);

static void draw_pupil_normal(U8G2* d, const EyeGeom_t* g) {
    if (g->pupil_r > 0) d->drawDisc(g->pupil_cx, g->pupil_cy, g->pupil_r, U8G2_DRAW_ALL);
}
static void draw_pupil_heart(U8G2* d, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, br = g->pupil_r;
    if (br < 2) { draw_pupil_normal(d, g); return; }
    /* 双相心跳 "咚-哒-停" lub-dub-pause 节律: 峰值 1.65x */
    uint32_t bt = millis() % 900; float s = 1.0f;
    if (bt < 100) {           /* "咚" lub — 急速放大至 1.65x */
        float t = (float)bt / 100.0f;
        s = 1.0f + 0.65f * sinf(t * M_PI);
    } else if (bt < 240) {    /* "哒" dub — 回弹 1.30x */
        float t = (float)(bt - 100) / 140.0f;
        s = 1.0f + 0.30f * sinf(t * M_PI);
    } else if (bt < 360) {    /* 二次收缩 1.50x */
        float t = (float)(bt - 240) / 120.0f;
        s = 1.0f + 0.50f * sinf(t * M_PI);
    }
    /* 360~900ms: 静止 (s=1.0) */

    float sw = 1.0f + (s - 1.0f) * 0.85f;
    float sh = s;
    int16_t rw = (int16_t)((float)br * sw), rh = (int16_t)((float)br * sh);
    if (rw < 4) rw = 4; if (rh < 4) rh = 4;
    int16_t lr = rw / 2;
    d->drawDisc(cx - lr, cy - lr / 2, lr, U8G2_DRAW_ALL);
    d->drawDisc(cx + lr, cy - lr / 2, lr, U8G2_DRAW_ALL);
    d->drawBox(cx - lr, cy - lr / 2 + 1, rw, rw / 2);
    d->drawTriangle(cx - rw, cy, cx + rw, cy, cx, cy + rh);
    if (rw > 5) d->drawTriangle(cx - rw + 2, cy, cx + rw - 2, cy, cx, cy + rh - 2);
    /* 心跳高潮闪光 (放大到 1.20x 以上时) */
    if (s > 1.20f) {
        d->setDrawColor(1);
        d->drawDisc(cx - lr + 2, cy - lr / 2 - 2, 3, U8G2_DRAW_ALL);
        d->drawDisc(cx + lr - 2, cy - lr / 2 - 2, 2, U8G2_DRAW_ALL);
        d->setDrawColor(0);
    }
}

static void draw_pupil_slit(U8G2* d, const EyeGeom_t* g) {
    int16_t sw = g->pupil_r / 3; if (sw < 1) sw = 1;
    int16_t h = g->pupil_r + 3;
    d->drawBox(g->pupil_cx - sw, g->pupil_cy - h, sw * 2, h * 2);
    if (g->pupil_r > 4) { d->setDrawColor(1); d->drawFrame(g->pupil_cx - sw - 1, g->pupil_cy - h - 1, sw * 2 + 2, h * 2 + 2); d->setDrawColor(0); }
}

static void draw_pupil_none(U8G2*, const EyeGeom_t*) {}

static void draw_pupil_shock(U8G2* d, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, r = g->pupil_r;
    d->drawCircle(cx, cy, r, U8G2_DRAW_ALL);
    if (r > 4) { d->setDrawColor(1); d->drawCircle(cx, cy, r - 3, U8G2_DRAW_ALL); d->setDrawColor(0); }
    if ((millis() / 35) % 2 == 0) {
        d->drawLine(cx - 8, cy - 8, cx - 18, cy - 18); d->drawLine(cx + 8, cy - 8, cx + 18, cy - 18);
        d->drawLine(cx - 8, cy + 8, cx - 18, cy + 18); d->drawLine(cx + 8, cy + 8, cx + 18, cy + 18);
        d->drawLine(cx - 16, cy, cx - 10, cy); d->drawLine(cx + 10, cy, cx + 16, cy);
    } else {
        d->drawLine(cx - 5, cy - 9, cx - 12, cy - 20); d->drawLine(cx + 5, cy - 9, cx + 12, cy - 20);
        d->drawLine(cx - 5, cy + 9, cx - 12, cy + 20); d->drawLine(cx + 5, cy + 9, cx + 12, cy + 20);
        d->drawLine(cx, cy - 18, cx, cy - 10); d->drawLine(cx, cy + 10, cx, cy + 18);
    }
}

static void draw_pupil_happy(U8G2* d, const EyeGeom_t* g) {
    /* 弯月由 eye_draw_happy_arc 绘制, 此处仅补充星光粒子 */
    uint32_t star_t = millis() % 1200;
    float alpha = 0.0f;
    if (star_t < 80) alpha = (float)star_t / 80.0f;
    else if (star_t < 160) alpha = 1.0f - (float)(star_t - 80) / 80.0f;
    if (alpha > 0.02f) {
        d->setDrawColor(1);
        uint8_t ss = (uint8_t)(alpha * 4.0f);
        if (ss < 1) ss = 1; if (ss > 3) ss = 3;
        for (uint8_t s = 0; s < ss; s++) {
            int16_t sx = g->pupil_cx - g->pupil_r + (rand() % (g->pupil_r * 2));
            int16_t sy = g->pupil_cy - g->pupil_r + (rand() % (g->pupil_r * 2));
            int16_t sl = 1 + (rand() % 2);
            d->drawHLine(sx - sl, sy, sl * 2);
            d->drawVLine(sx, sy - sl, sl * 2);
        }
        d->setDrawColor(0);
    }
}

static void draw_pupil_star(U8G2* d, const EyeGeom_t* g) {
    int16_t cx = g->pupil_cx, cy = g->pupil_cy, r = g->pupil_r;
    if (r < 3) { draw_pupil_normal(d, g); return; }
    int16_t cr = r / 3; if (cr < 2) cr = 2; d->drawDisc(cx, cy, cr, U8G2_DRAW_ALL);
    int16_t sl = r + 2;
    d->drawLine(cx, cy - cr, cx, cy - sl); d->drawLine(cx, cy + cr, cx, cy + sl);
    d->drawLine(cx - cr, cy, cx - sl, cy); d->drawLine(cx + cr, cy, cx + sl, cy);
    if ((millis() / 60) % 2 == 0) { int16_t dg = (int16_t)((float)sl * 0.7f); d->drawLine(cx - cr, cy - cr, cx - dg, cy - dg); d->drawLine(cx + cr, cy - cr, cx + dg, cy - dg); d->drawLine(cx - cr, cy + cr, cx - dg, cy + dg); d->drawLine(cx + cr, cy + cr, cx + dg, cy + dg); }
}

static const PupilDrawFunc s_pupil_draw[PUPIL_COUNT] = {
    [PUPIL_NORMAL] = draw_pupil_normal, [PUPIL_HEART] = draw_pupil_heart,
    [PUPIL_SLIT] = draw_pupil_slit, [PUPIL_NONE] = draw_pupil_none,
    [PUPIL_SHOCK] = draw_pupil_shock, [PUPIL_HAPPY] = draw_pupil_happy,
    [PUPIL_STAR] = draw_pupil_star,
};

const EyeStyle_t* eye_style_get(void) { return &g_style; }
void eye_geom_compute(EyeGeom_t* gm, const EyeConfig_t* cfg,
                      const EyeStyle_t* s, bool is_left) {
    const int16_t hw = s->eye_w / 2, hh = s->eye_h / 2;
    gm->hw = hw; gm->hh = hh; gm->is_left = is_left;
    gm->eye_l = cfg->cx - hw; gm->eye_t = cfg->cy - hh;
    gm->eye_r = cfg->cx + hw; gm->eye_b = cfg->cy + hh;
    gm->pupil_r = (int16_t)((float)s->pupil_r * cfg->cur_pupil_scale);
    if (gm->pupil_r < 1) gm->pupil_r = 1;
    int16_t ppx = (int16_t)(cfg->cur_look_x * (float)s->look_max / 127.0f);
    int16_t ppy = (int16_t)(cfg->cur_look_y * (float)s->look_max / 127.0f);
    gm->pupil_cx = clamp_i16(cfg->cx + ppx, gm->eye_l + gm->pupil_r + 1, gm->eye_r - gm->pupil_r - 1);
    gm->pupil_cy = clamp_i16(cfg->cy + ppy, gm->eye_t + gm->pupil_r + 1, gm->eye_b - gm->pupil_r - 1);
    gm->pupil_type = cfg->cur_pupil_type;
    int16_t sx = (int16_t)(ppx * s->shine_parallax), sy = (int16_t)(ppy * s->shine_parallax);
    if (s->s1_r > 0) { gm->s1_x = clamp_i16(gm->pupil_cx + s->s1_dx + sx, gm->eye_l + s->s1_r + 1, gm->eye_r - s->s1_r - 1); gm->s1_y = clamp_i16(gm->pupil_cy + s->s1_dy + sy, gm->eye_t + s->s1_r + 1, gm->eye_b - s->s1_r - 1); } else { gm->s1_x = 0; gm->s1_y = 0; }
    if (s->s2_r > 0) { gm->s2_x = clamp_i16(gm->pupil_cx + s->s2_dx + sx, gm->eye_l + s->s2_r + 1, gm->eye_r - s->s2_r - 1); gm->s2_y = clamp_i16(gm->pupil_cy + s->s2_dy + sy, gm->eye_t + s->s2_r + 1, gm->eye_b - s->s2_r - 1); } else { gm->s2_x = 0; gm->s2_y = 0; }
    if (s->s3_r > 0) { gm->s3_x = clamp_i16(gm->pupil_cx + s->s3_dx + sx, gm->eye_l + s->s3_r + 1, gm->eye_r - s->s3_r - 1); gm->s3_y = clamp_i16(gm->pupil_cy + s->s3_dy + sy, gm->eye_t + s->s3_r + 1, gm->eye_b - s->s3_r - 1); } else { gm->s3_x = 0; gm->s3_y = 0; }
    float lt, lb;
    if (cfg->lid > 0.001f) { lt = cfg->lid; lb = cfg->lid * 0.5f; }
    else { lt = is_left ? cfg->cur_lid_top_l : cfg->cur_lid_top_r; lb = cfg->cur_lid_bottom; }
    gm->lid_top_base_y = gm->eye_t + (int16_t)((float)(hh * 2 + 4) * lt);
    gm->lid_slope_px = (int16_t)(cfg->cur_lid_slope * (float)hh);
    int16_t yi = gm->lid_top_base_y + gm->lid_slope_px, yo = gm->lid_top_base_y - gm->lid_slope_px;
    gm->lid_y_left = is_left ? yo : yi; gm->lid_y_right = is_left ? yi : yo;
    if (lb > 0.001f) gm->lid_bottom_h = (int16_t)((float)(hh + 2) * lb); else gm->lid_bottom_h = 0;
}

void eye_draw_body(U8G2* d, const EyeGeom_t* gm) {
    const EyeStyle_t* s = eye_style_get();
    d->drawRBox(gm->eye_l, gm->eye_t, gm->eye_r - gm->eye_l + 1, gm->eye_b - gm->eye_t + 1, s->eye_radius);
}

void eye_draw_pupil(U8G2* d, const EyeGeom_t* gm) {
    d->setDrawColor(0);
    if (gm->pupil_type < PUPIL_COUNT && s_pupil_draw[gm->pupil_type])
        s_pupil_draw[gm->pupil_type](d, gm);
}

void eye_draw_shine(U8G2* d, const EyeGeom_t* gm) {
    const EyeStyle_t* s = eye_style_get();
    if (gm->pupil_type == PUPIL_HEART || gm->pupil_type == PUPIL_SHOCK || gm->pupil_type == PUPIL_HAPPY) return;
    d->setDrawColor(1);
    if (s->s1_r > 0) d->drawDisc(gm->s1_x, gm->s1_y, s->s1_r, U8G2_DRAW_ALL);
    if (s->s2_r > 0) d->drawDisc(gm->s2_x, gm->s2_y, s->s2_r, U8G2_DRAW_ALL);
    if (s->s3_r > 0) d->drawDisc(gm->s3_x, gm->s3_y, s->s3_r, U8G2_DRAW_ALL);
}

void eye_draw_lid_mask(U8G2* d, const EyeGeom_t* gm) {
    d->setDrawColor(0);
    if (gm->lid_y_left != gm->eye_t || gm->lid_y_right != gm->eye_t) {
        int16_t top = gm->eye_t - 20; if (top < 0) top = 0;
        d->drawTriangle(gm->eye_l - 4, top, gm->eye_r + 4, top, gm->eye_l - 4, gm->lid_y_left);
        d->drawTriangle(gm->eye_r + 4, top, gm->eye_l - 4, gm->lid_y_left, gm->eye_r + 4, gm->lid_y_right);
    }
    if (gm->lid_bottom_h > 0)
        d->drawBox(gm->eye_l - 2, gm->eye_b + 2 - gm->lid_bottom_h, gm->eye_r - gm->eye_l + 5, gm->lid_bottom_h + 2);
    d->setDrawColor(1);
}

void eye_draw_happy_arc(U8G2* d, const EyeGeom_t* gm) {
    /* Happy 弯月已由 lid_mask 实现, 此处仅绘制星光粒子 (在暗色 lid 上闪烁) */
    int16_t eye_w = gm->eye_r - gm->eye_l;
    int16_t eye_h = gm->eye_b - gm->eye_t;
    uint32_t star_t = millis() % 1500;
    float alpha = 0.0f;
    if (star_t < 100) alpha = (float)star_t / 100.0f;
    else if (star_t < 200) alpha = 1.0f - (float)(star_t - 100) / 100.0f;

    if (alpha > 0.05f) {
        d->setDrawColor(1);
        uint8_t cnt = (uint8_t)(alpha * 4.0f);
        if (cnt < 1) cnt = 1; if (cnt > 3) cnt = 3;
        for (uint8_t s = 0; s < cnt; s++) {
            int16_t sx = gm->eye_l + 6 + (rand() % (eye_w - 12));
            int16_t sy = gm->eye_t + eye_h / 4 + (rand() % (eye_h / 2));
            int16_t sl = 2 + (rand() % 3);
            d->drawHLine(sx - sl, sy, sl * 2);
            d->drawVLine(sx, sy - sl, sl * 2);
        }
        d->setDrawColor(0);
    }
}
static void eye_draw_tears(U8G2* d, const EyeGeom_t* gm) {
    int16_t pcx = gm->pupil_cx, pcy = gm->pupil_cy, pr = gm->pupil_r;
    int16_t toy = gm->eye_b - gm->lid_bottom_h; if (toy < pcy + pr) toy = pcy + pr;
    d->setDrawColor(1);
    uint32_t wt = millis() % 800;
    if (wt < 400) { int16_t wy = toy - 1; d->drawHLine(pcx - pr + 1, wy, pr * 2 - 2); if (pr > 5) d->drawHLine(pcx - pr / 2, toy - 3, pr); }
    d->setDrawColor(0);
    const uint32_t cm = 2000; uint32_t t1 = millis() % cm;
    int16_t y1 = toy + (int16_t)((float)t1 * 0.015f), tmy = gm->eye_b - 2;
    if (y1 > tmy) y1 = tmy; if (y1 < toy) y1 = toy;
    if (y1 < tmy) { int16_t x1 = gm->is_left ? (gm->eye_l + 9) : (gm->eye_r - 9); d->drawDisc(x1, y1, 4, U8G2_DRAW_ALL); d->drawTriangle(x1 - 4, y1, x1 + 4, y1, x1, y1 - 6); }
    uint32_t t2 = (millis() + 700) % cm; int16_t y2 = toy + (int16_t)((float)t2 * 0.010f);
    if (y2 > tmy) y2 = tmy; if (y2 < toy) y2 = toy;
    if (y2 < tmy) { int16_t x2 = gm->is_left ? (gm->eye_l + 19) : (gm->eye_r - 19); d->drawDisc(x2, y2, 3, U8G2_DRAW_ALL); d->drawTriangle(x2 - 3, y2, x2 + 3, y2, x2, y2 - 5); }
}
void eye_draw_sad_water(U8G2* d, const EyeGeom_t* gm) {
    d->setDrawColor(1); int16_t bx = gm->eye_l + (gm->eye_r - gm->eye_l) / 2;
    int16_t by = gm->eye_b - gm->lid_bottom_h - 1; uint32_t phase = millis() % 1200;
    if (phase < 400) { float alpha = (phase < 200) ? (float)phase / 200.0f : 1.0f - (float)(phase - 200) / 200.0f;
        if (alpha > 0.3f) { int16_t w = (int16_t)(alpha * (float)(gm->eye_r - gm->eye_l) * 0.4f); d->drawHLine(bx - w, by, w * 2); d->drawHLine(bx - w + 2, by - 1, (w - 2) * 2); } }
    else if (phase < 600) { d->drawPixel(bx - 6, by); d->drawPixel(bx + 6, by); }
    d->setDrawColor(0);
}

void eye_draw_sweat(U8G2* d, const EyeGeom_t* gm) {
    int16_t soy = gm->eye_t + 3;
    d->setDrawColor(0);
    const uint32_t cm = 600;  /* 加快循环 */
    uint32_t t = millis() % cm;

    /* 大汗珠 #1: r=4, 快速滑落 */
    int16_t sx1 = gm->is_left ? (gm->eye_l + 4) : (gm->eye_r - 4);
    int16_t y1 = soy + (int16_t)((float)t * 0.045f);
    int16_t smy = gm->eye_b - 1;
    if (y1 > smy) y1 = smy; if (y1 < soy) y1 = soy;
    if (y1 < smy) {
        d->drawDisc(sx1, y1 - 1, 4, U8G2_DRAW_ALL);
        d->drawTriangle(sx1 - 4, y1, sx1 + 4, y1, sx1, y1 + 5);
    }

    /* 大汗珠 #2: r=3, 错相滑落 */
    int16_t sx2 = gm->is_left ? (gm->eye_l + 16) : (gm->eye_r - 16);
    uint32_t t2 = (millis() + 300) % cm;
    int16_t y2 = soy + (int16_t)((float)t2 * 0.038f);
    if (y2 > smy) y2 = smy; if (y2 < soy) y2 = soy;
    if (y2 < smy) {
        d->drawDisc(sx2, y2 - 1, 3, U8G2_DRAW_ALL);
        d->drawTriangle(sx2 - 3, y2, sx2 + 3, y2, sx2, y2 + 4);
    }

    /* 汗珠溅落飞溅效果 */
    if (t > 570 && t < 600) {
        d->setDrawColor(1);
        d->drawPixel(sx1 - 2, smy - 1); d->drawPixel(sx1 + 2, smy - 1);
        d->drawPixel(sx1, smy - 2); d->drawPixel(sx1 - 1, smy - 2); d->drawPixel(sx1 + 1, smy - 2);
        d->setDrawColor(0);
    }
}

void eye_render(U8G2* d, EyeConfig_t* cfg, bool is_left) {
    const EyeStyle_t* s = eye_style_get(); EyeGeom_t gm;
    eye_geom_compute(&gm, cfg, s, is_left);
    d->setDrawColor(1); eye_draw_body(d, &gm);
    eye_draw_pupil(d, &gm);
    if (cfg->active_expr == 3) { eye_draw_sad_water(d, &gm); eye_draw_tears(d, &gm); }
    if (cfg->active_expr == 6) eye_draw_sweat(d, &gm);
    eye_draw_shine(d, &gm);
    eye_draw_lid_mask(d, &gm);
    if (cfg->active_expr == 1) eye_draw_happy_arc(d, &gm);
}

void eye_config_init(EyeConfig_t* cfg, uint8_t cx, uint8_t cy) {
    cfg->cx = cx; cfg->cy = cy; cfg->lid = 0.0f;
    cfg->target_look_x = 0; cfg->target_look_y = 0; cfg->cur_look_x = 0.0f; cfg->cur_look_y = 0.0f;
    cfg->active_expr = 255;
    cfg->target_lid_top = 0.0f; cfg->target_lid_top_l = 0.0f; cfg->target_lid_top_r = 0.0f;
    cfg->target_lid_bottom = 0.0f; cfg->target_lid_slope = 0.0f;
    cfg->target_pupil_scale = 1.0f; cfg->target_pupil_type = PUPIL_NORMAL; cfg->cur_pupil_type = PUPIL_NORMAL;
    cfg->cur_lid_top = 0.0f; cfg->cur_lid_top_l = 0.0f; cfg->cur_lid_top_r = 0.0f;
    cfg->cur_lid_bottom = 0.0f; cfg->cur_lid_slope = 0.0f; cfg->cur_pupil_scale = 1.0f; cfg->target_pupil_scale_r = 0.0f; cfg->cur_pupil_scale_r = 1.0f;
    cfg->anim_peak_scale = 0.0f; cfg->anim_start_ms = 0; cfg->anim_duration_ms = 0;
    cfg->sleepy_phase_ms = 0; cfg->sleepy_lid = 0.0f; cfg->sleepy_struggle_sub = 0;
    cfg->brow_phase = 0.0f; cfg->brow_angry_phase = 0.0f; cfg->brow_burst_timer = 0.0f;
    cfg->brow_anim_phase = 0.0f; cfg->brow_offset_l = 0; cfg->brow_offset_r = 0;
    cfg->tear_phase_ms = 0; cfg->tear_phase2_ms = 0; cfg->sad_water_phase_ms = 0;
    cfg->attention_next_ms = millis() + 3000; cfg->attention_target_x = 0; cfg->attention_target_y = 0;
    cfg->attention_prev_x = 0; cfg->attention_prev_y = 0; cfg->attention_phase = 0;
    cfg->overdrive_decay = 0.0f; cfg->overdrive_amount = 0.0f;
    cfg->idle_micro_next_ms = millis() + 2000; cfg->idle_micro_type = 0;
    cfg->idle_micro_lid_delta = 0.0f; cfg->idle_micro_pupil_delta = 0.0f;
    cfg->happy_wink_next_ms = millis() + 3500; cfg->happy_wink_eye = 0; cfg->happy_wink_start_ms = 0;
    cfg->panic_scan_next_ms = millis() + 120; cfg->panic_sweat_seed = 0;
    cfg->excited_heartbeat_ms = millis();
}

void eye_set_look(EyeConfig_t* cfg, int8_t x, int8_t y) { cfg->target_look_x = x; cfg->target_look_y = y; }

void eye_look_update(EyeConfig_t* cfg) {
    cfg->cur_look_x += ((float)cfg->target_look_x - cfg->cur_look_x) * LOOK_SMOOTH_FACTOR;
    cfg->cur_look_y += ((float)cfg->target_look_y - cfg->cur_look_y) * LOOK_SMOOTH_FACTOR;
}

void eye_look_reset(EyeConfig_t* cfg) { cfg->target_look_x = 0; cfg->target_look_y = 0; }
void eye_set_expression(EyeConfig_t* cfg, uint8_t ei) {
    if (ei >= 8) return; const ExpressionDef_t* e = &EXPRESSIONS[ei]; cfg->active_expr = ei;
    cfg->target_lid_top = e->lid_top; cfg->target_lid_top_l = e->lid_top_l; cfg->target_lid_top_r = e->lid_top_r;
    cfg->target_lid_bottom = e->lid_bottom; cfg->target_lid_slope = e->lid_slope; cfg->target_pupil_type = e->pupil_type;
    cfg->sleepy_phase_ms = 0; cfg->sleepy_lid = e->lid_top; cfg->sleepy_struggle_sub = 0;
    cfg->brow_phase = 0.0f; cfg->brow_angry_phase = 0.0f; cfg->brow_burst_timer = 0.0f;
    cfg->brow_anim_phase = 0.0f; cfg->brow_offset_l = 0; cfg->brow_offset_r = 0;
    cfg->tear_phase_ms = 0; cfg->tear_phase2_ms = 800; cfg->sad_water_phase_ms = 0;
    cfg->happy_wink_next_ms = millis() + 3500; cfg->happy_wink_eye = 0; cfg->happy_wink_start_ms = 0;
    cfg->panic_scan_next_ms = millis() + 120; cfg->panic_sweat_seed = (uint8_t)(rand() & 0xFF);
    cfg->excited_heartbeat_ms = millis();
    if (ei == 7) { cfg->target_pupil_scale = e->pupil_scale; cfg->anim_peak_scale = 0.0f; cfg->anim_start_ms = 0; cfg->anim_duration_ms = 0; }
    else if (e->anim_peak > 0.001f || e->anim_peak < -0.001f) { cfg->target_pupil_scale = e->anim_peak; cfg->anim_peak_scale = e->anim_peak; cfg->anim_start_ms = millis(); cfg->anim_duration_ms = e->anim_ms; }
    else { cfg->target_pupil_scale = e->pupil_scale; cfg->anim_peak_scale = 0.0f; cfg->anim_start_ms = 0; cfg->anim_duration_ms = 0; }
    if (e->brow_anim == BROW_ANIM_RAISE_BOUNCE) { cfg->overdrive_amount = e->brow_amp * 1.8f; cfg->overdrive_decay = 0.85f; }
    else { cfg->overdrive_amount = 0.0f; cfg->overdrive_decay = 0.0f; }
}

void eye_expr_update(EyeConfig_t* cfg, uint32_t now_ms) {
    if (cfg->anim_peak_scale > 0.001f || cfg->anim_peak_scale < -0.001f) { uint32_t el = now_ms - cfg->anim_start_ms; if (el >= cfg->anim_duration_ms) { cfg->anim_peak_scale = 0.0f; if (cfg->active_expr < 8) cfg->target_pupil_scale = EXPRESSIONS[cfg->active_expr].pupil_scale; } }
    /* ---- [1] Smirk 滑稽笑: 左眼半闭 + 右眼狡黠 + 斜视漂移 ---- */
    if (cfg->active_expr == 1) {
        uint32_t smirk_phase = (now_ms / 2200) % 3;
        if (smirk_phase == 0)      { cfg->target_look_x = -22; cfg->target_look_y = 6; }
        else if (smirk_phase == 1) { cfg->target_look_x = 18; cfg->target_look_y = -4; }
        else                       { cfg->target_look_x = -6; cfg->target_look_y = 10; }
    }
    /* ---- [4] Surprised: 160ms左眼瞳孔巨→缩, 右眼同时缩→巨 (反向跳动!) ---- */
    if (cfg->active_expr == 4) {
        uint32_t st = now_ms % 160;
        if (st < 40) {
            cfg->target_lid_top_l = -0.30f;  cfg->target_lid_top_r =  0.60f;
            cfg->target_pupil_scale   = 0.40f + (float)st / 40.0f * 1.30f;  /* 左: 0.4→1.7 巨! */
            cfg->target_pupil_scale_r = 1.70f - (float)st / 40.0f * 1.30f;  /* 右: 1.7→0.4 反向! */
        } else if (st < 80) {
            cfg->target_lid_top_l = -0.30f;  cfg->target_lid_top_r =  0.60f;
            cfg->target_pupil_scale   = 1.70f - (float)(st - 40) / 40.0f * 1.30f;
            cfg->target_pupil_scale_r = 0.40f + (float)(st - 40) / 40.0f * 1.30f;
        } else if (st < 120) {
            cfg->target_lid_top_l =  0.60f;  cfg->target_lid_top_r = -0.30f;
            cfg->target_pupil_scale   = 0.40f + (float)(st - 80) / 40.0f * 1.30f;
            cfg->target_pupil_scale_r = 1.70f - (float)(st - 80) / 40.0f * 1.30f;
        } else {
            cfg->target_lid_top_l =  0.60f;  cfg->target_lid_top_r = -0.30f;
            cfg->target_pupil_scale   = 1.70f - (float)(st - 120) / 40.0f * 1.30f;
            cfg->target_pupil_scale_r = 0.40f + (float)(st - 120) / 40.0f * 1.30f;
        }
    }
            /* ---- [5] Sleepy: 极限犯困挣扎 ---- */ if (cfg->active_expr == 5) { cfg->sleepy_phase_ms += 33; uint32_t cy = cfg->sleepy_phase_ms % 4000; if (cy < 2000) { /* 犯困挣扎: lid 逼近 0.95, 眉毛夸张向内收 (用力皱眉) */
            float t = (float)cy / 2000.0f;
            cfg->sleepy_lid = 0.45f + t * 0.50f;
            float dr = sinf((float)cy * 0.0025f) * 12.0f;
            cfg->target_look_x = (int8_t)dr;
            cfg->target_look_y = (int8_t)(cosf((float)cy * 0.003f) * 5.0f);
            int16_t bs = (int16_t)(t * t * 18.0f);
            cfg->brow_offset_l = -bs; cfg->brow_offset_r = -bs;
            cfg->sleepy_struggle_sub = 0;
        } else if (cy < 2400) {
            /* 惊醒瞬间: lid 跳到 0.05, 眉毛瞬间弹开高挑到 +20 */
            float t = (float)(cy - 2000) / 400.0f;
            float sn = 1.0f - t;
            cfg->sleepy_lid = 0.95f - sn * 0.90f;
            cfg->target_look_x = 0; cfg->target_look_y = 0;
            int16_t bp;
            if (t < 0.3f) bp = (int16_t)(expf(-t * 5.0f) * 20.0f);
            else bp = (int16_t)(5.0f * (1.0f - t));
            cfg->brow_offset_l = bp; cfg->brow_offset_r = bp;
            cfg->sleepy_struggle_sub = 1;
        } else if (cy < 3100) {
            /* 眼球不受控乱晃 + 眉毛微颤 */
            float t = (float)(cy - 2400) / 700.0f;
            float wb = sinf(t * M_PI * 3.0f) * 0.06f;
            cfg->sleepy_lid = 0.05f + wb;
            cfg->target_look_x = (int8_t)(sinf(t * M_PI * 3.5f) * 24.0f);
            cfg->target_look_y = (int8_t)(cosf(t * M_PI * 1.3f) * 10.0f);
            cfg->brow_offset_l = (int16_t)(sinf(t * M_PI * 2.0f) * 6.0f);
            cfg->brow_offset_r = (int16_t)(cosf(t * M_PI * 2.5f) * 5.0f);
            cfg->sleepy_struggle_sub = 2;
        } else {
            /* 重新犯困, 眉毛再次内收 */
            float t = (float)(cy - 3100) / 900.0f;
            cfg->sleepy_lid = 0.05f + t * 0.40f;
            cfg->target_look_x = 0; cfg->target_look_y = 0;
            cfg->brow_offset_l = (int16_t)(-t * 15.0f);
            cfg->brow_offset_r = (int16_t)(-t * 15.0f);
            cfg->sleepy_struggle_sub = 3;
        }
        cfg->target_lid_top   = cfg->sleepy_lid; cfg->target_lid_top_l = cfg->sleepy_lid; cfg->target_lid_top_r = cfg->sleepy_lid;
    }
    /* ---- [6] Panic: 高频乱颤扫视 (50~100ms) + 大幅漂移 + 瞳孔抖动 ---- */
    if (cfg->active_expr == 6) {
        float br = 0.60f + sinf((float)now_ms * 0.035f) * 0.25f;
        cfg->target_pupil_scale = br;
        if (now_ms >= cfg->panic_scan_next_ms) {
            cfg->target_look_x = (int8_t)((rand() % 121) - 60);
            cfg->target_look_y = (int8_t)((rand() % 81) - 40);
            cfg->panic_scan_next_ms = now_ms + 50 + (rand() % 51);
        }
    }
    /* ---- [7] Excited: 心跳由 draw_pupil_heart 驱动, 此处保持基础 scale ---- */
    if (cfg->active_expr == 7) { cfg->target_pupil_scale = 1.20f; }
    /* ---- 眉毛动画引擎 (所有非 Sleepy 表情) ---- */
    if (cfg->active_expr < 8 && cfg->active_expr != 5) {
        const ExpressionDef_t* e = &EXPRESSIONS[cfg->active_expr]; float f = e->brow_freq, a = e->brow_amp, as = e->brow_asymmetry, ol = 0.0f, orr = 0.0f;
        switch (e->brow_anim) {
        case BROW_ANIM_BREATHE: cfg->brow_anim_phase += f; ol = sinf(cfg->brow_anim_phase) * a; orr = ol; break;
        case BROW_ANIM_TREMBLE: { cfg->brow_anim_phase += f; float ca = sinf(cfg->brow_anim_phase) * a, bu = 0.0f; uint32_t cyc = millis() % (uint32_t)e->brow_burst_intv, bw = (uint32_t)e->brow_burst_intv / 6; if (cyc < bw) { float bt = (float)cyc / (float)bw; bu = sinf(bt * M_PI) * e->brow_burst_amp; } ol = ca + bu; orr = (ca + bu) * as; break; }
        case BROW_ANIM_SOB: cfg->brow_anim_phase += f; ol = sinf(cfg->brow_anim_phase) * a; orr = sinf(cfg->brow_anim_phase + M_PI * as) * a; break;
        case BROW_ANIM_RAISE_BOUNCE: { cfg->brow_anim_phase += f; float sw = sinf(cfg->brow_anim_phase) * a * 0.25f, ov = cfg->overdrive_amount; if (cfg->overdrive_decay > 0.0f) { cfg->overdrive_amount *= cfg->overdrive_decay; if (fabsf(cfg->overdrive_amount) < 0.1f) cfg->overdrive_amount = 0.0f; } ol = sw + ov; orr = ol; break; }
        case BROW_ANIM_SAG_DRIFT: cfg->brow_anim_phase += f; { float d1 = sinf(cfg->brow_anim_phase) * a * 0.5f, d2 = sinf(cfg->brow_anim_phase * 0.3f + 1.5f) * a * 0.5f * as; ol = -a + d1; orr = -a + d2; } break;
        case BROW_ANIM_TWITCH: { uint32_t tc = (uint32_t)(cfg->brow_anim_phase * 1000.0f), iv = 1200 + (tc * 7 % 800), ps = tc % iv; if (ps < 40) { float t = (float)ps / 40.0f; ol = a * t; orr = 0.0f; } else if (ps < 80) { float t = (float)(ps - 40) / 40.0f; ol = a * (1.0f - t); orr = 0.0f; } else { ol = 0.0f; orr = 0.0f; } cfg->brow_anim_phase += f; break; }
        case BROW_ANIM_SWAY: cfg->brow_anim_phase += f; { float dl = sinf(cfg->brow_anim_phase) * a; ol = dl; orr = -dl; } break;
        case BROW_ANIM_PANIC: cfg->brow_anim_phase += f; { float tr = sinf(cfg->brow_anim_phase) * a, no = ((float)(rand() % 100) / 100.0f - 0.5f) * a * 0.7f, dl = a + tr + no; ol = dl; orr = dl; } break;
        case BROW_ANIM_NONE: default: cfg->brow_anim_phase += f; ol = 0.0f; orr = 0.0f; break;
        }
        cfg->brow_offset_l = (int16_t)ol; cfg->brow_offset_r = (int16_t)orr;
    }
    /* 所有参数的通用平滑插值 */
    cfg->cur_lid_top += (cfg->target_lid_top - cfg->cur_lid_top) * 0.18f; cfg->cur_lid_top_l += (cfg->target_lid_top_l - cfg->cur_lid_top_l) * 0.18f; cfg->cur_lid_top_r += (cfg->target_lid_top_r - cfg->cur_lid_top_r) * 0.18f; cfg->cur_lid_bottom += (cfg->target_lid_bottom - cfg->cur_lid_bottom) * 0.18f; cfg->cur_lid_slope += (cfg->target_lid_slope - cfg->cur_lid_slope) * 0.18f; cfg->cur_pupil_scale += (cfg->target_pupil_scale - cfg->cur_pupil_scale) * 0.18f; cfg->cur_pupil_type = cfg->target_pupil_type;
}
void eye_attention_update(EyeConfig_t* cfg, uint32_t now_ms) {
    if (cfg->active_expr != 0 && cfg->active_expr != 255) return; if (cfg->lid > 0.1f) return;
    if (cfg->attention_phase == 0) { if (now_ms >= cfg->attention_next_ms) { cfg->attention_prev_x = cfg->target_look_x; cfg->attention_prev_y = cfg->target_look_y; cfg->attention_target_x = (int8_t)((rand() % 61) - 30); cfg->attention_target_y = (int8_t)((rand() % 41) - 20); cfg->attention_phase = 1; } }
    else if (cfg->attention_phase == 1) { float t = 0.05f; float dx = (float)(cfg->attention_target_x - cfg->target_look_x) * t, dy = (float)(cfg->attention_target_y - cfg->target_look_y) * t; cfg->target_look_x += (int8_t)dx; cfg->target_look_y += (int8_t)dy; if (abs(cfg->attention_target_x - cfg->target_look_x) <= 1 && abs(cfg->attention_target_y - cfg->target_look_y) <= 1) { cfg->attention_phase = 2; cfg->attention_next_ms = now_ms + 800 + (rand() % 1500); } }
    else if (cfg->attention_phase == 2) { if (now_ms >= cfg->attention_next_ms) cfg->attention_phase = 3; }
    else { float t = 0.03f; cfg->target_look_x += (int8_t)((float)(-cfg->target_look_x) * t); cfg->target_look_y += (int8_t)((float)(-cfg->target_look_y) * t); if (abs(cfg->target_look_x) <= 1 && abs(cfg->target_look_y) <= 1) { cfg->target_look_x = 0; cfg->target_look_y = 0; cfg->attention_phase = 0; cfg->attention_next_ms = now_ms + 2000 + (rand() % 4000); } }
}

void eye_idle_micro_update(EyeConfig_t* cfg, uint32_t now_ms) {
    if (cfg->lid > 0.2f) return;
    if (cfg->idle_micro_type == 0) { if (now_ms >= cfg->idle_micro_next_ms) { uint8_t r = rand() % 6; if (r == 0) { cfg->idle_micro_type = 1; cfg->idle_micro_pupil_delta = (rand() % 2) ? 0.05f : -0.05f; } else if (r == 1) { cfg->idle_micro_type = 2; } else if (r == 2) { cfg->idle_micro_type = 3; cfg->idle_micro_lid_delta = 0.04f; } else { cfg->idle_micro_next_ms = now_ms + 1500 + (rand() % 3000); } } }
    else { uint32_t el = now_ms - (cfg->idle_micro_next_ms - 500); if (el > 120) { if (cfg->idle_micro_type == 1) { cfg->target_pupil_scale -= cfg->idle_micro_pupil_delta; cfg->idle_micro_pupil_delta = 0.0f; } else if (cfg->idle_micro_type == 3) { cfg->target_lid_top -= cfg->idle_micro_lid_delta; cfg->idle_micro_lid_delta = 0.0f; } cfg->idle_micro_type = 0; cfg->idle_micro_next_ms = now_ms + 2000 + (rand() % 4000); } else { if (cfg->idle_micro_type == 1) cfg->target_pupil_scale += cfg->idle_micro_pupil_delta * 0.05f; else if (cfg->idle_micro_type == 2 && el < 60) cfg->brow_offset_l = (int16_t)((rand() % 5) - 2); else if (cfg->idle_micro_type == 3) cfg->target_lid_top += cfg->idle_micro_lid_delta * 0.1f; } }
}

void blink_state_init(BlinkState_t* st) {
    st->phase = BLINK_IDLE; st->phase_start_ms = 0; st->phase_duration_ms = 0;
    st->next_blink_ms = millis() + BLINK_INTERVAL_MIN + (rand() % (BLINK_INTERVAL_MAX - BLINK_INTERVAL_MIN));
}

void blink_state_update(BlinkState_t* st, EyeConfig_t* cfg, uint32_t now_ms) {
    switch (st->phase) {
    case BLINK_IDLE: if (now_ms >= st->next_blink_ms) { st->phase = BLINK_CLOSING; st->phase_start_ms = now_ms; st->phase_duration_ms = BLINK_CLOSING_MS; } break;
    case BLINK_CLOSING: { float t = (float)(now_ms - st->phase_start_ms) / st->phase_duration_ms; if (t >= 1.0f) { cfg->lid = 1.0f; st->phase = BLINK_HOLD; st->phase_start_ms = now_ms; st->phase_duration_ms = BLINK_HOLD_MS; } else cfg->lid = ease_in(t); break; }
    case BLINK_HOLD: if (now_ms - st->phase_start_ms >= st->phase_duration_ms) { st->phase = BLINK_OPENING; st->phase_start_ms = now_ms; st->phase_duration_ms = BLINK_OPENING_MS; } break;
    case BLINK_OPENING: { float t = (float)(now_ms - st->phase_start_ms) / st->phase_duration_ms; if (t >= 1.0f) { cfg->lid = 0.0f; st->phase = BLINK_IDLE; st->next_blink_ms = now_ms + BLINK_INTERVAL_MIN + (rand() % (BLINK_INTERVAL_MAX - BLINK_INTERVAL_MIN)); } else cfg->lid = 1.0f - ease_out(t); break; }
    }
}``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\force_return.cpp 
```c 
/**
 * @file    force_return.cpp
 * @brief   RobotEyes 安全归位机制实现
 *
 *  状态图说明:
 *    IDLE:         等待按键按下
 *    PRESSED:      按键按住中, 累计时间
 *    LONG_TRIGGERED: 已触发长按, 等待释放 (防止重复触发)
 *
 *  互斥规则:
 *    - 长按触发后, 不再产生短按事件
 *    - 释放后回到 IDLE 才会重置一切
 *
 *  GPIO 中断: 上升/下降沿均触发, 通过 digitalRead() 判断当前电平
 */

#include "force_return.h"
#include "event_bus.h"
#include "pin_config.h"
#include <Arduino.h>

/* ---- 状态 ---- */
typedef enum {
    FR_IDLE = 0,
    FR_PRESSED,
    FR_LONG_TRIGGERED
} FRState_t;

static FRState_t  g_state     = FR_IDLE;
static uint32_t   g_press_ms  = 0;
static volatile bool g_isr_fired = false;

/* ---- GPIO 中断回调 ---- */
static void IRAM_ATTR fr_isr(void) {
    g_isr_fired = true;
}

/* ================================================================
 *  force_return_init()
 * ================================================================ */
void force_return_init(void) {
    pinMode(PIN_JOY_SW, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_JOY_SW), fr_isr, CHANGE);
    g_state    = FR_IDLE;
    g_press_ms = 0;
}

/* ================================================================
 *  force_return_poll() — 每周期调用 (由 InputTask 驱动 ~30Hz)
 * ================================================================ */
void force_return_poll(uint32_t now_ms) {
    if (!g_isr_fired) return;
    g_isr_fired = false;

    bool pressed = (digitalRead(PIN_JOY_SW) == LOW);  /* 低电平有效 */

    switch (g_state) {

    case FR_IDLE:
        if (pressed) {
            g_state    = FR_PRESSED;
            g_press_ms = now_ms;
        }
        break;

    case FR_PRESSED:
        if (!pressed) {
            /* 短按: 释放 (< 2s) */
            EventMsg_t msg = { EVT_BUTTON_SHORT, 0, 0, {0} };
            event_bus_push(&msg);
            g_state = FR_IDLE;
        } else if (now_ms - g_press_ms >= FORCE_RETURN_HOLD_MS) {
            /* 长按触发 */
            EventMsg_t msg = { EVT_BUTTON_LONG, 0, 0, {0} };
            event_bus_push(&msg);
            g_state = FR_LONG_TRIGGERED;
        }
        break;

    case FR_LONG_TRIGGERED:
        if (!pressed) {
            /* 释放, 回到空闲 */
            g_state = FR_IDLE;
        }
        /* 按住中: 不再产生任何事件, 防止重复触发 */
        break;
    }
}
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\input_task.cpp 
```c 
/**
 * @file    input_task.cpp
 * @brief   RobotEyes 输入采集 Task 实现
 *
 *  摇杆去毛刺算法:
 *    1. 启动时自动校准中心点 (多次采样取平均)
 *    2. 每次采集: 10次 ADC → 排序 → 丢弃2个极值 → 平均8个
 *    3. 归一化到 [-127, 127], 加死区
 *    4. 稳定性确认: 连续3次读数一致才推送事件
 *
 *  SW 按键: 委托 force_return 模块处理 (GPIO中断 + 状态机)
 */

#include "input_task.h"
#include "event_bus.h"
#include "force_return.h"
#include "expressions.h"
#include "pin_config.h"
#include <Arduino.h>
#include <algorithm>

TaskHandle_t g_inputTaskHandle = NULL;

/* ---- 校准值 ---- */
static int16_t g_joy_center_x = 2048;
static int16_t g_joy_center_y = 2048;

/* ---- 上次推送的归一化值 ---- */
static int8_t g_last_sent_x = 0;
static int8_t g_last_sent_y = 0;

/* ---- 待确认值 + 稳定性计数器 ---- */
static int8_t  g_pending_x = 0;
static int8_t  g_pending_y = 0;
static uint8_t g_stable_cnt = 0;

/* ---- 稳定阈值 (归一化单位) ---- */
#define STABLE_THRESHOLD  5

/* ================================================================
 *  ADC 键盘去抖状态机 (v5.6)
 *
 *  时序设计 (防止"长按才响应"):
 *    边沿检测: NONE→KEY 跳变后, 30ms 去抖确认一次, 立即触发
 *    不积累稳定周期, 不要求持续按住
 *    长按判定是独立计时器, 不与触发判定混合
 *
 *  状态:
 *    adc_last_stable:  上次确认的按键 (0=NONE, 1-8=S1-S8)
 *    adc_current_raw:  当前采样的按键
 *    adc_debounce_ms:  去抖计时起点
 * ================================================================ */
static uint8_t  adc_last_stable = 0;
static uint8_t  adc_current_raw = 0;
static uint32_t adc_debounce_ms = 0;

/* 按键按下时刻追踪 (用于短按/长按判定) */
static uint32_t adc_press_start_ms = 0;
static uint8_t  adc_pressed_key     = 0;   /* 当前按下的键 (0=无) */

#define ADC_KEY_DEBOUNCE_MS   30   /* 边沿去抖窗口: 30ms */
#define ADC_KEY_NONE           0   /* 无按键 */

/* ---- 查表: ADC 值 → 表情索引 (0-7), 255=无按键 ---- */
static uint8_t adc_lookup_expr(uint16_t adc_val) {
    if (adc_val >= ADC_KEY_NONE_MIN && adc_val <= ADC_KEY_NONE_MAX) {
        return ADC_KEY_NONE;  /* 无按键 */
    }
    for (uint8_t i = 0; i < ADC_KEY_MAP_COUNT; i++) {
        if (adc_val >= ADC_KEY_MAP[i].min && adc_val <= ADC_KEY_MAP[i].max) {
            return ADC_KEY_MAP[i].expr_index + 1;  /* 返回 1-8 */
        }
    }
    return ADC_KEY_NONE;  /* 未命中任何区间, 视为无按键 */
}

/* ================================================================
 *  辅助: 快速排序 (冒泡, 10元素足够快)
 * ================================================================ */
static void sort_samples(int16_t* arr, uint8_t n) {
    for (uint8_t i = 0; i < n - 1; i++) {
        for (uint8_t j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int16_t tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

/* ================================================================
 *  辅助: 去极值平均
 * ================================================================ */
static int16_t filtered_average(int16_t* samples, uint8_t n, uint8_t discard) {
    sort_samples(samples, n);
    int32_t sum = 0;
    uint8_t count = n - discard * 2;
    for (uint8_t i = discard; i < n - discard; i++) {
        sum += samples[i];
    }
    return (int16_t)(sum / count);
}

/* ================================================================
 *  calibrate_center() — 启动时自动校准摇杆中心
 *
 *  采样 200 次 (约2秒), 取平均作为 center_x/center_y
 * ================================================================ */
static void calibrate_center(void) {
    Serial.println(F("[INPUT] Calibrating joystick center (2s)..."));
    int32_t sum_x = 0, sum_y = 0;
    const int cal_samples = 200;

    for (int i = 0; i < cal_samples; i++) {
        sum_x += analogRead(PIN_JOY_X);
        sum_y += analogRead(PIN_JOY_Y);
        delay(10);
    }

    g_joy_center_x = (int16_t)(sum_x / cal_samples);
    g_joy_center_y = (int16_t)(sum_y / cal_samples);

    Serial.print(F("[INPUT] Calibrated center: X="));
    Serial.print(g_joy_center_x);
    Serial.print(F(" Y="));
    Serial.println(g_joy_center_y);
}

/* ================================================================
 *  normalize_adc() — ADC原始值 → 归一化 [-127, 127]
 * ================================================================ */
static int8_t normalize_adc(int16_t raw, int16_t center) {
    int32_t diff = (int32_t)raw - (int32_t)center;
    int32_t norm = diff * 127 / INPUT_ADC_RANGE;
    if (norm > 127)  norm = 127;
    if (norm < -127) norm = -127;
    return (int8_t)norm;
}

/* ================================================================
 *  apply_deadzone() — 死区归零
 * ================================================================ */
static int8_t apply_deadzone(int8_t val) {
    if (val >= -INPUT_DEADZONE && val <= INPUT_DEADZONE) return 0;
    return val;
}

/* ================================================================
 *  input_task_init() — 初始化 ADC 引脚 + 自动校准
 * ================================================================ */
void input_task_init(void) {
    analogReadResolution(12);
    pinMode(PIN_JOY_X, INPUT);
    pinMode(PIN_JOY_Y, INPUT);

    /* 自动校准中心 */
    calibrate_center();

    /* 初始化 Force Return (GPIO中断) */
    force_return_init();

    g_last_sent_x = 0;
    g_last_sent_y = 0;
    g_pending_x   = 0;
    g_pending_y   = 0;
    g_stable_cnt  = 0;

    Serial.println(F("[INPUT] Init done."));
}

/* ================================================================
 *  input_task_run() — FreeRTOS Task 主循环
 *
 *  周期: ~10ms (vTaskDelay(10))
 *  流程:
 *    1. 采集 10 次 ADC → 去极值平均 → 归一化 → 死区
 *    2. 与 pending 值比较 → 稳定性计数
 *    3. 连续稳定 → 推送 EVT_JOYSTICK_MOVE
 *    4. 调用 force_return_poll() 处理 SW 按键
 * ================================================================ */
void input_task_run(void* pvParameters) {
    (void)pvParameters;
    int16_t x_samples[INPUT_RAW_SAMPLES];
    int16_t y_samples[INPUT_RAW_SAMPLES];

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        /* ---- 1. 采集 ADC ---- */
        for (uint8_t i = 0; i < INPUT_RAW_SAMPLES; i++) {
            x_samples[i] = analogRead(PIN_JOY_X);
            y_samples[i] = analogRead(PIN_JOY_Y);
        }

        /* ---- 2. 去极值 + 平均 ---- */
        int16_t raw_x = filtered_average(x_samples, INPUT_RAW_SAMPLES, INPUT_DISCARD_EXTREME);
        int16_t raw_y = filtered_average(y_samples, INPUT_RAW_SAMPLES, INPUT_DISCARD_EXTREME);

        /* ---- 3. 归一化 + 死区 ---- */
        int8_t norm_x = apply_deadzone(normalize_adc(raw_x, g_joy_center_x));
        int8_t norm_y = apply_deadzone(normalize_adc(raw_y, g_joy_center_y));

        /* ---- 4. 稳定性确认 ---- */
        if (abs(norm_x - g_pending_x) <= STABLE_THRESHOLD &&
            abs(norm_y - g_pending_y) <= STABLE_THRESHOLD) {
            g_stable_cnt++;
        } else {
            g_pending_x  = norm_x;
            g_pending_y  = norm_y;
            g_stable_cnt = 0;
        }

        /* 连续稳定 → 推送 */
        if (g_stable_cnt >= INPUT_STABLE_COUNT) {
            if (g_pending_x != g_last_sent_x || g_pending_y != g_last_sent_y) {
                EventMsg_t msg;
                msg.type    = EVT_JOYSTICK_MOVE;
                msg.value_x = g_pending_x;
                msg.value_y = g_pending_y;
                event_bus_push(&msg);

                g_last_sent_x = g_pending_x;
                g_last_sent_y = g_pending_y;
            }
            g_stable_cnt = 0;  /* 重置, 等待下一轮变化 */
        }

        /* ---- 5. ADC 键盘检测 (边沿触发, <50ms 响应) ---- */
        {
            /* 采集 ADC 键盘 (8次平均去噪) */
            int32_t kb_sum = 0;
            for (uint8_t i = 0; i < 8; i++) {
                kb_sum += analogRead(PIN_ADC_KEYBOARD);
            }
            uint16_t kb_avg = (uint16_t)(kb_sum / 8);

            /* 查表: ADC → 键值 (1-8) 或 0=NONE */
            uint8_t kb_current = adc_lookup_expr(kb_avg);

            /* 边沿去抖: 值变化 → 重置计时器 */
            if (kb_current != adc_current_raw) {
                adc_current_raw = kb_current;
                adc_debounce_ms = millis();
            }

            /* 去抖窗口到期 → 确认边沿 */
            uint32_t kb_now = millis();
            if (kb_now - adc_debounce_ms >= ADC_KEY_DEBOUNCE_MS) {
                if (adc_current_raw != adc_last_stable) {
                    /* 按键按下: 推送 EVT_EXPR_SET */
                    if (adc_current_raw != ADC_KEY_NONE) {
                        /* 记录按下时刻, 供释放时计算持有时长 */
                        adc_press_start_ms = kb_now;
                        adc_pressed_key    = adc_current_raw;

                        EventMsg_t kmsg;
                        kmsg.type    = EVT_EXPR_SET;
                        kmsg.value_x = adc_current_raw - 1;  /* 转为 0-based 索引 */
                        kmsg.value_y = 0;
                        event_bus_push(&kmsg);

                        Serial.print(F("[KEY] S"));
                        Serial.print((int)adc_current_raw);
                        Serial.print(F(" pressed (ADC="));
                        Serial.print(kb_avg);
                        Serial.print(F(") → expr="));
                        Serial.println(EXPRESSIONS[adc_current_raw - 1].name);
                    }
                    else {
                        /* 按键释放: 推送 EVT_EXPR_RELEASE (携带持有时长) */
                        uint8_t  released_key = adc_last_stable - 1;  /* 0-based */
                        uint16_t held_ms       = (uint16_t)(kb_now - adc_press_start_ms);

                        EventMsg_t kmsg;
                        kmsg.type    = EVT_EXPR_RELEASE;
                        kmsg.value_x = released_key;
                        kmsg.value_y = (int16_t)held_ms;
                        kmsg._pad[0] = 0;
                        event_bus_push(&kmsg);

                        Serial.print(F("[KEY] S"));
                        Serial.print((int)adc_last_stable);
                        Serial.print(F(" released (held="));
                        Serial.print(held_ms);
                        Serial.println(F("ms)"));
                    }

                    adc_last_stable = adc_current_raw;
                }
            }
        }

        /* ---- 6. SW 按键检测 (Force Return) ---- */
        force_return_poll(millis());

        /* ---- 7. 周期延迟 ---- */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}
``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\main.cpp 
```c 
/**
 * @file    main.cpp
 * @brief   RobotEyes v10.0 — int16_t全链路修复 + DEBUG_EYES穿透式日志
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-10
 *
 *  v10.0 关键升级:
 *    - int8_t → int16_t 全链路修复 (根治 Angry \\ / 镜像Bug)
 *    - DEBUG_EYES 三层穿透式日志: [STATE] [ANIM] [SERVO]
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "pin_config.h"
#include "event_bus.h"
#include "eye_renderer.h"
#include "input_task.h"
#include "servo_task.h"
#include "expressions.h"/* ================================================================
 *  DEBUG_EYES — 穿透式调试日志 (v10)
 *  设为 1 启用三层日志, 设为 0 编译排除所有日志
 * ================================================================ */
#define DEBUG_EYES 1

#if DEBUG_EYES
  #define DEBUG_STATE(fmt, ...)   Serial.print(F("[STATE] ")); Serial.printf(fmt, ##__VA_ARGS__); Serial.println()
  #define DEBUG_ANIM(fmt, ...)    Serial.print(F("[ANIM]  ")); Serial.printf(fmt, ##__VA_ARGS__); Serial.println()
  #define DEBUG_SERVO(fmt, ...)   Serial.print(F("[SERVO] ")); Serial.printf(fmt, ##__VA_ARGS__); Serial.println()
#else
  #define DEBUG_STATE(fmt, ...)   ((void)0)
  #define DEBUG_ANIM(fmt, ...)    ((void)0)
  #define DEBUG_SERVO(fmt, ...)   ((void)0)
#endif

U8G2_SSD1306_128X64_NONAME_F_HW_I2C g_leftDisp(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C g_rightDisp(U8G2_R0, PIN_SW_I2C_SCL, PIN_SW_I2C_SDA, U8X8_PIN_NONE);
bool g_leftReady = false, g_rightReady = false;

static EyeConfig_t  g_eyeCfg;
static BlinkState_t g_blinkState;
static uint32_t g_revert_deadline_ms = 0;
static uint32_t g_last_frame_ms = 0;
static uint32_t g_last_beat_ms = 0;
static uint32_t g_last_servo_debug_ms = 0;   /* v10: [SERVO] 日志计时器 */

/* v10: 眉毛偏移从 int8_t 升级为 int16_t */
static int16_t g_joy_brow_offset_l = 0;
static int16_t g_joy_brow_offset_r = 0;

extern "C" uint8_t esp32_fast_sw_i2c_gpio_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT: pinMode(PIN_SW_I2C_SCL, INPUT_PULLUP); pinMode(PIN_SW_I2C_SDA, INPUT_PULLUP); break;
    case U8X8_MSG_DELAY_MILLI: delay(arg_int); break;
    case U8X8_MSG_DELAY_10MICRO: delayMicroseconds(arg_int * 10); break;
    case U8X8_MSG_DELAY_100NANO: delayMicroseconds((arg_int + 9) / 10); break;
    case U8X8_MSG_DELAY_NANO: { uint32_t ns = *(uint32_t*)arg_ptr; delayMicroseconds((ns + 999) / 1000); break; }
    case U8X8_MSG_GPIO_I2C_CLOCK: if (arg_int) GPIO.enable_w1tc.val = (1UL << PIN_SW_I2C_SCL); else { GPIO.out_w1tc.val = (1UL << PIN_SW_I2C_SCL); GPIO.enable_w1ts.val = (1UL << PIN_SW_I2C_SCL); } break;
    case U8X8_MSG_GPIO_I2C_DATA:  if (arg_int) GPIO.enable_w1tc.val = (1UL << PIN_SW_I2C_SDA); else { GPIO.out_w1tc.val = (1UL << PIN_SW_I2C_SDA); GPIO.enable_w1ts.val = (1UL << PIN_SW_I2C_SDA); } break;
    default: return 0;
    }
    return 1;
}
bool initLeftDisplay() {
    Serial.print(F("[LEFT]  I2C probe @0x")); Serial.print(I2C_ADDR_LEFT, HEX); Serial.print(F(" ... "));
    Wire.beginTransmission(I2C_ADDR_LEFT);
    if (Wire.endTransmission() != 0) { Serial.println(F("[FAIL] skip")); return false; }
    Serial.println(F("[OK]")); Serial.print(F("[LEFT]  begin() ... "));
    g_leftDisp.setI2CAddress(I2C_ADDR_LEFT << 1);
    if (!g_leftDisp.begin()) { Serial.println(F("[FAIL]")); return false; }
    Serial.println(F("[OK]")); g_leftDisp.setBusClock(400000); g_leftDisp.setPowerSave(0); return true;
}

bool initRightDisplay() {
    Serial.print(F("[RIGHT] SW-I2C begin() ... "));
    u8x8_t *u8x8 = g_rightDisp.getU8x8(); u8x8->gpio_and_delay_cb = esp32_fast_sw_i2c_gpio_cb;
    if (!g_rightDisp.begin()) { Serial.println(F("[FAIL]")); return false; }
    Serial.println(F("[OK]")); g_rightDisp.setPowerSave(0); return true;
}

static void screenBlackOut(U8G2* d) { d->clearBuffer(); d->sendBuffer(); }

static void do_force_return(void) {
    Serial.println(F("[FORCE] Force Return triggered"));
    DEBUG_STATE("Force Return - resetting to Normal");
    event_bus_flush();
    eye_look_reset(&g_eyeCfg);
    eye_set_expression(&g_eyeCfg, 0);
    servo_set_target(SYM_L(0), SYM_R(0));
    servo_set_jitter(0, 0);
    g_joy_brow_offset_l = 0; g_joy_brow_offset_r = 0;
    g_revert_deadline_ms = 0;
}

static void process_event(const EventMsg_t* msg) {
    switch (msg->type) {
    case EVT_JOYSTICK_MOVE: {
        eye_set_look(&g_eyeCfg, msg->value_x, msg->value_y);
        int16_t bo = msg->value_y / 8;
        g_joy_brow_offset_l = -bo;
        g_joy_brow_offset_r =  bo;
        break;
    }
    case EVT_EXPR_SET: {
        uint8_t idx = msg->value_x;
        if (idx < 8) {
            eye_set_expression(&g_eyeCfg, idx);
            servo_set_target(EXPRESSIONS[idx].brow_left, EXPRESSIONS[idx].brow_right);
            if (idx == 0) g_revert_deadline_ms = 0;
            else g_revert_deadline_ms = millis() + 1500;
            DEBUG_STATE("Expression changed to: %s (index=%d)", EXPRESSIONS[idx].name, idx);
        }
        break;
    }
    case EVT_EXPR_RELEASE: {
        uint16_t hm = (uint16_t)msg->value_y;
        if (hm >= ADC_LONG_PRESS_MS) {
            g_revert_deadline_ms = 0;
            DEBUG_STATE("Long press lock (%dms) - expression locked", hm);
        }
        break;
    }
    case EVT_BUTTON_SHORT:
        DEBUG_STATE("Button short press");
        break;
    case EVT_BUTTON_LONG:
        DEBUG_STATE("Button LONG press -> Force Return");
        do_force_return();
        break;
    default: break;
    }
}

static void render_frame(void) {
    static uint32_t t0, t1, t2, t3;
    if (g_leftReady) { g_leftDisp.clearBuffer(); t0 = micros(); eye_render(&g_leftDisp, &g_eyeCfg, true); t1 = micros(); g_leftDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y, EYE_TILE_W, EYE_TILE_H); }
    if (g_rightReady) { g_rightDisp.clearBuffer(); t2 = micros(); eye_render(&g_rightDisp, &g_eyeCfg, false); t3 = micros(); g_rightDisp.updateDisplayArea(EYE_TILE_X, EYE_TILE_Y, EYE_TILE_W, EYE_TILE_H); }
}
void setup() {
    Serial.begin(115200); delay(500); Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  RobotEyes v10.0 - int16_t Fix + Visual Overhaul"));
    Serial.println(F("========================================"));
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    g_leftReady = initLeftDisplay(); Serial.println();
    g_rightReady = initRightDisplay(); Serial.println();
    if (g_leftReady) screenBlackOut(&g_leftDisp);
    if (g_rightReady) screenBlackOut(&g_rightDisp);
    event_bus_init(); Serial.println(F("[EVENT] OK"));
    eye_config_init(&g_eyeCfg, 64, 32); blink_state_init(&g_blinkState); Serial.println(F("[EYE] OK"));
    servo_task_init();
    input_task_init();
    xTaskCreate(input_task_run, "InputTask", 2048, NULL, 2, &g_inputTaskHandle);
    xTaskCreate(servo_task_run, "ServoTask", 1536, NULL, 2, &g_servoTaskHandle);
    Serial.println(F("[TASK] InputTask+ServoTask created"));
    Serial.println(F("========================================"));
    DEBUG_STATE("System initialized - v10.0 int16_t fix + DEBUG_EYES active");
    g_last_servo_debug_ms = millis();
}

void loop() {
    uint32_t now = millis();
    EventMsg_t msg;
    while (event_bus_pop(&msg, 0)) process_event(&msg);

    if (now - g_last_frame_ms < FRAME_INTERVAL_MS) { vTaskDelay(1); return; }
    g_last_frame_ms = now;

    /* ---- v10: auto-revert timeout (1.5s) ---- */
    if (g_revert_deadline_ms > 0 && now >= g_revert_deadline_ms) {
        g_revert_deadline_ms = 0;
        if (g_eyeCfg.active_expr != 0) {
            g_joy_brow_offset_l = 0; g_joy_brow_offset_r = 0;
            eye_set_expression(&g_eyeCfg, 0);
            servo_set_target(SYM_L(0), SYM_R(0));
            DEBUG_STATE("Auto-revert to Normal (timeout)");
        }
    }

    eye_look_update(&g_eyeCfg);
    eye_expr_update(&g_eyeCfg, now);
    blink_state_update(&g_blinkState, &g_eyeCfg, now);
    eye_attention_update(&g_eyeCfg, now);
    eye_idle_micro_update(&g_eyeCfg, now);

    /* ---- v10: brow servo assembly (int16_t, anti-overflow) ---- */
    if (g_eyeCfg.active_expr < 8) {
        int16_t bl = EXPRESSIONS[g_eyeCfg.active_expr].brow_left;
        int16_t br = EXPRESSIONS[g_eyeCfg.active_expr].brow_right;
        int16_t final_l = bl + g_joy_brow_offset_l - g_eyeCfg.brow_offset_l;
        int16_t final_r = br + g_joy_brow_offset_r + g_eyeCfg.brow_offset_r;

        servo_set_target(final_l, final_r);

        /* ---- v10: [SERVO] debug log every 500ms ---- */
        if (now - g_last_servo_debug_ms >= 500) {
            g_last_servo_debug_ms = now;
            DEBUG_SERVO("L_Base:%d L_JoyOff:%d L_AnimOff:%d R_Base:%d R_JoyOff:%d R_AnimOff:%d -> FINAL L:%d R:%d",
                (int)bl, (int)g_joy_brow_offset_l, (int)g_eyeCfg.brow_offset_l,
                (int)br, (int)g_joy_brow_offset_r, (int)g_eyeCfg.brow_offset_r,
                (int)final_l, (int)final_r);
        }
    } else {
        servo_set_target(
            (int16_t)(SERVO_CENTER_DEG + g_joy_brow_offset_l - g_eyeCfg.brow_offset_l),
            (int16_t)(SERVO_CENTER_DEG + g_joy_brow_offset_r + g_eyeCfg.brow_offset_r)
        );
    }

    /* ---- v10: Angry(2)/Panic(6) high-freq jitter ---- */
    if (g_eyeCfg.active_expr == 2 || g_eyeCfg.active_expr == 6) {
        int16_t jit_l = (int16_t)((rand() % 7) - 3);
        int16_t jit_r = (int16_t)((rand() % 7) - 3);
        servo_set_jitter(jit_l, jit_r);
    } else {
        servo_set_jitter(0, 0);
    }

    render_frame();
}``` 
 
--- 
 
## 文件路径：D:\Data_Backup\Robot_Eyes\ESP32_RobotEyes\src\servo_task.cpp 
```c 
/**
 * @file    servo_task.cpp
 * @brief   RobotEyes 舵机控制 Task 实现 v10.0 — int16_t 全链路 + 非阻塞步进 + 独立抖动通道
 *
 *  使用 ESP32Servo 库 (LGPL-2.1)
 *  非阻塞步进: 每 20ms 向目标移动 SERVO_STEP_DEG 度
 *  抖动通道: servo_set_jitter() 直接叠加到 write() 输出, 绕过缓动
 *
 *  v10.0 关键修复:
 *    - 所有角度变量 int8_t → int16_t
 *    - int8_t 最多存储 127, Angry 左眉 135deg 溢出为 -121
 *    - int16_t 支持 0-180 全范围无溢出
 */

#include "servo_task.h"
#include "pin_config.h"
#include <Arduino.h>
#include <ESP32Servo.h>

TaskHandle_t g_servoTaskHandle = NULL;

static Servo g_servo_left;
static Servo g_servo_right;

/* v10: int8_t → int16_t */
static volatile int16_t g_target_left  = SERVO_CENTER_DEG;
static volatile int16_t g_target_right = SERVO_CENTER_DEG;

static volatile int16_t g_jitter_left  = 0;
static volatile int16_t g_jitter_right = 0;

static int16_t g_current_left  = SERVO_CENTER_DEG;
static int16_t g_current_right = SERVO_CENTER_DEG;

/* ================================================================
 *  servo_task_init()
 * ================================================================ */
void servo_task_init(void) {
    ESP32PWM::allocateTimer(0);
    g_servo_left.setPeriodHertz(50);
    g_servo_right.setPeriodHertz(50);

    g_servo_left.attach(PIN_SERVO_LEFT, 500, 2500);
    g_servo_right.attach(PIN_SERVO_RIGHT, 500, 2500);

    g_servo_left.write(SERVO_CENTER_DEG);
    g_servo_right.write(SERVO_CENTER_DEG);

    g_current_left  = SERVO_CENTER_DEG;
    g_current_right = SERVO_CENTER_DEG;
    g_target_left   = SERVO_CENTER_DEG;
    g_target_right  = SERVO_CENTER_DEG;
    g_jitter_left   = 0;
    g_jitter_right  = 0;

    Serial.println(F("[SERVO] Init done. Center=90 deg (v10: int16_t)"));
}

/* ================================================================
 *  servo_set_target() — 线程安全设置目标角度 (v10: int16_t)
 * ================================================================ */
void servo_set_target(int16_t left_deg, int16_t right_deg) {
    if (left_deg  < SERVO_MIN_DEG) left_deg  = SERVO_MIN_DEG;
    if (left_deg  > SERVO_MAX_DEG) left_deg  = SERVO_MAX_DEG;
    if (right_deg < SERVO_MIN_DEG) right_deg = SERVO_MIN_DEG;
    if (right_deg > SERVO_MAX_DEG) right_deg = SERVO_MAX_DEG;

    g_target_left  = left_deg;
    g_target_right = right_deg;
}

/* ================================================================
 *  servo_get_target() — 读取当前目标角度 (v10: int16_t)
 * ================================================================ */
void servo_get_target(int16_t* left_deg, int16_t* right_deg) {
    *left_deg  = g_target_left;
    *right_deg = g_target_right;
}

/* ================================================================
 *  servo_add_relative() — 相对当前目标角度偏移 (v10: int16_t)
 * ================================================================ */
void servo_add_relative(int16_t left_offset, int16_t right_offset) {
    servo_set_target(g_target_left + left_offset,
                     g_target_right + right_offset);
}

/* ================================================================
 *  servo_set_jitter() — v10: 直接注频抖动 (int16_t)
 * ================================================================ */
void servo_set_jitter(int16_t left_jitter, int16_t right_jitter) {
    g_jitter_left  = left_jitter;
    g_jitter_right = right_jitter;
}

/* ================================================================
 *  servo_task_run() — FreeRTOS Task 主循环
 *
 *  每 20ms 执行一次:
 *    1. 按 SERVO_STEP_DEG 步进追目标
 *    2. 叠加 g_jitter_left/right (绕过缓动)
 *    3. 钳位后 write() 到舵机
 * ================================================================ */
void servo_task_run(void* pvParameters) {
    (void)pvParameters;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        int16_t cur_l  = g_current_left;
        int16_t cur_r  = g_current_right;
        int16_t tgt_l  = g_target_left;
        int16_t tgt_r  = g_target_right;

        /* 左舵机步进 */
        if (cur_l < tgt_l) {
            cur_l += SERVO_STEP_DEG;
            if (cur_l > tgt_l) cur_l = tgt_l;
        } else if (cur_l > tgt_l) {
            cur_l -= SERVO_STEP_DEG;
            if (cur_l < tgt_l) cur_l = tgt_l;
        }

        /* 右舵机步进 */
        if (cur_r < tgt_r) {
            cur_r += SERVO_STEP_DEG;
            if (cur_r > tgt_r) cur_r = tgt_r;
        } else if (cur_r > tgt_r) {
            cur_r -= SERVO_STEP_DEG;
            if (cur_r < tgt_r) cur_r = tgt_r;
        }

        /* v10: 叠加抖动 (绕过缓动, 直接加到输出) */
        int16_t out_l = cur_l + g_jitter_left;
        int16_t out_r = cur_r + g_jitter_right;

        /* 钳位保护硬件 */
        if (out_l < SERVO_MIN_DEG) out_l = SERVO_MIN_DEG;
        if (out_l > SERVO_MAX_DEG) out_l = SERVO_MAX_DEG;
        if (out_r < SERVO_MIN_DEG) out_r = SERVO_MIN_DEG;
        if (out_r > SERVO_MAX_DEG) out_r = SERVO_MAX_DEG;

        /* 写入舵机 */
        g_servo_left.write(out_l);
        g_servo_right.write(out_r);

        g_current_left  = cur_l;
        g_current_right = cur_r;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SERVO_UPDATE_MS));
    }
}
``` 
 
--- 
 
