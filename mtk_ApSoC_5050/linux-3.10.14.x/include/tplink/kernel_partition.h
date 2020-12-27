#ifndef __KERNEL_PARTITION_H__
#define __KERNEL_PARTITION_H__

int kernel_flash_readPartition(const char* name, char* buf, int offset, int size);
int kernel_flash_writePartition(const char* name, const char* buf, int offset, int size);
#endif