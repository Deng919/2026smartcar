/*********************************************************************************************************************
* TC264 Opensourec Library 即（TC264 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2022 SEEKFREE 逐飞科技
*
* 本文件是 TC264 开源库的一部分
*
* TC264 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
* 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
*
* 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
* 甚至没有隐含的适销性或适合特定用途的保证
* 更多细节请参见 GPL
*
* 您应该在收到本开源库的同时收到一份 GPL 的副本
* 如果没有，请参阅<https://www.gnu.org/licenses/>
*
* 额外注明：
* 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
* 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
* 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
*
* 文件名称          cpu0_main
* 公司名称          成都逐飞科技有限公司
* 版本信息          查看 libraries/doc 文件夹内 version 文件 版本说明
* 开发环境          ADS v1.10.2
* 适用平台          TC264D
* 店铺链接          https://seekfree.taobao.com/
*
* 修改记录
* 日期              作者                备注
* 2022-09-15       pudding            first version
********************************************************************************************************************/
#include "zf_common_headfile.h"
#include "config.h"
/*
 * CPU0 应用主循环。
 * 启动阶段初始化摄像头、显示、舵机、电机、编码器、按键和两个控制周期；
 * 运行阶段仅在 DMA 完成一帧后执行图像识别，控制环由周期中断独立运行。
 */
#pragma section all "cpu0_dsram"
/** KEY_1 启停计数，放置在 CPU0 DSRAM 中供主循环和中断共享。 */
uint8 start=0;

/**
 * @brief CPU0 应用入口。
 * @return 正常运行时不会返回。
 * @note 图像数组只在 mt9v03x_finish_flag 置位后复制，避免读取 DMA 正在写入的帧。
 */
int core0_main(void)
{
    // 记录上一帧显示状态，用于关闭显示时只清屏一次。
    uint8 last_image_display_enable = 1;

    /* ==================== 系统与外设初始化 ==================== */
    clock_init();                   // 获取时钟频率<务必保留>
    debug_init();                   // 初始化默认调试串口
    mt9v03x_init ();
    tft180_init ();
    Servo_Init();
    Motor_Init();
    Encoder_Init();
    key_init(2);
    pit_ms_init(CCU60_CH0,CONFIG_SERVO_CONTROL_PERIOD_MS);
    pit_ms_init(CCU60_CH1,CONFIG_MOTOR_CONTROL_PERIOD_MS);
    /* CPU0 外设就绪后再与其他核心同步。 */
    cpu_wait_event_ready();         // 等待所有核心初始化完毕
    while (TRUE)
    {
        /* ==================== 完整帧图像处理 ==================== */
        /* 只处理 DMA 已完成的图像帧，避免摄像头写入时读取缓冲区。 */
        if (mt9v03x_finish_flag)
        {
            memcpy(base_image[0],mt9v03x_image[0],MT9V03X_IMAGE_SIZE);
            // 复制完成后清零标志，等待下一帧 DMA 完成。
            mt9v03x_finish_flag = 0;
            /* 识别顺序固定为：阈值、二值化、起点、边线、中线。 */
            img_threshold = Ostu(base_image);
            set_image_twovalues(img_threshold);
            find_jidian(image);
            image_deal(image);
            final_mid_line = find_mid_line_weight();
            // 无论是否显示都完成图像识别，只跳过耗时的 TFT 绘制。
            uint8 display_enable = image_display_enable;
            if(display_enable)
            {
                draw_line();
                tft180_show_int(0,116,CONFIG_MID_WIDTH-final_mid_line,3);
                tft180_show_gray_image(0,0,image[0],MT9V03X_W,MT9V03X_H,128,81,0);
            }
            else if(last_image_display_enable)
            {
                tft180_clear();
            }
            last_image_display_enable = display_enable;
        }

    }
}

#pragma section all restore
