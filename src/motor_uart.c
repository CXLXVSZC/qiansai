#include <rtthread.h>
#include <rtdevice.h>
#include "motor_uart.h"

#define MOTOR_UART_CMD_SPEED_MODE   0xF6U
#define MOTOR_UART_CHECK_BYTE       0x6BU
#define MOTOR_UART_DEFAULT_DEV      "uart5"

static rt_device_t g_motor_uart_dev = RT_NULL;
static struct rt_mutex g_motor_uart_lock;
static rt_bool_t g_motor_uart_lock_inited = RT_FALSE;

static rt_bool_t motor_uart_valid_direction(rt_uint8_t direction)
{
    return (direction == MOTOR_UART_DIR_CW) || (direction == MOTOR_UART_DIR_CCW);
}

static rt_bool_t motor_uart_valid_sync(rt_uint8_t sync_flag)
{
    return (sync_flag == MOTOR_UART_SYNC_DISABLE) || (sync_flag == MOTOR_UART_SYNC_ENABLE);
}

rt_err_t motor_uart_init(const char *uart_name)
{
    rt_err_t ret;

    if (uart_name == RT_NULL)
    {
        uart_name = MOTOR_UART_DEFAULT_DEV;
    }

    g_motor_uart_dev = rt_device_find(uart_name);
    if (g_motor_uart_dev == RT_NULL)
    {
        return -RT_ERROR;
    }

    ret = rt_device_open(g_motor_uart_dev, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_TX);
    if (ret != RT_EOK && ret != -RT_EBUSY)
    {
        return ret;
    }

    if (!g_motor_uart_lock_inited)
    {
        ret = rt_mutex_init(&g_motor_uart_lock, "mutx", RT_IPC_FLAG_PRIO);
        if (ret != RT_EOK)
        {
            return ret;
        }
        g_motor_uart_lock_inited = RT_TRUE;
    }

    return RT_EOK;
}

rt_err_t motor_uart_build_speed_frame(rt_uint8_t address,
                                      rt_uint8_t direction,
                                      rt_uint16_t speed,
                                      rt_uint8_t acceleration,
                                      rt_uint8_t sync_flag,
                                      rt_uint8_t frame[8])
{
    if (frame == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (!motor_uart_valid_direction(direction) || !motor_uart_valid_sync(sync_flag))
    {
        return -RT_EINVAL;
    }

    frame[0] = address;
    frame[1] = MOTOR_UART_CMD_SPEED_MODE;
    frame[2] = direction;
    frame[3] = (rt_uint8_t)((speed >> 8) & 0xFF);
    frame[4] = (rt_uint8_t)(speed & 0xFF);
    frame[5] = acceleration;
    frame[6] = sync_flag;
    frame[7] = MOTOR_UART_CHECK_BYTE;

    return RT_EOK;
}

rt_err_t motor_uart_set_speed(rt_uint8_t address,
                              rt_uint8_t direction,
                              rt_uint16_t speed,
                              rt_uint8_t acceleration,
                              rt_uint8_t sync_flag)
{
    rt_err_t ret;
    rt_uint8_t frame[8];
    rt_size_t sent;

    if (g_motor_uart_dev == RT_NULL)
    {
        ret = motor_uart_init(RT_NULL);
        if (ret != RT_EOK)
        {
            return ret;
        }
    }

    ret = motor_uart_build_speed_frame(address, direction, speed, acceleration, sync_flag, frame);
    if (ret != RT_EOK)
    {
        return ret;
    }

    rt_mutex_take(&g_motor_uart_lock, RT_WAITING_FOREVER);
    sent = rt_device_write(g_motor_uart_dev, 0, frame, sizeof(frame));
    rt_mutex_release(&g_motor_uart_lock);

    return (sent == sizeof(frame)) ? RT_EOK : -RT_ERROR;
}
