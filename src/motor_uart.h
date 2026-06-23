#ifndef __MOTOR_UART_H__
#define __MOTOR_UART_H__

#include <rtthread.h>

#define MOTOR_UART_DIR_CW         0x00U
#define MOTOR_UART_DIR_CCW        0x01U

#define MOTOR_UART_SYNC_DISABLE   0x00U
#define MOTOR_UART_SYNC_ENABLE    0x01U

rt_err_t motor_uart_init(const char *uart_name);

rt_err_t motor_uart_build_speed_frame(rt_uint8_t address,
                                      rt_uint8_t direction,
                                      rt_uint16_t speed,
                                      rt_uint8_t acceleration,
                                      rt_uint8_t sync_flag,
                                      rt_uint8_t frame[8]);

rt_err_t motor_uart_set_speed(rt_uint8_t address,
                              rt_uint8_t direction,
                              rt_uint16_t speed,
                              rt_uint8_t acceleration,
                              rt_uint8_t sync_flag);

#endif
