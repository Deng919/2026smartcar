/*
 * pid.h
 *
 * PID 控制模块公开接口。
 * 同一状态结构用于舵机位置式 PD 和左右电机增量式 PID。
 */
#ifndef CODE_PID_H_
#define CODE_PID_H_

#include "zf_common_headfile.h"
#include "config.h"

/* ==================== PID 状态类型 ==================== */

typedef struct PID
{
    float Kp;           /**< 比例系数。 */
    float Ki;           /**< 积分系数。 */
    float Kd;           /**< 微分系数。 */
    float LowPass;      /**< 微分或测量滤波系数。 */

    float Out_P;        /**< 比例环节或当前误差增量。 */
    float Out_I;        /**< 积分环节输入，电机环中进行限幅。 */
    float Out_D;        /**< 微分环节输出。 */

    float PrevError;    /**< 上上次误差的滤波记录。 */
    float LastError;    /**< 上次误差的滤波记录。 */
    float Error;        /**< 本次目标值与测量值之差。 */
    float LastData;     /**< 上次测量值，保留用于状态观察。 */
} PID;

/**
 * @brief 创建清零后的 PID 初始值。
 * @note 未显式指定的结构体成员由 C 语言规则自动初始化为 0。
 */
#define PID_CREATE(_kp,_ki,_kd,_low_pass)\
{                                        \
    .Kp=_kp,                             \
    .Ki=_ki,                             \
    .LowPass=_low_pass,                  \
    .Out_P=0,                            \
    .Out_I=0,                            \
    .Out_D=0,                            \
}

/* ==================== 控制器实例 ==================== */

extern struct PID servo_pid;
extern struct PID motor_pid_l;
extern struct PID motor_pid_r;
/** 舵机动态 PD 的运行时基础参数，可由 VOFA+ 在线修改。 */
extern float Kp_base;
extern float Kd_base;

/* ==================== PID 计算接口 ==================== */

/** 舵机位置式 PD：输入当前量和目标量，返回本周期控制输出。 */
float PID_Normal(PID *PID, float NowData, float Point);
/** 电机增量式 PID：输入当前速度和目标速度，返回占空比修正增量。 */
float PID_Increase(PID *PID, float NowData, float Point);
/** 根据中线偏差更新舵机比例系数，并装载舵机 PD 参数。 */
void dynamic_pid_value_set(void);

#endif /* CODE_PID_H_ */
