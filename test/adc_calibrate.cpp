/**
 * @file    adc_calibrate.cpp
 * @brief   ADC 键盘标定程序 — 仅打印 GPIO2 原始读数
 * @note    临时替换 src/main.cpp 使用，标定完成后恢复原 main.cpp
 *
 * 使用方法:
 *   1. 备份 src/main.cpp
 *   2. 将本文件内容复制到 src/main.cpp
 *   3. pio run --target upload && pio device monitor
 *   4. 逐个按下 S1~S8，记录串口打印的稳定读数范围
 *   5. 恢复原 main.cpp
 *
 * 注意事项:
 *   - 本程序仅初始化串口和 ADC，不初始化 OLED/舵机/FreeRTOS
 *   - GPIO2 在 ESP32-C3 上是 ADC1_CH2
 *   - 12-bit ADC, 范围 0~4095
 */

#include <Arduino.h>

/* ---- 引脚 ---- */
#define PIN_ADC_KEYBOARD  2   /* GPIO2, ADC1_CH2 */

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  ADC Keyboard Calibration"));
    Serial.println(F("  GPIO2 (ADC1_CH2)"));
    Serial.println(F("========================================"));
    Serial.println(F("Press each key S1~S8 one by one."));
    Serial.println(F("Record the stable ADC range for each."));
    Serial.println(F("Format: raw (12-bit, 0-4095)"));
    Serial.println();

    analogReadResolution(12);
    pinMode(PIN_ADC_KEYBOARD, INPUT);
}

void loop() {
    /* 采集 8 次取平均，减少噪声 */
    int32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(PIN_ADC_KEYBOARD);
        delay(2);
    }
    int avg = sum / 8;

    static int last_avg = -1;

    /* 只在值变化超过阈值时才打印，减少串口刷屏 */
    if (abs(avg - last_avg) > 10) {
        last_avg = avg;
        Serial.print(F("ADC="));
        Serial.println(avg);
    }

    delay(50);  /* ~20Hz 刷新 */
}
