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
/** 当前帧每行候选边线的可信标志；无效行不会参与最终中线计算。 */
uint8 line_valid_list[MT9V03X_H];
/** 当前帧通过候选验证的有效行数量。 */
uint8 line_valid_count;
/** 上一帧的左右边线，用于锁定当前赛道并拒绝相邻赛道。 */
static uint8 previous_left_line_list[MT9V03X_H];
static uint8 previous_right_line_list[MT9V03X_H];
/** 已获得足够可信历史后置位；仅复位或模块重新初始化时清零。 */
static uint8 track_history_ready;
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

/* LINE_FILTER_TEST_BEGIN */
/** 返回两个无符号坐标的绝对差，避免直接相减产生下溢。 */
static uint8 line_abs_diff(uint8 first, uint8 second)
{
    return first >= second ? first - second : second - first;
}

/**
 * @brief 判断一行左右边线是否仍属于当前锁定赛道。
 * @param row 当前二值图像行，0 为黑色，255 为白色。
 * @param left 候选左边线。
 * @param right 候选右边线。
 * @param reference_left 参考左边线。
 * @param reference_right 参考右边线。
 * @param use_reference 是否检查候选相对参考线的中点和宽度跳变。
 * @return 1 表示候选可信，0 表示候选可能跨过围栏或跳到相邻赛道。
 */
static uint8 line_candidate_is_valid(
    const uint8 *row,
    uint8 left,
    uint8 right,
    uint8 reference_left,
    uint8 reference_right,
    uint8 use_reference)
{
    uint8 black_run = 0;
    uint8 candidate_mid;
    uint8 reference_mid;
    uint8 candidate_width;
    uint8 reference_width;

    /* 留出左右各一个像素，保证搜线邻域访问不会越界。 */
    if(left < 1 || right > MT9V03X_W - 2 || left >= right)
    {
        return 0;
    }

    /* 正常赛道内部应基本连续为白色，连续黑缝过长说明候选跨越了围栏间隔。 */
    for(uint8 column = left; column <= right; column++)
    {
        if(row[column] == 0)
        {
            black_run++;
            if(black_run > CONFIG_LINE_MAX_BLACK_GAP)
            {
                return 0;
            }
        }
        else
        {
            black_run = 0;
        }
    }

    if(!use_reference)
    {
        return 1;
    }

    if(reference_left < 1 || reference_right > MT9V03X_W - 2 || reference_left >= reference_right)
    {
        return 0;
    }

    candidate_mid = (left + right) / 2;
    reference_mid = (reference_left + reference_right) / 2;
    candidate_width = right - left;
    reference_width = reference_right - reference_left;

    /* 中点突跳通常是误锁相邻赛道；宽度突跳通常是把围栏和赛道合成了一条线。 */
    if(line_abs_diff(candidate_mid, reference_mid) > CONFIG_LINE_MAX_MID_JUMP)
    {
        return 0;
    }
    if(line_abs_diff(candidate_width, reference_width) > CONFIG_LINE_MAX_WIDTH_JUMP)
    {
        return 0;
    }
    return 1;
}
/* LINE_FILTER_TEST_END */

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
    uint8 left_point;
    uint8 right_point;
    uint8 current_reference_left = left_jidian;
    uint8 current_reference_right = right_jidian;
    uint8 current_reference_ready = 0;

    /* 每帧重新统计可信行；锁定状态和上一帧边线历史继续保留。 */
    line_valid_count = 0;
    for(uint8 i = 0; i < MT9V03X_H; i++)
    {
        line_valid_list[i] = 0;
    }

    /* 首次识别从极点起步；锁定后从上一帧同一赛道的起始行继续搜索。 */
    if(track_history_ready)
    {
        left_point = previous_left_line_list[CONFIG_SEARCH_START_LINE - 1];
        right_point = previous_right_line_list[CONFIG_SEARCH_START_LINE - 1];
    }
    else
    {
        left_point = left_jidian;
        right_point = right_jidian;
    }

    for(uint8 i = CONFIG_SEARCH_START_LINE - 1; i > CONFIG_SEARCH_END_LINE; i--)
    {
        uint8 left_search_judge = 0;
        uint8 mid_start_left_search_judge = 0;
        uint8 left_found = 0;
        uint8 right_found = 0;
        uint8 row_valid = 0;

        /* 左边线先在上一可信位置附近搜索。 */
        for(uint8 j = left_point; j < left_point + CONFIG_LEFT_SEARCH_RIGHT; j++)
        {
            if(index[i][j - 1] == 0 && index[i][j] == 255 && index[i][j + 1] == 255)
            {
                left_point = j;
                left_found = 1;
                break;
            }
            else if(j == MT9V03X_W - 2)
            {
                left_point = MT9V03X_W - 5;
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
            for(uint8 j = left_point; j > left_point - CONFIG_LEFT_SEARCH_LEFT; j--)
            {
                if(index[i][j - 1] == 0 && index[i][j] == 255 && index[i][j + 1] == 255 && j < MT9V03X_W - 5)
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
        /* 只允许未锁定的首帧从图像中心补搜，防止锁定后跳到相邻赛道。 */
        if(mid_start_left_search_judge == 1 && !track_history_ready)
        {
            for(uint8 j = CONFIG_MID_WIDTH; j > 0; j--)
            {
                if(index[i][j - 1] == 0 && index[i][j] == 255 && index[i][j + 1] == 255)
                {
                    left_point = j;
                    left_found = 1;
                    break;
                }
                else if(j == 1)
                {
                    left_point = 1;
                    break;
                }
            }
        }

        /* 右边线按相反方向在上一可信位置附近搜索。 */
        uint8 right_search_judge = 0;
        uint8 mid_start_right_search_judge = 0;
        for(uint8 j = right_point; j > right_point - CONFIG_RIGHT_SEARCH_LEFT; j--)
        {
            if(index[i][j - 1] == 255 && index[i][j] == 255 && index[i][j + 1] == 0)
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
            for(uint8 j = right_point; j < right_point + CONFIG_RIGHT_SEARCH_RIGHT; j++)
            {
                if(index[i][j - 1] == 255 && index[i][j] == 255 && index[i][j + 1] == 0 && j > 4)
                {
                    right_point = j;
                    right_found = 1;
                    break;
                }
                else if(j == MT9V03X_W - 2)
                {
                    right_point = MT9V03X_W - 2;
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
        if(mid_start_right_search_judge == 1 && !track_history_ready)
        {
            for(uint8 j = CONFIG_MID_WIDTH; j < MT9V03X_W - 1; j++)
            {
                if(index[i][j - 1] == 255 && index[i][j] == 255 && index[i][j + 1] == 0)
                {
                    right_point = j;
                    right_found = 1;
                    break;
                }
                else if(j == MT9V03X_W - 2)
                {
                    right_point = MT9V03X_W - 2;
                    break;
                }
            }
        }

        /* 两侧都找到后，先检查本帧逐行连续性，再检查上一帧同一行。 */
        if(left_found && right_found)
        {
            row_valid = line_candidate_is_valid(
                index[i], left_point, right_point,
                current_reference_left, current_reference_right,
                current_reference_ready);

            if(row_valid && track_history_ready)
            {
                row_valid = line_candidate_is_valid(
                    index[i], left_point, right_point,
                    previous_left_line_list[i], previous_right_line_list[i], 1);
            }
        }

        if(row_valid)
        {
            /* 只有通过双参考过滤的候选才能推进本帧搜索参考。 */
            current_reference_left = left_point;
            current_reference_right = right_point;
            current_reference_ready = 1;
            line_valid_list[i] = 1;
            line_valid_count++;
        }
        else
        {
            /* 无效行不参与控制，只为下一行搜索保留最近可信位置。 */
            if(current_reference_ready)
            {
                left_point = current_reference_left;
                right_point = current_reference_right;
            }
            else if(track_history_ready)
            {
                left_point = previous_left_line_list[i];
                right_point = previous_right_line_list[i];
            }
            else
            {
                left_point = left_jidian;
                right_point = right_jidian;
            }
        }

        left_line_list[i] = Limit_uint8(1, left_point, MT9V03X_W - 2);
        right_line_list[i] = Limit_uint8(1, right_point, MT9V03X_W - 2);
        mid_line_list[i] = Limit_uint8(1, (left_line_list[i] + right_line_list[i]) / 2, MT9V03X_W - 2);
    }

    /* 每帧保存完整边线。首次得到足够可信行后永久锁定，运行中不自动换道。 */
    for(uint8 i = 0; i < MT9V03X_H; i++)
    {
        previous_left_line_list[i] = left_line_list[i];
        previous_right_line_list[i] = right_line_list[i];
    }
    if(line_valid_count >= CONFIG_LINE_MIN_VALID_ROWS)
    {
        track_history_ready = 1;
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

/* ==================== 加权中线计算 ==================== */

/** 各图像行的中线权重，数值来自 config.h。 */
uint8 mid_weight_list[120]=CONFIG_MID_WEIGHT_LIST;
uint8 final_mid_line = CONFIG_MID_WIDTH;
uint8 last_mid_line = CONFIG_MID_WIDTH;
/**
 * @brief 计算供转向和弯道降速使用的加权中线。
 * @return 加权并进行一阶滤波后的中线横坐标。
 * @note 权重较大的行对结果影响更强；上一帧结果占 0.2，当前帧占 0.8，
 *       用于抑制识别抖动，同时保留主要转向响应。
 */
uint8 find_mid_line_weight(void)
{
    uint8 mid_line_value;
    uint8 mid_line;
    uint32 weight_midline_sum = 0;
    uint32 weight_sum = 0;

    /* 有效行太少时保持上一帧控制中线，避免偶发干扰直接打舵。 */
    if(line_valid_count < CONFIG_LINE_MIN_VALID_ROWS)
    {
        return last_mid_line;
    }

    for(uint8 i = MT9V03X_H - 1; i > CONFIG_SEARCH_END_LINE; i--)
    {
        if(line_valid_list[i])
        {
            weight_midline_sum += mid_line_list[i] * mid_weight_list[i];
            weight_sum += mid_weight_list[i];
        }
    }

    /* 防止配置异常或有效行权重全为零时发生除零。 */
    if(weight_sum == 0)
    {
        return last_mid_line;
    }

    mid_line = (uint8)(weight_midline_sum / weight_sum);
    mid_line_value = (uint8)(last_mid_line * 0.2f + mid_line * 0.8f);
    last_mid_line = mid_line_value;
    return mid_line_value;
}
