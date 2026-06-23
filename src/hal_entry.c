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
#include "motor_can.h"
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

extern sensor_t sensor;
static uint8_t g_image_rgb565_sdram_buffer[CAM_WIDTH * CAM_HEIGHT * 2] BSP_PLACE_IN_SECTION(".ospi1_cs0_noinit") BSP_ALIGN_VARIABLE(8);
static uint8_t g_display_rgb565_sdram_buffer[CAM_WIDTH * CAM_HEIGHT * 2] BSP_PLACE_IN_SECTION(".ospi1_cs0_noinit") BSP_ALIGN_VARIABLE(8);
static uint8_t g_lcd_rgb565_sdram_buffer[CAM_WIDTH * CAM_HEIGHT * 2] BSP_PLACE_IN_SECTION(".ospi1_cs0_noinit") BSP_ALIGN_VARIABLE(8);
static det_box_t g_detect_boxes[MAX_BOXES];
static rt_int32_t g_detect_box_num = 0;
static struct rt_mutex g_detect_lock;
static struct rt_semaphore g_display_sem;
static rt_bool_t g_display_sem_ready = RT_FALSE;

extern d2_device *d2_handle_obj_get(void);
extern d2_renderbuffer *d2_renderbuffer_get(void);

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
{
    static det_box_t pool[DETECT_POOL_SIZE];

    RT_UNUSED(parameter);

    while (1)
    {
        int8_t *output_p5;
        int8_t *output_p4;
        int16_t total = 0;
        int32_t kept;
        int32_t out_n;

        sensor_snapshot(&sensor, g_image_rgb565_sdram_buffer, 0);

        rgb565_to_rgb_hwc_int8_resize_192((const uint16_t *)g_image_rgb565_sdram_buffer,
                                          CAM_WIDTH,
                                          CAM_HEIGHT,
                                          GetModelInputPtr_net1_serving_default_images_0());

        RunModel_net1(false);

        output_p5 = GetModelOutputPtr_net1_PartitionedCall_1_70275();
        output_p4 = GetModelOutputPtr_net1_PartitionedCall_0_70286();

        total += decode_output_layer_int8_hwc(output_p4,
                                              GRID_SIZE_P4,
                                              0,
                                              OUTPUT_P4_SCALE,
                                              OUTPUT_P4_ZERO_POINT,
                                              LCD_WIDTH,
                                              LCD_HEIGHT,
                                              CONF_THRESH,
                                              pool + total,
                                              (int16_t)(sizeof(pool) / sizeof(pool[0])) - total);

        total += decode_output_layer_int8_hwc(output_p5,
                                              GRID_SIZE_P5,
                                              1,
                                              OUTPUT_P5_SCALE,
                                              OUTPUT_P5_ZERO_POINT,
                                              LCD_WIDTH,
                                              LCD_HEIGHT,
                                              CONF_THRESH,
                                              pool + total,
                                              (int16_t)(sizeof(pool) / sizeof(pool[0])) - total);

        kept = nms_filter(pool, total, NMS_THRESH);
        out_n = MIN(kept, MAX_BOXES);

        rt_mutex_take(&g_detect_lock, RT_WAITING_FOREVER);
        rt_memcpy(g_display_rgb565_sdram_buffer, g_image_rgb565_sdram_buffer, sizeof(g_display_rgb565_sdram_buffer));
        if (out_n > 0)
        {
            rt_memcpy(g_detect_boxes, pool, out_n * sizeof(det_box_t));
        }
        g_detect_box_num = out_n;
        rt_mutex_release(&g_detect_lock);

        if (g_display_sem_ready)
        {
            rt_sem_release(&g_display_sem);
        }
        motor_can_set_speed(0x02,
                                MOTOR_CAN_DIR_CW,
                                20,
                                5,
                                MOTOR_CAN_SYNC_DISABLE);
        led_status = !led_status;
        rt_pin_write(LED_PIN, led_status ? PIN_HIGH : PIN_LOW);
    }
}

static void app_display_thread_entry(void *parameter)
{
    static det_box_t local_boxes[MAX_BOXES];
    int32_t local_box_num;
    d2_width thickness = 1;
    uint32_t argb = 0xFF00FF00;

    RT_UNUSED(parameter);

    while (1)
    {
        rt_sem_take(&g_display_sem, RT_WAITING_FOREVER);

        rt_mutex_take(&g_detect_lock, RT_WAITING_FOREVER);
        rt_memcpy(g_lcd_rgb565_sdram_buffer, g_display_rgb565_sdram_buffer, sizeof(g_lcd_rgb565_sdram_buffer));
        local_box_num = g_detect_box_num;
        if (local_box_num > 0)
        {
            rt_memcpy(local_boxes, g_detect_boxes, local_box_num * sizeof(det_box_t));
        }
        rt_mutex_release(&g_detect_lock);

        lcd_draw_camera_with_boxes(0,
                                   0,
                                   g_lcd_rgb565_sdram_buffer,
                                   CAM_WIDTH,
                                   CAM_HEIGHT,
                                   argb,
                                   thickness,
                                   local_boxes,
                                   local_box_num);
    }
}

void hal_entry(void)
{
    rt_thread_t detect_thread;
    rt_thread_t display_thread;
    int16_t status = FSP_SUCCESS;

    sensor_init();
    sensor_reset();
    sensor_set_pixformat(PIXFORMAT_RGB565);
    sensor_set_framesize(FRAMESIZE_VGA);

    status = RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg);
    if (status != FSP_SUCCESS)
    {
        LOG_E("Failed to start NPU");
        return;
    }

    if (rt_mutex_init(&g_detect_lock, "detlock", RT_IPC_FLAG_PRIO) != RT_EOK)
    {
        LOG_E("detect lock init failed");
        return;
    }

    if (rt_sem_init(&g_display_sem, "dispsem", 0, RT_IPC_FLAG_FIFO) != RT_EOK)
    {
        LOG_E("display sem init failed");
        return;
    }
    g_display_sem_ready = RT_TRUE;

    if (can_app_init(motor_can_get_payload) != RT_EOK)
    {
        LOG_E("CAN init failed");
        return;
    }

    detect_thread = rt_thread_create("detect",
                                     app_detect_thread_entry,
                                     RT_NULL,
                                     DETECT_THREAD_STACK_SIZE,
                                     11,
                                     THREAD_TIMESLICE);
    if (detect_thread == RT_NULL)
    {
        LOG_E("detect thread create failed");
        return;
    }

    display_thread = rt_thread_create("display",
                                      app_display_thread_entry,
                                      RT_NULL,
                                      DISPLAY_THREAD_STACK_SIZE,
                                      13,
                                      THREAD_TIMESLICE);
    if (display_thread == RT_NULL)
    {
        LOG_E("display thread create failed");
        return;
    }

    rt_thread_startup(detect_thread);
    rt_thread_startup(display_thread);
}
