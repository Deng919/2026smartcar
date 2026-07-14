/*
 * image.c
 *
 *  Created on: 2026年7月7日
 *      Author: 珩泊霄
 */
#include "image.h"
/*
 * 图像处理模块。
 * 主循环每收到一帧完整图像后，依次完成阈值计算、二值化、起点搜索、
 * 边线跟踪和加权中线计算；TFT 绘制可由 KEY_2 独立关闭。
 */

/* ==================== 图像缓冲与识别结果 ==================== */
uint8 base_image[MT9V03X_H][MT9V03X_W];
uint8 image[MT9V03X_H][MT9V03X_W];
uint8 left_jidian;
uint8 right_jidian;
uint8 left_line_list[MT9V03X_H];
uint8 right_line_list[MT9V03X_H];
uint8 mid_line_list[MT9V03X_H];
/** KEY_2 控制的 TFT 显示标志；中断修改，主循环读取。 */
volatile uint8 image_display_enable = 1;

/* ==================== Otsu 阈值计算状态 ==================== */
uint16 hist[CONFIG_GRAY_SCALE] = {0};
float P[CONFIG_GRAY_SCALE] = {0};
float PK[CONFIG_GRAY_SCALE] = {0};
float MK[CONFIG_GRAY_SCALE] = {0};
uint8 img_threshold;
float imgsize;
/**
 * @brief 根据整帧灰度直方图计算 Otsu 全局阈值。
 * @param index 输入灰度图，尺寸为 MT9V03X_H x MT9V03X_W。
 * @return 使前景与背景类间方差最大的灰度阈值。
 * @note 由 CPU0 主循环每帧调用；当图像无法形成有效两类时返回中间灰度。
 */
uint8 Ostu(uint8 index[MT9V03X_H][MT9V03X_W])
{
    /* 没有有效类间方差时，使用中间灰度作为安全默认值。 */
    uint8 threshold = CONFIG_GRAY_SCALE / 2;
    imgsize = MT9V03X_H * MT9V03X_W;
    uint8 images_value_temp;

    float sumPK = 0;
    float sumMK = 0;
    float var = 0;
    float vartmp = 0;

    /* 每帧重新清空直方图和累计概率，避免上一帧数据残留。 */
    for(uint16 i=0; i < CONFIG_GRAY_SCALE; i++)
    {
        hist[i] = 0;
        P[i] = 0;
        PK[i] = 0;
        MK[i] = 0;
    }

    /* 统计整幅图的灰度直方图。 */
    for(uint8 i = 0; i < MT9V03X_H; i++)
    {
        for(uint8 j = 0; j < MT9V03X_W; j++)
        {
            images_value_temp = index[i][j];
            hist[images_value_temp]++;
        }
    }

    /* 计算累计概率 PK 与累计灰度均值 MK。 */
    for(uint16 i = 0; i < CONFIG_GRAY_SCALE; i++)
    {
        P[i] = (float) hist[i] / imgsize;
        PK[i] = sumPK + P[i];
        sumPK = PK[i];
        MK[i] = sumMK + i * P[i];
        sumMK = MK[i];
    }

    /* 避开灰度两端的少量噪声，在候选阈值中寻找最大类间方差。 */
    for(uint8 i = 5; i < 245;i++)
    {
        /* 两类概率为 0 或 1 时分母无效，跳过该候选阈值。 */
        float denominator = PK[i] * (1-PK[i]);
        if(denominator <= 0.0f)
        {
            continue;
        }
        vartmp = ((MK[CONFIG_GRAY_SCALE - 1] * PK[i] - MK[i])*(MK[CONFIG_GRAY_SCALE - 1] * PK[i] - MK[i])) / denominator;
        if(vartmp > var)
        {
            var = vartmp;
            threshold = i;
        }
    }
    return threshold;
}


/* ==================== 图像二值化 ==================== */

/**
 * @brief 将原始灰度帧转换为黑白二值图。
 * @param value 当前帧 Otsu 阈值；小于阈值置 0，否则置 255。
 * @return 无。结果写入全局数组 image。
 * @note 保留现有两段遍历顺序：先处理前 40 行，再按统一阈值处理整帧。
 */
void set_image_twovalues(uint8 value)
{
    uint8 temp_value;
    for(uint8 i=0; i < 40; i++)//40
    {
        for(uint8 j = 0; j < MT9V03X_W; j++)
        {
            temp_value=base_image[i][j];
            if(temp_value<value+CONFIG_THRESHOLD_OFFSET)
            {
                image[i][j]=0;
            }
            else
            {
                image[i][j]=255;
            }
        }
    }
    for(uint8 i=0; i < MT9V03X_H; i++)
        {
            for(uint8 j = 0; j < MT9V03X_W; j++)
            {
                temp_value=base_image[i][j];
                if(temp_value<value)
                {
                    image[i][j]=0;
                }
                else
                {
                    image[i][j]=255;
                }
            }
        }
}

/* ==================== 边线起点搜索 ==================== */

/**
 * @brief 在固定搜索行寻找左右边线起点。
 * @param index 输入二值图；白色赛道区域为 255，黑色间隔或背景为 0。
 * @return 无。结果写入 left_jidian 和 right_jidian。
 * @note 依次尝试图像中心、左侧四分之一点和右侧四分之一点，
 *       只有采样点附近连续为白色时才从该位置向两侧寻找黑白跳变。
 */
void find_jidian(uint8 index[MT9V03X_H][MT9V03X_W])
{
    /* 优先从图像中心开始；中心不可用时再尝试左右四分之一位置。 */
    if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/2]==255&&index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/2+1]==255&&index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/2-1]==255)
    {
        for(uint8 j = MT9V03X_W/2; j > 0; j--)
        {
            if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][j-1]==0 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j+1]==255)
            {
                left_jidian = j;
                break;
            }
            if(j-1==1)
            {
                left_jidian = 1;
                break;
            }
        }
        for(uint8 j = MT9V03X_W/2; j < MT9V03X_W-2; j++)
        {
            if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][j-1]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j+1]==0)
            {
                right_jidian = j;
                break;
            }
            if(j+1==MT9V03X_W - 2)
            {
                right_jidian = MT9V03X_W - 2;
                break;
            }
        }
    }
    else if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/4]==255&&index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/4+1]==255&&index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/4-1]==255)
        {
            for(uint8 j = MT9V03X_W/4; j > 0; j--)
            {
                if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][j-1]==0 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j+1]==255)
                {
                    left_jidian = j;
                    break;
                }
                if(j-1==1)
                {
                    left_jidian = 1;
                    break;
                }
            }
            for(uint8 j = MT9V03X_W/4; j < MT9V03X_W-2; j++)
            {
                if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][j-1]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j+1]==0)
                {
                    right_jidian = j;
                    break;
                }
                if(j+1==MT9V03X_W - 2)
                {
                    right_jidian = MT9V03X_W - 2;
                    break;
                }
            }
        }
    else if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/4*3]==255&&index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/4*3+1]==255&&index[CONFIG_JIDIAN_SEARCH_LINE - 1][MT9V03X_W/4*3-1]==255)
        {
            for(uint8 j = MT9V03X_W/4*3; j > 0; j--)
            {
                if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][j-1]==0 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j+1]==255)
                {
                    left_jidian = j;
                    break;
                }
                if(j-1==1)
                {
                    left_jidian = 1;
                    break;
                }
            }
            for(uint8 j = MT9V03X_W/4*3; j < MT9V03X_W-2; j++)
            {
                if(index[CONFIG_JIDIAN_SEARCH_LINE - 1][j-1]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j]==255 && index[CONFIG_JIDIAN_SEARCH_LINE - 1][j+1]==0)
                {
                    right_jidian = j;
                    break;
                }
                if(j+1==MT9V03X_W - 2)
                {
                    right_jidian = MT9V03X_W - 2;
                    break;
                }
            }
        }
}

/* ==================== 通用限幅函数 ==================== */

/**
 * @brief 将无符号 8 位数限制在闭区间 [a, c]。
 * @param a 下限。
 * @param b 待限制数值。
 * @param c 上限。
 * @return 限幅后的数值。
 */
uint8 Limit_uint8(uint8 a,uint8 b,uint8 c)
{
    if((b>=a)&&(b<=c))
    {
        return b;
    }
    else if(b<a)
    {
        return a;
    }
    else if(b>c)
    {
        return c;
    }
    return 0;
}
/**
 * @brief 将整数限制在闭区间 [a, c]。
 * @param a 下限。
 * @param b 待限制数值。
 * @param c 上限。
 * @return 限幅后的数值。
 */
int Limit_int(int a,int b,int c)
{
    if((b>=a)&&(b<=c))
    {
        return b;
    }
    else if(b<a)
    {
        return a;
    }
    else if(b>c)
    {
        return c;
    }
    return 0;
}
/**
 * @brief 将浮点数限制在闭区间 [a, c]。
 * @param a 下限。
 * @param b 待限制数值。
 * @param c 上限。
 * @return 限幅后的数值。
 */
float Limit_float(float a,float b,float c)
{
    if((b>=a)&&(b<=c))
    {
        return b;
    }
    else if(b<a)
    {
        return a;
    }
    else if(b>c)
    {
        return c;
    }
    return 0;
}

/* ==================== 逐行边线跟踪 ==================== */

/**
 * @brief 从左右起点向图像远端逐行跟踪边线，并计算每行中线。
 * @param index 输入二值图。
 * @return 无。结果写入 left_line_list、right_line_list 和 mid_line_list。
 * @note 每行先在上一位置附近局部搜索，失败后从图像中心扩大搜索；
 *       若仍未找到，则保留上一行边线，避免中线因单行丢失突然跳变。
 */
void image_deal(uint8 index[MT9V03X_H][MT9V03X_W])
{
    uint8 left_point = left_jidian;
    uint8 right_point = right_jidian;
    for(uint8 i = CONFIG_SEARCH_START_LINE-1;i>CONFIG_SEARCH_END_LINE ; i--)
    {
        uint8 left_search_judge = 0;
        uint8 mid_start_left_search_judge = 0;
        uint8 left_found = 0;
        uint8 right_found = 0;

        /* 左边线先向右搜索，未找到时再向左和图像中心回退搜索。 */
        for(uint8 j = left_point; j < left_point+CONFIG_LEFT_SEARCH_RIGHT; j++)
        {
            if(index[i][j-1]==0 && index[i][j]==255 && index[i][j+1]==255)
            {
                left_point = j;
                left_found = 1;
                break;
            }
            else if(j == MT9V03X_W-2)
            {
                left_point = MT9V03X_W-5;
                break;
            }
            else if(j == left_point + CONFIG_LEFT_SEARCH_RIGHT - 1)
            {
                left_search_judge = 1;
                break;
            }
        }
        if(left_search_judge == 1)
        {
                    for(uint8 j = left_point; j > left_point-CONFIG_LEFT_SEARCH_LEFT; j--)
                    {
                        if(index[i][j-1]==0 && index[i][j]==255 && index[i][j+1]==255 && j<MT9V03X_W - 5)
                        {
                            left_point = j;
                            left_found = 1;
                            break;
                        }
                        else if(j == 1)
                        {
                            left_point = 1;
                            mid_start_left_search_judge = 1;
                            break;
                        }
                        else if(j == left_point - CONFIG_LEFT_SEARCH_LEFT + 1)
                        {
                            mid_start_left_search_judge = 1;
                            break;
                        }
                    }
        }
        if(mid_start_left_search_judge == 1)
        {
            for(uint8 j = CONFIG_MID_WIDTH; j > 0; j--)
            {
                if(index[i][j-1]==0 && index[i][j]==255 && index[i][j+1]==255)
                {
                    left_point = j;
                    left_found = 1;
                    break;
                }
                else if(j==1)
                {
                    left_point = 1;
                    break;
                }
            }
        }
        /* 右边线按相反方向搜索，并在局部搜索失败后从中心补搜。 */
            uint8 right_search_judge = 0;
            uint8 mid_start_right_search_judge = 0;

            for(uint8 j = right_point; j > right_point-CONFIG_RIGHT_SEARCH_LEFT; j--)
            {
                if(index[i][j-1]==255 && index[i][j]==255 && index[i][j+1]==0)
                {
                    right_point = j;
                    right_found = 1;
                    break;
                }
                else if(j == 1)
                {
                    right_point = 4;
                    break;
                }
                else if(j == right_point - CONFIG_RIGHT_SEARCH_LEFT + 1)
                {
                    right_search_judge = 1;
                    break;
                }
            }
            if(right_search_judge == 1)
            {
                        for(uint8 j = right_point; j < right_point+CONFIG_RIGHT_SEARCH_RIGHT; j++)
                        {
                            if(index[i][j-1]==255 && index[i][j]==255 && index[i][j+1]==0 && j > 4)
                            {
                                right_point = j;
                                right_found = 1;
                                break;
                            }
                            else if(j == MT9V03X_W-2)
                            {
                                right_point = MT9V03X_W-2;
                                mid_start_right_search_judge = 1;
                                break;
                            }
                            else if(j == right_point + CONFIG_RIGHT_SEARCH_RIGHT - 1)
                            {
                                mid_start_right_search_judge = 1;
                                break;
                            }
                        }
            }
            if(mid_start_right_search_judge == 1)
            {
                for(uint8 j = CONFIG_MID_WIDTH; j < MT9V03X_W-1; j++)
                {
                    if(index[i][j-1]==255 && index[i][j]==255 && index[i][j+1]==0)
                    {
                        right_point = j;
                        right_found = 1;
                        break;
                    }
                    else if(j==MT9V03X_W-2)
                    {
                        right_point = MT9V03X_W-2;
                        break;
                    }
                }
            }
            /* 当前行丢线时沿用上一行位置，第一行则退回本帧起点。 */
            if(!left_found)
            {
                if(i < CONFIG_SEARCH_START_LINE - 1)
                {
                    left_point = left_line_list[i + 1];
                }
                else
                {
                    left_point = left_jidian;
                }
            }
            if(!right_found)
            {
                if(i < CONFIG_SEARCH_START_LINE - 1)
                {
                    right_point = right_line_list[i + 1];
                }
                else
                {
                    right_point = right_jidian;
                }
            }
            left_line_list[i]=Limit_uint8(1,left_point,MT9V03X_W-2);
            right_line_list[i]=Limit_uint8(1,right_point,MT9V03X_W-2);
            mid_line_list[i]=Limit_uint8(1,(left_line_list[i]+right_line_list[i])/2,MT9V03X_W-2);
    }
}

/* ==================== 识别结果显示 ==================== */

/**
 * @brief 将已跟踪的左右边线和中线叠加绘制到 TFT。
 * @return 无。
 * @note 仅在 image_display_enable 为 1 时由主循环调用，不参与控制计算。
 */
void draw_line(void)
{
    for(uint8 i=MT9V03X_H - 1;i>CONFIG_SEARCH_END_LINE;i--)
    {
        tft180_draw_point(0.68*Limit_uint8(1,left_line_list[i],MT9V03X_W-2),0.68*i,RGB565_BLUE);
        tft180_draw_point(0.68*Limit_uint8(1,right_line_list[i],MT9V03X_W-2),0.68*i,RGB565_GREEN);
        tft180_draw_point(0.68*Limit_uint8(1,mid_line_list[i],MT9V03X_W-2),0.68*i,RGB565_RED);
    }
}

/* STEERING_LOOKAHEAD_TEST_BEGIN */
/** 计算指定行区间的平均中线，区间端点均参与计算。 */
static uint8 image_mid_line_average(const uint8 *lines, uint8 start, uint8 end)
{
    unsigned int sum = 0;
    unsigned int count = 0;

    for(uint8 row = start; row <= end; row++)
    {
        sum += lines[row];
        count++;
    }
    return (uint8)(sum / count);
}

/** 返回带符号整数的绝对值。 */
static int steer_lookahead_abs(int value)
{
    return value >= 0 ? value : -value;
}

/**
 * @brief 将远近中线形成的航向趋势转换为受限、滤波后的舵机目标中线。
 * @param final_mid 原有加权中线，继续作为稳定的基础位置。
 * @param near_mid 近处平均中线。
 * @param far_mid 远处平均中线。
 * @param previous_mid 上一帧舵机专用中线。
 * @return 本帧舵机专用中线。
 */
static uint8 steer_lookahead_calculate(
    uint8 final_mid,
    uint8 near_mid,
    uint8 far_mid,
    uint8 previous_mid)
{
    int heading_error = (int)far_mid - (int)near_mid;
    int offset = 0;
    int target_mid = final_mid;

    /* 差值过大通常来自远处围栏或丢线，本帧只使用原有中线。 */
    if(steer_lookahead_abs(heading_error) <= CONFIG_STEER_LOOKAHEAD_MAX_HEADING)
    {
        offset = (int)(heading_error * CONFIG_STEER_LOOKAHEAD_GAIN);
        if(offset > CONFIG_STEER_LOOKAHEAD_MAX_OFFSET)
        {
            offset = CONFIG_STEER_LOOKAHEAD_MAX_OFFSET;
        }
        else if(offset < -CONFIG_STEER_LOOKAHEAD_MAX_OFFSET)
        {
            offset = -CONFIG_STEER_LOOKAHEAD_MAX_OFFSET;
        }
        target_mid += offset;
    }

    if(target_mid < 1)
    {
        target_mid = 1;
    }
    else if(target_mid > MT9V03X_W - 2)
    {
        target_mid = MT9V03X_W - 2;
    }

    return (uint8)(
        previous_mid * CONFIG_STEER_LOOKAHEAD_FILTER_OLD +
        target_mid * (1.0f - CONFIG_STEER_LOOKAHEAD_FILTER_OLD));
}
/* STEERING_LOOKAHEAD_TEST_END */

/* ==================== 加权中线计算 ==================== */

/** 各图像行的中线权重，数值来自 config.h。 */
uint8 mid_weight_list[120]=CONFIG_MID_WEIGHT_LIST;
uint8 final_mid_line = CONFIG_MID_WIDTH;
/** 舵机专用软前瞻中线；电机仍使用 final_mid_line。 */
uint8 steer_mid_line = CONFIG_MID_WIDTH;
uint8 last_mid_line = CONFIG_MID_WIDTH;
/**
 * @brief 计算供转向和弯道降速使用的加权中线。
 * @return 加权并进行一阶滤波后的中线横坐标。
 * @note 权重较大的行对结果影响更强；上一帧结果占 0.2，当前帧占 0.8，
 *       用于抑制识别抖动，同时保留主要转向响应。
 */
uint8 find_mid_line_weight(void)
{
    uint8 mid_line_value = CONFIG_MID_WIDTH;
    uint8 mid_line = CONFIG_MID_WIDTH;
    uint32 weight_midline_sum = 0;
    uint32 weight_sum = 0;
    for(uint8 i=MT9V03X_H - 1;i>CONFIG_SEARCH_END_LINE;i--)
    {
            weight_midline_sum += mid_line_list[i] * mid_weight_list[i];
            weight_sum += mid_weight_list[i];
    }
    mid_line=(uint8)(weight_midline_sum/weight_sum);
    mid_line_value=last_mid_line*0.2+mid_line*0.8;
    /* 电机保留原中线；舵机额外使用远近中线趋势进行有限前瞻。 */
    steer_mid_line = steer_lookahead_calculate(
        mid_line_value,
        image_mid_line_average(mid_line_list, CONFIG_STEER_LOOKAHEAD_NEAR_START, CONFIG_STEER_LOOKAHEAD_NEAR_END),
        image_mid_line_average(mid_line_list, CONFIG_STEER_LOOKAHEAD_FAR_START, CONFIG_STEER_LOOKAHEAD_FAR_END),
        steer_mid_line);
    last_mid_line=mid_line_value;
    return mid_line_value;
}
