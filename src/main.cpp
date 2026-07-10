/**
 * @file    main.cpp
 * @brief   RobotEyes v9.0 — 镜像统一 + 抖动通道
 * @author  Rennick (AI 辅助开发)
 * @date    2026-07-10
 *
 *  v9.0 关键修复:
 *    - 眉毛总装镜像统一: brow_offset_l 用减法 (遵循 SYM_L 惯例)
 *    - Angry(2)/Panic(6) 通过 servo_set_jitter 注频高频抖动
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
#include "expressions.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C g_leftDisp(U8G2_R0, U8X8_PIN_NONE);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C g_rightDisp(U8G2_R0, PIN_SW_I2C_SCL, PIN_SW_I2C_SDA, U8X8_PIN_NONE);
bool g_leftReady=false, g_rightReady=false;

static EyeConfig_t  g_eyeCfg;
static BlinkState_t g_blinkState;
static uint32_t g_revert_deadline_ms=0;
static uint32_t g_last_frame_ms=0;
static uint32_t g_last_beat_ms=0;
static int8_t g_joy_brow_offset_l=0, g_joy_brow_offset_r=0;

extern "C" uint8_t esp32_fast_sw_i2c_gpio_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT: pinMode(PIN_SW_I2C_SCL, INPUT_PULLUP); pinMode(PIN_SW_I2C_SDA, INPUT_PULLUP); break;
    case U8X8_MSG_DELAY_MILLI: delay(arg_int); break;
    case U8X8_MSG_DELAY_10MICRO: delayMicroseconds(arg_int*10); break;
    case U8X8_MSG_DELAY_100NANO: delayMicroseconds((arg_int+9)/10); break;
    case U8X8_MSG_DELAY_NANO: { uint32_t ns=*(uint32_t*)arg_ptr; delayMicroseconds((ns+999)/1000); break; }
    case U8X8_MSG_GPIO_I2C_CLOCK: if(arg_int) GPIO.enable_w1tc.val=(1UL<<PIN_SW_I2C_SCL); else { GPIO.out_w1tc.val=(1UL<<PIN_SW_I2C_SCL); GPIO.enable_w1ts.val=(1UL<<PIN_SW_I2C_SCL); } break;
    case U8X8_MSG_GPIO_I2C_DATA:  if(arg_int) GPIO.enable_w1tc.val=(1UL<<PIN_SW_I2C_SDA); else { GPIO.out_w1tc.val=(1UL<<PIN_SW_I2C_SDA); GPIO.enable_w1ts.val=(1UL<<PIN_SW_I2C_SDA); } break;
    default: return 0;
    }
    return 1;
}

bool initLeftDisplay() {
    Serial.print(F("[LEFT]  I2C probe @0x")); Serial.print(I2C_ADDR_LEFT,HEX); Serial.print(F(" ... "));
    Wire.beginTransmission(I2C_ADDR_LEFT); if(Wire.endTransmission()!=0){Serial.println(F("[FAIL] skip")); return false;}
    Serial.println(F("[OK]")); Serial.print(F("[LEFT]  begin() ... "));
    g_leftDisp.setI2CAddress(I2C_ADDR_LEFT<<1); if(!g_leftDisp.begin()){Serial.println(F("[FAIL]")); return false;}
    Serial.println(F("[OK]")); g_leftDisp.setBusClock(400000); g_leftDisp.setPowerSave(0); return true;
}

bool initRightDisplay() {
    Serial.print(F("[RIGHT] SW-I2C begin() ... "));
    u8x8_t *u8x8=g_rightDisp.getU8x8(); u8x8->gpio_and_delay_cb=esp32_fast_sw_i2c_gpio_cb;
    if(!g_rightDisp.begin()){Serial.println(F("[FAIL]")); return false;}
    Serial.println(F("[OK]")); g_rightDisp.setPowerSave(0); return true;
}

static void screenBlackOut(U8G2*d){d->clearBuffer(); d->sendBuffer();}

static void do_force_return(void) {
    Serial.println(F("[FORCE] Force Return triggered"));
    event_bus_flush();
    eye_look_reset(&g_eyeCfg);
    eye_set_expression(&g_eyeCfg,0);
    servo_set_target(SYM_L(0),SYM_R(0));
    servo_set_jitter(0,0);
    g_joy_brow_offset_l=0; g_joy_brow_offset_r=0;
    g_revert_deadline_ms=0;
}

static void process_event(const EventMsg_t* msg) {
    switch(msg->type) {
    case EVT_JOYSTICK_MOVE: {
        eye_set_look(&g_eyeCfg, msg->value_x, msg->value_y);
        int8_t bo=msg->value_y/8;
        g_joy_brow_offset_l=-bo; g_joy_brow_offset_r=bo;
        break;
    }
    case EVT_EXPR_SET: {
        uint8_t idx=msg->value_x; if(idx<8) {
            eye_set_expression(&g_eyeCfg,idx);
            servo_set_target(EXPRESSIONS[idx].brow_left, EXPRESSIONS[idx].brow_right);
            if(idx==0) g_revert_deadline_ms=0; else g_revert_deadline_ms=millis()+1500;
            Serial.print(F("[EXPR] Switched to ")); Serial.print(EXPRESSIONS[idx].name); Serial.println();
        }
        break;
    }
    case EVT_EXPR_RELEASE: {
        uint16_t hm=(uint16_t)msg->value_y;
        if(hm>=ADC_LONG_PRESS_MS){g_revert_deadline_ms=0;Serial.print(F("[EXPR] Long press ("));Serial.print(hm);Serial.println(F("ms) locked"));}
        break;
    }
    case EVT_BUTTON_SHORT: Serial.println(F("[BTN] Short press")); break;
    case EVT_BUTTON_LONG: do_force_return(); break;
    default: break;
    }
}

static void render_frame(void) {
    static uint32_t t0,t1,t2,t3;
    if(g_leftReady){g_leftDisp.clearBuffer();t0=micros();eye_render(&g_leftDisp,&g_eyeCfg,true);t1=micros();g_leftDisp.updateDisplayArea(EYE_TILE_X,EYE_TILE_Y,EYE_TILE_W,EYE_TILE_H);}
    if(g_rightReady){g_rightDisp.clearBuffer();t2=micros();eye_render(&g_rightDisp,&g_eyeCfg,false);t3=micros();g_rightDisp.updateDisplayArea(EYE_TILE_X,EYE_TILE_Y,EYE_TILE_W,EYE_TILE_H);}
    uint32_t now=millis();
    if(now-g_last_beat_ms>=2000){g_last_beat_ms=now;Serial.print(F("[BEAT] "));
      if(g_leftReady&&g_rightReady){Serial.print(F("Lren="));Serial.print(t1-t0);Serial.print(F("us Lsend="));Serial.print(t2-t1);Serial.print(F("us | Rren="));Serial.print(t3-t2);Serial.print(F("us Rsend="));Serial.print(t3-t2);Serial.print(F("us | "));}
      Serial.print(F("blink="));Serial.print(g_blinkState.phase);Serial.print(F(" lid="));Serial.print(g_eyeCfg.lid,2);Serial.print(F(" look=("));Serial.print((int)g_eyeCfg.cur_look_x);Serial.print(F(","));Serial.print((int)g_eyeCfg.cur_look_y);Serial.println(F(")"));}
}

void setup() {
    Serial.begin(115200); delay(500); Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("  RobotEyes v9.0 — Mirror Fix + Jitter + Panic"));
    Serial.println(F("========================================"));
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    g_leftReady=initLeftDisplay(); Serial.println();
    g_rightReady=initRightDisplay(); Serial.println();
    if(g_leftReady) screenBlackOut(&g_leftDisp);
    if(g_rightReady) screenBlackOut(&g_rightDisp);
    event_bus_init(); Serial.println(F("[EVENT] OK"));
    eye_config_init(&g_eyeCfg,64,32); blink_state_init(&g_blinkState); Serial.println(F("[EYE] OK"));
    servo_task_init();
    input_task_init();
    xTaskCreate(input_task_run,"InputTask",2048,NULL,2,&g_inputTaskHandle);
    xTaskCreate(servo_task_run,"ServoTask",1536,NULL,2,&g_servoTaskHandle);
    Serial.println(F("[TASK] InputTask+ServoTask created"));
    Serial.println(F("========================================"));
}
void loop() {
    uint32_t now=millis();
    EventMsg_t msg; while(event_bus_pop(&msg,0)) process_event(&msg);
    if(now-g_last_frame_ms<FRAME_INTERVAL_MS){vTaskDelay(1);return;}
    g_last_frame_ms=now;

    if(g_revert_deadline_ms>0&&now>=g_revert_deadline_ms){g_revert_deadline_ms=0;
      if(g_eyeCfg.active_expr!=0){g_joy_brow_offset_l=0;g_joy_brow_offset_r=0;eye_set_expression(&g_eyeCfg,0);servo_set_target(SYM_L(0),SYM_R(0));Serial.println(F("[EXPR] Auto-revert Normal"));}}

    eye_look_update(&g_eyeCfg);
    eye_expr_update(&g_eyeCfg,now);
    blink_state_update(&g_blinkState,&g_eyeCfg,now);
    eye_attention_update(&g_eyeCfg,now);
    eye_idle_micro_update(&g_eyeCfg,now);

    /* ---- v9.0 眉毛舵机总装 (镜像统一修复) ---- */
    if(g_eyeCfg.active_expr<8){
        int8_t bl=EXPRESSIONS[g_eyeCfg.active_expr].brow_left;
        int8_t br=EXPRESSIONS[g_eyeCfg.active_expr].brow_right;
        /* 镜像统一: 左眉 brow_offset_l 用减法 (遵循 SYM_L 左减右加惯例) */
        servo_set_target(
            bl + g_joy_brow_offset_l - g_eyeCfg.brow_offset_l,
            br + g_joy_brow_offset_r + g_eyeCfg.brow_offset_r
        );
    }else{
        servo_set_target(
            SERVO_CENTER_DEG + g_joy_brow_offset_l - g_eyeCfg.brow_offset_l,
            SERVO_CENTER_DEG + g_joy_brow_offset_r + g_eyeCfg.brow_offset_r
        );
    }

    /* ---- v9.0: Angry(2)/Panic(6) 高频抖动 (绕过舵机缓动) ---- */
    if(g_eyeCfg.active_expr==2||g_eyeCfg.active_expr==6){
        int8_t jit_l=(int8_t)((rand()%5)-2);
        int8_t jit_r=(int8_t)((rand()%5)-2);
        servo_set_jitter(jit_l,jit_r);
    }else{
        servo_set_jitter(0,0);
    }

    render_frame();
}