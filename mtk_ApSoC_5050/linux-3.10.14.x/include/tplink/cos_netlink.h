#ifndef __COS_NETLINK__H_
#define __COS_NETLINK__H_


#include <linux/spinlock.h>
#include <linux/workqueue.h>


#define WLAN_NETLINK_MSG_NAME_LEN 32

typedef struct
{
	char name[WLAN_NETLINK_MSG_NAME_LEN];
	int data;
}
COS_NETLINK_DATA_t;


typedef struct
{
    void (*callback)(void *arg);
    /* for kernel internal use */
	void *cb_arg;
	struct work_struct work;
}
COMMON_WORK_t;


int cos_netlink_send_msg(const u8 *msg, u32 len);
int cos_kernel_thread(void *func, void *data);
#endif