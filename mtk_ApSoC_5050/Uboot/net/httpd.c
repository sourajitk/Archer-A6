/*
 *	Copyright 1994, 1995, 2000 Neil Russell.
 *	(See License)
 *	Copyright 2000, 2001 DENX Software Engineering, Wolfgang Denk, wd@denx.de
 */

#include <common.h>
#include <command.h>
#include <net.h>
#include <asm/byteorder.h>
#include <tpLinuxTag.h>
#include "httpd.h"

#include "../httpd/uipopt.h"
#include "../httpd/uip.h"
#include "../httpd/uip_arp.h"

#include "partition.h"

#include <cyassl/md5.h>
#include <cyassl/asn.h>

#if !defined(NO_FIRMWARE_CHECK)
#include <nvram_mngr/rsaVerify.h>
#endif

static int arptimer = 0;
#define TAG_LEN         512

int tpFirmwareVerify(unsigned char * ptr, int size);

void HttpdHandler(void){
	int i;

	for(i = 0; i < UIP_CONNS; i++){
		uip_periodic(i);

		if(uip_len > 0){
			uip_arp_out();
			NetSendHttpd();
		}
	}

	if(++arptimer == 20){
		uip_arp_timer();
		arptimer = 0;
	}
}

// start http daemon
void HttpdStart(void){
	uip_init();
	httpd_init(tpFirmwareVerify);
}



/**************************************************************
*					moved from bba
**************************************************************/

int eraseenv()
{
#if defined(USE_UBOOT_ENV)
	
#if defined (CFG_ENV_IS_IN_NAND)
	ranand_erase(CFG_ENV_ADDR- CFG_FLASH_BASE, CFG_ENV_SIZE);
#elif defined (CFG_ENV_IS_IN_SPI)
	raspi_erase(CFG_ENV_ADDR-CFG_FLASH_BASE, CFG_ENV_SIZE);
#else/*CFG_ENV_IS_IN_SPI*/
	flash_sect_erase(CFG_ENV_ADDR, CFG_ENV_ADDR+CFG_ENV_SIZE-1);
#endif/*CFG_ENV_IS_IN_NAND*/
	
#endif /*USE_UBOOT_ENV*/

}

//static const char *l_factory_partitions[]={"uboot", "factory", NULL};
static const char *l_factory_partitions[]={"uboot", "tp_data", "radio",  NULL};

#if 0
static bool checkMd5(char *up_image, int size)
{
	Md5 digest;
	char  hash[MD5_DIGEST_LEN];
	char  plain[MD5_DIGEST_LEN];
	unsigned char md5Key[16] =
	{
	        0x7a, 0x2b, 0x15, 0xed,  0x9b, 0x98, 0x59, 0x6d,
	        0xe5, 0x04, 0xab, 0x44,  0xac, 0x2a, 0x9f, 0x4e
	};
	unsigned char rsaSignature[RSA_SIG_SIZE];
	fw_head *ptr = (fw_head *)up_image;
	UPGRADE_HEADER *up_head = (UPGRADE_HEADER *)(up_image + UPGRADE_HEAD_OFFSET);

	memcpy(plain, ptr->fileMd5, MD5_DIGEST_LEN);
	memcpy(ptr->fileMd5, md5Key, MD5_DIGEST_LEN);

	//solve the bug exist temp
	memcpy(rsaSignature, up_head->rsaSignature, RSA_SIG_SIZE);
	memset(up_head->rsaSignature, 0, RSA_SIG_SIZE);

	InitMd5(&digest);
	Md5Update (&digest, (const byte *)up_image + sizeof(UINT32), size - sizeof(UINT32));
	Md5Final (&digest, (byte *)hash);

	//restore
	memcpy(ptr->fileMd5, plain, MD5_DIGEST_LEN);
	memcpy(up_head->rsaSignature, rsaSignature, RSA_SIG_SIZE);
	
	if (memcmp(hash, plain, MD5_DIGEST_LEN))
		{
		printf("   ## MD5 check failed ##\n");
		return false;
		}
	
	printf("   ## MD5 check success ##\n");

	return true;
}

/* FIXME: NOT COMPLETED. */
static bool checkSig(char *up_image, int image_len)
{	
	/* check signature */
	Md5 digest;
	char *input;
	char plain[RSA_SIG_SIZE], hash[MD5_DIGEST_LEN];
	char buf[RSA_SIG_SIZE];
	int ret = 0;
	RsaKey pubkey;
	u32 idx = 0;
	
	UPGRADE_HEADER *up_head = (UPGRADE_HEADER *)(up_image + UPGRADE_HEAD_OFFSET);
	/* back up sig to calc md5. */
	memcpy(plain, up_head->rsaSignature, RSA_SIG_SIZE);
	memset(up_head->rsaSignature, 0, RSA_SIG_SIZE);

	InitMd5 (&digest);
	Md5Update (&digest, (const byte *)up_image + UPGRADE_HEAD_OFFSET, image_len - UPGRADE_HEAD_OFFSET);
	Md5Final (&digest, (byte *)hash);

	UINT8  public_key[] = 
	"********************************************************************************************************";
	/* verify signature of image */
	InitRsaKey (&pubkey, 0);
	if ((ret = RsaPublicKeyDecode((const byte*)public_key, &idx, &pubkey, strlen(public_key))) < 0) {
            printf("Decode public failed, err: %d\n", ret);
			
            return false;
        }
	memset (plain, 0, sizeof(plain));
	ret = RsaSSL_Verify ((const byte*)plain, RSA_SIG_SIZE, (byte *)buf, sizeof(buf), &pubkey);

	printf("\n");

	if (memcmp (buf, hash, ret)) {
		printf("   ## RsaSSL_Verify failed ##\n");
		return false;
	} else
		printf("   ## RsaSSL_Verify succeed ##\n");
	
	return true;
}

#endif

int getOSoffset(const char *pAppBuf, int appSize, int *offset)
{
	if (pAppBuf == NULL || offset == NULL)
	{
		return -1;
	}
	
	nflash_partitions part;

/* NOTE!!! This is UBOOT which will operate physical memory directly, so you can't use partPtr to get data as partPtr may not be aligned!!!!
*/
	const char *partPtr = pAppBuf;
	
	memset(&part, 0, sizeof(nflash_partitions));
	while(partPtr < (pAppBuf + appSize))
	{
		memcpy(&part, partPtr, sizeof(nflash_partitions));
		if(part.name[0] == 0)
		{
			return -1;
		}
		if (ntohl(part.start_addr) < partPtr - pAppBuf + sizeof(nflash_partitions))
		{
			printf("error start_addr %d\n",ntohl(part.start_addr));
			return -1;
		}
		
		if ((ntohl(part.start_addr) + ntohl(part.size)) >= appSize) 
		{
			printf("error start_addr 0x%d, size 0x%x\n",ntohl(part.start_addr), ntohl(part.size));
			return -1;
		}

		if (ntohl(part.next_offset) && (ntohl(part.size) + ntohl(part.start_addr) != ntohl(part.next_offset)))
		{
			printf("error addr %s: startAddr 0x%x, size 0x%x, nextOffset 0x%x.\n", part.name,
				ntohl(part.start_addr), ntohl(part.size), ntohl(part.next_offset));
			return -1;
		}
		
		if (ntohl(part.next_offset))
		{
			partPtr = pAppBuf +ntohl(part.next_offset);
		}
		else
		{
			*offset = ntohl(part.size) + ntohl(part.start_addr);
   			return 0;
		}
	}

	return -1;
}

/*oops: don't need perform transforming between little-endian and big-endian */
static int _readPtnFromBuf(const char *name, const char *pAppBuf, int appSize, int *offset, int *size)
{
	if (name == NULL || pAppBuf == NULL || offset == NULL || size == NULL)
	{
		return -1;
	}
	
	nflash_partitions part;
	
	/* NOTE!!! This is UBOOT which will operate physical memory directly, so you can't use partPtr to get data as partPtr may not be aligned!!!!
	*/

	const char *partPtr = pAppBuf;
	
	memset(&part, 0, sizeof(nflash_partitions));
	while(partPtr < (pAppBuf + appSize))
	{
		memcpy(&part, partPtr, sizeof(nflash_partitions));
		if(part.name[0] == 0)
		{
			break;
		}

		if (ntohl(part.start_addr) < partPtr - pAppBuf + sizeof(nflash_partitions))
		{
			printf("error start_addr %d\n",ntohl(part.start_addr));
			return -1;
		}
		
		if (ntohl(part.start_addr) + ntohl(part.size) >= appSize)	
		{
			printf("error start_addr %d, size %d\n",ntohl(part.start_addr), ntohl(part.size));
			return -1;
		}

		if (ntohl(part.next_offset) && (ntohl(part.size) + ntohl(part.start_addr) != ntohl(part.next_offset)))
		{
			printf("error addr %s: startAddr %x, size %x, nextOffset %x.\n", part.name,
				ntohl(part.start_addr), ntohl(part.size), ntohl(part.next_offset));

			return -1;
		}

		//printf("check: %s\n",part->name);
		if (strcmp(part.name, name) == 0)
        {
            *offset = ntohl(part.start_addr);
            *size = ntohl(part.size);
			printf("find part name %s offset %d size %d\n", part.name, *offset, *size);
            return 0;
        }

		if (ntohl(part.next_offset))
		{
			partPtr = pAppBuf +ntohl(part.next_offset);
		}
		else
		{
			break;
		}
	}
	
	return -1;
}


#define LEAVE(retVal)       do{ret = retVal;goto leave;}while(0)

static SYSMGR_PROINFO_STR_MAP l_mapProductInfoId[] = {
    {PRODUCTINFO_VENDOR_NAME,         "vendor_name"},
    {PRODUCTINFO_VENDOR_URL,          "vendor_url"},
    {PRODUCTINFO_PRODUCT_NAME,        "product_name"},
    {PRODUCTINFO_PRODUCT_ID,          "product_id"},

    {PRODUCTINFO_PRODUCT_VER,         "product_ver"},
    {PRODUCTINFO_SPECIAL_ID,          "special_id"},
//    {PRODUCTINFO_LANGUAGE,            "language"},
    {PRODUCTINFO_HW_ID,                "hw_id" },
    {PRODUCTINFO_OEM_ID,               "oem_id"},
    {PRODCUTINFO_ID_VOID,                NULL}
};

/* replace the srcchar with dstchar */
static char* _charReplace(char* string, const char srcchar, const char dstchar)
{
    int i;
    int len;
    if((NULL == string) || ('\0' == string[0]))
    {
        return NULL;
    }
    len = strlen(string);
    for (i = 0; i < len; ++i)
    {
        if (*(string + i) == srcchar)
        {
            *(string + i) = dstchar;
        }
    }
    
    return string;
}

static int _makeSubStrByChar(char *string, char delimit, int maxNum, char *subStrArr[])
{
    char ws[8];
    char *pChar = NULL;
    char *pLast = NULL;
    int     cnt    = 0;

    if ((NULL == string) || (NULL == subStrArr))
    {
        printf("Some parameter is null.\r\n");
        return -1;
    }

    if (maxNum <= 0)
    {
        printf("Maximum number is invalid. maxNum = %d\r\n", maxNum);
        return -1;
    }

    memset(ws, 0, sizeof(ws));

    ws[0] = delimit;
    ws[1] = '\0';
    strncat(ws, "\t\r\n", strlen("\t\r\n")+1);
#if 0
    for (pChar=strtok_r(string,ws,&pLast); pChar; pChar=strtok_r(NULL,ws,&pLast))
#else
	for (pChar=strtok(string,ws); pChar; pChar=strtok(NULL,ws))
#endif
    {
        subStrArr[cnt++] = pChar;

        if (cnt >= maxNum)
        {
            printf("Too many substrings to string.\r\n");
            
            return -1;
        }
    }
    
    subStrArr[cnt] = NULL;

    return cnt;
}

static char* _delLeadingSpace(char *str)
{
    int     i;
    int     len;
    char    *tmpStart = NULL;

    if(NULL == str)
    {
        return NULL;
    }
    
    len = strlen(str);
    
    for (i = 0; i < len; ++i)
    {
        if(*(str + i) != ' ')
        {
            tmpStart = (str+i);
            break;
        }
        
    }
    if(NULL == tmpStart)
    {
        (*str) =  '\0';
        return str;
    }
    
    if(tmpStart > str)
    {
        memmove(str, tmpStart, strlen(tmpStart)+1);
    }
    
    return str;
}

static char* _delTailSpace(char *str)
{
    int     i;
    int     len;
    int    bAllBlank = 1;

    if(NULL == str)
    {
        return NULL;
    }
    
    len = strlen(str);
    
    bAllBlank = 1; 
    for (i = (len-1); i >= 0; --i)
    {
        if(*(str + i) != ' ')
        {
            bAllBlank = 0;
            break;
        }
    }
    if(0 == bAllBlank)
    {
    *(str + i + 1) = '\0';
    }
    else
    {
        (*str) = '\0';
    }
    
    return str;
}

static char* _delSpace(char* str)
{
    _delLeadingSpace(str);
    _delTailSpace(str);
    return str;
}

static int _strToStr(char* outStr, int outStrSize, const char* inStr)
{
    int len  = 0;

    if ((NULL == inStr) || ('\0' == inStr[0]))
    {
        return -1;
    }

    if (NULL == outStr)
    {
        return -1;
    }

    if (strlen(inStr) > (outStrSize - 1))
    {
        return -1;
    }
#if 0
    len = snprintf(outStr, outStrSize, "%s", inStr);
#else
	len = sprintf(outStr, "%s", inStr);
#endif
    if ((len < 0) || (len != strlen(inStr)))
    {
        return -1;
    }
    outStr[len] = '\0';

    return 0;
}

static int
_strToKey(const SYSMGR_PROINFO_STR_MAP *pMap, const char *str)
{
    int i = 0;
    
    while(pMap[i].str != NULL)
    {
        if (0 == strcmp(pMap[i].str, str))
            return pMap[i].key;

        i++;
    }

    return -1;
}

static int _hexStrToUL(UINT32 *u32, const char* hexStr)
{
    char tmpStr[16] = {0};

    if (8 != strlen(hexStr))
    {
        return -1;
    }
#if 0
    snprintf(tmpStr, sizeof(tmpStr), "0x%s", hexStr);
#else
	sprintf(tmpStr, "0x%s", hexStr);
#endif
    
    (*u32) = simple_strtoul(tmpStr, NULL, 16);

    return 0;
}

static int _strToVendorName(char* vendorName, const char* str)
{
    return _strToStr(vendorName, PRODUCTINFO_VENDOR_NAME_LEN, str);
}

static int  _strToVendorUrl(char* vendorUrl, const char* str)
{
    return _strToStr(vendorUrl, PRODUCTINFO_VENDOR_URL_LEN, str);
}

static int  _strToProductName(char* modelName, const char* str)
{
    return _strToStr(modelName, PRODUCTINFO_PRODUCT_NAME_LEN, str);
}

static int  _strToProductId(UINT32 *modelId, const char* str)
{
    return _hexStrToUL(modelId, str);
}

static int  _strToSpecialId(UINT32 *specialId,const char *str)
{
    return _hexStrToUL(specialId,str);
}

static int _countDot(const char *str)
{
	const char *tmp = str;
	int   dot = 0;
	
	while (tmp && *tmp != '\0')
	{
		if (*tmp == '.') 
		{
			dot++;
		}
		tmp++;
	}

	return dot;
}

#if 0
static int  _strToProductVer(UINT32 *modelVer, const char* str)
{
    
    unsigned int ver = 0;
    int cnt = 0;
	int dot = 0;
    unsigned int val[3] = {0};

    if (NULL == modelVer)
    {
        return -1;
    }

    if ((NULL == str) || ('\0' == str[0]))
    {
        return -1;
    }

	dot = _countDot(str);

	if (dot == 1)
	{
		cnt = sscanf((char *)str, "%u.%u", &(val[0]), &(val[1]));

		if (cnt != 2) 
		{
			return -1;
		}

        if ((val[0] > 0xff) || (val[1] > 0xff))
        {
            return -1;
        }
		
		/* 1.0 */		
        //ver  = 0xffff0000;
        ver  = 0;
		
        ver |= ((unsigned char)val[0]) << 8;
        ver |= (unsigned char)val[1];
		

	}
	else if (dot == 2)
	{
		cnt = sscanf((char *)str, "%u.%u.%u", &(val[0]), &(val[1]), &(val[2]));
		if (cnt != 3)
		{
			return -1;
		}

		if ((val[0] > 0xff) || (val[1] > 0xff) || (val[2] > 0xff))
		{
			return = -1;
		}

		/* 1.0.0 */
		//ver  = 0xff000000;
        ver  = 0;
		
		ver |= ((unsigned char)val[0]) << 16;
		ver |= ((unsigned char)val[1]) <<  8;
		ver |= (unsigned char)val[2];	
		
	}
	else
	{
		return -1;	
	}
    
    *modelVer = ver;

    return 0;
}
#else
static int  _strToProductVer(char* ver, const char* str)
{
    return _strToStr(ver, PRODUCTINFO_PRODUCT_NAME_LEN, str);
}

#endif

static int  _strToHwId(char *hwId, const char* str)
{
    return _strToStr(hwId, PRODUCTINFO_HW_ID_LEN, str);
}

static int  _strToOemId(char *oemId, const char* str)
{
    return _strToStr(oemId, PRODUCTINFO_OEM_ID_LEN, str);
}
/*
static CMM_RET _strToProductLanguage(char *language, const char* str)
{
    return _strToStr(language, PRODUCTINFO_LANGUAGE_LEN, str);
}
*/

static int _checkSupportList(PRODUCT_INFO_STRUCT *pProductInfo, const char* listContent, int fileLen)
{
    int     ret = 0;

    char*   entryInfoVal[SYSMGR_PROINFO_INFO_MAX_CNT] = {0};
    char*   itemInfoVal [SYSMGR_SUPPORTLIST_MAX_CNT]  = {0};
    
    int     entryInfoCnt     = 0;
    int     itemInfoCnt      = 0;
    int     entryInfoIndex   = 0;
    int     itemInfoIndex    = 0;

    int isProductNameFound   = 0;
    int isProductVerFound    = 0;
    PRODUCT_INFO_STRUCT supportList;

    char*   buf         = NULL;

    if (NULL == pProductInfo)
    {
        printf("Invalid productinfo pointer.\n");
        LEAVE(-1);
    }

    if ((NULL == listContent) || (fileLen <= 0))
    {
        printf("Invalid file.\n");
        LEAVE(-1);
    }

    buf = malloc(fileLen + 1);
    if (NULL == buf)
    {
        printf("No memory.\n");
        LEAVE(-1);
    }

	memset(buf, 0, fileLen + 1);
    memcpy(buf, listContent, fileLen);

	//printf("support-list checked buff %s", buf);

    _charReplace(buf, '\r', '\n');
    _charReplace(buf, '{',  '\n');
    _charReplace(buf, '}',  '\n');
    _charReplace(buf, '\n', SYSMGR_PROINFO_CHAR_INFO_SEPERATOR);

    entryInfoCnt = _makeSubStrByChar(buf, SYSMGR_PROINFO_CHAR_INFO_SEPERATOR, 
                                          SYSMGR_SUPPORTLIST_MAX_CNT, entryInfoVal);
    if (entryInfoCnt <= 0)
    {
        printf("ucm_string_makeSubStrByChar() failed.\n");
        LEAVE(-1);
    }
    
    printf("%d support-list entries found.", entryInfoCnt);
    for (entryInfoIndex = 0; entryInfoIndex < entryInfoCnt; ++entryInfoIndex)
    {
        int forbidden = 0;

        isProductNameFound   = 0;
        isProductVerFound    = 0;

       	printf("Checking entry:%d ...\n", entryInfoIndex);
        _charReplace(entryInfoVal[entryInfoIndex], ',', SYSMGR_PROINFO_CHAR_INFO_SEPERATOR);
        itemInfoCnt = _makeSubStrByChar(entryInfoVal[entryInfoIndex], 
                                        SYSMGR_PROINFO_CHAR_INFO_SEPERATOR, 
                                        SYSMGR_PROINFO_INFO_MAX_CNT, itemInfoVal);
        for(itemInfoIndex = 0; itemInfoIndex < itemInfoCnt; ++itemInfoIndex)
        {
            char*   argv[32] = {NULL};
            int     argc     = 0;
            int     argIndex = 0;
            int     id    = 0;

            if (forbidden)
            {
                printf("Entry %d NOT Match.\n", entryInfoIndex);
                break;
            }

            printf("itemInfoValue[%d] = (%s).\n", itemInfoIndex, itemInfoVal[itemInfoIndex]);
            argc = _makeSubStrByChar(itemInfoVal[itemInfoIndex], ':', 32, argv);        

            if (2 != argc)
            {
                printf("should be 2 args (%d).\n", argc);
                continue;
            }
            
            for (argIndex = 0; argIndex < argc; ++argIndex)
            {
                printf("argv[%d] = (%s).\n", argIndex, argv[argIndex]);
                _delSpace(argv[argIndex]);
                printf("argv[%d] = (%s).\n", argIndex, argv[argIndex]);
            }
            
            id = _strToKey(l_mapProductInfoId, argv[0]);

            switch (id)
            {
#if 0
                case PRODUCTINFO_VENDOR_NAME:
                {
                    ret = _strToVendorName((char*)supportList.vendorName, argv[1]);
                    if (-1 == ret)
                    {
                        printf("_strToVendorName(%s) failed.", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    
                    if (0 != strcasecmp((const char*)pProductInfo->vendorName, (const char*)supportList.vendorName))
                    {
                        printf("vendorName %s NOT Match.", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    printf("%s Matched!!!", argv[1]);
                    break;
                }
                case PRODUCTINFO_VENDOR_URL:
                {
                    ret = _strToVendorUrl((char*)supportList.vendorUrl, argv[1]);
                    if (-1 == ret)
                    {
                        printf("_strToVendorUrl(%s) failed.", argv[1]);
                        forbidden = 1;
                        break;
                    }

                    if (0 != strcasecmp((const char*)pProductInfo->vendorUrl, (const char*)supportList.vendorUrl))
                    {
                        printf("vendorUrl %s NOT Match.", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    printf("%s Matched!!!", argv[1]);
                    break;
                }
#endif
                case PRODUCTINFO_PRODUCT_NAME:
                {
                    ret = _strToProductName((char*)supportList.productName, argv[1]);
                    if (-1 == ret)
                    {
                        printf("_strToProductName(%s) failed.\n", argv[1]);
                        forbidden = 1;
                        break;
                    }

                    if (0 != strnicmp((const char*)pProductInfo->productName, (const char*)supportList.productName, strlen((const char*)pProductInfo->productName)))
                    {
                        printf("productName %s NOT Match.\n", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    printf("%s Matched!!!", argv[1]);
                    isProductNameFound = 1;
                    break;
                }
#if 0
                case PRODUCTINFO_PRODUCT_ID:
                {
                    ret = _strToProductId(&(supportList.productId), argv[1]);
                    if (-1 == ret)
                    {
                        printf("_strToProductId(%s) failed.\n", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    
                    if (pProductInfo->productId != supportList.productId)
                    {
                        printf("productId %s NOT Match.\n", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    printf("%s Matched!!!\n", argv[1]);
                    break;
                }

                case PRODUCTINFO_SPECIAL_ID:
                {
                    ret = _strToSpecialId(&(supportList.specialId),argv[1]);
                    if(-1 == ret)
                    {
                        printf("_strToSpecialId(%s) failed.",argv[1]);
                        forbidden = 1;
                        break;
                    }

                    if (pProductInfo->specialId != supportList.specialId)
                    {
                        printf("specialId %s NOT Match.", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    printf("%s Matched!!!", argv[1]);
                    break;
                }
#endif
                case PRODUCTINFO_PRODUCT_VER:
                {
                    ret = _strToProductVer((char *)(supportList.productVer), argv[1]);
                    if (-1 == ret)
                    {
                        printf("_strToProductVer(%s) failed.\n", argv[1]);
                        forbidden = 1;
                        break;
                    }

                    if (strcmp(pProductInfo->productVer, supportList.productVer))
                    {
                        printf("productVer %s NOT Match.\n", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    printf("%s Matched!!!\n", argv[1]);
                    isProductVerFound = 1;
                    break;
                }
#if 0
                case PRODUCTINFO_LANGUAGE:
                {
                    ret = _strToProductLanguage((char *)supportList.productLanguage, argv[1]);
                    if (-1 == ret)
                    {
                        printf("_strToProductLanguage(%s) failed.\n", argv[1]);
                        forbidden = 1;
                        break;
                    }

                    if (0 != strcasecmp((const char*)pProductInfo->productLanguage, (const char*)supportList.productLanguage))
                    {
                        printf("productLanguage %s NOT Match.\r\n", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    CDBG_DEBUG("%s Matched!!!\r\n", argv[1]);
                    break;

                }

                case PRODUCTINFO_HW_ID:
                {
                    ret = _strToHwId((char *)supportList.hwId, argv[1]);
                    if (-1 == ret)
                    {
						printf("_strToHwId(%s) failed.", argv[1]);
                        forbidden = 1;
                        break;
                    }

					if (0 != strcasecmp((const char*)pProductInfo->hwId, (const char*)supportList.hwId))
                    {
						printf("hwId %s NOT Match.", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    printf("%s Matched!!!", argv[1]);
                    break;

                }
                case PRODUCTINFO_OEM_ID:
                {
                    ret = _strToOemId((char *)supportList.oemId, argv[1]);
                    if (-1 == ret)
                    {
                        printf("_strToOemId(%s) failed.", argv[1]);
                        forbidden = 1;
                        break;
                    }

                    if (0 != strcasecmp((const char*)pProductInfo->oemId, (const char*)supportList.oemId))
                    {
						printf("oemId %s NOT Match.", argv[1]);
                        forbidden = 1;
                        break;
                    }
                    printf("%s Matched!!!", argv[1]);
                    break;
                }
#endif
                default:
                {
                    printf("unknown id(%s), skip it.\n", argv[0]);
                    break;
                }
            } /* end of switch() */
        } /* item parcing loops */
        
        /* We got here because an entry matched or forbidden */
        /* It is COMPULSORY that 'product_name' and 'product_ver' be written to every entry
         * of firmware's support-list. If this is not satified, we will consider this entry
         * as invalid. */
        if (!isProductNameFound || !isProductVerFound)
        {
            /* This entry regarded as invalid, try next entry. */
            printf("ProductName or ProductVersion not Found. "
                                 "Invalid support-list entry.\n");
            continue;
        }

        if (!forbidden)
        {
            /* Matched */       
            ret = 0;
            goto leave;
        }
        /* if forbidden, then we try next entry. */

    } /* entry parcing loops */

    /* All entries tried. NOT SUPPORTED */
    ret = -1;

leave:
    if (NULL != buf)
    {
        free(buf);
    }
    return ret;

}

static bool isFactoryPartition(const char *name)
{
	int i = 0;
	for (i=0;l_factory_partitions[i]!=NULL;i++)
	{
	if (strncmp(name, l_factory_partitions[i], strlen( l_factory_partitions[i])+1)==0)
		{
		return true;
		}
	}
	return false;
}

#if !defined(NO_FIRMWARE_CHECK)

int tpFirmwareFindType(char *ptr, int len, char *buf, int buf_len)
{
	int end = 0;
	int begin = 0;
	char *pBuf = NULL;
	char *type = "fw-type:"; //the fw type stored as "fw-type:cloud\n"

	if (buf_len < IMAGE_CLOUD_HEAD_OFFSET || 
		len < (IMAGE_SIZE_FWTYPE + IMAGE_CLOUD_HEAD_OFFSET))
	{
		return -1;
	}
	
	pBuf = ptr + IMAGE_SIZE_FWTYPE;

	//find the fw type name begin and end
	while (*(pBuf + end) != '\n' && end < IMAGE_CLOUD_HEAD_OFFSET)
	{
		if (begin < strlen(type))
		{			
			if (*(pBuf + end) != type[begin])
			{
				return -1;
			}

			begin++;
		}

		end++;
	}

	if (end >= IMAGE_CLOUD_HEAD_OFFSET || begin != strlen(type) || end <= begin) 
		return -1;

	//copy to the fw type name buffer
	memcpy(buf, pBuf + begin, end - begin);

	return 0;
}

int tpFirmwareMd5Check(unsigned char *ptr,int bufsize)
{
	unsigned char fileMd5Checksum[IMAGE_SIZE_MD5];
	unsigned char digst[IMAGE_SIZE_MD5];
	unsigned char md5Key[IMAGE_SIZE_MD5] = 
	{
		0x7a, 0x2b, 0x15, 0xed,  0x9b, 0x98, 0x59, 0x6d,
		0xe5, 0x04, 0xab, 0x44,  0xac, 0x2a, 0x9f, 0x4e
	};

	memcpy(fileMd5Checksum, ptr + IMAGE_SIZE_LEN, IMAGE_SIZE_MD5);
	memcpy(ptr + IMAGE_SIZE_LEN, md5Key, IMAGE_SIZE_MD5);

	Md5Hash(ptr + IMAGE_SIZE_LEN, bufsize - IMAGE_SIZE_LEN, (byte *)digst);
	
	if (0 != memcmp(digst, fileMd5Checksum, IMAGE_SIZE_MD5))
	{
		printf("Check md5 error.\n");
		return -1;
	}

	memcpy(ptr + IMAGE_SIZE_LEN, fileMd5Checksum, IMAGE_SIZE_MD5);

	return 0;
}

int tpFirmwareSigCheck(unsigned char *buf, int buf_len)
{	
	unsigned char md5_dig[IMAGE_SIZE_MD5];
	unsigned char sig_buf[IMAGE_LEN_RSA_SIG];
	unsigned char tmp_rsa_sig[IMAGE_LEN_RSA_SIG];
	int ret = 0;
	unsigned char rsaPubKey[] = "BgIAAACkAABSU0ExAAQAAAEAAQD9lxDCQ5DFNSYJBriTmTmZlEMYVgGcZTO+AIwm" \
					"dVjhaeJI6wWtN7DqCaHQlOqJ2xvKNrLB+wA1NxUh7VDViymotq/+9QDf7qEtJHmesji" \
					"rvPN6Hfrf+FO4/hmjbVXgytHORxGta5KW4QHVIwyMSVPOvMC4A5lFIh+D1kJW5GXWtA==";

	/*backup data*/
	memcpy(tmp_rsa_sig,buf + IMAGE_SIZE_RSA_SIG,IMAGE_LEN_RSA_SIG);
	memcpy(sig_buf, buf + IMAGE_SIZE_RSA_SIG, IMAGE_LEN_RSA_SIG);
	
	/* fill with 0x0 */
	memset(buf + IMAGE_SIZE_RSA_SIG, 0x0, IMAGE_LEN_RSA_SIG);


	Md5Hash(buf + IMAGE_SIZE_FWTYPE, buf_len - IMAGE_SIZE_FWTYPE, (byte *)md5_dig);

	ret = rsaVerifySignByBase64EncodePublicKeyBlob(rsaPubKey, strlen((char *)rsaPubKey),
                md5_dig, IMAGE_SIZE_MD5, sig_buf, IMAGE_LEN_RSA_SIG);

	memcpy(buf + IMAGE_SIZE_RSA_SIG,tmp_rsa_sig,IMAGE_LEN_RSA_SIG);

	if (NULL == ret)
	{
		printf("Check rsa error.\n");
		return -1;
	}
	else
	{
		return 0;
	}
}

static bool tpFirmwareCheck(unsigned char *ptr,int size)
{
	int ret;
	char fw_type_name[FW_TYPE_NAME_LEN_MAX];
	
	memset(fw_type_name, 0x0, FW_TYPE_NAME_LEN_MAX);
	tpFirmwareFindType((char *)ptr, size, fw_type_name, FW_TYPE_NAME_LEN_MAX);
	//printf("fw type name : %s.\n", fw_type_name);

	if (strcmp("Cloud", fw_type_name))
	{
		//common firmware MD5 check
		printf("Firmware process common.\n");
		ret = tpFirmwareMd5Check(ptr, size);
	}
	else
	{
		printf("Firmware process cloud.\n");
		ret = tpFirmwareSigCheck(ptr, size);
	}


	if (ret != 0)
	{
		printf("Firmware process verify fail.\n");
		return false;
	}
	
	printf("Firmware verify OK!\r\n");

#if 0
	int offset = 0;
    int supportListSize = 0;	
	unsigned char *tmpAddr = ptr + sizeof(fw_head);
	//get support list content from buffer
	if(_readPtnFromBuf(NAME_SUPPORT_LIST, tmpAddr, size-sizeof(fw_head), &offset, &supportListSize) < 0)
	{
		printf("Read %s from buffer fail.\n", NAME_SUPPORT_LIST);
		return false;
	}

    // check list prefix string
	tmpAddr = tmpAddr + offset;
    if (supportListSize < 12 || strncmp(tmpAddr, "SupportList:", 12) != 0)
    {
		printf("support list form invalid.\n");
		return false;
    }
	
    supportListSize -= 12;
    tmpAddr += 12;

	//printf("run product info: vendorName(%s) vendorUrl(%s) productName(%s) "
	//	"productId(%x) productVer(%x) oemId(%s) hwId(%s) specialId(%x)",
	//	productInfo.vendorName, productInfo.vendorUrl, productInfo.productName,
	//	productInfo.productId, productInfo.productVer, productInfo.oemId,
	//	productInfo.hwId, productInfo.specialId);
	PRODUCT_INFO_STRUCT productInfo;

	memset(&productInfo, 0, sizeof(productInfo));
	productInfo.productId = CONFIG_PRODUCT_ID;
	strncpy(&productInfo.productName, CONFIG_BOOT_WEBPAGE_PRODUCT_TAG, sizeof(productInfo.productName));
	strncpy(&productInfo.productVer, CONFIG_BOOT_WEBPAGE_PRODUCT_VERSION, sizeof(productInfo.productVer));
	
	if (_checkSupportList(&productInfo,tmpAddr, supportListSize) != 0)
	{
		printf("check support list fail.");
		return false;
	}
#endif

	return true;
}
#endif

/*
retcode:
case "-2": upgrade_probar_result.innerHTML = _char.BOOT.INVALID_FILE;break;
case "-3": 
case "-4":
case "-5":
case "-6":upgrade_probar_result.innerHTML = _char.BOOT.INCOMPATIBLE_FIRMWARE;break;
default: upgrade_probar_result.innerHTML = _char.BOOT.FAILED;break;
*/
int tpFirmwareVerify(unsigned char * ptr, int size)
{
	int no_firmware_check = 0;
	fw_head *pfw_hdr = (fw_head*)ptr;

	if (size > CONFIG_MAX_UPLOAD_FILE_LEN || size < CONFIG_MIN_UPLOAD_FILE_LEN)
	{
		printf("size 0x%x is illegal!\n", size);
		return -2;
	}

	/* TODO: Should do more check. */
	if (ntohl(pfw_hdr->fileSize) != size)
	{
		printf("The size of firmware (0x%x) is not the same as transferred size(0x%x)\n", ntohl(pfw_hdr->fileSize),size);
		return -3;
	}
	
#if !defined(NO_FIRMWARE_CHECK)
	char *str = getenv(NO_FIRMWARE_CHECK_ENV);
	if (str == NULL || strncmp(str, "1", sizeof("1")) != 0)
	{
		if (!tpFirmwareCheck(ptr, size))
		{
			return -4;
		}
	}
#endif	

	printf("firmware(size 0x%x) verify pass!\n", size);
	return(0);
}

//mod by zengwei for being compatible with IPF nand below 20180619
#if 0
int tpFirmwareRecovery(unsigned char *ptr,int size)
{
    /* NEED REVISION. */
	fw_head *pfw_hdr = (fw_head*)ptr;
	UINT32 fw_size						= ntohl(pfw_hdr->fileSize);

	/* partitions num in up bin */
	UINT32 up_entry_num 				= ntohl(pfw_hdr->up_ptn_entry_num);

	/* final partitions num in target device */
	UINT32 target_entry_num 			= ntohl(pfw_hdr->flash_ptn_entry_num);

	mtd_partition *up_table		= (mtd_partition *)(ptr + sizeof(fw_head));
	mtd_partition *target_table	= (mtd_partition *)(ptr + sizeof(fw_head) + PTN_TABLE_SIZE);

	UINT8* base 						= ptr + sizeof(fw_head);

	image_header_t *hdr = NULL;

	int ret = 0;

	
	int no_firmware_check =0;
		
#if defined(NO_FIRMWARE_CHECK)
	/* do nothing? */
	no_firmware_check =1;
#else

	char *str = getenv(NO_FIRMWARE_CHECK_ENV);
	if (str && (strncmp(str, "1", sizeof("1") )== 0))
		{
		no_firmware_check=1;
		}

#endif
	int tg_index = 0;
	int up_index=  0;
	int in_up = 0;

		/* update mtd partitions */
	g_part_num= target_entry_num;
	/* back up whole flash */
	g_pasStatic_Partition[g_part_num] = 	g_pasStatic_Partition[0];
	
	for (tg_index=0;tg_index<target_entry_num;tg_index++)
		{
		g_pasStatic_Partition[tg_index].size = ntohl(target_table[tg_index].size);
		g_pasStatic_Partition[tg_index].offset = ntohl(target_table[tg_index].offset);
		strncpy(g_pasStatic_Partition[tg_index].name, target_table[tg_index].name, sizeof(g_pasStatic_Partition[tg_index].name)); 
		}

	for (tg_index=0;tg_index<target_entry_num;tg_index++)
		{
		printf("find target partition %s\n", target_table[tg_index].name);
		
		
		in_up = 0;
		for (up_index=0;up_index<up_entry_num;up_index++)
			{
			if (strcmp(up_table[up_index].name, target_table[tg_index].name) == 0)
				{
				/* find image, copy it */	
				printf("find image %s in up.bin, it will cover the target partition\n", up_table[up_index].name);
				
				if ( !no_firmware_check && isFactoryPartition(target_table[tg_index].name))
				{
					printf("target partition %s is factory partition, you can't modify it!\n", target_table[tg_index].name);
					ret = -1;
					goto exit;
				}
				
				hdr = (image_header_t *)(base + ntohl(up_table[up_index].offset));
				/* just treat them as bin. some image is not the true image. */
				#if 0
				if ((ntohl(hdr->ih_magic ) == IH_MAGIC) && (strcmp(up_table[up_index].name, "uboot")!=0))
					{
					/* this is an image */
					/* for ubifs, should erase the whole partition first */
					erase_partition(ntohl(target_table[tg_index].offset), ntohl(target_table[tg_index].size));
					copy_image(ntohl(hdr->ih_size) + sizeof(image_header_t), base + up_table[up_index].offset, target_table[tg_index].offset + CFG_FLASH_BASE);
					}
				else
					#endif
					{
					/* this is raw bin, just copy it.*/
					erase_partition(ntohl(target_table[tg_index].offset), ntohl(target_table[tg_index].size));
					copy_image(ntohl(up_table[up_index].size), base + ntohl(up_table[up_index].offset), ntohl(target_table[tg_index].offset) + CFG_FLASH_BASE);
					}
				
				in_up = 1;
				break;
				}
			}
		if (!in_up)
			{
			/* no image existance in up.bin, erase it. */
			/* DO NO ERASE FACTORY PARTITIONS */			
			if (!isFactoryPartition(target_table[tg_index].name))
				{
				printf("erase %s.\n",target_table[tg_index].name);
				erase_partition(ntohl(target_table[tg_index].offset), ntohl(target_table[tg_index].size));
				}
				
			}
			
		}
	/* it will be restarted later */

exit:
	g_pasStatic_Partition[0] = g_pasStatic_Partition[g_part_num];
	g_part_num = 1;
	return ret;

}
#else
int tpFirmwareRecovery(unsigned char *ptr,int size)
{
#if 0
	mtd_partition *osPart = get_part(IMAGE0_PART_NAME);
	mtd_partition *tmpPart = NULL;
	int osOffset = 0;
	int envFlag = 0;
	int partIndex = 0;
	char *envStr = NULL;
	int ret = 0;	
	if (!osPart)
	{
		printf("target partition %s is not exist and firmware recovery fail!\n", IMAGE0_PART_NAME);
		ret = -1;
		goto exit;
	}
	
	if (getOSoffset(ptr + sizeof(fw_head), size - sizeof(fw_head), &osOffset) < 0)
	{
		printf("get os image offset in buffer fail and firmware recovery fail!\n");
		ret = -1;
		goto exit;
	}
	
	printf("erase %s offset 0x%08x size 0x%08x.\n", 
						osPart->name, osPart->offset, osPart->size);
	erase_partition(osPart->offset, osPart->size);
	copy_image(size - sizeof(fw_head) - osOffset, 
			ptr + sizeof(fw_head) + osOffset, osPart->offset + CFG_FLASH_BASE);
	//printf("copy image end size 0x%08x.\n", size - sizeof(fw_head) - osOffset);
	
	/* erase the partitions which not os0 and factory partitions. */
	for (partIndex = 0; partIndex < g_part_num; partIndex++)
	{
		tmpPart = &g_pasStatic_Partition[partIndex];
		if (0 != strcmp(tmpPart->name, IMAGE0_PART_NAME)
			&& false == isFactoryPartition(tmpPart->name))
		{
			printf("erase %s offset 0x%08x size 0x%08x.\n", 
								tmpPart->name, tmpPart->offset, tmpPart->size);
			erase_partition(tmpPart->offset, tmpPart->size);
		}
	}

exit:
	return ret;
#else
	return (0);
#endif
}
#endif
//mod by zengwei for being compatible with IPF nand above 20180619


int do_http_upgrade(const ulong size, const int upgrade_type){
	int ret;
	printf("do http upgrade\n");
	if(upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE){
		ret = tpFirmwareRecovery((unsigned char *)WEBFAILSAFE_UPLOAD_RAM_ADDRESS, (int)size);
		if (ret == 0)
		{
			return(0);
		}
	}

	printf("Web recovery failed type %d.\n", upgrade_type);

	return(-1);
}

// info about current progress of failsafe mode
int do_http_progress(const int state){
	/* toggle LED's here */
	switch(state){
		case WEBFAILSAFE_PROGRESS_START:
			printf("HTTP server is ready!\n\n");
			break;

		case WEBFAILSAFE_PROGRESS_TIMEOUT:
			//printf("Waiting for request...\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPLOAD_READY:
			printf("HTTP upload is done! Upgrading...\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPGRADE_READY:
			printf("HTTP ugrade is done! Rebooting...\n\n");
			break;

		case WEBFAILSAFE_PROGRESS_UPGRADE_FAILED:
			printf("## Error: HTTP ugrade failed!\n\n");
			break;

		case WEBFAILSAFE_PROGRESS_CHECK_FAILED:
			printf("## Error: HTTP upgrade file check failed!\n\n");
			break;
			
		default:
			printf("## Error: Wrong state!\n\n");
			break;
	}

	return(0);
}
