/*!
 * \ Copyright(c) 2010-2017 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * \file	oal_flash.c
 * \brief	linux platform system functions. 
 *
 * \detail	we have 3 kinds of partitions:
	1) mtd partitions: read from /proc/mtd. They can be changed during firmware upgrading.
	2) file partitions: for NAND flash, some partitions are virtual in /etc/user/.
	3) sub mtd partitions: located in mtd partitions. For example, mtd 'factory' can be made of of 2 sub mtd partitions: 
		factory-config and radio.
		The map is read from /etc/virpar.conf.
	Search sequence: sub mtd partitions -> mtd partitions ->file partitions. 
 * \author	Xie Jiabai
 * \version	1.0.0
 * \date	28Apr11
 *
 * \history 	\arg 1.0.0, 09Nov17, Xie Jiabai, Create the file.
 			\arg 1.1.0 25Dec17, Xie Jiabai, For nand flash, random write is not allowed.
 */
 #define __KERNEL__

#ifdef __USER_SPACE__
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>



#include <cmm_std.h>
#include <oal_sys.h>
#include <dm_paramLen.h>
#include <dm_types.h>

#include "oal_flash.h"
#endif

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>

#include <linux/version.h>
#include <tplink/kernel_common.h>
#include <tplink/kernel_partition.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#endif

/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/

#ifdef __KERNEL__

#define CMM_OK KERNEL_OK
#define CMM_ERROR KERNEL_ERR
#define CHECK_CMM_RET CHECK_KERNEL_RET
#define CHECK_CMM_RET_ONLY CHECK_KERNEL_RET_ONLY
#define CHECK_CMM_RET_NO_PRINT CHECK_KERNEL_RET_NO_PRINT

#define CMM_RET int

#define UINT32 uint32_t
#define UINT64 uint64_t

#define ENTRY_MAX_NUM 16
#define ENTRY_NAME_LEN 32

#define printf printk

#define CDBG_ERROR printk
#define CDBG_DEBUG(_x,...) {do{}while (0);}
#endif



#ifdef __USER_SPACE__
/**
	\brief		copied from kernel
*/
struct erase_info_user {
	unsigned int start;
	unsigned int length;
};

/**
	\brief		copied from kernel
*/
struct mtd_info_user {
	unsigned char type;
	unsigned int flags;
	unsigned int size;
	unsigned int erasesize;
	unsigned int oobblock;  
	unsigned int oobsize;  
	unsigned int ecctype;
	unsigned int eccsize;
};

/**
	\brief		copied from kernel
*/
#define MEMGETINFO 				_IOR('M', 1, struct mtd_info_user)
#define MEMERASE 					_IOW('M', 2, struct erase_info_user)
#define MEMUNLOCK               _IOW('M', 6, struct erase_info_user)

/**
	\brief		copied from kernel
*/
#define MTD_ABSENT		0
#define MTD_RAM			1
#define MTD_ROM			2
#define MTD_NORFLASH		3
#define MTD_NANDFLASH		4
#define MTD_DATAFLASH		6

/* in kernel it check if the return value is valid pointer or an error code. */
#define IS_ERR(_x) ((long)_x < 0)

typedef  UINT32 uint32_t;
typedef UINT8 uint8_t;

#endif /*__USER_SPACE__ */


/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/

#define IH_MAGIC	0x27051956	/* Image Magic Number		*/

#define IH_NMLEN		32	/* Image Name Length		*/

#ifndef N_CONST_ARR
#define N_CONST_ARR(_x) (sizeof(_x)/sizeof(_x[0]))
#endif
#define FLASH_NAME "wholeflash"

/* virtual mtd is not used now. factory partition is mounted as ubifs. by xjb@2018.6.21 */
#if 0
#define VIRTUAL_PARTITION_CONF "/etc/virpar.conf"
#endif

static const char *l_mtd_mount_dirs[] = {"/etc/user", "/tp_data", "/radio", "/data", "/etc/partition_config", NULL};

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/
typedef struct image_header {
	uint32_t	ih_magic;	/* Image Header Magic Number	*/
	uint32_t	ih_hcrc;	/* Image Header CRC Checksum	*/
	uint32_t	ih_time;	/* Image Creation Timestamp	*/
	uint32_t	ih_size;	/* Image Data Size		*/
	uint32_t	ih_load;	/* Data	 Load  Address		*/
	uint32_t	ih_ep;		/* Entry Point Address		*/
	uint32_t	ih_dcrc;	/* Image Data CRC Checksum	*/
	uint8_t		ih_os;		/* Operating System		*/
	uint8_t		ih_arch;	/* CPU architecture		*/
	uint8_t		ih_type;	/* Image Type			*/
	uint8_t		ih_comp;	/* Compression Type		*/
	uint8_t		ih_name[IH_NMLEN];	/* Image Name		*/
} image_header_t;

typedef struct
{
	struct
	{
		char name[ENTRY_NAME_LEN];			/* identifier string */
		UINT64 size;			/* partition size */
		UINT64 offset;		/* offset within the master MTD space */
	}
	sub_mtd;
	char mtdName[ENTRY_NAME_LEN];
}
sub_mtd_t;

/**************************************************************************************************/
/*                                           LOCAL_PROTOTYPES                                     */
/**************************************************************************************************/
static int getMtdNum(const char *name);
static int getFileLen(const char *path);
static CMM_RET getMtdInfo(const char *path, struct mtd_info_user *mtdInfo);
static CMM_RET getMtdInfoByName(const char *name, struct mtd_info_user *mtdInfo);
static BOOL isFileExist(const char *path);
static CMM_RET getParPath(const char *name, char *path, int pathLen);
static int getParLen(const char *path);
static BOOL isRandomWriteEnable(void);
static CMM_RET getMtdEraseSize(int *size);
static CMM_RET checkSubMtd(char *name, int nameLen, int *offset, int *size);
static CMM_RET checkParPath(const char *name, char *path, int pathLen, int *offset, int *size);
static CMM_RET readPartition(const char* path, char* buf, int offset, int size);
static CMM_RET writePartitionAlign(const char* path, const char* buf, int offset, int size);
static CMM_RET erasePartitionMtdAlign(const char *path, int offset, int size);
static CMM_RET erasePartitionMtd(const char *path, int offset, int size);
static CMM_RET erasePartitionFile(const char* path, int offset, int size);
static CMM_RET writePartitionMtd(const char *path, const char* buf, int offset, int size);
static CMM_RET erasePartition(const char* path, int offset, int size);
static CMM_RET writePartition(const char* path, const char *buf, int offset, int size);
static CMM_RET getSubMtdParTable(sub_mtd_t par[], int len, int *parNum);
static CMM_RET getSubMtdPartition(const char *virtualName, sub_mtd_t *virpar);
static CMM_RET getMtdWithSubSize(const char *mtdName, int *size);

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_FUNCTIONS                                      */
/**************************************************************************************************/

#ifdef __KERNEL__

static int open(const char *pPath, int flag)
{
	return (int)filp_open(pPath, flag, 0);
}

static void close(int osfd)
{
    filp_close((struct file *)osfd, NULL);
	return;
}

static void lseek(int osfd, int offset, int whence)
{
	((struct file *)osfd)->f_pos = offset;
}

static ssize_t read(int osfd, char *pDataPtr, int readLen)
{
	/* The object must have a read method */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
	if (((struct file *)osfd)->f_op && ((struct file *)osfd)->f_op->read) {
		return ((struct file *)osfd)->f_op->read((struct file *)osfd, pDataPtr, readLen, &((struct file *)osfd)->f_pos);
#else
	if (((struct file *)osfd)->f_mode & FMODE_CAN_READ) {
		return __vfs_read((struct file *)osfd, pDataPtr, readLen,&((struct file *)osfd)->f_pos);
#endif
	}
	printk("find no write method!\n");
	return KERNEL_ERR;
}

static ssize_t write(int osfd, const char *pDataPtr, int writeLen)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	return ((struct file *)osfd)->f_op->write((struct file *)osfd, pDataPtr, (size_t) writeLen, &((struct file *)osfd)->f_pos);
#else
	return __vfs_write((struct file *)osfd), pDataPtr, (size_t) writeLen, &((struct file *)osfd)->f_pos);
#endif
}

static int ioctl(int osfd, unsigned int dataPtr, unsigned long dataLen)
{
	return ((struct file*)osfd)->f_op->unlocked_ioctl((struct file *)osfd, dataPtr, dataLen);
}


static void *malloc(size_t len)
{
	return kmalloc(len, GFP_KERNEL);
}

static void free(void *ptr)
{
	return kfree(ptr);
}


static const struct mtd_info *getMtdInfoKernel(const char *name)
{
	/* the return of getMtdInfo can be minus number!!! Regard it as an int. */
	int ret = (int)get_mtd_device_nm(name);

	if (IS_ERR((const void *)ret))
		return NULL;
	
	return  (const struct mtd_info*)ret;	
}

static char *cstr_strncpy(char *dest, const char *src, size_t n)
{
	char *res = strncpy(dest, src, n);
	dest[n-1] = '\0';
	return res;
}


static int getMtdNum(const char *name)
{

	int ret = 0;
	const struct mtd_info *mtdInfo = NULL;
	
	mtdInfo = getMtdInfoKernel(name);
	if (mtdInfo == NULL)
		return -1;
	
	return (mtdInfo->index);


}

/* NEED REVISION */
static int getFileLen(const char *path)
{
#ifdef __KERNEL__
	/* NOTE: set segment to kernel ds, cause Read and Write use user-space buffer ordinally. */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif
	int ret= 0;
	struct kstat fileStat;
	ret = vfs_stat(path, &fileStat);
	
exit:
#ifdef __KERNEL__
	set_fs(old_fs);
#endif

	if (IS_ERR(ret))
		return -1;
	else
		return fileStat.size;
}

#endif /* __KERNEL__ */


#ifdef __USER_SPACE__
static int getMtdNum(const char *name)
{
	CMM_RET ret = CMM_OK;
	int num = -1;
	char buf[1024];
	
	FILE *mtdFile = fopen("/proc/mtd", "r");
	if (mtdFile == NULL)
		CHECK_CMM_RET(CMM_ERROR, "open /proc/mtd error");

	
	memset(buf, 0, sizeof(buf));
	while(NULL != fgets(buf, sizeof(buf), mtdFile))
		{
		/* TODO: need revision */
		if (strstr(buf, name))
			{
			if (1 != sscanf(buf, "mtd%d", &num))
				CHECK_CMM_RET_NO_PRINT(CMM_ERROR);
				
			break;
			}
		}
	
	if (num == -1)
		CHECK_CMM_RET_NO_PRINT(CMM_ERROR);
exit:
	if (mtdFile)
		fclose(mtdFile);

	if (ret == CMM_OK)
		return num;
	else
		return -1;
}

static int getFileLen(const char *path)
{


	int ret = 0;
	struct stat fileStat;
	
	ret = stat(path, &fileStat);

	if (ret < 0)
		return -1;
	else
		return fileStat.st_size;
	
}



#endif /* __USER_SPACE__ */


static CMM_RET getMtdInfo(const char *path, struct mtd_info_user *mtdInfo)
{
	CMM_RET ret = CMM_OK;

	int mtdFile =0;

#ifdef __KERNEL__
	/* NOTE: set segment to kernel ds, cause Read and Write use user-space buffer ordinally. */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif




	
	mtdFile = open(path, O_RDWR | O_SYNC);
	if (IS_ERR((const void *)mtdFile))
		CHECK_CMM_RET(CMM_ERROR, "open %s error", path);

	if(ioctl(mtdFile, MEMGETINFO, mtdInfo) < 0) {
		CHECK_CMM_RET(CMM_ERROR, "ioctl MEMGETINFO %s error", path);

	}

exit:
	if (!IS_ERR((const void *)mtdFile))
		close(mtdFile);

#ifdef __KERNEL__
		set_fs(old_fs);
#endif

	return ret;

}

static CMM_RET getMtdInfoByName(const char *name, struct mtd_info_user *mtdInfo)
{
	CMM_RET ret = CMM_OK;
	char path[ENTRY_NAME_LEN];
	memset(path, 0, sizeof(path));
	CHECK_CMM_RET_ONLY(getParPath(name, path, sizeof(path)));
	CHECK_CMM_RET_ONLY(getMtdInfo(path, mtdInfo));

exit:
	return ret;
}

static BOOL isFileExist(const char *path)
{
	int ret = FALSE;
	int fp  =0;
#ifdef __KERNEL__
	/* NOTE: set segment to kernel ds, cause Read and Write use user-space buffer ordinally. */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif
	fp = open(path, O_RDONLY | O_SYNC);
	if (IS_ERR((const void *)fp))
		{
		ret = FALSE;
		goto exit;
		}
	else
		{
		close(fp);
		ret =  TRUE;
		goto exit;
		}

exit:
#ifdef __KERNEL__
	set_fs(old_fs);
#endif


	return ret;
}

static CMM_RET getParPath(const char *name, char *path, int pathLen)
{
	CMM_RET ret = CMM_OK;
	int num = 0;
	if (0 <= (num = getMtdNum(name)))
	{
		snprintf(path, pathLen,  "/dev/mtd%d", num);
	}
	else
	{
		/* can't find mtd, check /etc/ */
		int i = 0;
		BOOL found = FALSE;
		for (i=0;l_mtd_mount_dirs[i]!=NULL;i++)
		{
			snprintf(path, pathLen, "%s/%s", l_mtd_mount_dirs[i], name);
			
			if	(isFileExist(path))
			{
				found = TRUE;
				break;
			}
		}

		if	(!found)
		{
			CHECK_CMM_RET(CMM_OK, "can't find partition %s path!", name);	
		}
	}

exit:
	return ret;
}

static int getParLen(const char *path)
{
	struct mtd_info_user mtdInfo;
	/* check if file or mtd device according to path */
	if(strstr(path, "/dev/") == path)
		{
		if (CMM_OK != getMtdInfo(path, &mtdInfo))
			return -1;
		
		return mtdInfo.size;
		}

	return getFileLen(path);
}



static BOOL isRandomWriteEnable(void)
{
	CMM_RET ret = CMM_OK;
	struct mtd_info_user mtdInfo;
	
	CHECK_CMM_RET_ONLY(getMtdInfoByName(FLASH_NAME, &mtdInfo));

	/* for nand flash, skip_bad_block is used, so random write is forbidden, or the data behind may be covered. */
	if (mtdInfo.type == MTD_NANDFLASH)
		return FALSE;


exit:
	return TRUE;
}

static CMM_RET getMtdEraseSize(int *size)
{
	CMM_RET ret = CMM_OK;
	struct mtd_info_user mtdInfo;
	CHECK_CMM_RET_ONLY(getMtdInfoByName(FLASH_NAME, &mtdInfo));

	*size = mtdInfo.erasesize;

exit:
	return ret;
}



/* Factory partition is mounted as ubifs and sub mtd is not used now. by xjb 2018.6.20. */

#if 0
static CMM_RET checkSubMtd(char *name, int nameLen, int *offset, int *size)
{
	CMM_RET ret = CMM_OK;
	sub_mtd_t virPar;
	int newOffset = *offset;
	int newSize = *size;
	int mtdSize = 0;
	int eraseSize = 0;
	/* check if sub mtd partition */
	if (CMM_OK == getSubMtdPartition(name, &virPar))
		{
		if (*size == -1)
			newSize = virPar.sub_mtd.size - *offset;

		if (newSize < 0 || newSize + *offset > virPar.sub_mtd.size)
			CHECK_CMM_RET(CMM_ERROR, "offset+size exceed partition %s size!", name);
			
		newOffset= *offset + virPar.sub_mtd.offset;
		
		if (!isRandomWriteEnable() )
		{
			CHECK_CMM_RET_ONLY(getMtdWithSubSize(virPar.mtdName, &mtdSize));
			CHECK_CMM_RET_ONLY(getMtdEraseSize(&eraseSize));
			if ( (mtdSize <=0) || (mtdSize >eraseSize ))
			{
				CHECK_CMM_RET(CMM_ERROR, "random write not supported! check offset paramter!");
			}
			else
			{
				CDBG_ERROR("Random write not supported. But We can try reading the whole data out first and then write.\n");
					
			}
		}

		cstr_strncpy(name, virPar.mtdName, nameLen);
		*size = newSize;
		*offset = newOffset;	
		}

exit:
	return ret;
}

#endif

	
static CMM_RET checkParPath(const char *name, char *path, int pathLen, int *offset, int *size)
{
	
	/* check mtd first. If not exist, change to /etc/user/ to find the mtd */
	CMM_RET ret = CMM_OK;
	char newName[ENTRY_NAME_LEN];	

	cstr_strncpy(newName, name, sizeof(newName));
	name = newName;

/* Factory partition is mounted as ubifs and sub mtd is not used now. by xjb 2018.6.20. */
#if 0
	CHECK_CMM_RET_ONLY(checkSubMtd(newName, sizeof(newName), offset, size));
#endif	
	CHECK_CMM_RET_ONLY(getParPath(newName, path, pathLen));

	int parLen = getParLen(path);

	if (parLen < 0)
		CHECK_CMM_RET(CMM_ERROR, "get par %s len error!", name);
	
	if (*size == -1)
		*size = parLen -*offset;

	CDBG_ERROR("offset:0x%x, size:0x%x, partition len:0x%x\n", *offset, *size, parLen);
	
	if (*size < 0 || (*offset + *size > parLen))
		CHECK_CMM_RET(CMM_ERROR, "input offset %d size %d error!", *offset, *size);

exit:
	return ret;
}

		

static CMM_RET readPartition(const char* path, char* buf, int offset, int size)
{
	CMM_RET ret = CMM_OK;
	int num = 0;

	int mtdFile = 0;

#ifdef __KERNEL__
	/* NOTE: set segment to kernel ds, cause Read and Write use user-space buffer ordinally. */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif

	mtdFile = open(path, O_RDONLY);

	if (IS_ERR((const void *)mtdFile))
		CHECK_CMM_RET(CMM_ERROR, "open %s error", path);

	lseek(mtdFile, offset, SEEK_SET);

	num = read(mtdFile, buf, size);

	if (IS_ERR((const void *)num))
		CHECK_CMM_RET(CMM_ERROR, "read %s error %d", path, num);
	
	if (num != size)
		CHECK_CMM_RET(CMM_ERROR, "read bytes not equal to required!request 0x%x, read 0x%x", size, num);


exit:
	if (!IS_ERR((const void *)mtdFile))
		close(mtdFile);

#ifdef __KERNEL__
	set_fs(old_fs);
#endif

	return ret;
}

static CMM_RET writePartitionAlign(const char* path, const char* buf, int offset, int size)
{
	CMM_RET ret = CMM_OK;
	int num = 0;
	

	int mtdFile = 0;

#ifdef __KERNEL__
			/* NOTE: set segment to kernel ds, cause Read and Write use user-space buffer ordinally. */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif

	mtdFile = open(path, O_RDWR | O_SYNC);

	if (IS_ERR((const void *)mtdFile))
		CHECK_CMM_RET(CMM_ERROR, "open %s error", path);

	lseek(mtdFile, offset, SEEK_SET);

	num = write(mtdFile, buf, size);

	if (IS_ERR((const void *)num))
		CHECK_CMM_RET(CMM_ERROR, "write %s error", path);
	
	if (num != size)
		CHECK_CMM_RET(CMM_ERROR, "read bytes not equal to required!");


exit:
	if (!IS_ERR((const void *)mtdFile))
		close(mtdFile);
	
#ifdef __KERNEL__
	set_fs(old_fs);
#endif	
	return ret;

}


static CMM_RET erasePartitionMtdAlign(const char *path, int offset, int size)
{
	CMM_RET ret = CMM_OK;
	
	struct erase_info_user mtdEraseInfo;

	int mtdFile = 0;
	
	int eraseSize = 0;

	CHECK_CMM_RET_ONLY(getMtdEraseSize(&eraseSize));
	/* this function calls set_fs too. I don't know if set_fs can be nested, so just move them ahead. */

#ifdef __KERNEL__
			/* NOTE: set segment to kernel ds, cause Read and Write use user-space buffer ordinally. */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif
	
	
	mtdFile = open(path, O_RDWR | O_SYNC);
	if (IS_ERR((const void *)mtdFile))
		CHECK_CMM_RET(CMM_ERROR, "open %s error", path);

	mtdEraseInfo.length = eraseSize;
	
	for (mtdEraseInfo.start = offset;mtdEraseInfo.start < offset + size;
		mtdEraseInfo.start += eraseSize) 
	{
		ioctl(mtdFile, MEMUNLOCK, &mtdEraseInfo);
		if(ioctl(mtdFile, MEMERASE, &mtdEraseInfo) < 0)
		{
			CHECK_CMM_RET(CMM_ERROR, "ioctl MEMERASE %s error", path);
		}
	}
	
	
exit:
	if (!IS_ERR((const void *)mtdFile))
		close(mtdFile);

#ifdef __KERNEL__
		set_fs(old_fs);
#endif	

	return ret;

}


static CMM_RET erasePartitionMtd(const char *path, int offset, int size)
{
	CMM_RET ret = CMM_OK;

	char *pBuf = NULL;

	int mtdSize = 0;
	int erasesize = 0;

	CHECK_CMM_RET_ONLY(getMtdEraseSize(&erasesize));

	pBuf = (char *)malloc(erasesize);
	if (!pBuf)
		CHECK_CMM_RET_ONLY(CMM_ERROR);

	
	int writeSize = 0;
	int offsetTemp = offset;
	int offsetAlignLeft = 0;

	while(offsetTemp < offset+size)
		{
		
		offsetAlignLeft = offsetTemp /erasesize* erasesize;
		
		writeSize = erasesize - (offsetTemp - offsetAlignLeft);

		if (offsetTemp +  writeSize> offset + size)
			writeSize = offset + size - offsetTemp;

		CDBG_ERROR("offsetTemp 0x%x, offsetAlignLeft 0x%x, writeSize 0x%x\n", offsetTemp, offsetAlignLeft, writeSize);
		
		if (offsetAlignLeft < offsetTemp)
			{
			CHECK_CMM_RET_ONLY(readPartition(path, pBuf, offsetAlignLeft, erasesize));
			memset(pBuf + offsetTemp - offsetAlignLeft, 0xff, writeSize);
			CHECK_CMM_RET_ONLY(erasePartitionMtdAlign(path, offsetAlignLeft, erasesize));
			CHECK_CMM_RET_ONLY(writePartitionAlign(path, pBuf, offsetAlignLeft, erasesize));
			}
		else if (writeSize < erasesize)
			{
			CHECK_CMM_RET_ONLY(readPartition(path, pBuf, offsetAlignLeft, erasesize));
			memset(pBuf, 0xff, writeSize);
			CHECK_CMM_RET_ONLY(erasePartitionMtdAlign(path, offsetAlignLeft, erasesize));
			CHECK_CMM_RET_ONLY(writePartitionAlign(path, pBuf, offsetAlignLeft, erasesize));
			}
		else
			{
			CHECK_CMM_RET_ONLY(erasePartitionMtdAlign(path, offsetAlignLeft, erasesize));
			}
		
		

		offsetTemp += writeSize;
		}


	
	
exit:
	if (pBuf)
		free(pBuf);

	/* for erasing, it may return error due to too many bad blocks. But we ignore it. */
	if (!isRandomWriteEnable() && ret != CMM_OK)
		SHOW_CMM_RET(ret, "erasing not ok, but random write is not allowed, so we ignore it.");
	
	return CMM_OK;

}


static CMM_RET erasePartitionFile(const char* path, int offset, int size)
{
	CMM_RET ret = CMM_OK;
	int num = 0;
	

	int mtdFile = -1;
	
#ifdef __KERNEL__
		/* NOTE: set segment to kernel ds, cause Read and Write use user-space buffer ordinally. */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif	

	
	mtdFile = open(path, O_RDWR);

	if (IS_ERR((const void *)mtdFile))
		CHECK_CMM_RET(CMM_ERROR, "open %s error", path);

	
	
	lseek(mtdFile, offset, SEEK_SET);

	

	/* TODO: should use a buffer here? */
	char buffer[512];
	memset(buffer, 0xff, sizeof(buffer));
	int i = 0;
	for (i = 0;i<size/sizeof(buffer);i++)
		write(mtdFile , buffer, sizeof(buffer));

	/* write the left data */
	write(mtdFile, buffer, size - sizeof(buffer) *(size/sizeof(buffer)));
	
exit:
	if (!IS_ERR((const void *)mtdFile))
		close(mtdFile);
	
#ifdef __KERNEL__
		set_fs(old_fs);
#endif		
	return ret;

}



static CMM_RET writePartitionMtd(const char *path, const char* buf, int offset, int size)
{
	CMM_RET ret = CMM_OK;
	
	char *pBuf = NULL;

	int writeSize = 0;
	int offsetTemp = offset;
	int offsetAlignLeft = 0;
	const char *bufTemp = NULL;
	int mtdSize = 0;
	int erasesize = 0;
	CHECK_CMM_RET_ONLY(getMtdEraseSize(&erasesize));

	
	pBuf = (char *)malloc(erasesize);
	if (!pBuf)
		CHECK_CMM_RET_ONLY(CMM_ERROR);



	while(offsetTemp < offset+size)
		{
		
		offsetAlignLeft = offsetTemp /erasesize * erasesize;
		
		writeSize = erasesize - (offsetTemp - offsetAlignLeft);

		if (offsetTemp +  writeSize> offset + size)
			writeSize = offset + size - offsetTemp;

		CDBG_ERROR("offsetTemp 0x%x, offsetAlignLeft 0x%x, writeSize 0x%x\n", offsetTemp, offsetAlignLeft, writeSize);
		
		if (offsetAlignLeft < offsetTemp)
			{
			CHECK_CMM_RET_ONLY(readPartition(path, pBuf, offsetAlignLeft, erasesize));
			memcpy(pBuf + offsetTemp - offsetAlignLeft, buf + offsetTemp - offset, writeSize);

			bufTemp = pBuf;
			}
		else if (writeSize < erasesize)
			{
			CHECK_CMM_RET_ONLY(readPartition(path, pBuf, offsetAlignLeft, erasesize));
			memcpy(pBuf, buf + offsetTemp - offset, writeSize);

			bufTemp = pBuf;
			}
		else
			{
			bufTemp = buf + offsetTemp - offset;
			}
		
		CHECK_CMM_RET_ONLY(erasePartitionMtdAlign(path, offsetAlignLeft, erasesize));
		
		CHECK_CMM_RET_ONLY(writePartitionAlign(path, bufTemp, offsetAlignLeft, erasesize));

		offsetTemp += writeSize;
		}
exit:
	if (pBuf)
		free(pBuf);
	
	return ret;

}


static CMM_RET erasePartition(const char* path, int offset, int size)
{
	CMM_RET ret = CMM_OK;

	/* check if file or mtd device according to path */
	if(strstr(path, "/dev/") == path)
		{
		CHECK_CMM_RET_ONLY(erasePartitionMtd(path, offset, size));
		}
	else
		{
		CHECK_CMM_RET_ONLY(erasePartitionFile(path, offset, size));
		}
	
exit:
	return ret;

}


static CMM_RET writePartition(const char* path, const char *buf, int offset, int size)
{
	CMM_RET ret = CMM_OK;

	/* check if file or mtd device according to path */
	if(strstr(path, "/dev/") == path)
		{
		CHECK_CMM_RET_ONLY(writePartitionMtd(path, buf, offset, size));
		}
	else
		{
		CHECK_CMM_RET_ONLY(writePartitionAlign(path, buf, offset, size));
		}
	
exit:
	return ret;

}

/* Factory partition is mounted as ubifs and sub mtd is not used now. by xjb 2018.6.20. */
#if 0
static CMM_RET getSubMtdParTable(sub_mtd_t par[], int len, int *parNum)
{	

	CMM_RET ret = CMM_OK;
	char buffer[512];
	*parNum = 0;
	int mtdFile = 0;

#ifdef __KERNEL__
	/* NOTE: set segment to kernel ds, cause Read and Write use user-space buffer ordinally. */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif /* __KERNEL__ */

	mtdFile = open(VIRTUAL_PARTITION_CONF, O_RDONLY);

	/* NOTE: DO NOTE check if mtdFile<0. open may return address more than 0x80000000. */
	if (IS_ERR((const void *)mtdFile))
		{
		/* sub mtd partition may not exist. */
		CHECK_CMM_RET_NO_PRINT(CMM_ERROR);
		}

	CDBG_DEBUG("sub partition table %s found. ", VIRTUAL_PARTITION_CONF);
	int dataLen = read(mtdFile, buffer, sizeof(buffer)-1);
	buffer[dataLen]='\0';

	/*virtualmtd=0x00020000(factory-config)@0x00000000(factory),0x00010000(radio)@0x00020000(factory) */
	char *strNext = NULL;
	char *strFound = NULL;
	char *strTemp = NULL;
	if (strstr(buffer, "virtualmtd=") == buffer)
		{
		strNext  = buffer + strlen("virtualmtd=");
		strFound = strsep(&strNext, ",");
		while (strFound && (*parNum < len))
			{
			/* NOTE: sscanf in kernel is limited. '[]' is not supported. */
			strTemp = strstr(strFound, ")");
			if (strTemp)
				{
				*strTemp = ' ';
				strTemp = strstr(strFound, ")");
				if (strTemp)
					{
					*strTemp = ' ';
					}
				}
			/* %s will get all chars until space or '\0' */
			if (4!= sscanf(strFound,"0x%Lx(%s @0x%Lx(%s ", &par[*parNum].sub_mtd.size, 
				par[*parNum].sub_mtd.name, &par[*parNum].sub_mtd.offset,  par[*parNum].mtdName))
				CHECK_CMM_RET(CMM_ERROR, "parse virtual partition conf: %s error!", VIRTUAL_PARTITION_CONF);

			(*parNum) ++;
			
			strFound = strsep(&strNext, ",");
			}
		}
	else
		{
		CHECK_CMM_RET(CMM_ERROR, "parse virtual partition conf: %s error!", VIRTUAL_PARTITION_CONF);
		}
	
exit:
	if (!IS_ERR((const void *)mtdFile))
		close(mtdFile);

#ifdef __KERNEL__
	set_fs(old_fs);
#endif	

	return ret;
}


static CMM_RET getSubMtdPartition(const char *virtualName, sub_mtd_t *virpar)
{
	CMM_RET ret = CMM_OK;
	int i = 0;
	int num = 0;
	sub_mtd_t subMtdParList[ENTRY_MAX_NUM];
	CHECK_CMM_RET_NO_PRINT(getSubMtdParTable(subMtdParList, N_CONST_ARR(subMtdParList), &num));
	for(i=0;i<num;i++)
		{
		if (strcmp(virtualName, subMtdParList[i].sub_mtd.name) == 0)
			{
			*virpar = subMtdParList[i];
			CDBG_ERROR("sub mtd %s is map to %s\n", virtualName, subMtdParList[i].mtdName);
			goto exit;
			}
		}

	CHECK_CMM_RET_NO_PRINT(CMM_ERROR);
exit:
	return ret;
}

static CMM_RET getMtdWithSubSize(const char *mtdName, int *size)
{
	CMM_RET ret = CMM_OK;
	int i = 0;
	int num = 0;
	sub_mtd_t subMtdParList[ENTRY_MAX_NUM];
	const char *subMtdName = NULL;
	*size = 0;
	CHECK_CMM_RET_NO_PRINT(getSubMtdParTable(subMtdParList, N_CONST_ARR(subMtdParList), &num));

	/* get all sub partitions belongs to the mtd */
	for(i=0;i<num;i++)
		{
		if (strcmp(mtdName, subMtdParList[i].mtdName) == 0)
			{
			*size +=  subMtdParList[i].sub_mtd.size;
			}
		}
	
exit:

	return ret;
}
#endif


#ifdef __USER_SPACE__
CMM_RET oal_flash_readPartition(const char* name, char* buf, int offset, int size)
#endif
#ifdef __KERNEL__
int kernel_flash_readPartition(const char* name, char* buf, int offset, int size)
#endif
{
	CMM_RET ret = CMM_OK;

	char path[ENTRY_NAME_LEN];		
	memset(path, 0, sizeof(path));
	CHECK_CMM_RET_ONLY(checkParPath(name, path, sizeof(path), &offset, &size));

	CHECK_CMM_RET_ONLY( readPartition(path, buf, offset, size));

exit:
	return ret;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(kernel_flash_readPartition);
#endif




#ifdef __USER_SPACE__
CMM_RET oal_flash_writePartition(const char* name, const char* buf, int offset, int size)
#endif
#ifdef __KERNEL__
int kernel_flash_writePartition(const char* name, const char* buf, int offset, int size)
#endif
{
	CMM_RET ret = CMM_OK;
	
	char path[ENTRY_NAME_LEN];		
	memset(path, 0, sizeof(path));
	CHECK_CMM_RET_ONLY(checkParPath(name, path, sizeof(path), &offset, &size));

	CHECK_CMM_RET_ONLY(writePartition(path, buf, offset, size));
exit:
	return ret;

}

#ifdef __KERNEL__
EXPORT_SYMBOL(kernel_flash_writePartition);
#endif


#ifdef __USER_SPACE__
CMM_RET oal_flash_erasePartition(const char* name, int offset, int size)
#endif
#ifdef __KERNEL__
int kernel_flash_erasePartition(const char* name, int offset, int size)
#endif
{
	CMM_RET ret = CMM_OK;

	char path[ENTRY_NAME_LEN];		
	memset(path, 0, sizeof(path));
	CHECK_CMM_RET_ONLY(checkParPath(name, path, sizeof(path), &offset, &size));

	CHECK_CMM_RET_ONLY(erasePartition(path, offset, size));
	

exit:
	return ret;	
}
#ifdef __KERNEL__
EXPORT_SYMBOL(kernel_flash_erasePartition);
#endif




#ifdef __USER_SPACE__
CMM_RET oal_flash_write(const char *buf, int offset, int len)
{
	return oal_flash_writePartition(FLASH_NAME, buf, offset, len);
}

BOOL oal_flash_isRandomWriteEnabled(void)
{
	return isRandomWriteEnable();
}


static CMM_RET getImageLen(const char *parName, int *len)
{
	int ret = CMM_OK;
	image_header_t hdr;

	CHECK_CMM_RET_ONLY(oal_flash_readPartition(parName, (char *)&hdr, 0, sizeof(hdr)));

	if (ntohl(hdr.ih_magic) !=IH_MAGIC)
		CHECK_CMM_RET(CMM_ERROR, "Read image header error!");
		

	*len = (ntohl(hdr.ih_size));
exit:
	return ret;
}


CMM_RET oal_flash_copyImage(const char *fromName, const char *toName)
{
	CMM_RET ret = CMM_OK;

	int erasesize = 0;

	char *pBuf = NULL;

	int i = 0;
	int len = 0;
	CHECK_CMM_RET_ONLY(getMtdEraseSize(&erasesize));
	
	CHECK_CMM_RET_ONLY(getImageLen(fromName, &len));

	/* erase it all first */
	CHECK_CMM_RET_ONLY(oal_flash_erasePartition(toName, 0, -1));

	pBuf = (char *)malloc(erasesize);
	if (!pBuf)
		CHECK_CMM_RET_ONLY(CMM_ERROR);

	for (i = 0; i <  (len + sizeof(image_header_t))/erasesize; i++)
		{
		CHECK_CMM_RET_ONLY(oal_flash_readPartition(fromName, pBuf, erasesize * i, erasesize));
		CHECK_CMM_RET_ONLY(oal_flash_writePartition(toName, pBuf, erasesize * i, erasesize));
		}
exit:
	if (pBuf)
		free(pBuf);
	
	return ret;

}

CMM_RET oal_flash_getSize(int *size)
{
	CMM_RET ret = CMM_OK;

	struct mtd_info_user mtdInfo;
	CHECK_CMM_RET_ONLY(getMtdInfoByName(FLASH_NAME, &mtdInfo));
	if (size)
		*size = mtdInfo.size;
exit:
	return ret;
}

CMM_RET oal_flash_getParTable(mtd_partition mtdInfo[], int mtdInfoLen, int *mtdNum)
{
	CMM_RET ret = CMM_OK;
	int num = -1;
	FILE *mtdFile = fopen("/proc/mtd", "r");
	if (mtdFile == NULL)
		CHECK_CMM_RET(CMM_ERROR, "open /proc/mtd error");

	char buf[1024];

	int size = 0;
	int eraseSize = 0;
	int offset =  0;
	int offsetTemp = 0;
	int mtdCount = 0;
	memset(buf, 0, sizeof(buf));
	while((NULL != fgets(buf, sizeof(buf), mtdFile)) && (mtdCount < mtdInfoLen))
		{
		/* Be careful: /proc/mtd doesn't provide offset info. Some partition tables are not contiguous. */

		/* We have modified /proc/mtd. Try 5 elements first */
		if (5 == sscanf(buf, "mtd%d: %x %x \"%[^\"]\" %x", &num, &size, &eraseSize, mtdInfo->name,&offsetTemp))
			{
			/* we only need partable before wholeflash */
			if (strncmp(mtdInfo->name, FLASH_NAME, sizeof(FLASH_NAME)) == 0)
				break;
			mtdInfo->offset = offsetTemp;
			mtdInfo->size = size;
			mtdInfo ++;
			mtdCount ++;
			}
		else
		if (4 == sscanf(buf, "mtd%d: %x %x \"%[^\"]\"", &num, &size, &eraseSize, mtdInfo->name))
			{
			/* we only need partable before wholeflash */
			if (strncmp(mtdInfo->name, FLASH_NAME, sizeof(FLASH_NAME)) == 0)
				break;
			/* assume they are contiguous. */
			mtdInfo->offset = offset;
			mtdInfo->size = size;
			offset += size;
			mtdInfo ++;
			mtdCount ++;
			}
		}
	
	if (eraseSize == 0)
		CHECK_CMM_RET(CMM_ERROR, "can't find mtd info!");
exit:
	if (mtdFile)
		fclose(mtdFile);

	if( (ret == CMM_OK) && mtdNum)
		*mtdNum = mtdCount;
	
	return ret;

}

#endif /*__USER_SPACE__ */


