/******************************************************************************
*
* Copyright (c) 2010 TP-LINK Technologies CO.,LTD.
* All rights reserved.
*
* FILE NAME  :   ubus_json.h
* VERSION    :   1.0
* DESCRIPTION:   定义使用ubus时各个模块通信的json数据结构.
*
* AUTHOR     :   Huangwenzhong <Huangwenzhong@tp-link.net>
* CREATE DATE:   08/17/2015
*
* HISTORY    :
* 01   08/17/2015  Huangwenzhong     Create.
*
******************************************************************************/
#ifndef __IBUS_JSON_H__
#define __IBUS_JSON_H__

/* for ubus to define the json message translate  between ubus server and client*/

/* for object define used in ubus */
#define UBUS_OBJ_WPS		"wps"
#define UBUS_OBJ_TIMERJOB	"timerjob"
#define UBUS_OBJ_GPIO		"gpio"
#define UBUS_OBJ_WIFI		"wifi"
#define UBUS_OBJ_SMARTIP	"smartip"
#define UBUS_OBJ_WPS_CLI	"wps_cli"

#define UBUS_OBJ_ID		"id"

/* for gpio  json definition*/
/*
{
	"type": "wps",
	"pressed": "0"
}
*/
#define UBUS_GPIO_PRESS			"pressed"	/*int8 type, value is 0 or 1 */
#define UBUS_GPIO_TYPE			"type"		/* string type, value is below ,such wps, reset, power and son on */
#define UBUS_GPIO_TYPE_WPS		"wps"




/* for wps json definition */
/*
{
	"action": "0",
	"2g_status": "0"
	"5g_status": "0"
}
*/
#define UBUS_WPS_ACTION			"action"/* int8 type, 0 - none, 1 - start, 2 - done */

/* 
wps status
0 - init, 1 - associated, 2 - ok, 3 - msg err, 4 - timeout, 5 - send m2, 6 - send m7,
8 - msg done, 9 - pbc overlap, 10 - find pbc ap, 11 - associating, 12 - scan ap, 13 - silent
*/
#define UBUS_WPS_2G_STATUS		"2g_status"/* int16 */
#define UBUS_WPS_5G_STATUS		"5g_status"/* int16 */




/* for timer job json definition */
/* 
{
	"status": "0"
}
*/
#define UBUS_TIMERJOB_STATUS			"status"/* int16, 0 - led off, 1 - led on */






/* for smart ip json definition */
/*
{
	"action": "0",
	"status": "0",
	"dhcpc_info":
	{
		"ip": "192.168.0.254",
		"mask": "255.255.255.0",
		"gw": "192.168.0.1",
		"dns" ["192.168.0.1", "192.168.0.2"]
	}
}
*/

/* int8, 0 - none, 1 - dhcp, 2 -wifi, 3 - ip change */
#define UBUS_SMARTIP_ACTION		"action"
/*
u16
for dhcpc detect: 0 - success, 1 -fail
for wifi status: 0 - no change, 1 - connected->disconnected, 2 - disconnected->connected
for smart ip pub msg: 0 - lan ip no change, 1 - lan ip changed
*/
#define UBUS_SMARTIP_STATUS		"status"
#define UBUS_SMARTIP_DHCPC_INFO		"dhcpc_info"
#define UBUS_SMARTIP_DHCPC_IP		"ip"
#define UBUS_SMARTIP_DHCPC_MASK		"mask"
#define UBUS_SMARTIP_DHCPC_GW		"gw"
#define UBUS_SMARTIP_DHCPC_DNS1		"dns1"
#define UBUS_SMARTIP_DHCPC_DNS2		"dns2"


/* for wifi json definition */
/*
{
	"band": "0",
	"region": "china",
	"action": "none"
}
*/
#define UBUS_WIFI_BAND		"band"		/* u32, 0 - 2G, 1 - 5G */
#define UBUS_WIFI_REGION	"region"	/* string */
/*
u8
0 - none, 1 - restart, 2 - stop, 3 - start scan, 4 - start dual scan, 5 - end scan, 6 - rcv scan msg
7 - disconn sta, 8 - acl, 9 - wps ready, 10 - hostapd restart
*/
#define UBUS_WIFI_ACTION	"action"





/* for wps cli json definition */
/* 
{
	"action": "0",
	"method": "0",
	"pin": "12345678"
}
*/
/* int 8, 0 - none, 1 - connect, 2 - cancel */
#define UBUS_WPS_CLI_ACTION			"action"
/* int 8, 0 - pbc, 1 - pin */
#define UBUS_WPS_CLI_METHOD			"method"
#define UBUS_WPS_CLI_PIN			"pin"

#endif

