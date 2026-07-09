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
