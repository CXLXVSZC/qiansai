#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/rt_drv_pwm.h>

#include "servo_pwm.h"

// RT-Thread注册的GPT2 PWM设备名称
#define SERVO_PWM_DEVICE_NAME        "pwm2"

// 当前RA PWM驱动固定使用通道0访问一个GPT设备
#define SERVO_PWM_DEVICE_CHANNEL     0

// 舵机PWM周期为20毫秒，对应50Hz
#define SERVO_PWM_PERIOD_NS          20000000U

// 0度对应的控制脉宽为500微秒
#define SERVO_PWM_MIN_PULSE_US       500U

// 180度对应的控制脉宽为2500微秒
#define SERVO_PWM_MAX_PULSE_US       2500U

// 微秒转换为纳秒的倍率
#define SERVO_PWM_US_TO_NS           1000U

// 保存RT-Thread PWM设备指针
static struct rt_device_pwm *g_servo_pwm_device = RT_NULL;

rt_err_t servo_pwm_init(void)
{
    // 如果PWM设备已经初始化
    if (g_servo_pwm_device != RT_NULL)
    {
        // 直接返回成功，避免重复初始化
        return RT_EOK;
    }

    // 按设备名称查找GPT2对应的PWM设备
    g_servo_pwm_device = (struct rt_device_pwm *)rt_device_find(SERVO_PWM_DEVICE_NAME);

    // 如果没有找到PWM2设备
    if (g_servo_pwm_device == RT_NULL)
    {
        // 返回设备不存在错误
        return -RT_ENOSYS;
    }

    // 初始化阶段只查找设备，不立即启动PWM，避免舵机上电突然动作
    return RT_EOK;
}

rt_err_t servo_pwm_set_pulse_us(rt_uint32_t pulse_us)
{
    rt_err_t ret;
    rt_uint32_t pulse_ns;

    // 如果输入脉宽小于安全下限
    if (pulse_us < SERVO_PWM_MIN_PULSE_US)
    {
        // 将脉宽限制到安全下限
        pulse_us = SERVO_PWM_MIN_PULSE_US;
    }
    // 如果输入脉宽大于安全上限
    else if (pulse_us > SERVO_PWM_MAX_PULSE_US)
    {
        // 将脉宽限制到安全上限
        pulse_us = SERVO_PWM_MAX_PULSE_US;
    }

    // 如果PWM设备还没有初始化
    if (g_servo_pwm_device == RT_NULL)
    {
        // 尝试初始化PWM设备
        ret = servo_pwm_init();

        // 如果PWM设备初始化失败
        if (ret != RT_EOK)
        {
            // 返回初始化错误
            return ret;
        }
    }

    // RT-Thread PWM接口使用纳秒，将微秒脉宽转换为纳秒
    pulse_ns = pulse_us * SERVO_PWM_US_TO_NS;

    // 先设置周期和脉宽，防止启用瞬间输出错误占空比
    ret = rt_pwm_set(g_servo_pwm_device,
                     SERVO_PWM_DEVICE_CHANNEL,
                     SERVO_PWM_PERIOD_NS,
                     pulse_ns);

    // 如果周期或脉宽设置失败
    if (ret != RT_EOK)
    {
        // 返回PWM设置错误
        return ret;
    }

    // 在参数设置完成后启用PWM输出
    return rt_pwm_enable(g_servo_pwm_device, SERVO_PWM_DEVICE_CHANNEL);
}

rt_err_t servo_pwm_set_angle(rt_uint16_t angle)
{
    rt_uint32_t pulse_us;

    // 如果输入角度超过最大允许角度
    if (angle > SERVO_PWM_ANGLE_MAX)
    {
        // 将角度限制到180度
        angle = SERVO_PWM_ANGLE_MAX;
    }

    // 将0到180度线性映射为500到2500微秒
    pulse_us = SERVO_PWM_MIN_PULSE_US +
               ((rt_uint32_t)angle *
                (SERVO_PWM_MAX_PULSE_US - SERVO_PWM_MIN_PULSE_US) /
                SERVO_PWM_ANGLE_MAX);

    // 使用计算出的脉宽控制舵机位置
    return servo_pwm_set_pulse_us(pulse_us);
}

rt_err_t servo_pwm_disable(void)
{
    // 如果PWM设备还没有初始化
    if (g_servo_pwm_device == RT_NULL)
    {
        // PWM尚未启动，直接返回成功
        return RT_EOK;
    }

    // 停止GPT2的PWM输出
    return rt_pwm_disable(g_servo_pwm_device, SERVO_PWM_DEVICE_CHANNEL);
}
