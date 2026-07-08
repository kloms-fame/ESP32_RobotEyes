/**
 * @file    pin_config.h
 * @brief   RobotEyes 硬件引脚定义
 * @note    左眼: 硬件 I2C (GPIO 8/9)，地址 0x3C
 *          右眼: 软件 I2C (GPIO 6/7)，地址 0x3C（GM009605 地址锁死，独立总线无冲突）
 */

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

/* ---- 左眼: 硬件 I2C ---- */
#define I2C_SDA             8
#define I2C_SCL             9
#define I2C_ADDR_SSD1306    0x3C

/* ---- 右眼: 软件 I2C（独立总线，避免地址冲突） ---- */
#define SW_I2C_SCL          6
#define SW_I2C_SDA          7
#define I2C_ADDR_GM009605   0x3C    // 出厂锁死，无法修改

#endif // PIN_CONFIG_H
