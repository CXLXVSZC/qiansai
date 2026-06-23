#include <rtthread.h>
#include <rtdevice.h>
#include "can_app.h"

#define CAN_TX_DEV_NAME        "canfd0"
#define CAN_RX_DEV_NAME        "canfd1"
#define CAN_TX_PERIOD_MS       1000
#define CAN_RX_WAIT_TICK       10

static struct rt_semaphore g_can_rx_sem;
static rt_bool_t g_can_rx_sem_inited = RT_FALSE;
static rt_device_t g_can_rx_dev = RT_NULL;
static rt_device_t g_can_tx_dev = RT_NULL;
static rt_thread_t g_can_thread = RT_NULL;
static can_app_payload_getter_t g_can_payload_getter = RT_NULL;

static rt_err_t can_rx_indicate(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);
    RT_UNUSED(size);

    if (g_can_rx_sem_inited)
    {
        rt_sem_release(&g_can_rx_sem);
    }

    return RT_EOK;
}

static rt_err_t can_open_device(rt_device_t dev)
{
    rt_err_t ret;

    if (dev == RT_NULL)
    {
        return -RT_ERROR;
    }

    ret = rt_device_open(dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    if ((ret != RT_EOK) && (ret != -RT_EBUSY))
    {
        return ret;
    }

    return RT_EOK;
}

static void can_print_rx_frame(const struct rt_can_msg *msg)
{
    int i;

    if (msg == RT_NULL)
    {
        return;
    }

    rt_kprintf("can rx id=0x%03x len=%d data=", msg->id, msg->len);
    for (i = 0; i < msg->len; i++)
    {
        rt_kprintf("%02X", msg->data[i]);
        if (i != (msg->len - 1))
        {
            rt_kprintf(" ");
        }
    }
    rt_kprintf("\n");
}

static void can_send_latest_payload(void)
{
    struct rt_can_msg txmsg;

    if ((g_can_tx_dev == RT_NULL) || (g_can_payload_getter == RT_NULL))
    {
        return;
    }

    rt_memset(&txmsg, 0, sizeof(txmsg));
    if (g_can_payload_getter(&txmsg) != RT_EOK)
    {
        return;
    }

    rt_device_write(g_can_tx_dev, 0, &txmsg, sizeof(txmsg));
}

static void can_drain_rx_fifo(void)
{
    struct rt_can_msg rxmsg;

    if (g_can_rx_dev == RT_NULL)
    {
        return;
    }

    do
    {
        rxmsg.hdr_index = -1;
        if (rt_device_read(g_can_rx_dev, 0, &rxmsg, sizeof(rxmsg)) != sizeof(rxmsg))
        {
            break;
        }

        can_print_rx_frame(&rxmsg);
    } while (1);
}

static void can_thread_entry(void *parameter)
{
    rt_tick_t next_tx_tick;

    RT_UNUSED(parameter);

    rt_device_set_rx_indicate(g_can_rx_dev, can_rx_indicate);
    next_tx_tick = rt_tick_get() + rt_tick_from_millisecond(CAN_TX_PERIOD_MS);

    while (1)
    {
        if (rt_sem_take(&g_can_rx_sem, CAN_RX_WAIT_TICK) == RT_EOK)
        {
            can_drain_rx_fifo();
        }

        if ((rt_int32_t)(rt_tick_get() - next_tx_tick) >= 0)
        {
            can_send_latest_payload();
            next_tx_tick = rt_tick_get() + rt_tick_from_millisecond(CAN_TX_PERIOD_MS);
        }
    }
}

rt_err_t can_app_init(can_app_payload_getter_t payload_getter)
{
    rt_err_t ret;

    if (g_can_thread != RT_NULL)
    {
        return RT_EOK;
    }

    g_can_payload_getter = payload_getter;
    g_can_rx_dev = rt_device_find(CAN_RX_DEV_NAME);
    g_can_tx_dev = rt_device_find(CAN_TX_DEV_NAME);
    if ((g_can_rx_dev == RT_NULL) || (g_can_tx_dev == RT_NULL))
    {
        return -RT_ERROR;
    }

    if (!g_can_rx_sem_inited)
    {
        ret = rt_sem_init(&g_can_rx_sem, "canrx", 0, RT_IPC_FLAG_FIFO);
        if (ret != RT_EOK)
        {
            return ret;
        }
        g_can_rx_sem_inited = RT_TRUE;
    }

    ret = can_open_device(g_can_rx_dev);
    if (ret != RT_EOK)
    {
        return ret;
    }

    ret = can_open_device(g_can_tx_dev);
    if (ret != RT_EOK)
    {
        return ret;
    }

    g_can_thread = rt_thread_create("can_task",
                                    can_thread_entry,
                                    RT_NULL,
                                    1024,
                                    12,
                                    10);
    if (g_can_thread == RT_NULL)
    {
        return -RT_ERROR;
    }

    rt_thread_startup(g_can_thread);

    return RT_EOK;
}
