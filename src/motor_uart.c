#include <rtthread.h>
#include <rtdevice.h>
#include "motor_uart.h"


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

static void motor_uart_drain_rx(void)
  {
      rt_uint8_t dump[32];
      while (rt_device_read(g_motor_uart_dev, 0, dump, sizeof(dump)) > 0)
      {
      }
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

    ret = rt_device_open(g_motor_uart_dev, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
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

static rt_bool_t motor_uart_valid_pos_mode(rt_uint8_t mode)
{
    return (mode == MOTOR_UART_POS_MODE_RELATIVE) || (mode == MOTOR_UART_POS_MODE_ABSOLUTE);
}
/**
 * @brief 构建位置模式控制帧（13字节）
 * @param address       从机地址
 * @param direction     方向 (MOTOR_UART_DIR_CW/CCW)
 * @param speed         转速 (RPM, 16位)
 * @param acceleration  加速度档位 (0~255, 0表示无加减速)
 * @param pulses        脉冲数 (32位, 大端)
 * @param pos_mode      位置模式 (相对/绝对)
 * @param sync_flag     多机同步 (启用/禁用)
 * @param frame         输出帧缓冲区（至少13字节）
 * @return RT_EOK 成功，否则错误码
 */
rt_err_t motor_uart_build_position_frame(rt_uint8_t address,
                                         rt_uint8_t direction,
                                         rt_uint16_t speed,
                                         rt_uint8_t acceleration,
                                         rt_uint32_t pulses,
                                         rt_uint8_t pos_mode,
                                         rt_uint8_t sync_flag,
                                         rt_uint8_t frame[13])
{
    if (frame == RT_NULL)
        return -RT_EINVAL;

    if (!motor_uart_valid_direction(direction) ||
        !motor_uart_valid_sync(sync_flag) ||
        !motor_uart_valid_pos_mode(pos_mode))
    {
        return -RT_EINVAL;
    }

    frame[0] = address;
    frame[1] = MOTOR_UART_CMD_POS_MODE;
    frame[2] = direction;
    frame[3] = (rt_uint8_t)((speed >> 8) & 0xFF);
    frame[4] = (rt_uint8_t)(speed & 0xFF);
    frame[5] = acceleration;
    frame[6] = (rt_uint8_t)((pulses >> 24) & 0xFF);
    frame[7] = (rt_uint8_t)((pulses >> 16) & 0xFF);
    frame[8] = (rt_uint8_t)((pulses >> 8) & 0xFF);
    frame[9] = (rt_uint8_t)(pulses & 0xFF);
    frame[10] = pos_mode;
    frame[11] = sync_flag;
    frame[12] = MOTOR_UART_CHECK_BYTE;   // 固定校验

    return RT_EOK;
}



/**
 * @brief 设置电机位置控制（发送位置命令）
 * @param address       从机地址
 * @param direction     方向 (CW/CCW)
 * @param speed         转速 (RPM)
 * @param acceleration  加速度档位
 * @param pulses        脉冲数 (决定转动角度)
 * @param pos_mode      位置模式 (相对/绝对)
 * @param sync_flag     多机同步
 * @return RT_EOK 成功，否则错误码
 */
rt_err_t motor_uart_set_position(rt_uint8_t address,
                                 rt_uint8_t direction,
                                 rt_uint16_t speed,
                                 rt_uint8_t acceleration,
                                 rt_uint32_t pulses,
                                 rt_uint8_t pos_mode,
                                 rt_uint8_t sync_flag)
{
    rt_err_t ret;
    rt_uint8_t frame[13];
    rt_size_t sent;

    if (g_motor_uart_dev == RT_NULL)
    {
        ret = motor_uart_init(RT_NULL);
        if (ret != RT_EOK)
            return ret;
    }

    ret = motor_uart_build_position_frame(address, direction, speed,
                                          acceleration, pulses,
                                          pos_mode, sync_flag, frame);
    if (ret != RT_EOK)
        return ret;

    rt_mutex_take(&g_motor_uart_lock, RT_WAITING_FOREVER);
    sent = rt_device_write(g_motor_uart_dev, 0, frame, sizeof(frame));
    rt_mutex_release(&g_motor_uart_lock);

    return (sent == sizeof(frame)) ? RT_EOK : -RT_ERROR;
}

rt_err_t motor_uart_read_position(rt_uint8_t address, int32_t *position)
{
    rt_err_t ret;
    rt_uint8_t tx_frame[3];
    rt_uint8_t rx_frame[8] = {0};  // 正确返回8字节
    rt_size_t rx_len = 0;
    rt_tick_t start_tick, timeout_tick;

    if (position == RT_NULL)
        return -RT_EINVAL;

    if (g_motor_uart_dev == RT_NULL)
    {
        ret = motor_uart_init(RT_NULL);
        if (ret != RT_EOK)
            return ret;
    }

    tx_frame[0] = address;
    tx_frame[1] = MOTOR_UART_CMD_READ_POS;
    tx_frame[2] = MOTOR_UART_CHECK_BYTE;

    rt_mutex_take(&g_motor_uart_lock, RT_WAITING_FOREVER);
    motor_uart_drain_rx();
     rt_thread_mdelay(2);
    // 发送
    if (rt_device_write(g_motor_uart_dev, 0, tx_frame, sizeof(tx_frame)) != sizeof(tx_frame))
    {
        rt_mutex_release(&g_motor_uart_lock);
        return -RT_ERROR;
    }

    // 接收，超时
    start_tick = rt_tick_get();
    timeout_tick = rt_tick_from_millisecond(MOTOR_UART_READ_TIMEOUT_MS);

    while (rx_len < sizeof(rx_frame))
    {
        rt_size_t bytes = rt_device_read(g_motor_uart_dev, 0, rx_frame + rx_len, sizeof(rx_frame) - rx_len);
        if (bytes > 0)
        {
            rx_len += bytes;
        }
        else
        {
            if (rt_tick_get() - start_tick >= timeout_tick)
                break;
            rt_thread_mdelay(MOTOR_UART_READ_RETRY_INTERVAL_MS);
        }
    }

    rt_mutex_release(&g_motor_uart_lock);

    // 检查收到的长度
    if (rx_len < 4)  // 错误帧至少4字节
        return -RT_ETIMEOUT;

    // 如果是错误帧（4字节）
    if (rx_len == 4)
    {
        if (rx_frame[0] == address && rx_frame[1] == 0x00 && rx_frame[2] == 0xEE && rx_frame[3] == 0x6B)
            return -RT_ERROR;  // 命令错误（如条件不满足）
        else
            return -RT_ERROR;  // 格式错误
    }

    // 正确返回8字节
    if (rx_len != 8)
        return -RT_ERROR;

    // 校验
    if (rx_frame[0] != address || rx_frame[1] != MOTOR_UART_CMD_READ_POS || rx_frame[7] != MOTOR_UART_CHECK_BYTE)
        return -RT_ERROR;

    // 提取符号位（0正，1负）
    uint8_t sign = rx_frame[2];
    // 提取4字节位置（大端）：索引3~6
    uint32_t pos_abs = ((uint32_t)rx_frame[3] << 24) |
                       ((uint32_t)rx_frame[4] << 16) |
                       ((uint32_t)rx_frame[5] << 8) |
                       (uint32_t)rx_frame[6];

    int32_t pos_val = (int32_t)pos_abs;
    if (sign == 1)
        pos_val = -pos_val;

    *position = pos_val;
    return RT_EOK;
}

/**
 * @brief 立即停止电机（不检查返回）
 * @param address   从机地址
 * @param sync_flag 多机同步标志 (MOTOR_UART_SYNC_DISABLE/ENABLE)
 * @return RT_EOK 发送成功，否则错误码
 */
rt_err_t motor_uart_stop(rt_uint8_t address, rt_uint8_t sync_flag)
{
    rt_err_t ret;
    rt_uint8_t frame[5];
    rt_size_t sent;

    // 检查同步标志有效性
    if (!motor_uart_valid_sync(sync_flag))
        return -RT_EINVAL;

    // 设备自动初始化
    if (g_motor_uart_dev == RT_NULL)
    {
        ret = motor_uart_init(RT_NULL);
        if (ret != RT_EOK)
            return ret;
    }

    // 构建停止帧
    frame[0] = address;
    frame[1] = MOTOR_UART_CMD_STOP;
    frame[2] = MOTOR_UART_STOP_SUB_CMD;
    frame[3] = sync_flag;
    frame[4] = MOTOR_UART_CHECK_BYTE;

    // 线程安全发送
    rt_mutex_take(&g_motor_uart_lock, RT_WAITING_FOREVER);
    sent = rt_device_write(g_motor_uart_dev, 0, frame, sizeof(frame));
    rt_mutex_release(&g_motor_uart_lock);

    return (sent == sizeof(frame)) ? RT_EOK : -RT_ERROR;
}
