#ifndef __CAN_APP_H__
#define __CAN_APP_H__

#include <rtthread.h>

typedef rt_err_t (*can_app_payload_getter_t)(rt_uint8_t data[8]);

rt_err_t can_app_init(can_app_payload_getter_t payload_getter);

#endif
