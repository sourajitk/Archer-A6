/******************************************************************************
*
* Copyright (c) 2010 TP-LINK Technologies CO.,LTD.
* All rights reserved.
*
* FILE NAME  :   ubus_type.h
* VERSION    :   1.0
* DESCRIPTION:   定义ibus使用到的基本的数据类型.
*
* AUTHOR     :   Huangwenzhong <Huangwenzhong@tp-link.net>
* CREATE DATE:   08/17/2015
*
* HISTORY    :
* 01   08/17/2015  Huangwenzhong     Create.
*
******************************************************************************/
#ifndef __IBUS_TYPE_H__
#define __IBUS_TYPE_H__

#ifndef U32
#define U32		unsigned int
#endif

#ifndef U16
#define U16		unsigned short
#endif

#ifndef U8
#define U8		unsigned char
#endif

#ifndef INT32
#define INT32		int
#endif

#ifndef INT16
#define INT16		short
#endif

#ifndef INT8
#define INT8		char
#endif

#ifndef NULL
#define NULL			((void*)0)
#endif

#define IBUS_STATUS		int
#define IBUS_OK			(0)
#define IBUS_ERROR		(-1)

#endif

