/*
 * motor.h
 *
 * 电机与编码器模块公开接口。
 * 负责声明驱动引脚、车轮状态、编码器采样、速度给定和闭环控制函数。
 */
#ifndef CODE_MOTOR_H_
#define CODE_MOTOR_H_

#include "zf_common_headfile.h"
#include "config.h"

/* ==================== 电机硬件映射 ==================== */

/** 左电机 PWM 通道与方向控制引脚。 */
#define MotorL_pwm  ATOM0_CH2_P21_4
#define MotorL_turn P21_5
/** 右电机 PWM 通道与方向控制引脚。 */
#define MotorR_turn P21_3
#define MotorR_pwm  ATOM0_CH0_P21_2
/** 保留的舵机通道别名，当前舵机模块直接使用 ATOM 通道。 */
#define duoji_jiao  ATOM1_CH1_P33_9

/* ==================== 电机运行状态 ==================== */

/** KEY_1 控制的启停计数；奇数运行，偶数停车。 */
extern uint8 start;

/** 单个车轮的速度环状态。 */
typedef struct motorl
{
    int target_speed;       /**< 目标编码器速度，单位为每个控制周期的计数。 */
    int duty;               /**< 最终输出 PWM 占空比指令，正负号表示方向。 */
    float encoder_speed;    /**< 低通滤波后的编码器速度。 */
    int encoder_raw;        /**< 本周期读取的原始编码器增量。 */
    int32 total_encoder;    /**< 从启动以来累计的编码器计数。 */
} motorl;

extern struct motorl motor_l;
extern struct motorl motor_r;

/* ==================== 初始化与底层输出 ==================== */

/** 初始化左右电机方向 GPIO 和 PWM 通道。 */
void Motor_Init(void);
/** 直接设置左电机 PWM；正负号决定转动方向。 */
void MotorL_SetSpeed(int pwm);
/** 直接设置右电机 PWM；正负号决定转动方向。 */
void MotorR_SetSpeed(int pwm);
/** 初始化左右轮正交编码器接口。 */
void Encoder_Init(void);
/** 向指定 PWM 通道和方向引脚输出带符号速度指令。 */
void Speed_Set(pwm_channel_enum pin1, gpio_pin_enum pin2, int speed, uint8 just, uint8 lose);

/* ==================== 速度采样与闭环控制 ==================== */

/** 读取、滤波并清零左右轮编码器本周期计数。 */
void Encode_Data_Get(void);
/** 根据中线偏差生成左右轮差速目标，并进入速度闭环。 */
void Final_Motor_Control(int speed, int limit);
/** 处理 KEY_1 启停状态，并根据弯道误差选择目标速度。 */
void car_start(void);
/** 叠加速度前馈与增量 PID 修正，输出左右电机 PWM。 */
void Motor_Control(int Speed_L, int Speed_R);

#endif /* CODE_MOTOR_H_ */
