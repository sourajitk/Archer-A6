#ifndef __KERNEL_COMMON_H__
#define __KERNEL_COMMON_H__

#define KERNEL_OK 0
#define KERNEL_ERR -1

#ifndef BOOL
#define BOOL char
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


#define CHECK_KERNEL_RET(_x,_y, ...) 	\
{\
	do \
	{\
		ret=_x;\
		if (ret != KERNEL_OK)\
		{\
			printk(_y, ##__VA_ARGS__); \
			printk(" ret = %d, return\n", ret);\
			goto exit;\
		}\
	}\
	while(0);\
}


#define CHECK_KERNEL_RET_ONLY(_x) 	\
{\
	do \
	{\
		ret=_x;\
		if (ret != KERNEL_OK)\
		{\
			printk("ret = %d, return\n", ret);\
			goto exit;\
		}\
	}\
	while(0);\
}


#define CHECK_KERNEL_RET_NO_PRINT(_x) 	\
{\
	do \
	{\
		ret=_x;\
		if (ret != KERNEL_OK)\
		{\
			goto exit;\
		}\
	}\
	while(0);\
}

#define SHOW_CMM_RET(_x, _y,...)  \
{\
	do \
	{\
		if (_x != KERNEL_OK)\
		{\
			printk(_y, ##__VA_ARGS__); \
			printk("ret = %d", ret);\
		}\
	}\
	while(0);\
}



#endif
