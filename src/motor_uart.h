#ifndef __MOTOR_UART_H__
#define __MOTOR_UART_H__

#include <rtthread.h>

#define MOTOR_UART_DIR_CW         0x00U
#define MOTOR_UART_DIR_CCW        0x01U

#define MOTOR_UART_SYNC_DISABLE   0x00U
#define MOTOR_UART_SYNC_ENABLE    0x01U

#define MOTOR_UART_CMD_SPEED_MODE   0xF6U
#define MOTOR_UART_CHECK_BYTE       0x6BU
#define MOTOR_UART_DEFAULT_DEV      "uart5"


#define MOTOR_UART_CMD_POS_MODE         0xFDU
#define MOTOR_UART_POS_MODE_RELATIVE    0x00U
#define MOTOR_UART_POS_MODE_ABSOLUTE    0x01U

#define MOTOR_UART_CMD_READ_POS       0x36U
#define MOTOR_UART_READ_TIMEOUT_MS    300    // 读取超时
#define MOTOR_UART_READ_RETRY_INTERVAL_MS 30 // 重试间隔

/* 命令码 */
#define MOTOR_UART_CMD_STOP           0xFEU
#define MOTOR_UART_STOP_SUB_CMD       0x98U   // 固定子命令

/* 函数声明 */
rt_err_t motor_uart_stop(rt_uint8_t address, rt_uint8_t sync_flag);

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

rt_err_t motor_uart_set_position(rt_uint8_t address,
                                 rt_uint8_t direction,
                                 rt_uint16_t speed,
                                 rt_uint8_t acceleration,
                                 rt_uint32_t pulses,
                                 rt_uint8_t pos_mode,
                                 rt_uint8_t sync_flag);

rt_err_t motor_uart_read_position(rt_uint8_t address, int32_t *position);

#endif
