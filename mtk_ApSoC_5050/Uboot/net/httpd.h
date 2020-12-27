#ifndef _NET_HTTPD_H__
#define _NET_HTTPD_H__

#include "sysdefs.h"

#define MD5_DIGEST_LEN 16
#define RSA_SIG_SIZE 128

//#define PTN_ENTRY_MAX_NUM		(32)
//#define PTN_ENTRY_LEN			sizeof(mtd_partition)

//#define PTN_TABLE_SIZE			(PTN_ENTRY_MAX_NUM * PTN_ENTRY_LEN) /* 0x500 */

#define IMAGE_SIZE_LEN  (0x04)
#define IMAGE_SIZE_MD5  MD5_DIGEST_LEN
#define IMAGE_SIZE_PRODUCT  (0x1000)
#define IMAGE_SIZE_BASE (IMAGE_SIZE_LEN + IMAGE_SIZE_MD5 + IMAGE_SIZE_PRODUCT)
#define IMAGE_SIZE_FWTYPE	(IMAGE_SIZE_LEN + IMAGE_SIZE_MD5)
#define IMAGE_SIZE_RSA_SIG	(IMAGE_SIZE_FWTYPE + 0x11C)
#define IMAGE_LEN_RSA_SIG	RSA_SIG_SIZE
#define IMAGE_CLOUD_HEAD_OFFSET (252)
#define	FW_TYPE_NAME_LEN_MAX	0x100

#define FWUP_HDR_PRODUCT_ID_LEN 			(0x1000)

#define FWUP_HDR_PRODUCT_ID_LEN 			(0x1000)
#define SOFTWARE_VERSION_LEN    			(256)
#define PRODUCTINFO_VENDOR_NAME_LEN         (64)
#define PRODUCTINFO_VENDOR_URL_LEN          (64)
#define PRODUCTINFO_PRODUCT_NAME_LEN        (64)
#define PRODUCTINFO_PRODUCT_VER_LEN        (64)

#define PRODUCTINFO_HW_ID_LEN         		(48)
#define PRODUCTINFO_OEM_ID_LEN         		(48)
//#define PRODUCTINFO_LANGUAGE_LEN            ( 8)
#define NAME_SOFT_VERSION 					"soft-version"
#define NAME_SUPPORT_LIST 					"support-list"
#define SYSMGR_PROINFO_INFO_MAX_CNT         (32)
#define SYSMGR_SUPPORTLIST_MAX_CNT          (32)
#define SYSMGR_PROINFO_CHAR_INFO_SEPERATOR  (0x03)

typedef enum _SYSMGR_PROINFO_ID
{
    PRODCUTINFO_ID_VOID = -1,

    PRODUCTINFO_VENDOR_NAME,
    PRODUCTINFO_VENDOR_URL,
    PRODUCTINFO_PRODUCT_NAME,
    PRODUCTINFO_PRODUCT_ID,
    PRODUCTINFO_PRODUCT_VER,
    PRODUCTINFO_SPECIAL_ID,
//    PRODUCTINFO_LANGUAGE, 
    PRODUCTINFO_HW_ID,
    PRODUCTINFO_OEM_ID,

    SYSMGR_PROINFO_ID_NUM,
} SYSMGR_PROINFO_ID;

typedef struct 
{
    char name[32];
    UINT32 start_addr;
	UINT32 next_offset;
    UINT32 size;
}nflash_partitions;

typedef struct _fw_head
{
	unsigned long fileSize;
	char fileMd5[MD5_DIGEST_LEN];
	char modelId[FWUP_HDR_PRODUCT_ID_LEN];
} fw_head;

typedef struct _SYSMGR_PROINFO_STR_MAP
{
    int             key;
    char*           str;
} SYSMGR_PROINFO_STR_MAP;

typedef struct _PRODUCT_INFO_STRUCT
{
    unsigned char   vendorName[PRODUCTINFO_VENDOR_NAME_LEN];
    unsigned char   vendorUrl[PRODUCTINFO_VENDOR_URL_LEN];
    unsigned char   productName[PRODUCTINFO_PRODUCT_NAME_LEN];
//    unsigned char   productLanguage[PRODUCTINFO_LANGUAGE_LEN];

    UINT32			productId;
    unsigned char	productVer[PRODUCTINFO_PRODUCT_VER_LEN];
    UINT32			specialId;
    unsigned char   hwId[PRODUCTINFO_HW_ID_LEN];
    unsigned char   oemId[PRODUCTINFO_OEM_ID_LEN];
}PRODUCT_INFO_STRUCT;


void HttpdStart(void);
void HttpdHandler(void);

/* board specific implementation */
extern int do_http_upgrade(const ulong size, const int upgrade_type);
extern int do_http_progress(const int state);
extern void all_led_on(void);
extern void all_led_off(void);

#endif
