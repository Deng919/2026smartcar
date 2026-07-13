/*
 * steer.h
 *
 * 舵机转向模块公开接口。
 * 负责舵机 PWM 初始化、直接输出和基于中线偏差的闭环转向。
 */
#ifndef CODE_STEER_H_
#define CODE_STEER_H_

#include "zf_common_headfile.h"
#include "config.h"

/* ==================== 舵机共享状态 ==================== */

/** 舵机中位 PWM 初始值，保留用于调试观察。 */
extern uint16 pwm;
/** 旧版直接转向函数使用的临时偏差量。 */
extern int temp;

/* ==================== 舵机控制接口 ==================== */

/** 以配置的中位值初始化舵机 PWM 通道。 */
void Servo_Init(void);
/** 直接写入舵机 PWM，主要用于中位和机械范围标定。 */
void Servo_Ctrl(uint16 pwm);
/** 将检测到的中线横坐标转换为限幅后的闭环舵机指令。 */
void servo(uint8 mid);

#endif /* CODE_STEER_H_ */
