/*
 * config.h
 *
 * 智能车应用的集中调试参数。
 * 硬件引脚映射和运行时状态保留在各自模块中。
 */
#ifndef CODE_CONFIG_H_
#define CODE_CONFIG_H_

/* ==================== 图像处理参数 ==================== */
/* 灰度级数量，MT9V03X 灰度图通常使用 256 级。 */
#define CONFIG_GRAY_SCALE                 256
/* 极点搜索所在的图像行，数值越小越靠近图像上方。 */
#define CONFIG_JIDIAN_SEARCH_LINE         120
/* 边线跟踪的起始行和结束行。 */
#define CONFIG_SEARCH_START_LINE          120
#define CONFIG_SEARCH_END_LINE            30
/* 左右边线在当前点附近的横向搜索范围。 */
#define CONFIG_LEFT_SEARCH_RIGHT          10
#define CONFIG_LEFT_SEARCH_LEFT           5
#define CONFIG_RIGHT_SEARCH_RIGHT         5
#define CONFIG_RIGHT_SEARCH_LEFT          10
/* 图像中心对应的横坐标，需根据摄像头安装位置调整。 */
#define CONFIG_MID_WIDTH                  93
/* 图像二值化时，前 40 行使用的额外阈值偏移。 */
#define CONFIG_THRESHOLD_OFFSET           40

/* ==================== 舵机参数 ==================== */
/* 舵机 PWM 中值，以及允许使用的 PWM 范围。 */
#define CONFIG_SERVO_PWM_MIDDLE           750
#define CONFIG_SERVO_PWM_MIN              675
#define CONFIG_SERVO_PWM_MAX              835
/* 闭环舵机输出相对于中值的最大偏差。 */
#define CONFIG_SERVO_OUTPUT_LIMIT         80
/* 舵机 PID：基础 Kp、动态 Kp 增益、Kd 和微分低通系数。 */
#define CONFIG_SERVO_KP_BASE              0.5f
#define CONFIG_SERVO_KP_DYNAMIC_GAIN      0.045f
#define CONFIG_SERVO_KD_BASE              3.8f
#define CONFIG_SERVO_LOW_PASS             0.8f

/* ==================== 电机参数 ==================== */
/* 左右电机速度环 PID 参数。 */
#define CONFIG_MOTOR_PID_KP               5.0f
#define CONFIG_MOTOR_PID_KI               0.3f
#define CONFIG_MOTOR_PID_KD               0.0f
#define CONFIG_MOTOR_LOW_PASS             1.0f
/* 编码器速度一阶滤波：历史值 60%，当前采样 40%。 */
#define CONFIG_ENCODER_FILTER_HISTORY     0.6f
#define CONFIG_ENCODER_FILTER_CURRENT     0.4f
/* 积分项限幅和电机 PWM 输出限幅。 */
#define CONFIG_MOTOR_I_LIMIT              30
#define CONFIG_MOTOR_PWM_LIMIT            2500
/* 按键启动后的小车目标速度和左右轮最大速度差。 */
#define CONFIG_MOTOR_TARGET_SPEED         130
#define CONFIG_MOTOR_SPEED_LIMIT          25
/* 根据中线绝对偏差选择弯道速度档位。 */
#define CONFIG_MOTOR_CURVE_ERROR_MID      10
#define CONFIG_MOTOR_CURVE_ERROR_HIGH     20
#define CONFIG_MOTOR_CURVE_SPEED_MID      110
#define CONFIG_MOTOR_CURVE_SPEED_LOW      115
/* 每个编码器速度单位对应的前馈占空比增量。 */
#define CONFIG_MOTOR_FEEDFORWARD_GAIN     8.0f
/* 基础目标速度斜坡：缓慢加速、较快减速，单位为每个电机控制周期。 */
#define CONFIG_MOTOR_ACCEL_STEP           2
#define CONFIG_MOTOR_DECEL_STEP           4

/* ==================== 控制周期 ==================== */
/* 舵机控制中断周期和编码器/电机控制中断周期，单位：毫秒。 */
#define CONFIG_SERVO_CONTROL_PERIOD_MS    20
#define CONFIG_MOTOR_CONTROL_PERIOD_MS   5

/* ==================== 中线权重 ==================== */
/*
 * 中线加权数组，数组下标对应图像行号。
 * 权重越大，该行对最终中线位置的影响越大。
 */
#define CONFIG_MID_WEIGHT_LIST \
{ \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    1,1,1,1,1,1,1,1,1,1, \
    7,8,9,10,11,12,13,14,15,16, \
    17,18,19,20,20,20,20,19,18,17, \
    16,15,14,13,12,11,10,9,8,7, \
    6,6,6,6,6,6,6,6,6,6, \
    6,6,6,6,6,6,6,6,6,6 \
}

#endif /* CODE_CONFIG_H_ */
