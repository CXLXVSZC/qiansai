#ifndef __CAN_APP_H__
#define __CAN_APP_H__

#include <rtthread.h>
#include <rtdevice.h>

typedef rt_err_t (*can_app_payload_getter_t)(struct rt_can_msg *msg);

rt_err_t can_app_init(can_app_payload_getter_t payload_getter);

#endif
