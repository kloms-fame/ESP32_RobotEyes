# 开源复用登记

> 依据《AGENT.md》第 9 节要求，每开发一个功能必须在本文档中登记对应条目。

---

## Phase 1: 双 I2C 独立刷新 MVP

| 字段 | 内容 |
|------|------|
| 功能名称 | 双 OLED I2C 独立初始化与静态测试图案渲染 |
| 参考项目/模块 | olikraus/U8g2 — U8g2lib.h, u8x8_d_ssd1306.c |
| 复用方式 | 直接使用（PlatformIO 依赖 olikraus/U8g2@^2.34.22） |
| 修改内容 | 无修改 — 标准库调用 |
| 工作量 | 0.25 人天 |
| 开源协议 | BSD-2-Clause |
| 不自研的理由 | U8g2 是嵌入式 OLED 显示的事实标准库，支持 200+ 控制器、全字体渲染、帧缓冲管理，自研同等功能的驱动层预估需 15+ 人天且稳定性远不及 |

---

## 待登记功能

| 功能名称 | 预计依赖 | 状态 |
|----------|---------|------|
| 舵机控制 | ESP32Servo (LGPL-2.1) | 未开发 |
| ADC 键盘输入 | esp32-adc-keyboard (MIT) | 未开发 |
| EventBus 事件总线 | FreeRTOS Queue (MIT) | 未开发 |
| Web 控制台 | ESPAsyncWebServer (LGPL-2.1) | 未开发 |
| 眼动引擎 | esp32-eyes (AGPL-3.0) | 未开发 |
| 表情资源 | Irisoled (MIT) | 未开发 |
