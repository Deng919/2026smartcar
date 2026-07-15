/*
 * vofa.c
 *
 * UART0 / 115200 上的 VOFA+ FireWater 文本协议。
 * 中断只产生发送标志，串口收发与字符串解析全部在主循环执行。
 */
#include "zf_common_headfile.h"
#include "vofa.h"

#define VOFA_COMMAND_BUFFER_SIZE      64
#define VOFA_TX_BUFFER_SIZE           160
#define VOFA_DEFAULT_UPLOAD_DIVIDER   4
#define VOFA_MIN_UPLOAD_DIVIDER       1
#define VOFA_MAX_UPLOAD_DIVIDER       40

static volatile uint8 vofa_upload_pending = 0;
static volatile uint8 vofa_tick_count = 0;
static volatile uint8 vofa_upload_divider = VOFA_DEFAULT_UPLOAD_DIVIDER;
static uint8 vofa_stream_enable = 1;
static char vofa_command_buffer[VOFA_COMMAND_BUFFER_SIZE];
static uint8 vofa_command_length = 0;

/** 清除控制器历史状态，避免在线修改参数后保留旧的误差增量。 */
static void vofa_reset_pid_state(PID *pid)
{
    pid->Out_P = 0.0f;
    pid->Out_I = 0.0f;
    pid->Out_D = 0.0f;
    pid->PrevError = 0.0f;
    pid->LastError = 0.0f;
    pid->Error = 0.0f;
    pid->LastData = 0.0f;
}

/** 解析一条以换行结束的在线调参命令。 */
static void vofa_parse_command(char *command)
{
    float kp;
    float ki;
    float kd;
    int value;
    uint32 interrupt_state;

    if(3 == sscanf(command, "MOTOR,%f,%f,%f", &kp, &ki, &kd))
    {
        if(kp >= 0.0f && kp <= 100.0f
        && ki >= 0.0f && ki <= 50.0f
        && kd >= 0.0f && kd <= 100.0f)
        {
            interrupt_state = interrupt_global_disable();
            motor_pid_l.Kp = kp;
            motor_pid_l.Ki = ki;
            motor_pid_l.Kd = kd;
            motor_pid_r.Kp = kp;
            motor_pid_r.Ki = ki;
            motor_pid_r.Kd = kd;
            vofa_reset_pid_state(&motor_pid_l);
            vofa_reset_pid_state(&motor_pid_r);
            interrupt_global_enable(interrupt_state);
        }
    }
    else if(2 == sscanf(command, "SERVO,%f,%f", &kp, &kd))
    {
        if(kp >= 0.0f && kp <= 10.0f && kd >= 0.0f && kd <= 50.0f)
        {
            interrupt_state = interrupt_global_disable();
            Kp_base = kp;
            Kd_base = kd;
            vofa_reset_pid_state(&servo_pid);
            interrupt_global_enable(interrupt_state);
        }
    }
    else if(1 == sscanf(command, "STREAM,%d", &value))
    {
        vofa_stream_enable = (value != 0);
    }
    else if(1 == sscanf(command, "RATE,%d", &value))
    {
        if(value >= VOFA_MIN_UPLOAD_DIVIDER && value <= VOFA_MAX_UPLOAD_DIVIDER)
        {
            vofa_upload_divider = (uint8)value;
            vofa_tick_count = 0;
        }
    }
}

/** 从调试串口环形缓冲区读取数据，并按行组装命令。 */
static void vofa_receive_commands(void)
{
    uint8 rx_buffer[32];
    uint32 rx_length = debug_read_ring_buffer(rx_buffer, sizeof(rx_buffer));
    uint32 i;

    for(i = 0; i < rx_length; i++)
    {
        char value = (char)rx_buffer[i];
        if(value == '\n' || value == '\r')
        {
            if(vofa_command_length > 0)
            {
                vofa_command_buffer[vofa_command_length] = '\0';
                vofa_parse_command(vofa_command_buffer);
                vofa_command_length = 0;
            }
        }
        else
        {
            if(vofa_command_length < VOFA_COMMAND_BUFFER_SIZE - 1)
            {
                vofa_command_buffer[vofa_command_length++] = value;
            }
            else
            {
                vofa_command_length = 0;
            }
        }
    }
}

/** 发送一帧 FireWater 数据，实际速度放大 10 倍以保留一位小数。 */
static void vofa_send_frame(void)
{
    char tx_buffer[VOFA_TX_BUFFER_SIZE];
    int length;

    length = sprintf(tx_buffer, "%d,%d,%d,%d,%d,%d,%d,%u\r\n",
                     motor_l.target_speed * 10,
                     (int)(motor_l.encoder_speed * 10.0f),
                     motor_l.duty,
                     motor_r.target_speed * 10,
                     (int)(motor_r.encoder_speed * 10.0f),
                     motor_r.duty,
                     (int)CONFIG_MID_WIDTH - (int)final_mid_line,
                     (uint16)pwm);
    if(length > 0 && length < VOFA_TX_BUFFER_SIZE)
    {
        debug_send_buffer((const uint8 *)tx_buffer, (uint32)length);
    }
}

void vofa_control_tick(void)
{
    vofa_tick_count++;
    if(vofa_tick_count >= vofa_upload_divider)
    {
        vofa_tick_count = 0;
        vofa_upload_pending = 1;
    }
}

void vofa_process(void)
{
    vofa_receive_commands();
    if(vofa_upload_pending)
    {
        vofa_upload_pending = 0;
        if(vofa_stream_enable)
        {
            vofa_send_frame();
        }
    }
}
