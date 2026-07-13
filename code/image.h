/*
 * image.h
 *
 * 图像处理模块公开接口。
 * 负责声明摄像头帧缓冲、二值图、边线数据、中线结果及图像处理函数。
 */
#ifndef CODE_IMAGE_H_
#define CODE_IMAGE_H_

#include "zf_common_headfile.h"
#include "config.h"

/* ==================== 共享图像数据 ==================== */

/** 摄像头原始灰度帧，尺寸为 MT9V03X_H x MT9V03X_W。 */
extern uint8 base_image[MT9V03X_H][MT9V03X_W];
/** 二值化后的识别图像：0 表示黑色，255 表示白色。 */
extern uint8 image[MT9V03X_H][MT9V03X_W];
/** 起始搜索行上得到的左、右边线横坐标。 */
extern uint8 left_jidian;
extern uint8 right_jidian;
/** 各图像行跟踪到的左边线、右边线和中线横坐标。 */
extern uint8 left_line_list[MT9V03X_H];
extern uint8 right_line_list[MT9V03X_H];
extern uint8 mid_line_list[MT9V03X_H];
/** 当前帧由 Otsu 方法计算得到的灰度阈值。 */
extern uint8 img_threshold;
/** 多行加权并滤波后的最终中线横坐标，供舵机和电机控制使用。 */
extern uint8 final_mid_line;
/** TFT 图像显示开关；只控制绘图，关闭后图像识别仍持续运行。 */
extern volatile uint8 image_display_enable;

/* ==================== 图像处理接口 ==================== */

/** 根据输入灰度帧计算 Otsu 全局阈值。 */
uint8 Ostu(uint8 index[MT9V03X_H][MT9V03X_W]);
/** 使用给定阈值把 base_image 转换到二值图 image。 */
void set_image_twovalues(uint8 value);
/** 在指定搜索行寻找左右边线的初始横坐标。 */
void find_jidian(uint8 index[MT9V03X_H][MT9V03X_W]);
/** 从初始点逐行跟踪左右边线，并生成每行中线。 */
void image_deal(uint8 index[MT9V03X_H][MT9V03X_W]);
/** 对有效行中线加权、滤波并返回最终中线横坐标。 */
uint8 find_mid_line_weight(void);
/** 将左右边线和中线叠加绘制到 TFT。 */
void draw_line(void);

/* ==================== 通用限幅接口 ==================== */

/** 将 uint8 数值 b 限制到闭区间 [a, c]。 */
uint8 Limit_uint8(uint8 a, uint8 b, uint8 c);
/** 将 int 数值 b 限制到闭区间 [a, c]。 */
int Limit_int(int a, int b, int c);
/** 将 float 数值 b 限制到闭区间 [a, c]。 */
float Limit_float(float a, float b, float c);

#endif /* CODE_IMAGE_H_ */
