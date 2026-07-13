/*
 * steer.c
 *
 *  Created on: 2026年7月8日
 *      Author: 珩泊霄
 */
#include "steer.h"
/*
 * 舵机转向模块。
 * CH0 周期中断把图像加权中线传入 servo()，经动态 PD 计算与机械范围
 * 限幅后输出到舵机 PWM；直接输出接口用于静态标定。
 */

/* ==================== 舵机状态与初始化 ==================== */
uint16 pwm=CONFIG_SERVO_PWM_MIDDLE;
/**
 * @brief 初始化舵机 PWM，并输出配置的机械中位值。
 * @return 无。
 * @note 在 CPU0 启动阶段调用一次，PWM 频率为 50 Hz。
 */
void Servo_Init(void)
{
    pwm_init(ATOM0_CH1_P33_9,50,CONFIG_SERVO_PWM_MIDDLE);
}


/* ==================== 直接舵机输出 ==================== */

/**
 * @brief 直接写入舵机 PWM。
 * @param pwm 待输出的 PWM 占空比值。
 * @return 无。
 * @note 该函数不执行限幅，主要用于中位与机械极限标定。
 */
void Servo_Ctrl(uint16 pwm)
{
    pwm_set_duty(ATOM0_CH1_P33_9,pwm);
}
int temp;

/**
 * @brief 按固定比例把全局中线偏差转换为舵机输出。
 * @return 无。
 * @note 这是保留的旧版直接转向函数，当前周期中断使用 servo()。
 */
void turn(void)
{
    temp=2.2*((int)CONFIG_MID_WIDTH-(int)final_mid_line);
    if (temp>80){temp=80;}
    else if(temp<-70){temp=-70;}
    pwm_set_duty(ATOM0_CH1_P33_9,CONFIG_SERVO_PWM_MIDDLE+temp);
}


/* ==================== 闭环转向控制 ==================== */

/**
 * @brief 将检测到的中线横坐标转换为闭环舵机指令。
 * @param mid 当前帧加权中线横坐标。
 * @return 无。
 * @note 位置式 PD 以图像中心为目标，输出最终限制在中位值上下
 *       CONFIG_SERVO_OUTPUT_LIMIT 范围内，防止超过机械转角。
 */
void servo(uint8 mid)
{
    int servo_value;
    int temp1;
    servo_value=PID_Normal(&servo_pid,CONFIG_MID_WIDTH-mid,0);
    temp1=(int)CONFIG_SERVO_PWM_MIDDLE-(int)servo_value;
    if(temp1<CONFIG_SERVO_PWM_MIDDLE-CONFIG_SERVO_OUTPUT_LIMIT)
    {
        temp1=CONFIG_SERVO_PWM_MIDDLE-CONFIG_SERVO_OUTPUT_LIMIT;
    }
    else if(temp1>CONFIG_SERVO_PWM_MIDDLE+CONFIG_SERVO_OUTPUT_LIMIT)
    {
        temp1=CONFIG_SERVO_PWM_MIDDLE+CONFIG_SERVO_OUTPUT_LIMIT;
    }
    pwm_set_duty(ATOM0_CH1_P33_9,temp1);
}
