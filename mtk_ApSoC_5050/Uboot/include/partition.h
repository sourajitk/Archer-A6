#ifndef __PARTITIONS_H__
#define __PARTITIONS_H__


#define ENTRY_NAME_LEN			(32)

typedef struct _mtd_partition {
	char name[ENTRY_NAME_LEN];
	unsigned int size;
	unsigned int offset;
}mtd_partition;

#define MAX_PARTITIONS_NUM 32
extern mtd_partition g_pasStatic_Partition[];
extern int g_part_num;

#endif
