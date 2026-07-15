/*
 * vofa.h
 *
 * VOFA+ FireWater 数据上传与串口在线调参接口。
 */
#ifndef CODE_VOFA_H_
#define CODE_VOFA_H_

#include "zf_common_typedef.h"

/** 由 5 ms 电机控制中断调用，只更新发送节拍标志。 */
void vofa_control_tick(void);
/** 由 CPU0 主循环调用，处理串口命令并发送 FireWater 数据。 */
void vofa_process(void);

#endif /* CODE_VOFA_H_ */
