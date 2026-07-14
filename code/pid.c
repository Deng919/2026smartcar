/*
 * pid.c
 *
 *  Created on: 2026年7月8日
 *      Author: 珩泊霄
 */
#include "pid.h"
/*
 * PID 控制模块。
 * 舵机使用随中线偏差变化的比例系数和位置式 PD；左右电机分别使用
 * 增量式 PID 计算 PWM 修正量，控制器状态在各自 PID 实例中独立保存。
 */

/* ==================== 控制器实例与基础增益 ==================== */
PID servo_pid=PID_CREATE(0,0,0,CONFIG_SERVO_LOW_PASS);

float Kp_base=CONFIG_SERVO_KP_BASE;
float Kd_base=CONFIG_SERVO_KD_BASE;

PID motor_pid_l=PID_CREATE(CONFIG_MOTOR_PID_KP,CONFIG_MOTOR_PID_KI,CONFIG_MOTOR_PID_KD,CONFIG_MOTOR_LOW_PASS);
PID motor_pid_r=PID_CREATE(CONFIG_MOTOR_PID_KP,CONFIG_MOTOR_PID_KI,CONFIG_MOTOR_PID_KD,CONFIG_MOTOR_LOW_PASS);


/* ==================== 舵机动态增益 ==================== */

/**
 * @brief 计算浮点数绝对值。
 * @param i 输入数值。
 * @return i 的非负绝对值。
 * @note 当前仅供舵机比例增益调度使用。
 */
float my_abs_float(float i)
{
    if(i>=0)
    {
        return i;
    }
    else
    {
        return -i;
    }
}
/**
 * @brief 根据当前中线偏差更新舵机 PD 参数。
 * @return 无。结果写入全局控制器 servo_pid。
 * @note 由舵机周期中断在每次控制计算前调用；偏差越大，Kp 越高，
 *       用于增强弯道响应，Ki 固定为 0，Kd 使用配置的基础值。
 */
void dynamic_pid_value_set(void)
{
    /* 动态增益与实际舵机目标保持一致，电机控制仍读取 final_mid_line。 */
    servo_pid.Kp=Kp_base+my_abs_float(CONFIG_MID_WIDTH-steer_mid_line)*CONFIG_SERVO_KP_DYNAMIC_GAIN;
    //servo_pid.Kp=Kp_base+(CONFIG_MID_WIDTH-final_mid_line)*(CONFIG_MID_WIDTH-final_mid_line) * 0.0015;
    servo_pid.Ki=0;
    servo_pid.Kd=Kd_base;
}


/* ==================== PID 计算 ==================== */

/**
 * @brief 计算舵机位置式 PD 输出。
 * @param PID 控制器状态指针。
 * @param NowData 当前测量量。
 * @param Point 目标量。
 * @return 本周期比例项与滤波微分项之和。
 * @note 微分项按 LowPass 系数进行一阶滤波，降低图像中线抖动的影响。
 */
float PID_Normal(PID *PID,float NowData,float Point)
{
    PID->Error = Point - NowData;
    PID->Out_D=(PID->Error - PID ->Out_P)*PID->LowPass+PID->Out_D*(1-PID->LowPass);
    PID->Out_P=PID->Error;
    return (PID->Kp*PID->Out_P+PID->Kd*PID->Out_D);
}
/**
 * @brief 计算电机速度环的增量式 PID 修正。
 * @param PID 左轮或右轮控制器状态指针。
 * @param NowData 当前滤波编码器速度。
 * @param Point 目标编码器速度。
 * @return 本周期应叠加到历史 PWM 修正量上的增量。
 * @note 积分输入先限幅，历史误差采用现有平滑更新方式，避免误差突变。
 */
float PID_Increase(PID *PID,float NowData,float Point)
{
    PID->Error = Point - NowData;
    PID->Out_P=(PID->Error - PID->LastError);
    PID->Out_I=Limit_float(-CONFIG_MOTOR_I_LIMIT,PID->Error,CONFIG_MOTOR_I_LIMIT);
    PID->Out_D=(PID->Error - 2 * PID->LastError + PID->PrevError);
    PID->PrevError = 0.9* PID->LastError + 0.1 * PID->PrevError;
    PID->LastError = 0.9*PID->Error+0.1*PID->LastError;
    PID->LastData = NowData;
    return (PID->Kp*PID->Out_P+PID->Ki*PID->Out_I+PID->Kd*PID->Out_D);
}
