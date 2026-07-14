#include <rtthread.h>
#include "hal_data.h"
#include <rtdevice.h>

#include <board.h>
#include "sensor.h"
#include <lcd_port.h>
#include "model_net1.h"
#include "pmu_ethosu.h"
#include "yolo_rtthread.h"
#include "can_app.h"
#include "motor_uart.h"
 #include "lcd_display.h"
#define DBG_TAG     "main"
#define DBG_LVL     DBG_INFO
#include "rtdbg.h"

#define LED_PIN     BSP_IO_PORT_00_PIN_12

#define CAM_WIDTH   640
#define CAM_HEIGHT  480
#define DETECT_POOL_SIZE 540
#define DETECT_THREAD_STACK_SIZE 4096
#define DISPLAY_THREAD_STACK_SIZE 4096
#define THREAD_TIMESLICE 10

static volatile bool led_status = false;
int16_t ii=0;
extern sensor_t sensor;
static uint8_t g_image_rgb565_sdram_buffer[CAM_WIDTH * CAM_HEIGHT * 2] BSP_PLACE_IN_SECTION(".ospi1_cs0_noinit") BSP_ALIGN_VARIABLE(8);
static uint8_t g_display_rgb565_sdram_buffer[CAM_WIDTH * CAM_HEIGHT * 2] BSP_PLACE_IN_SECTION(".ospi1_cs0_noinit") BSP_ALIGN_VARIABLE(8);
static uint8_t g_lcd_rgb565_sdram_buffer[CAM_WIDTH * CAM_HEIGHT * 2] BSP_PLACE_IN_SECTION(".ospi1_cs0_noinit") BSP_ALIGN_VARIABLE(8);
static det_box_t g_detect_boxes[MAX_BOXES];
static rt_int32_t g_detect_box_num = 0;
static struct rt_mutex g_detect_lock;
static struct rt_semaphore g_display_sem;
static rt_bool_t g_display_sem_ready = RT_FALSE;
int32_t record_pos[4];
static uint8_t  record_update_mask = 0;
static rt_bool_t record_all_updated = RT_FALSE;

static uint8_t record_count = 0;
extern d2_device *d2_handle_obj_get(void);
extern d2_renderbuffer *d2_renderbuffer_get(void);

static void record_pos_mark_updated(uint8_t idx)
 {
     if (idx >= 4)
         return;

     record_update_mask |= (1U << idx); // 这个下标已更新过

     if (record_update_mask == 0x0F)   // 1111，四个都至少更新过一次
         record_all_updated = RT_TRUE;
 }

static uint16_t app_argb8888_to_rgb565(uint32_t argb)
{
    uint8_t r = (uint8_t)((argb >> 16) & 0xFF);
    uint8_t g = (uint8_t)((argb >> 8) & 0xFF);
    uint8_t b = (uint8_t)(argb & 0xFF);

    return (uint16_t)(((uint16_t)(r & 0xF8) << 8) |
                      ((uint16_t)(g & 0xFC) << 3) |
                      ((uint16_t)b >> 3));
}

static uint16_t app_class_color_rgb565(uint8_t cls, uint32_t fallback_argb)
{
    static const uint16_t class_colors[CLASS_NUM] =
    {
        0xFFE0,
        0x07E0,
        0xF81F,
    };

    if (cls < CLASS_NUM)
    {
        return class_colors[cls];
    }

    return app_argb8888_to_rgb565(fallback_argb);
}
static void app_handle_motor_logic(const det_box_t *box)
 {

     int32_t x1;
     int32_t y1;
     int32_t x2;
     int32_t y2;
     int32_t x;
     int32_t y;
     int16_t cls;
     static int8_t y_state_prev = 0;
     static int8_t x_state_prev = 0;
     static int8_t y_state_cur = 0;
     static int8_t x_state_cur = 0;
     if (box == RT_NULL)return;
     x1 = CLAMP(box->x1, 0, LCD_WIDTH - 1);
     y1 = CLAMP(box->y1, 0, LCD_HEIGHT - 1);
     x2 = CLAMP(box->x2, 0, LCD_WIDTH - 1);
     y2 = CLAMP(box->y2, 0, LCD_HEIGHT - 1);
     x = (x1 + x2) / 2;
     y = (y1 + y2) / 2;
     cls = box->cls;
     if(record_all_updated==RT_TRUE){
            record_update_mask = 0;
            static uint8_t k = 0;
            k++;
             int32_t target11 = (record_pos[2] + record_pos[3]) * 25 / 1024;
             int32_t target22 = (record_pos[0] + record_pos[1]) * 25 / 1024;
             rt_uint8_t dir1 = (target11 >= 0) ? MOTOR_UART_DIR_CW : MOTOR_UART_DIR_CCW;
             rt_uint8_t dir2 = (target22 >= 0) ? MOTOR_UART_DIR_CW : MOTOR_UART_DIR_CCW;
             rt_uint32_t pulse1 = (target11 >= 0) ? (rt_uint32_t)target11 : (rt_uint32_t)(-target11);
             rt_uint32_t pulse2 = (target22 >= 0) ? (rt_uint32_t)target22 : (rt_uint32_t)(-target22);
             lcd_display_show_number(pulse1);
             rt_thread_mdelay(100);
             motor_uart_set_position(1, dir1, 10, 1, pulse1,MOTOR_UART_POS_MODE_ABSOLUTE, MOTOR_UART_SYNC_DISABLE);
             rt_thread_mdelay(100);
             motor_uart_set_position(2, dir2, 10, 1, pulse2,MOTOR_UART_POS_MODE_ABSOLUTE, MOTOR_UART_SYNC_DISABLE);
             rt_thread_mdelay(5000);
             lcd_display_show_number(pulse2);
             motor_uart_set_position(1, MOTOR_UART_DIR_CCW, 50, 1, 27799,MOTOR_UART_POS_MODE_RELATIVE, MOTOR_UART_SYNC_DISABLE);
             rt_thread_mdelay(100);
             motor_uart_set_position(2, MOTOR_UART_DIR_CW, 50, 1, 6400,MOTOR_UART_POS_MODE_RELATIVE, MOTOR_UART_SYNC_DISABLE);
             rt_thread_mdelay(10000);
             motor_uart_set_position(3, MOTOR_UART_DIR_CCW, 50, 1, 17000,MOTOR_UART_POS_MODE_RELATIVE, MOTOR_UART_SYNC_DISABLE);
             rt_thread_mdelay(15000);
             motor_uart_set_position(3, MOTOR_UART_DIR_CW, 50, 1, 17000,MOTOR_UART_POS_MODE_RELATIVE, MOTOR_UART_SYNC_DISABLE);
             rt_thread_mdelay(200);
             if(k==4||k==8)motor_uart_set_position(2, MOTOR_UART_DIR_CW, 50, 1, 27000,MOTOR_UART_POS_MODE_RELATIVE, MOTOR_UART_SYNC_DISABLE);
             rt_thread_mdelay(5000);
             if(k==4||k==8)motor_uart_set_position(1, MOTOR_UART_DIR_CW, 50, 1, 27799,MOTOR_UART_POS_MODE_RELATIVE, MOTOR_UART_SYNC_DISABLE);
             if(4<k&&k<8)motor_uart_set_position(1, MOTOR_UART_DIR_CW, 50, 1, 55000,MOTOR_UART_POS_MODE_RELATIVE, MOTOR_UART_SYNC_DISABLE);
             rt_thread_mdelay(15000);
             rt_memset(record_pos, 0, sizeof(record_pos));
             record_update_mask = 0;
             record_all_updated = RT_FALSE;
             ii = 0;
             x_state_prev = 0;
             x_state_cur = 0;
             y_state_prev = 0;
             y_state_cur = 0;}
          else if(ii==0&&record_all_updated == RT_FALSE){
             if(x>240){
                 x_state_prev=x_state_cur;x_state_cur=1;
                 if(x_state_cur*x_state_prev==-1){if (motor_uart_read_position(0x01, &record_pos[2]) == RT_EOK){
                         record_pos_mark_updated(2);
                         led_status = !led_status;
                                rt_pin_write(LED_PIN, led_status ? PIN_HIGH : PIN_LOW);}
                 else{rt_thread_mdelay(10);}}
                 motor_uart_set_speed(0x01,MOTOR_UART_DIR_CCW,20,5,MOTOR_UART_SYNC_DISABLE);}
             else if(x<239){x_state_prev=x_state_cur;x_state_cur=-1;
                if(x_state_cur*x_state_prev==-1){
                  if (motor_uart_read_position(0x01, &record_pos[3]) == RT_EOK){record_pos_mark_updated(3);led_status = !led_status;
                     rt_pin_write(LED_PIN, led_status ? PIN_HIGH : PIN_LOW);}else
                  {rt_thread_mdelay(10);}}
                  motor_uart_set_speed(0x01,MOTOR_UART_DIR_CW,20,5,MOTOR_UART_SYNC_DISABLE);}
             else motor_uart_set_speed(0x01,MOTOR_UART_DIR_CW,0,5,MOTOR_UART_SYNC_DISABLE);
        }else if(ii==2&&record_all_updated == RT_FALSE){if(y>400){y_state_prev=y_state_cur;y_state_cur=1;
              if(y_state_cur*y_state_prev==-1){
               if (motor_uart_read_position(0x02, &record_pos[0]) == RT_EOK)
              {record_pos_mark_updated(0);
               led_status = !led_status;
               rt_pin_write(LED_PIN, led_status ? PIN_HIGH : PIN_LOW);}
               else{rt_thread_mdelay(10);}}
                  motor_uart_set_speed(0x02,MOTOR_UART_DIR_CW,20,5,MOTOR_UART_SYNC_DISABLE);}
                     else if(y<399){y_state_prev=y_state_cur;y_state_cur=-1;
                       if(y_state_cur*y_state_prev==-1){if (motor_uart_read_position(0x02, &record_pos[1]) == RT_EOK){record_pos_mark_updated(1);
                    led_status = !led_status;rt_pin_write(LED_PIN, led_status ? PIN_HIGH : PIN_LOW);}
                    else{rt_thread_mdelay(10); }}motor_uart_set_speed(0x02,MOTOR_UART_DIR_CCW,20,5,MOTOR_UART_SYNC_DISABLE);}
                     else motor_uart_set_speed(0x02,MOTOR_UART_DIR_CW,0,5,MOTOR_UART_SYNC_DISABLE);
 }}
static void app_lcd_draw_hline(uint16_t *fb, int16_t x1, int16_t x2, int16_t y, uint16_t color)
{
    if ((fb == RT_NULL) || (y < 0) || (y >= LCD_HEIGHT))
    {
        return;
    }

    x1 = CLAMP(x1, 0, LCD_WIDTH - 1);
    x2 = CLAMP(x2, 0, LCD_WIDTH - 1);
    if (x2 < x1)
    {
        int16_t t = x1;
        x1 = x2;
        x2 = t;
    }

    uint16_t *row = fb + (y * LCD_WIDTH);
    for (int16_t x = x1; x <= x2; x++)
    {
        row[x] = color;
    }
}

static void app_lcd_draw_vline(uint16_t *fb, int16_t x, int16_t y1, int16_t y2, uint16_t color)
{
    if ((fb == RT_NULL) || (x < 0) || (x >= LCD_WIDTH))
    {
        return;
    }

    y1 = CLAMP(y1, 0, LCD_HEIGHT - 1);
    y2 = CLAMP(y2, 0, LCD_HEIGHT - 1);
    if (y2 < y1)
    {
        int16_t t = y1;
        y1 = y2;
        y2 = t;
    }

    for (int16_t y = y1; y <= y2; y++)
    {
        fb[y * LCD_WIDTH + x] = color;
    }
}

static void app_lcd_draw_rect_cpu(const det_box_t *box, uint32_t fallback_argb, d2_width thickness)
{
    uint16_t *fb = (uint16_t *)&fb_background[0];
    uint16_t color;
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    int16_t x;
    int16_t y;

    ii++;
    if(ii==4)ii=0;
    if (box == RT_NULL)
    {
        return;
    }

    color = app_class_color_rgb565(box->cls, fallback_argb);

    if (thickness < 1)
    {
        thickness = 1;
    }
    if (thickness > 8)
    {
        thickness = 8;
    }

    x1 = CLAMP(box->x1, 0, LCD_WIDTH - 1);
    y1 = CLAMP(box->y1, 0, LCD_HEIGHT - 1);
    x2 = CLAMP(box->x2, 0, LCD_WIDTH - 1);
    y2 = CLAMP(box->y2, 0, LCD_HEIGHT - 1);

    if ((x2 <= x1) || (y2 <= y1))
    {
        return;
    }

    for (d2_width t = 0; t < thickness; t++)
    {
        app_lcd_draw_hline(fb, x1, x2, y1 + t, color);
        app_lcd_draw_hline(fb, x1, x2, y2 - t, color);
        app_lcd_draw_vline(fb, x1 + t, y1, y2, color);
        app_lcd_draw_vline(fb, x2 - t, y1, y2, color);
    }
}

static void lcd_draw_camera_with_boxes(int32_t x, int32_t y,
                                       const void *p, int32_t xSize, int32_t ySize,
                                       uint32_t argb, d2_width thickness,
                                       const det_box_t *boxes, int32_t box_num)
{
    d2_device *hdl = d2_handle_obj_get();
    d2_renderbuffer *renderbuffer = d2_renderbuffer_get();
    uint32_t mode_src = d2_mode_rgb565;

    if ((hdl == RT_NULL) || (renderbuffer == RT_NULL) || (p == RT_NULL))
    {
        return;
    }

    if (thickness < 1)
    {
        thickness = 1;
    }

    if (box_num < 0)
    {
        box_num = 0;
    }
    if (box_num > MAX_BOXES)
    {
        box_num = MAX_BOXES;
    }

    d2_framebuffer(hdl, (uint16_t *)&fb_background[0], LCD_WIDTH, LCD_WIDTH, LCD_HEIGHT, mode_src);
    d2_selectrenderbuffer(hdl, renderbuffer);
    d2_cliprect(hdl, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    d2_setblitsrc(hdl, (void *)p, xSize, xSize, ySize, mode_src);
    d2_blitcopy(hdl, xSize, ySize, 0, 0,
                D2_FIX(LCD_WIDTH), D2_FIX(LCD_HEIGHT),
                D2_FIX(x), D2_FIX(y), 0);
    d2_executerenderbuffer(hdl, renderbuffer, 0);
    d2_flushframe(hdl);

    if (boxes != RT_NULL)
    {
        for (int i = 0; i < box_num; i++)
        {
            app_lcd_draw_rect_cpu(&boxes[i], argb, thickness);
        }
    }
}

static rt_err_t app_can_get_payload(rt_uint8_t data[8])
{
    rt_err_t ret;
    int score_x100;

    if (data == RT_NULL)
    {
        return -RT_ERROR;
    }

    rt_memset(data, 0, 8);

    ret = rt_mutex_take(&g_detect_lock, RT_WAITING_FOREVER);
    if (ret != RT_EOK)
    {
        return ret;
    }



    rt_mutex_release(&g_detect_lock);

    return RT_EOK;
}

static void app_detect_thread_entry(void *parameter)
{ /* 本地检测框池，保存一帧图像经后处理得到的全部候选框。 */
    static det_box_t pool[DETECT_POOL_SIZE];
    /* 当前线程没有使用入口参数。 */
    RT_UNUSED(parameter);
    /* 检测线程常驻运行，持续执行“采集 -> 预处理 -> 推理 -> 后处理 -> 发布结果”的循环。 */
    while (1)
    {/* P5 输出层指针。 */
        int8_t *output_p5;
        /* P4 输出层指针，当前版本保留变量但未启用。 */
        int8_t *output_p4;
        /* 记录所有解码出来的候选框数量。 */
        int16_t total = 0;
        /* NMS 之后剩余的框数量。 */
        int32_t kept;
        /* 真正发布到共享区的框数量，受 MAX_BOXES 限制。 */
        int32_t out_n;
        /* 从相机抓拍一帧 VGA RGB565 图像到原始缓冲区。 */
        sensor_snapshot(&sensor, g_image_rgb565_sdram_buffer, 0);
        /* 将 RGB565 图像缩放并量化为模型需要的 192x192 int8 HWC 输入格式。 */
        rgb565_to_rgb_hwc_int8_resize_192((const uint16_t *)g_image_rgb565_sdram_buffer,CAM_WIDTH,CAM_HEIGHT,GetModelInputPtr_net1_serving_default_images_0());
        /* 调用已经部署到 Ethos-U NPU 的模型执行一次推理。 */
        RunModel_net1(false);
        /* 取出 P5 输出分支地址。 */
        output_p5 = GetModelOutputPtr_net1_PartitionedCall_1_70275();
        /* 对当前启用的 P5 分支输出执行量化后处理，得到候选目标框。 */
        total += decode_output_layer_int8_hwc(output_p5,GRID_SIZE_P5,1,OUTPUT_P5_SCALE,OUTPUT_P5_ZERO_POINT,LCD_WIDTH,LCD_HEIGHT,CONF_THRESH,pool + total,(int16_t)(sizeof(pool) / sizeof(pool[0])) - total);
        /* 对候选框执行 NMS 去重。 */
        kept = nms_filter(pool, total, NMS_THRESH);
        /* 共享区最多只保留 MAX_BOXES 个结果。 */
        out_n = MIN(kept, MAX_BOXES);
        /* 进入临界区，开始向显示线程发布最新图像和检测结果。 */
        rt_mutex_take(&g_detect_lock, RT_WAITING_FOREVER);
        /* 将当前原始图像复制到显示共享缓冲区。 */
        rt_memcpy(g_display_rgb565_sdram_buffer, g_image_rgb565_sdram_buffer, sizeof(g_display_rgb565_sdram_buffer));
        /* 当有检测结果时，将前 out_n 个框复制到共享数组。 */
        if (out_n > 0){rt_memcpy(g_detect_boxes, pool, out_n * sizeof(det_box_t));}
        /* 更新当前共享检测框数量。 */
        g_detect_box_num = out_n;
        /* 退出临界区，允许显示线程读取结果。 */
        rt_mutex_release(&g_detect_lock);
        /* 如果显示信号量已经初始化，则唤醒显示线程消费这一帧结果。 */
        if (g_display_sem_ready){rt_sem_release(&g_display_sem);}
    }
}
static void app_display_thread_entry(void *parameter)
{ /* 显示线程的本地检测框副本，避免直接在共享数组上做显示和控制。 */
    static det_box_t local_boxes[MAX_BOXES];
    /* 当前本地副本中的检测框数量。 */
    int32_t local_box_num;
    /* 叠框线宽。 */
    d2_width thickness = 1;
    /* 默认叠框颜色，ARGB 绿色。 */
    uint32_t argb = 0xFF00FF00;
    /* 当前线程没有使用入口参数。 */
    RT_UNUSED(parameter);
    /* 系统上电后先等待一段时间，给外设初始化和机构稳定预留时间。 */
    rt_thread_mdelay(2000);
    /* 启动阶段先让 Z 轴重复走几次到预设绝对位置，用于样机归位或消除机械间隙。 */
    for (int16_t var = 0; var < 10; ++var)
    {/* 让电机 3 运动到设定绝对位置。 */
        motor_uart_set_position(3, MOTOR_UART_DIR_CW, 50, 1, 17000, MOTOR_UART_POS_MODE_ABSOLUTE, MOTOR_UART_SYNC_DISABLE);
        /* 相邻归位命令之间留短延时。 */
        rt_thread_mdelay(100);}
    /* 初始化舵机 PWM 输出。 */servo_pwm_init();
    /* 给 PWM 初始化留一点时间。 */rt_thread_mdelay(100);
    /* 将舵机打到一个固定起始角度。 */servo_pwm_set_angle(20);
    /* 显示线程常驻运行，等待检测结果并驱动显示/动作逻辑。 */
    while (1)
    { /* 只有在本轮采样尚未完成时，才继续等待并消费新的视觉结果。 */
        if (record_all_updated == RT_FALSE){
            /* 阻塞等待检测线程发布新的一帧结果。 */
            rt_sem_take(&g_display_sem, RT_WAITING_FOREVER);
            /* 进入临界区，读取检测线程共享出来的图像和检测框。 */
            rt_mutex_take(&g_detect_lock, RT_WAITING_FOREVER);
            /* 把共享显示帧复制到本地 LCD 帧缓冲，避免后续绘制过程影响共享数据。 */
            rt_memcpy(g_lcd_rgb565_sdram_buffer, g_display_rgb565_sdram_buffer, sizeof(g_lcd_rgb565_sdram_buffer));
            /* 读取当前共享检测框数量。 */
            local_box_num = g_detect_box_num;
            /* 当存在检测结果时，把共享检测框复制到本地数组。 */
            if (local_box_num > 0){rt_memcpy(local_boxes, g_detect_boxes, local_box_num * sizeof(det_box_t));}
            /* 退出临界区，允许检测线程继续更新下一帧。 */
            rt_mutex_release(&g_detect_lock);
            /* 将本地图像和本地检测框一起绘制到 LCD。 */
            lcd_draw_camera_with_boxes(0,0,g_lcd_rgb565_sdram_buffer,CAM_WIDTH,CAM_HEIGHT,argb,thickness,local_boxes,local_box_num);}
        /* 当前策略只取第一个目标框参与运动控制。 */
        if (local_box_num > 0){
            /* 用优先目标框驱动视觉伺服/焊接动作逻辑。 */
            app_handle_motor_logic(&local_boxes[0]);}}}
void hal_entry(void)
{
    /* 检测线程句柄。 */
    rt_thread_t detect_thread;
    /* 显示线程句柄。 */
    rt_thread_t display_thread;
    /* FSP / NPU 初始化返回值。 */
    int16_t status = FSP_SUCCESS;
    /* 初始化摄像头驱动。 */
    sensor_init();
    /* 对摄像头做一次复位，确保寄存器回到已知状态。 */
    sensor_reset();
    /* 设置相机输出格式为 RGB565，和后续显示/预处理链路匹配。 */
    sensor_set_pixformat(PIXFORMAT_RGB565);
    /* 设置相机分辨率为 VGA。 */
    sensor_set_framesize(FRAMESIZE_VGA);
    /* 打开 Ethos-U NPU，为后续模型推理做准备。 */
    status = RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);
    /* NPU 启动失败则整个视觉链路无法工作，直接报错返回。 */
    if (status != FSP_SUCCESS)
    {LOG_E("Failed to start NPU");return;}
    /* 初始化检测结果互斥锁，保护检测线程与显示线程的共享数据。 */
    if (rt_mutex_init(&g_detect_lock, "detlock", RT_IPC_FLAG_PRIO) != RT_EOK)
    {/* 互斥锁初始化失败则无法安全共享检测数据。 */LOG_E("detect lock init failed");return;}
    /* 初始化显示信号量，用于检测线程通知显示线程“有新结果可消费”。 */
    if (rt_sem_init(&g_display_sem, "dispsem", 0, RT_IPC_FLAG_FIFO) != RT_EOK)
    {/* 信号量初始化失败则线程之间无法按预期同步。 */LOG_E("display sem init failed");return;}
    /* 标记显示信号量已经可用。 */
    g_display_sem_ready = RT_TRUE;
    /* 初始化电机串口控制模块，后续所有运动控制都依赖这个接口。 */
    if (motor_uart_init(RT_NULL) != RT_EOK){/* 电机串口初始化失败则无法执行任何机构动作。 */LOG_E("UART motor init failed");return;}
    /* 创建检测线程，负责相机采集、模型推理和后处理。 */
    detect_thread = rt_thread_create("detect",app_detect_thread_entry,RT_NULL,DETECT_THREAD_STACK_SIZE,11,THREAD_TIMESLICE);
    /* 检测线程创建失败则直接退出启动流程。 */
    if (detect_thread == RT_NULL){LOG_E("detect thread create failed");return;}
    /* 创建显示线程，负责画框显示和执行机构联动。 */
    display_thread = rt_thread_create("display",app_display_thread_entry,RT_NULL,DISPLAY_THREAD_STACK_SIZE,13,THREAD_TIMESLICE);
    /* 显示线程创建失败则无法进入完整运行状态。 */
    if (display_thread == RT_NULL){LOG_E("display thread create failed");return;}
    /* 启动检测线程。 */
    rt_thread_startup(detect_thread);
    /* 启动显示线程。 */
    rt_thread_startup(display_thread);
}
