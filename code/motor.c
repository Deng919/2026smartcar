/*
 * motor.c
 *
 *  Created on: 2026年7月8日
 *      Author: 珩泊霄
 */
#include "motor.h"
/*
 * 电机控制模块。
 * CH1 周期中断负责读取编码器、处理启停状态，并通过速度前馈和增量 PID
 * 生成左右轮 PWM；图像中线偏差同时用于差速和弯道目标速度选择。
 */

/* ==================== 车轮状态与控制器输出 ==================== */

motorl motor_l;
motorl motor_r;
/* 将 PID 修正量与前馈基础占空比分开保存。 */
static int motor_pid_duty_l;
static int motor_pid_duty_r;

/* ==================== 电机初始化与直接输出 ==================== */

/**
 * @brief 初始化左右电机方向 GPIO 和 PWM 通道。
 * @return 无。
 * @note 在 CPU0 启动阶段调用，PWM 频率固定为 17 kHz。
 */
void Motor_Init()
{
    gpio_init(MotorL_turn,GPO,1,GPO_PUSH_PULL);
    pwm_init(MotorL_pwm,17000,0);
    gpio_init(MotorR_turn,GPO,1,GPO_PUSH_PULL);
    pwm_init(MotorR_pwm,17000,0);
}

/**
 * @brief 直接设置左电机带符号 PWM。
 * @param pwm PWM 指令；非负为正向，负值为反向，绝对值为占空比。
 * @return 无。
 * @note 该接口绕过速度闭环，主要用于底层调试。
 */
void MotorL_SetSpeed(int pwm)
{
      if(pwm>=0)
      {
          gpio_set_level(MotorL_turn,1);
          pwm_set_duty(MotorL_pwm,pwm);
      }
      else{
          gpio_set_level(MotorL_turn,0);
          pwm_set_duty(MotorL_pwm,-pwm);
      }
}
/**
 * @brief 直接设置右电机带符号 PWM。
 * @param pwm PWM 指令；非负为正向，负值进入反向输出分支。
 * @return 无。
 * @note 该接口保留现有底层实现，实车反转调试时应核对方向引脚。
 */
void MotorR_SetSpeed(int pwm)
{
      if(pwm>=0)
      {
          gpio_set_level(MotorR_turn,1);
          pwm_set_duty(MotorR_pwm,pwm);
      }
      else{
          gpio_set_level(MotorL_turn,0);
          pwm_set_duty(MotorR_pwm,-pwm);
      }
}


/* ==================== 编码器采样 ==================== */

/**
 * @brief 初始化左右轮正交编码器接口。
 * @return 无。
 * @note 在 CPU0 启动阶段调用一次。
 */
void Encoder_Init(void)
{
    encoder_dir_init(TIM5_ENCODER,TIM5_ENCODER_CH1_P10_3,TIM5_ENCODER_CH2_P10_1);
    encoder_dir_init(TIM6_ENCODER,TIM6_ENCODER_CH1_P20_3,TIM6_ENCODER_CH2_P20_0);
}
/**
 * @brief 读取左右轮编码器增量，并更新滤波速度与累计里程。
 * @return 无。结果写入 motor_l 和 motor_r。
 * @note 由 CH1 周期中断调用；读取后立即清零硬件计数器。右轮计数取反，
 *       使车辆前进时左右轮速度符号保持一致。
 */
void Encode_Data_Get(void)
{
    motor_l.encoder_raw=encoder_get_count(TIM5_ENCODER);
    motor_l.encoder_speed=motor_l.encoder_speed*0.2f+motor_l.encoder_raw*0.8f;
    motor_l.total_encoder+=motor_l.encoder_raw;
    encoder_clear_count(TIM5_ENCODER);

    motor_r.encoder_raw=-encoder_get_count(TIM6_ENCODER);
    motor_r.encoder_speed=motor_r.encoder_speed*0.2f+motor_r.encoder_raw*0.8f;
    motor_r.total_encoder+=motor_r.encoder_raw;
    encoder_clear_count(TIM6_ENCODER);
}

/* ==================== 目标速度与差速调度 ==================== */

/**
 * @brief 根据图像中线偏差生成左右轮目标速度。
 * @param speed 当前基础目标速度，单位为每个控制周期的编码器计数。
 * @param limit 左右轮相对基础速度允许的最大偏差。
 * @return 无。函数将限幅后的目标传递给 Motor_Control。
 * @note 中线位于图像中心两侧时，左右轮以相反方向调整形成转弯差速。
 */
void Final_Motor_Control(int speed,int limit)
{
    Motor_Control(Limit_int(speed-limit,speed-1*((int)CONFIG_MID_WIDTH-(int)final_mid_line),speed+limit),Limit_int(speed-limit,speed+1*((int)CONFIG_MID_WIDTH-(int)final_mid_line),speed+limit));
}

/**
 * @brief 处理车辆启停，并根据中线绝对误差选择弯道目标速度。
 * @return 无。
 * @note KEY_1 每次短按使 start 加一：奇数运行、偶数停车；函数由 CH1
 *       周期中断调用，弯道速度阈值和各档速度均在 config.h 中配置。
 */
void car_start(void)
{
    if(key_get_state(KEY_1)==KEY_SHORT_PRESS)
    {
        start++;
    }
    if(start!=0)
    {
        if(start%2==1)
        {
           int target_speed = CONFIG_MOTOR_TARGET_SPEED;
           int line_error = (int)CONFIG_MID_WIDTH - (int)final_mid_line;
           if(line_error < 0)
           {
               line_error = -line_error;
           }

           /* 中线离图像中心越远，说明转弯需求越强，按阈值降低基础速度。 */
           if(line_error >= CONFIG_MOTOR_CURVE_ERROR_HIGH)
           {
               target_speed = CONFIG_MOTOR_CURVE_SPEED_LOW;
           }
           else if(line_error >= CONFIG_MOTOR_CURVE_ERROR_MID)
           {
               target_speed = CONFIG_MOTOR_CURVE_SPEED_MID;
           }

           Final_Motor_Control(target_speed,CONFIG_MOTOR_SPEED_LIMIT);
        }
        else
        {
            Motor_Control(0,0);
        }
    }
}

/* ==================== 速度闭环与 PWM 输出 ==================== */

/**
 * @brief 向指定 PWM 通道和方向引脚输出带符号速度指令。
 * @param pin1 电机 PWM 通道。
 * @param pin2 电机方向 GPIO。
 * @param speed 带符号 PWM 指令，绝对值作为占空比。
 * @param just speed 为正时写入方向引脚的电平。
 * @param lose speed 非正时写入方向引脚的电平。
 * @return 无。
 */
void Speed_Set(pwm_channel_enum pin1,gpio_pin_enum pin2,int speed,uint8 just,uint8 lose)
{
    if(speed>0)
    {
        pwm_set_duty(pin1,speed);
        gpio_set_level(pin2,just);
    }
    else
    {
        pwm_set_duty(pin1,-speed);
        gpio_set_level(pin2,lose);
    }
}
/**
 * @brief 计算左右轮速度闭环并输出最终 PWM。
 * @param Speed_L 左轮目标编码器速度。
 * @param Speed_R 右轮目标编码器速度。
 * @return 无。
 * @note 前馈按目标速度直接提供基础占空比，增量 PID 仅修正实际速度误差；
 *       目标速度为 0 时清零累计修正，防止再次启动时保留旧控制量。
 */
void Motor_Control(int Speed_L,int Speed_R)
{
    motor_l.target_speed=Speed_L;
    motor_r.target_speed=Speed_R;

    if(motor_l.target_speed == 0)
    {
        motor_pid_duty_l = 0;
    }
    else
    {
        motor_pid_duty_l=Limit_int(-CONFIG_MOTOR_PWM_LIMIT,motor_pid_duty_l+(int)PID_Increase(&motor_pid_l,motor_l.encoder_speed,(float)motor_l.target_speed),CONFIG_MOTOR_PWM_LIMIT);
    }

    if(motor_r.target_speed == 0)
    {
        motor_pid_duty_r = 0;
    }
    else
    {
        motor_pid_duty_r=Limit_int(-CONFIG_MOTOR_PWM_LIMIT,motor_pid_duty_r+(int)PID_Increase(&motor_pid_r,motor_r.encoder_speed,(float)motor_r.target_speed),CONFIG_MOTOR_PWM_LIMIT);
    }

    /* 前馈提供基础占空比，PID 只修正速度误差。 */
    motor_l.duty=Limit_int(-CONFIG_MOTOR_PWM_LIMIT,(int)(motor_l.target_speed*CONFIG_MOTOR_FEEDFORWARD_GAIN)+motor_pid_duty_l,CONFIG_MOTOR_PWM_LIMIT);
    motor_r.duty=Limit_int(-CONFIG_MOTOR_PWM_LIMIT,(int)(motor_r.target_speed*CONFIG_MOTOR_FEEDFORWARD_GAIN)+motor_pid_duty_r,CONFIG_MOTOR_PWM_LIMIT);

    Speed_Set(MotorL_pwm,MotorL_turn,motor_l.duty,1,0);
    Speed_Set(MotorR_pwm,MotorR_turn,motor_r.duty,1,0);
}
