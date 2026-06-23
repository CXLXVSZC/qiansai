#include <rtthread.h>
#include <rtdevice.h>
#include "motor_can.h"

#define MOTOR_CAN_CMD_SPEED_MODE     0xF6U
/* Protocol examples use 0x6B as the last check byte. */
#define MOTOR_CAN_CHECK_BYTE         0x6BU

static struct rt_can_msg g_motor_can_msg = {0};

static rt_bool_t motor_can_is_valid_direction(rt_uint8_t direction)
{
    return (direction == MOTOR_CAN_DIR_CW) || (direction == MOTOR_CAN_DIR_CCW);
}

static rt_bool_t motor_can_is_valid_sync_flag(rt_uint8_t sync_flag)
{
    return (sync_flag == MOTOR_CAN_SYNC_DISABLE) || (sync_flag == MOTOR_CAN_SYNC_ENABLE);
}

rt_err_t motor_can_build_speed_payload(rt_uint8_t address,
                                       rt_uint8_t direction,
                                       rt_uint16_t speed,
                                       rt_uint8_t acceleration,
                                       rt_uint8_t sync_flag,
                                       rt_uint8_t data[7])
{
    RT_UNUSED(address);

    if (data == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (!motor_can_is_valid_direction(direction) || !motor_can_is_valid_sync_flag(sync_flag))
    {
        return -RT_EINVAL;
    }

    data[0] = MOTOR_CAN_CMD_SPEED_MODE;
    data[1] = direction;
    data[2] = (rt_uint8_t)((speed >> 8) & 0xFF);
    data[3] = (rt_uint8_t)(speed & 0xFF);
    data[4] = acceleration;
    data[5] = sync_flag;
    data[6] = MOTOR_CAN_CHECK_BYTE;

    return RT_EOK;
}

rt_err_t motor_can_set_speed(rt_uint8_t address,
                             rt_uint8_t direction,
                             rt_uint16_t speed,
                             rt_uint8_t acceleration,
                             rt_uint8_t sync_flag)
{
    rt_base_t level;
    rt_uint8_t payload[7];
    rt_err_t ret;

    ret = motor_can_build_speed_payload(address, direction, speed, acceleration, sync_flag, payload);
    if (ret != RT_EOK)
    {
        return ret;
    }

    level = rt_hw_interrupt_disable();
    rt_memset(&g_motor_can_msg, 0, sizeof(g_motor_can_msg));
    g_motor_can_msg.id = ((rt_uint32_t) address) << 8;
    g_motor_can_msg.ide = RT_CAN_EXTID;
    g_motor_can_msg.rtr = RT_CAN_DTR;
    g_motor_can_msg.len = sizeof(payload);
    rt_memcpy(g_motor_can_msg.data, payload, sizeof(payload));
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

rt_err_t motor_can_get_payload(struct rt_can_msg *msg)
{
    rt_base_t level;

    if (msg == RT_NULL)
    {
        return -RT_EINVAL;
    }

    level = rt_hw_interrupt_disable();
    if (g_motor_can_msg.len == 0)
    {
        rt_hw_interrupt_enable(level);
        return -RT_ERROR;
    }
    rt_memcpy(msg, &g_motor_can_msg, sizeof(g_motor_can_msg));
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}
