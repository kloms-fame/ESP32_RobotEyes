/**
 * @file    pin_config.h
 * @brief   RobotEyes 硬件引脚集中定义
 * @note    阶段1 仅使用 OLED 相关引脚，其余为后续阶段预留
 *
 * 硬件基线:
 *   左眼: SSD1306 0.96寸 四针 I2C，硬件 I2C，地址 0x3C
 *   右眼: GM12864，软件 I2C (GPIO 位冲击)，地址 0x3C
 *   摇杆: X=GPIO0(ADC), Y=GPIO1(ADC), SW=GPIO10(数字)
 *   ADC键盘: GPIO2(ADC), LY-ADCKkeyboard-01 8键分压式
 *   舵机: 左眉=GPIO4, 右眉=GPIO5, SG92R 50Hz PWM
 */

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

/* ================================================================
 *  阶段1: OLED 双屏引脚
 * ================================================================ */

/* ---- 左眼: 硬件 I2C ---- */
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9
#define I2C_ADDR_LEFT       0x3C

/* ---- 右眼: 软件 I2C（独立总线，避免地址冲突） ---- */
#define PIN_SW_I2C_SCL      6
#define PIN_SW_I2C_SDA      7
#define I2C_ADDR_RIGHT      0x3C    /* GM12864 地址锁死，无法修改 */

/* ================================================================
 *  后续阶段预留（阶段1 不使用）
 * ================================================================ */

/* ---- 摇杆 ---- */
#define PIN_JOY_X           0       /* ADC1_CH0 */
#define PIN_JOY_Y           1       /* ADC1_CH1 */
#define PIN_JOY_SW          10      /* 数字输入, 长按2秒=安全归位 */

/* ---- ADC 键盘 ---- */
#define PIN_ADC_KEYBOARD    2       /* ADC1_CH2, LY-ADCKkeyboard-01 */

/* ---- 舵机 ---- */
#define PIN_SERVO_LEFT      4       /* 左眉 SG92R PWM */
#define PIN_SERVO_RIGHT     5       /* 右眉 SG92R PWM */

#endif /* PIN_CONFIG_H */
