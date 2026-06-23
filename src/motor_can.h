#ifndef __MOTOR_CAN_H__
#define __MOTOR_CAN_H__

#include <rtthread.h>
#include <rtdevice.h>

#define MOTOR_CAN_DIR_CW         0x00U
#define MOTOR_CAN_DIR_CCW        0x01U

#define MOTOR_CAN_SYNC_DISABLE   0x00U
#define MOTOR_CAN_SYNC_ENABLE    0x01U

rt_err_t motor_can_build_speed_payload(rt_uint8_t address,
                                       rt_uint8_t direction,
                                       rt_uint16_t speed,
                                       rt_uint8_t acceleration,
                                       rt_uint8_t sync_flag,
                                       rt_uint8_t data[7]);

rt_err_t motor_can_set_speed(rt_uint8_t address,
                             rt_uint8_t direction,
                             rt_uint16_t speed,
                             rt_uint8_t acceleration,
                             rt_uint8_t sync_flag);

rt_err_t motor_can_get_payload(struct rt_can_msg *msg);

#endif
