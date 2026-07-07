#ifndef __SERVO_PWM_H__
#define __SERVO_PWM_H__

#include <rtthread.h>

// 舵机允许设置的最小角度
#define SERVO_PWM_ANGLE_MIN          0U

// 舵机允许设置的最大角度
#define SERVO_PWM_ANGLE_MAX          180U

// 初始化舵机PWM设备
rt_err_t servo_pwm_init(void);

// 按微秒设置舵机控制脉宽
rt_err_t servo_pwm_set_pulse_us(rt_uint32_t pulse_us);

// 按角度设置舵机位置
rt_err_t servo_pwm_set_angle(rt_uint16_t angle);

// 停止舵机PWM输出
rt_err_t servo_pwm_disable(void);

#endif
