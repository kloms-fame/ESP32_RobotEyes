/**
 * @file    adc_verify.cpp
 * @brief   ADC 键盘映射验证程序 — 标定后确认映射关系正确
 * @note    临时替换 src/main.cpp 使用，验证完成后恢复
 *
 * 使用方法:
 *   1. 备份 src/main.cpp  (如已备份可跳过)
 *   2. 将本文件内容复制到 src/main.cpp
 *   3. pio run --target upload && pio device monitor
 *   4. 逐个按下 S1~S8，确认串口打印的键名与实际按键一致
 *   5. 恢复原 main.cpp
 */

#include <Arduino.h>

#define PIN_ADC_KEYBOARD  2   /* GPIO2, ADC1_CH2 */

/* ================================================================
 *  ADC 键盘映射表 (基于 2026-07-09 实测标定)
 *
 *  无按键: 120~210 (浮空接近 GND)
 *  按键按下时 ADC 值升高 (上拉分压结构)
 *
 *  区间 (min, max]: ADC 值落在此区间内判定为对应按键
 *  各区间之间留 ≥200 安全间距
 * ================================================================ */
typedef struct {
    uint16_t min;
    uint16_t max;
    uint8_t  key;    /* 1~8 对应 S1~S8, 0=无按键 */
    const char* name;
} AdcKeyRange_t;

static const AdcKeyRange_t KEY_MAP[] = {
    {    0,  350, 0, "NONE"},
    {  450,  800, 8, "S8"},
    {  800, 1300, 7, "S7"},
    { 1300, 1750, 6, "S6"},
    { 1750, 2150, 5, "S5"},
    { 2150, 2550, 4, "S4"},
    { 2550, 3000, 3, "S3"},
    { 3000, 3600, 2, "S2"},
    { 3600, 4095, 1, "S1"},
};
#define KEY_MAP_COUNT (sizeof(KEY_MAP) / sizeof(KEY_MAP[0]))

/* ---- 去抖参数 ---- */
#define DEBOUNCE_MS      30    /* 边沿去抖窗口 */
#define SAMPLE_INTERVAL  10    /* 采样间隔 ms */

/* ================================================================
 *  lookup_key() — 根据 ADC 值查找按键
 * ================================================================ */
static uint8_t lookup_key(uint16_t adc) {
    for (uint8_t i = 0; i < KEY_MAP_COUNT; i++) {
        if (adc >= KEY_MAP[i].min && adc <= KEY_MAP[i].max) {
            return i;  /* 返回 MAP 索引 */
        }
    }
    return 0;  /* 默认 NONE */
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  ADC Keyboard Mapping Verification"));
    Serial.println(F("========================================"));
    Serial.println(F("Press S1~S8 one by one."));
    Serial.println(F("Verify serial output matches the key you pressed."));
    Serial.println(F("If mismatch, record which key was wrong and the ADC value."));
    Serial.println();
    Serial.println(F("Expected mapping (ADC high→low = S1→S8):"));
    Serial.println(F("  S1: 3600-4095    S5: 1750-2150"));
    Serial.println(F("  S2: 3000-3600    S6: 1300-1750"));
    Serial.println(F("  S3: 2550-3000    S7:  800-1300"));
    Serial.println(F("  S4: 2150-2550    S8:  450-800"));
    Serial.println(F("  NONE:    0-350"));
    Serial.println(F("========================================"));
    Serial.println();

    analogReadResolution(12);
    pinMode(PIN_ADC_KEYBOARD, INPUT);
}

void loop() {
    /* 采集 8 次取平均 */
    int32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(PIN_ADC_KEYBOARD);
        delay(1);
    }
    uint16_t avg = sum / 8;

    /* 查表 */
    uint8_t idx = lookup_key(avg);
    uint8_t key = KEY_MAP[idx].key;
    const char* name = KEY_MAP[idx].name;

    /* 去抖状态机 */
    static uint8_t  stable_idx  = 0;
    static uint32_t stable_since = 0;
    static uint8_t  reported_key = 0;
    uint32_t now = millis();

    if (idx == stable_idx) {
        /* 状态不变，检查是否已稳定超过去抖窗口 */
        if (now - stable_since >= DEBOUNCE_MS && key != reported_key) {
            reported_key = key;
            if (key == 0) {
                Serial.println(F(">>> RELEASED"));
            } else {
                Serial.print(F(">>> PRESSED  "));
                Serial.print(name);
                Serial.print(F("  (ADC="));
                Serial.print(avg);
                Serial.println(F(")"));
            }
        }
    } else {
        /* 状态变化，重置去抖计时 */
        stable_idx  = idx;
        stable_since = now;
    }

    delay(SAMPLE_INTERVAL);
}
