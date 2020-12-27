/*************************************************************/
/*
 * author: 	xiejiabai@tp-link.com.cn
 * brief:	public functions of mt7621
 * history:	2017.05.09, create the file.
 */
/*************************************************************/


/*************************************************************/
/* headers */
/*************************************************************/
#include <common.h>

#include <asm/mipsregs.h>
#include <rt_mmap.h>



#include "ralink_gpio_MT7621.h"
#include "ralink_gpio_reg.h"



/*************************************************************/
/* public functions */
/*************************************************************/
int get_gpio_reg_bit(u32 gpio, u32 *data_reg, u32 *bit, u32 *dir_reg)
{
	/* Get reg and bit of the reg */
	if (gpio > 95)
	{
		printf( ": %s, Unsupport GPIO(%d)\n", __FUNCTION__, gpio);
		return -1;
	}
	if (gpio <= 31)
	{
		/* RALINK_REG_PIODATA for GPIO 0~31 */
		*data_reg = RALINK_REG_PIODATA;
		*dir_reg = RALINK_REG_PIODIR;
		*bit = gpio;
	}
	else if (gpio <= 63)
	{
		/* RALINK_REG_PIO3924DATA for GPIO 32~63 */
		*data_reg = RALINK_REG_PIO6332DATA;
		*dir_reg = RALINK_REG_PIO6332DIR;
		*bit = (gpio - 32);
	}
	else if (gpio <= 95)
	{
		/* RALINK_REG_PIO7140DATA for GPIO 64~95 */
		*data_reg = RALINK_REG_PIO9564DATA;
		*dir_reg = RALINK_REG_PIO9564DIR;
		*bit = (gpio - 64);
	}
	return 0;
}

/* set gpio output */
int set_gpio_data(u32 gpio, u32 data)
{
	u32 reg = 0;
	u32 bit = 0;
	u32 tmp = 0;
	u32 dir_reg = 0;
	int ret = 0;
	ret = get_gpio_reg_bit(gpio, &reg, &bit, &dir_reg);
	if (ret != 0)
		return ret;
	
	/* Set to reg base on bit and data */
	tmp = le32_to_cpu(*(volatile u32 *)(reg));
	
	if (0 == data)
	{
		CLR_BIT(tmp, bit);
	}
	else
	{
		SET_BIT(tmp, bit);
	}
	*(volatile u32 *)(reg) = tmp;
	
	return 0;
}

/* get gpio state */
int get_gpio_data(u32 gpio, u32 *data)
{
	u32 reg = 0;
	u32 bit = 0;
	u32 tmp = 0;
	u32 dir_reg = 0;
	
	int ret = 0;
	ret = get_gpio_reg_bit(gpio, &reg, &bit, &dir_reg);
	if (ret != 0)
		return ret;
	
	/* Set to reg base on bit and data */
	tmp = le32_to_cpu(*(volatile u32 *)(reg));
	
	if (tmp & BIT(bit))
	{
		*data = 1;
	}
	else
	{
		*data = 0;
	}
	
	return 0;
}

/* set gpio direction */
int set_gpio_dir(u32 gpio, u32 dir)
{
	u32 reg = 0;
	u32 bit = 0;
	u32 tmp = 0;
	u32 dir_reg = 0;
	
	int ret = 0;
	ret = get_gpio_reg_bit(gpio, &reg, &bit, &dir_reg);
	if (ret != 0)
		return ret;
	
	/* Set to reg base on bit and data */
	tmp = le32_to_cpu(*(volatile u32 *)(dir_reg));
	if (0 == dir)
	{
		CLR_BIT(tmp, bit);
	}
	else
	{
		SET_BIT(tmp, bit);
	}
	*(volatile u32 *)(dir_reg) = tmp;
	
	return 0;
}

