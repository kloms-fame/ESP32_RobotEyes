/**
 * @file    main.cpp
 * @brief   RobotEyes Phase 1 MVP — 双 I2C OLED 独立初始化 + I2C 扫描
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-08
 *
 * 功能边界: 仅点亮双屏，显示静态测试图案。无按键/摇杆/舵机/WiFi/FreeRTOS。
 *
 * 硬件架构:
 *   - 左眼 SSD1306: 硬件 I2C, GPIO 8(SDA) + GPIO 9(SCL), 地址 0x3C
 *   - 右眼 GM009605v4.3: 软件 I2C, GPIO 7(SDA) + GPIO 6(SCL), 地址 0x3C
 *   - 两块屏地址相同但挂在不同的 I2C 总线上，完全独立，无冲突
 *
 * 设计理由:
 *   GM009605 的 I2C 地址被硬件锁死在 0x3C（FPC 排线第 15 脚直连 GND），
 *   无法通过跳线修改。因此为右眼单独开辟一条软件 I2C 总线。
 *
 * 备用构造函数（右眼不亮时依次尝试）:
 *   // U8G2_SH1106_128X64_NONAME_F_SW_I2C rightDisp(...)
 *   // U8G2_SSD1309_128X64_NONAME_F_SW_I2C rightDisp(...)
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "pin_config.h"

/* ================================================================
 *  全局显示对象
 *  ----------------------------------------------------------------
 *  左眼: 硬件 I2C，复用 setup() 中的 Wire.begin(8, 9)
 *  右眼: 软件 I2C，GPIO 6=SCL, GPIO 7=SDA, 独立总线
 * ================================================================ */

U8G2_SSD1306_128X64_NONAME_F_HW_I2C leftDisp(U8G2_R0, U8X8_PIN_NONE);

// ★ 软件 I2C: clock=GPIO6, data=GPIO7, reset=不使用
//    SW_I2C 构造函数会通过 GPIO 模拟 I2C 时序，不依赖 Wire 库
U8G2_SSD1306_128X64_NONAME_F_SW_I2C rightDisp(U8G2_R0,
                                              SW_I2C_SCL,
                                              SW_I2C_SDA,
                                              U8X8_PIN_NONE);

bool g_leftReady  = false;
bool g_rightReady = false;

/* ================================================================
 *  i2cScan() — 硬件 I2C 总线扫描（仅扫描 GPIO 8/9）
 *  ----------------------------------------------------------------
 *  注意: 软件 I2C 总线上的设备不会出现在此扫描中。
 *        右眼 GM009605 在 GPIO 6/7 上，需要独立验证。
 * ================================================================ */
void i2cScan() {
    Serial.println(F("[SCAN] 硬件 I2C 总线扫描 (SDA=GPIO8, SCL=GPIO9) ..."));
    Serial.println(F("       注意: 右眼在软件 I2C (GPIO6/7) 上，不会出现在此扫描"));
    Serial.println();

    int foundCount = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print(F("       [FOUND] 0x"));
            if (addr < 0x10) Serial.print(F("0"));
            Serial.print(addr, HEX);
            if (addr == I2C_ADDR_SSD1306) {
                Serial.print(F("  <- 左眼 SSD1306"));
            }
            Serial.println();
            foundCount++;
        }
    }

    Serial.println();
    Serial.print(F("[SCAN] 共发现 "));
    Serial.print(foundCount);
    Serial.println(F(" 个设备（不含软件 I2C 总线）"));
    Serial.println();
}

/* ================================================================
 *  initDisplays() — 双屏硬件初始化
 * ================================================================ */
bool initDisplays() {
    /* ---- 左眼 SSD1306 硬件 I2C @0x3C ---- */
    Serial.print(F("[INIT] Left  SSD1306 HW-I2C @0x"));
    Serial.print(I2C_ADDR_SSD1306, HEX);
    Serial.print(F(" ... "));
    Serial.flush();

    leftDisp.setI2CAddress(I2C_ADDR_SSD1306 << 1);
    g_leftReady = leftDisp.begin();
    if (g_leftReady) {
        leftDisp.setPowerSave(0);
        Serial.println(F("[OK]"));
    } else {
        Serial.println(F("[FAIL]"));
    }

    /* ---- 右眼 GM009605 软件 I2C @0x3C ---- */
    Serial.print(F("[INIT] Right GM009605 SW-I2C @0x"));
    Serial.print(I2C_ADDR_GM009605, HEX);
    Serial.print(F(" (GPIO6=SCL, GPIO7=SDA) ... "));
    Serial.flush();

    // ★ 不需要 setI2CAddress — 软件 I2C 独占总线，默认 0x3C 无冲突
    g_rightReady = rightDisp.begin();
    if (g_rightReady) {
        rightDisp.setPowerSave(0);
        Serial.println(F("[OK]"));
    } else {
        Serial.println(F("[FAIL]"));
        Serial.println(F("       排查: 1) GPIO6/7 接线 2) 尝试 SH1106/SSD1309 备选构造函数"));
    }

    return (g_leftReady && g_rightReady);
}

/* ================================================================
 *  renderLeftFrame() — 左屏: 同心圆 + L
 * ================================================================ */
void renderLeftFrame() {
    if (!g_leftReady) return;
    leftDisp.clearBuffer();
    leftDisp.drawCircle(64, 32, 25, U8G2_DRAW_ALL);
    leftDisp.drawCircle(64, 32, 15, U8G2_DRAW_ALL);
    leftDisp.setFont(u8g2_font_ncenB24_tr);
    leftDisp.setFontMode(1);
    leftDisp.drawStr(46, 44, "L");
    leftDisp.sendBuffer();
    Serial.println(F("[DRAW] 左屏 -> 同心圆 + L"));
}

/* ================================================================
 *  renderRightFrame() — 右屏: 矩形方框 + R
 * ================================================================ */
void renderRightFrame() {
    if (!g_rightReady) return;
    rightDisp.clearBuffer();
    rightDisp.drawFrame(20, 8, 88, 48);
    rightDisp.setFont(u8g2_font_ncenB24_tr);
    rightDisp.setFontMode(1);
    rightDisp.drawStr(46, 44, "R");
    rightDisp.sendBuffer();
    Serial.println(F("[DRAW] 右屏 -> 矩形方框 + R"));
}

/* ================================================================
 *  setup()
 * ================================================================ */
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  RobotEyes Phase 1 — 双 I2C 独立刷新 MVP"));
    Serial.println(F("========================================"));
    Serial.println();

    // 硬件 I2C 初始化（仅左眼使用）
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println(F("[WIRE] 硬件 I2C 初始化 (SDA=GPIO8, SCL=GPIO9)"));
    Serial.println(F("[WIRE] 软件 I2C 由 U8g2 SW_I2C 内部管理 (SCL=GPIO6, SDA=GPIO7)"));
    Serial.println();

    i2cScan();

    bool allOk = initDisplays();
    Serial.println();

    if (allOk) {
        Serial.println(F("[SUMMARY] 双屏全部成功！"));
    } else {
        Serial.println(F("[SUMMARY] 部分屏幕初始化失败"));
    }

    Serial.println();
    renderLeftFrame();
    renderRightFrame();

    Serial.println(F("[DONE] 初始化结束。"));
    Serial.println(F("========================================"));
}

void loop() {
    // Phase 1 MVP — 静态显示
}
