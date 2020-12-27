#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/timer.h>

#include <asm/uaccess.h>

#include <linux/rtnetlink.h>
#include <net/rtnetlink.h>

#include <tplink/cos_netlink.h>

#ifndef BUF_SIZE_RTNL
#define BUF_SIZE_RTNL 	256
#endif

static int nluport = -1;

int msgcent_getnluport()
{
	return nluport;
}
EXPORT_SYMBOL(msgcent_getnluport);

void msgcent_setnluport(unsigned long data)
{
	nluport = data;
}
EXPORT_SYMBOL(msgcent_setnluport);

static int rtnetlink_fill_info(struct sk_buff *skb, int type, char *data, int data_len)
{	
	struct ifinfomsg *r;	
	struct nlmsghdr  *nlh;
	unsigned char	 *b = skb_tail_pointer(skb);
	nlh = nlmsg_put(skb, 0, 0, type, sizeof(*r), 0);
	r = nlmsg_data(nlh);	
	r->ifi_family = AF_UNSPEC;	
	r->__ifi_pad = 0;	
	r->ifi_type = 0;	
	r->ifi_index = 0;	
	r->ifi_flags = 0;	
	r->ifi_change = 0;	
	/* Wireless changes don't affect those flags */	
	/* Add the wireless events in the netlink packet */	
	nla_put(skb, IFLA_WIRELESS, data_len, data);	
	nlmsg_end(skb, nlh);	
	return nlmsg_end(skb, nlh);
    
nla_put_failure:
    return -1;
}


/*
 * For cos, the input msg should be 
 * typedef struct
{
	char name[WLAN_NETLINK_MSG_NAME_LEN];
	int data;
}
COS_NETLINK_DATA_t;
The data can be other type.
*/
int cos_netlink_send_msg(const u8 *msg, u32 len)
{

	unsigned int size = BUF_SIZE_RTNL;
	int ret = 0;

    /* used in interrupt, should be GFP_ATOMIC !*/
	struct sk_buff *skb = nlmsg_new(size, GFP_ATOMIC);

	if (skb == NULL)
	{
		printk(KERN_ERR "no enough memory!\n");
		return -1;
	}

	if (rtnetlink_fill_info(skb, RTM_NEWLINK,
				  msg, len) < 0) 
	{
		printk(KERN_ERR "fill msg info error!\n");
		kfree_skb(skb);
		return -1;
	}

	return netlink_unicast((&init_net)->rtnl, skb, msgcent_getnluport(), MSG_DONTWAIT);

}
EXPORT_SYMBOL(cos_netlink_send_msg);

static void func_invoke(struct work_struct *workPtr)
{
    COMMON_WORK_t *workStruct =  (COMMON_WORK_t *)container_of(workPtr, COMMON_WORK_t, work);
    
    printk(KERN_ERR "%s\n", __FUNCTION__);
    
    if (workStruct->callback)
    {
        workStruct->callback(workStruct->cb_arg);
    }
    
    /* should free the work here */
    kfree(workStruct);
    workStruct = NULL;
    return;
    
}

int cos_kernel_thread(void *func, void *data)
{
    /* we are in interrupt and are not sure if the callback can be used in interrupt. open a new kernel thread and do it */
   
    COMMON_WORK_t *workStruct = kmalloc(sizeof(*workStruct), GFP_ATOMIC);
    if (!workStruct)
    {
        return -1;
    }
    INIT_WORK (&(workStruct->work), func_invoke);
    workStruct->callback = func;
    workStruct->cb_arg = data;

    schedule_work(&(workStruct->work));
    
    return 0;
}
EXPORT_SYMBOL(cos_kernel_thread);