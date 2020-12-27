/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <devices.h>
#include <version.h>
#include <net.h>
#include <environment.h>
#include <asm/mipsregs.h>
#include <rt_mmap.h>
#include <spi_api.h>
#include <nand_api.h>

#include <partition.h>
#include <jffs2.h>

DECLARE_GLOBAL_DATA_PTR;
#undef DEBUG

#define SDRAM_CFG1_REG RALINK_SYSCTL_BASE + 0x0304


int modifies= 0;

#ifdef DEBUG
   #define DATE      "05/25/2006"
   #define VERSION   "v0.00e04"
#endif
#if ( ((CFG_ENV_ADDR+CFG_ENV_SIZE) < CFG_MONITOR_BASE) || \
      (CFG_ENV_ADDR >= (CFG_MONITOR_BASE + CFG_MONITOR_LEN)) ) || \
    defined(CFG_ENV_IS_IN_NVRAM)
#define	TOTAL_MALLOC_LEN	(CFG_MALLOC_LEN + CFG_ENV_SIZE)
#else
#define	TOTAL_MALLOC_LEN	CFG_MALLOC_LEN
#endif
#define ARGV_LEN  128

#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)	
static int watchdog_reset();
#endif

extern int timer_init(void);

extern void  rt2880_eth_halt(struct eth_device* dev);

extern void setup_internal_gsw(void); 
//extern void pci_init(void);

extern int incaip_set_cpuclk(void);
extern int do_bootm (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_tftpb (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_mem_cp ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int flash_sect_protect (int p, ulong addr_first, ulong addr_last);
int flash_sect_erase (ulong addr_first, ulong addr_last);
int get_addr_boundary (ulong *addr);
extern int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern void input_value(u8 *str);
#if defined (RT6855_ASIC_BOARD) || defined (RT6855_FPGA_BOARD) || \
    defined (MT7620_ASIC_BOARD) || defined (MT7620_FPGA_BOARD)
extern void rt_gsw_init(void);
#elif defined (RT6855A_ASIC_BOARD) || defined (RT6855A_FPGA_BOARD) 
extern void rt6855A_gsw_init(void);
#elif defined (RT3883_ASIC_BOARD) && defined (MAC_TO_MT7530_MODE)
extern void rt3883_gsw_init(void);
#else
extern void rt305x_esw_init(void);
#endif
extern void LANWANPartition(void);

extern struct eth_device* 	rt2880_pdev;

extern ulong uboot_end_data;
extern ulong uboot_end;
#ifdef RALINK_HTTP_UPGRADE_FUN
extern int NetUipLoop;
#endif
#if defined (RALINK_USB ) || defined (MTK_USB)
extern int usb_stor_curr_dev;
#endif

unsigned char load_addr_str[9];
ulong monitor_flash_len;

const char version_string[] =
	U_BOOT_VERSION" (" __DATE__ " - " __TIME__ ")";



/* this variable stores boot address in flash. 
It will be copied to a special place to be seen by the next uboot.
*/
uboot_data g_uboot_data = {IH_MAGIC, CFG_FLASH_BASE, CFG_FLASH_BASE};


mtd_partition g_pasStatic_Partition[MAX_PARTITIONS_NUM];
int g_part_num = 0;

int g_next_boot_type = IH_TYPE_INVALID;


static int auto_load = 0;


unsigned long mips_cpu_feq;
unsigned long mips_bus_feq;


/*
 * Begin and End of memory area for malloc(), and current "brk"
 */
static ulong mem_malloc_start;
static ulong mem_malloc_end;
static ulong mem_malloc_brk;

static char  file_name_space[ARGV_LEN];

#define read_32bit_cp0_register_with_select1(source)            \
({ int __res;                                                   \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set\treorder\n\t"                                     \
        "mfc0\t%0,"STR(source)",1\n\t"                          \
        ".set\tpop"                                             \
        : "=r" (__res));                                        \
        __res;})

#if defined (CONFIG_DDR_CAL)
__attribute__((nomips16)) void dram_cali(void);
#endif

static void Init_System_Mode(void)
{
	u32 reg;
#ifdef ASIC_BOARD
	u8	clk_sel;
#endif
#if defined(RT5350_ASIC_BOARD)
	u8	clk_sel2;
#endif

	reg = RALINK_REG(RT2880_SYSCFG_REG);
		
	/* 
	 * CPU_CLK_SEL (bit 21:20)
	 */
#ifdef RT2880_FPGA_BOARD
	mips_cpu_feq = 25 * 1000 *1000;
	mips_bus_feq = mips_cpu_feq/2;
#elif defined (RT2883_FPGA_BOARD) || defined (RT3052_FPGA_BOARD) || defined (RT3352_FPGA_BOARD) || defined (RT5350_FPGA_BOARD)
	mips_cpu_feq = 40 * 1000 *1000;
	mips_bus_feq = mips_cpu_feq/3;
#elif defined (RT6855A_FPGA_BOARD)
	mips_cpu_feq = 50 * 1000 *1000;
	mips_bus_feq = mips_cpu_feq/2;
#elif defined (RT3883_FPGA_BOARD)
	mips_cpu_feq = 40 * 1000 *1000;
	mips_bus_feq = mips_cpu_feq;
#elif defined (RT6855_FPGA_BOARD) || defined (MT7620_FPGA_BOARD) || defined (MT7628_FPGA_BOARD)
	mips_cpu_feq = 50 * 1000 *1000;
	mips_bus_feq = mips_cpu_feq/4;
#elif defined (MT7621_FPGA_BOARD)
	mips_cpu_feq = 50 * 1000 *1000;
	mips_bus_feq = mips_cpu_feq/4;
#elif defined (RT2883_ASIC_BOARD) 
	clk_sel = (reg>>20) & 0x03;
	switch(clk_sel) {
		case 0:
			mips_cpu_feq = (380*1000*1000);
			break;
		case 1:
			mips_cpu_feq = (400*1000*1000);
			break;
		case 2:
			mips_cpu_feq = (420*1000*1000);
			break;
		case 3:
			mips_cpu_feq = (430*1000*1000);
			break;
	}
	mips_bus_feq = mips_cpu_feq/2;
#elif defined(RT3052_ASIC_BOARD)
#if defined(RT3350_ASIC_BOARD) 
	//MA10 is floating
	mips_cpu_feq = (320*1000*1000);
#else
        clk_sel = (reg>>18) & 0x01;
        switch(clk_sel) {
                case 0:
                        mips_cpu_feq = (320*1000*1000);
                        break;
                case 1:
                        mips_cpu_feq = (384*1000*1000);
                        break;
        }
#endif
        mips_bus_feq = mips_cpu_feq / 3;
#elif defined(RT3352_ASIC_BOARD)
	clk_sel = (reg>>8) & 0x01;
	switch(clk_sel) {
		case 0:
			mips_cpu_feq = (384*1000*1000);
			break;
		case 1:
			mips_cpu_feq = (400*1000*1000);
			break;
	}
	mips_bus_feq = (133*1000*1000);
#elif defined(RT5350_ASIC_BOARD)
	clk_sel2 = (reg>>10) & 0x01;
	clk_sel = ((reg>>8) & 0x01) + (clk_sel2 * 2);
	switch(clk_sel) {
		case 0:
			mips_cpu_feq = (360*1000*1000);
			mips_bus_feq = (120*1000*1000);
			break;
		case 1:
			//reserved
			break;
		case 2:
			mips_cpu_feq = (320*1000*1000);
			mips_bus_feq = (80*1000*1000);
			break;
		case 3:
			mips_cpu_feq = (300*1000*1000);
			mips_bus_feq = (100*1000*1000);
			break;
	}
#elif defined(RT6855_ASIC_BOARD)
	mips_cpu_feq = (400*1000*1000);
	mips_bus_feq = (133*1000*1000);
#elif defined (RT6855A_ASIC_BOARD)
	/* FPGA is 25/32Mhz
	 * ASIC RT6856/RT63368: DDR(0): 233.33, DDR(1): 175, SDR: 140
	 *      RT6855/RT6855A: DDR(0): 166.67, DDR(1): 125, SDR: 140 */
	reg = RALINK_REG(RT2880_SYSCFG_REG);
	if ((reg & (1 << 25)) == 0) { /* SDR */
		if ((reg & (1 << 9)) != 0)
			mips_cpu_feq = (560*1000*1000);
		else {
			if ((reg & (1 << 26)) != 0)	
				mips_cpu_feq = (560*1000*1000);
			else
				mips_cpu_feq = (420*1000*1000);
		}	
		mips_bus_feq = (140*1000*1000);
	} else { /* DDR */
		if ((reg & (1 << 9)) != 0) {
			mips_cpu_feq = (700*1000*1000);
			if ((reg & (1 << 26)) != 0)
				mips_bus_feq = (175*1000*1000);
			else
				mips_bus_feq = 233333333;
		} else {
			mips_cpu_feq = (500*1000*1000);
			if ((reg & (1 << 26)) != 0)
				mips_bus_feq = (125*1000*1000);
			else
				mips_bus_feq = 166666667;
		}
	}
#elif defined(MT7620_ASIC_BOARD)
	reg = RALINK_REG(RALINK_CPLLCFG1_REG);
	if( reg & ((0x1UL) << 24) ){
		mips_cpu_feq = (480*1000*1000);	/* from BBP PLL */
	}else{
		reg = RALINK_REG(RALINK_CPLLCFG0_REG);
		if(!(reg & CPLL_SW_CONFIG)){
			mips_cpu_feq = (600*1000*1000); /* from CPU PLL */
		}else{
			/* read CPLL_CFG0 to determine real CPU clock */
			int mult_ratio = (reg & CPLL_MULT_RATIO) >> CPLL_MULT_RATIO_SHIFT;
			int div_ratio = (reg & CPLL_DIV_RATIO) >> CPLL_DIV_RATIO_SHIFT;
			mult_ratio += 24;       /* begin from 24 */
			if(div_ratio == 0)      /* define from datasheet */
				div_ratio = 2;
			else if(div_ratio == 1)
				div_ratio = 3;
			else if(div_ratio == 2)
				div_ratio = 4;
			else if(div_ratio == 3)
				div_ratio = 8;
			mips_cpu_feq = ((BASE_CLOCK * mult_ratio ) / div_ratio) * 1000 * 1000;
		}
	}
	reg = (RALINK_REG(RT2880_SYSCFG_REG)) >> 4 & 0x3;
	if(reg == 0x0){				/* SDR (MT7620 E1) */
		mips_bus_feq = mips_cpu_feq/4;
	}else if(reg == 0x1 || reg == 0x2 ){	/* DDR1 & DDR2 */
		mips_bus_feq = mips_cpu_feq/3;
	}else{					/* SDR (MT7620 E2) */
		mips_bus_feq = mips_cpu_feq/5;
	}
#elif defined (MT7628_ASIC_BOARD)
	reg = RALINK_REG(RALINK_CLKCFG0_REG);
	if (reg & (0x1<<1)) {
		mips_cpu_feq = (480*1000*1000)/CPU_FRAC_DIV;
	}else if (reg & 0x1) {
		mips_cpu_feq = ((RALINK_REG(RALINK_SYSCTL_BASE+0x10)>>6)&0x1) ? (40*1000*1000)/CPU_FRAC_DIV \
					   : (25*1000*1000)/CPU_FRAC_DIV;
	}else {
		if ((RALINK_REG(RALINK_SYSCTL_BASE+0x10)>>6)&0x1)
			mips_cpu_feq = (580*1000*1000)/CPU_FRAC_DIV;
		else
			mips_cpu_feq = (575*1000*1000)/CPU_FRAC_DIV;
	}
	mips_bus_feq = mips_cpu_feq/3;
#elif defined(MT7621_ASIC_BOARD)
	reg = RALINK_REG(RALINK_SYSCTL_BASE + 0x2C);
	if( reg & ((0x1UL) << 30)) {
		reg = RALINK_REG(RALINK_MEMCTRL_BASE + 0x648);
		mips_cpu_feq = (((reg >> 4) & 0x7F) + 1) * 1000 * 1000;
		reg = RALINK_REG(RALINK_SYSCTL_BASE + 0x10);
		reg = (reg >> 6) & 0x7;
		if(reg >= 6) { //25Mhz Xtal
			mips_cpu_feq = mips_cpu_feq * 25;
		} else if (reg >=3) { //40Mhz Xtal
			mips_cpu_feq = mips_cpu_feq * 20;
		} else { //20Mhz Xtal
			/* TODO */
		}
	}else {
		reg = RALINK_REG(RALINK_SYSCTL_BASE + 0x44);
		mips_cpu_feq = (500 * (reg & 0x1F) / ((reg >> 8) & 0x1F)) * 1000 * 1000;
	}
	mips_bus_feq = mips_cpu_feq/4;
#elif defined (RT3883_ASIC_BOARD) 
	clk_sel = (reg>>8) & 0x03;
	switch(clk_sel) {
		case 0:
			mips_cpu_feq = (250*1000*1000);
			break;
		case 1:
			mips_cpu_feq = (384*1000*1000);
			break;
		case 2:
			mips_cpu_feq = (480*1000*1000);
			break;
		case 3:
			mips_cpu_feq = (500*1000*1000);
			break;
	}
#if defined (CFG_ENV_IS_IN_SPI)
	if ((reg>>17) & 0x1) { //DDR2
		switch(clk_sel) {
			case 0:
				mips_bus_feq = (125*1000*1000);
				break;
			case 1:
				mips_bus_feq = (128*1000*1000);
				break;
			case 2:
				mips_bus_feq = (160*1000*1000);
				break;
			case 3:
				mips_bus_feq = (166*1000*1000);
				break;
		}
	}
	else {
		switch(clk_sel) {
			case 0:
				mips_bus_feq = (83*1000*1000);
				break;
			case 1:
				mips_bus_feq = (96*1000*1000);
				break;
			case 2:
				mips_bus_feq = (120*1000*1000);
				break;
			case 3:
				mips_bus_feq = (125*1000*1000);
				break;
		}
	}
#elif defined ON_BOARD_SDR
	switch(clk_sel) {
		case 0:
			mips_bus_feq = (83*1000*1000);
			break;
		case 1:
			mips_bus_feq = (96*1000*1000);
			break;
		case 2:
			mips_bus_feq = (120*1000*1000);
			break;
		case 3:
			mips_bus_feq = (125*1000*1000);
			break;
	}
#elif defined ON_BOARD_DDR2
	switch(clk_sel) {
		case 0:
			mips_bus_feq = (125*1000*1000);
			break;
		case 1:
			mips_bus_feq = (128*1000*1000);
			break;
		case 2:
			mips_bus_feq = (160*1000*1000);
			break;
		case 3:
			mips_bus_feq = (166*1000*1000);
			break;
	}
#else
#error undef SDR or DDR
#endif
#else /* RT2880 ASIC version */
	clk_sel = (reg>>20) & 0x03;
	switch(clk_sel) {
		#ifdef RT2880_MP
			case 0:
			mips_cpu_feq = (250*1000*1000);
			break;
		case 1:
			mips_cpu_feq = (266*1000*1000);
			break;
		case 2:
			mips_cpu_feq = (280*1000*1000);
			break;
		case 3:
			mips_cpu_feq = (300*1000*1000);
			break;
		#else
			case 0:
			mips_cpu_feq = (233*1000*1000);
			break;
		case 1:
			mips_cpu_feq = (250*1000*1000);
			break;
		case 2:
			mips_cpu_feq = (266*1000*1000);
			break;
		case 3:
			mips_cpu_feq = (280*1000*1000);
			break;
		
		#endif
	}
	mips_bus_feq = mips_cpu_feq/2;
#endif

   	//RALINK_REG(RT2880_SYSCFG_REG) = reg;

	/* in general, the spec define 8192 refresh cycles/64ms
	 * 64ms/8192 = 7.8us
	 * 7.8us * 106.7Mhz(SDRAM clock) = 832
	 * the value of refresh cycle shall smaller than 832. 
	 * so we config it at 0x300 (suggested by ASIC)
	 */
#if defined(ON_BOARD_SDR) && defined(ON_BOARD_256M_DRAM_COMPONENT) && (!defined(MT7620_ASIC_BOARD))
	{
	u32 tREF;
	tREF = RALINK_REG(SDRAM_CFG1_REG);
	tREF &= 0xffff0000;
#if defined(ASIC_BOARD)
	tREF |= 0x00000300;
#elif defined(FPGA_BOARD) 
	tREF |= 0x000004B;
#else
#error "not exist"
#endif
	RALINK_REG(SDRAM_CFG1_REG) = tREF;
	}
#endif

}


/*
 * The Malloc area is immediately below the monitor copy in DRAM
 */
static void mem_malloc_init (void)
{

	ulong dest_addr = CFG_MONITOR_BASE + gd->reloc_off;

	mem_malloc_end = dest_addr;
	mem_malloc_start = dest_addr - TOTAL_MALLOC_LEN;
	mem_malloc_brk = mem_malloc_start;

	memset ((void *) mem_malloc_start,
		0,
		mem_malloc_end - mem_malloc_start);
}

void *sbrk (ptrdiff_t increment)
{
	ulong old = mem_malloc_brk;
	ulong new = old + increment;

	if ((new < mem_malloc_start) || (new > mem_malloc_end)) {
		return (NULL);
	}
	mem_malloc_brk = new;
	return ((void *) old);
}

static int init_func_ram (void)
{

#ifdef	CONFIG_BOARD_TYPES
	int board_type = gd->board_type;
#else
	int board_type = 0;	/* use dummy arg */
#endif
	puts ("DRAM:  ");

/*init dram config*/
#ifdef RALINK_DDR_OPTIMIZATION
#ifdef ON_BOARD_DDR2
/*optimize ddr parameter*/
{	
	u32 tDDR;
	tDDR = RALINK_REG(DDR_CFG0_REG);

        tDDR &= 0xf0780000; 
	tDDR |=  RAS_VALUE << RAS_OFFSET;
	tDDR |=  TRFC_VALUE << TRFC_OFFSET;
	tDDR |=  TRFI_VALUE << TRFI_OFFSET;
	RALINK_REG(DDR_CFG0_REG) = tDDR;
}
#endif
#endif


	if ((gd->ram_size = initdram (board_type)) > 0) {
		print_size (gd->ram_size, "\n");
		return (0);  
	}
	puts ("*** failed ***\n");

	return (1);
}

static int display_banner(void)
{
   
	printf ("\n\n%s\n\n", version_string);
	return (0);
}

/*
static void display_flash_config(ulong size)
{
	puts ("Flash: ");
	print_size (size, "\n");
}
*/

static int init_baudrate (void)
{
	//uchar tmp[64]; /* long enough for environment variables */
	//int i = getenv_r ("baudrate", tmp, sizeof (tmp));
	//kaiker 
	gd->baudrate = CONFIG_BAUDRATE;
/*
	gd->baudrate = (i > 0)
			? (int) simple_strtoul (tmp, NULL, 10)
			: CONFIG_BAUDRATE;
*/
	return (0);
}


/*
 * Breath some life into the board...
 *
 * The first part of initialization is running from Flash memory;
 * its main purpose is to initialize the RAM so that we
 * can relocate the monitor code to RAM.
 */

/*
 * All attempts to come up with a "common" initialization sequence
 * that works for all boards and architectures failed: some of the
 * requirements are just _too_ different. To get rid of the resulting
 * mess of board dependend #ifdef'ed code we now make the whole
 * initialization sequence configurable to the user.
 *
 * The requirements for any new initalization function is simple: it
 * receives a pointer to the "global data" structure as it's only
 * argument, and returns an integer return code, where 0 means
 * "continue" and != 0 means "fatal error, hang the system".
 */
#if 0
typedef int (init_fnc_t) (void);

init_fnc_t *init_sequence[] = {
	timer_init,
	env_init,		/* initialize environment */
	init_baudrate,		/* initialze baudrate settings */
	serial_init,		/* serial communications setup */
	console_init_f,
	display_banner,		/* say that we are here */
	checkboard,
	init_func_ram,
	NULL,
};
#endif

//  
__attribute__((nomips16)) void board_init_f(ulong bootflag)
{
	gd_t gd_data, *id;
	bd_t *bd;  
	//init_fnc_t **init_fnc_ptr;
	ulong addr, addr_sp, len = (ulong)&uboot_end - CFG_MONITOR_BASE;
	ulong *s;
	u32 value;
	void (*ptr)(void);
	u32 fdiv = 0, step = 0, frac = 0, i;

#if defined RT6855_FPGA_BOARD || defined RT6855_ASIC_BOARD || \
    defined MT7620_FPGA_BOARD || defined MT7620_ASIC_BOARD
	value = le32_to_cpu(*(volatile u_long *)(RALINK_SPI_BASE + 0x10));
	value &= ~(0x7);
	value |= 0x2;
	*(volatile u_long *)(RALINK_SPI_BASE + 0x10) = cpu_to_le32(value);	
#elif defined MT7621_FPGA_BOARD || defined MT7628_FPGA_BOARD
	value = le32_to_cpu(*(volatile u_long *)(RALINK_SPI_BASE + 0x3c));
	value &= ~(0xFFF);
	*(volatile u_long *)(RALINK_SPI_BASE + 0x3c) = cpu_to_le32(value);	
#elif defined MT7621_ASIC_BOARD
	value = le32_to_cpu(*(volatile u_long *)(RALINK_SPI_BASE + 0x3c));
	value &= ~(0xFFF);
	value |= 5; //work-around 3-wire SPI issue (3 for RFB, 5 for EVB)
	*(volatile u_long *)(RALINK_SPI_BASE + 0x3c) = cpu_to_le32(value);	
#elif  defined MT7628_ASIC_BOARD
	value = le32_to_cpu(*(volatile u_long *)(RALINK_SPI_BASE + 0x3c));
	value &= ~(0xFFF);
	value |= 8;
	*(volatile u_long *)(RALINK_SPI_BASE + 0x3c) = cpu_to_le32(value);	
#endif

#if defined(MT7620_FPGA_BOARD) || defined(MT7620_ASIC_BOARD)
/* Adjust CPU Freq from 60Mhz to 600Mhz(or CPLL freq stored from EE) */
	value = RALINK_REG(RT2880_SYSCLKCFG_REG);
	fdiv = ((value>>8)&0x1F);
	step = (unsigned long)(value&0x1F);
	while(step < fdiv) {
		value = RALINK_REG(RT2880_SYSCLKCFG_REG);
		step = (unsigned long)(value&0x1F) + 1;
		value &= ~(0x1F);
		value |= (step&0x1F);
		RALINK_REG(RT2880_SYSCLKCFG_REG) = value;
		udelay(10);
	};	
#elif defined(MT7628_ASIC_BOARD)
	value = RALINK_REG(RALINK_DYN_CFG0_REG);
	fdiv = (unsigned long)((value>>8)&0x0F);
	if ((CPU_FRAC_DIV < 1) || (CPU_FRAC_DIV > 10))
	frac = (unsigned long)(value&0x0F);
	else
		frac = CPU_FRAC_DIV;
	i = 0;
	
	while(frac < fdiv) {
		value = RALINK_REG(RALINK_DYN_CFG0_REG);
		fdiv = ((value>>8)&0x0F);
		fdiv--;
		value &= ~(0x0F<<8);
		value |= (fdiv<<8);
		RALINK_REG(RALINK_DYN_CFG0_REG) = value;
		udelay(500);
		i++;
		value = RALINK_REG(RALINK_DYN_CFG0_REG);
		fdiv = ((value>>8)&0x0F);
		//frac = (unsigned long)(value&0x0F);
	}	
#elif defined (MT7621_ASIC_BOARD)
#if (MT7621_CPU_FREQUENCY!=50)
	value = RALINK_REG(RALINK_CUR_CLK_STS_REG);
	fdiv = ((value>>8)&0x1F);
	frac = (unsigned long)(value&0x1F);

	i = 0;
	
	while(frac < fdiv) {
		value = RALINK_REG(RALINK_DYN_CFG0_REG);
		fdiv = ((value>>8)&0x0F);
		fdiv--;
		value &= ~(0x0F<<8);
		value |= (fdiv<<8);
		RALINK_REG(RALINK_DYN_CFG0_REG) = value;
		udelay(500);
		i++;
		value = RALINK_REG(RALINK_CUR_CLK_STS_REG);
		fdiv = ((value>>8)&0x1F);
		frac = (unsigned long)(value&0x1F);
	}
	
#endif
#if ((MT7621_CPU_FREQUENCY!=50) && (MT7621_CPU_FREQUENCY!=500))
	//change CPLL from GPLL to MEMPLL
	value = RALINK_REG(RALINK_CLKCFG0_REG);
	value &= ~(0x3<<30);
	value |= (0x1<<30);
	RALINK_REG(RALINK_CLKCFG0_REG) = value;	
#endif
#endif

#ifdef CONFIG_PURPLE
	void copy_code (ulong); 
#endif
	//*pio_mode = 0xFFFF;

	/* Pointer is writable since we allocated a register for it.
	 */
	gd = &gd_data;
	/* compiler optimization barrier needed for GCC >= 3.4 */
	__asm__ __volatile__("": : :"memory");
	
		
	memset ((void *)gd, 0, sizeof (gd_t));

#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)	
	watchdog_reset();
#endif
	timer_init();
	env_init();		/* initialize environment */
	init_baudrate();		/* initialze baudrate settings */
	serial_init();		/* serial communications setup */
	console_init_f();
#if (TEXT_BASE == 0xBFC00000) || (TEXT_BASE == 0xBF000000) || (TEXT_BASE == 0xBC000000)
#if defined (CONFIG_DDR_CAL)	
	ptr = dram_cali;
	ptr = (void*)((u32)ptr & ~(1<<29));
	(*ptr)();
#endif	
#endif
	display_banner();		/* say that we are here */
	checkboard();
	init_func_ram(); 

	/* reset Frame engine */
	value = le32_to_cpu(*(volatile u_long *)(RALINK_SYSCTL_BASE + 0x0034));
	udelay(100);    
#if defined (RT2880_FPGA_BOARD) || defined (RT2880_ASIC_BOARD)
	value |= (1 << 18);
#elif defined (MT7621_FPGA_BOARD) || defined (MT7621_ASIC_BOARD)
	/* nothing */
	//value |= (1<<26 | 1<<25 | 1<<24); /* PCIE de-assert for E3 */
#else
	//2880 -> 3052 reset Frame Engine from 18 to 21
	value |= (1 << 21);
#endif
	*(volatile u_long *)(RALINK_SYSCTL_BASE + 0x0034) = cpu_to_le32(value);	
#if defined (RT2880_FPGA_BOARD) || defined (RT2880_ASIC_BOARD)
	value &= ~(1 << 18);
#elif defined (MT7621_FPGA_BOARD) || defined (MT7621_ASIC_BOARD)
	/* nothing */
#else
	value &= ~(1 << 21);
#endif
	*(volatile u_long *)(RALINK_SYSCTL_BASE + 0x0034) = cpu_to_le32(value);	
	udelay(200);      

#if 0	
	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
	
		if ((*init_fnc_ptr)() != 0) {
			hang ();
		}
	}
#endif
#ifdef DEBUG	
	debug("rt2880 uboot %s %s\n", VERSION, DATE);
#endif

	/*
	 * Now that we have DRAM mapped and working, we can
	 * relocate the code and continue running from DRAM.
	 */
	addr = CFG_SDRAM_BASE + gd->ram_size;

	/* We can reserve some RAM "on top" here.
	 */
#ifdef DEBUG	    
	debug ("SERIAL_CLOCK_DIVISOR =%d \n", SERIAL_CLOCK_DIVISOR);
	debug ("kaiker,,CONFIG_BAUDRATE =%d \n", CONFIG_BAUDRATE); 
	debug ("SDRAM SIZE:%08X\n",gd->ram_size);
#endif

	/* round down to next 4 kB limit.
	 */
	addr &= ~(4096 - 1);
#ifdef DEBUG
	debug ("Top of RAM usable for U-Boot at: %08lx\n", addr);
#endif	 
   
	/* Reserve memory for U-Boot code, data & bss
	 * round down to next 16 kB limit
	 */
	addr -= len;
	addr &= ~(16 * 1024 - 1);
#ifdef DEBUG
	debug ("Reserving %ldk for U-Boot at: %08lx\n", len >> 10, addr);
#endif
	 /* Reserve memory for malloc() arena.
	 */
	addr_sp = addr - TOTAL_MALLOC_LEN;
#ifdef DEBUG
	debug ("Reserving %dk for malloc() at: %08lx\n",
			TOTAL_MALLOC_LEN >> 10, addr_sp);
#endif
	/*
	 * (permanently) allocate a Board Info struct
	 * and a permanent copy of the "global" data
	 */
	addr_sp -= sizeof(bd_t);
	bd = (bd_t *)addr_sp;
	gd->bd = bd;
#ifdef DEBUG
	debug ("Reserving %d Bytes for Board Info at: %08lx\n",
			sizeof(bd_t), addr_sp);
#endif
	addr_sp -= sizeof(gd_t);
	id = (gd_t *)addr_sp;
#ifdef DEBUG
	debug ("Reserving %d Bytes for Global Data at: %08lx\n",
			sizeof (gd_t), addr_sp);
#endif
 	/* Reserve memory for boot params.
	 */
	addr_sp -= CFG_BOOTPARAMS_LEN;
	bd->bi_boot_params = addr_sp;
#ifdef DEBUG
	debug ("Reserving %dk for boot params() at: %08lx\n",
			CFG_BOOTPARAMS_LEN >> 10, addr_sp);
#endif
	/*
	 * Finally, we set up a new (bigger) stack.
	 *
	 * Leave some safety gap for SP, force alignment on 16 byte boundary
	 * Clear initial stack frame
	 */
	addr_sp -= 16;
	addr_sp &= ~0xF;
	s = (ulong *)addr_sp;
	*s-- = 0;
	*s-- = 0;
	addr_sp = (ulong)s;
#ifdef DEBUG
	debug ("Stack Pointer at: %08lx\n", addr_sp);
#endif

	/*
	 * Save local variables to board info struct
	 */
	bd->bi_memstart	= CFG_SDRAM_BASE;	/* start of  DRAM memory */
	bd->bi_memsize	= gd->ram_size;		/* size  of  DRAM memory in bytes */
	bd->bi_baudrate	= gd->baudrate;		/* Console Baudrate */

	memcpy (id, (void *)gd, sizeof (gd_t));

	/* On the purple board we copy the code in a special way
	 * in order to solve flash problems
	 */
#ifdef CONFIG_PURPLE
	copy_code(addr);
#endif

    
#if defined(CFG_RUN_CODE_IN_RAM)
	/* 
	 * tricky: relocate code to original TEXT_BASE
	 * for ICE souce level debuggind mode 
	 */	
	debug ("relocate_code Pointer at: %08lx\n", addr);
	relocate_code (addr_sp, id, /*TEXT_BASE*/ addr);	
#else
	debug ("relocate_code Pointer at: %08lx\n", addr);
	relocate_code (addr_sp, id, addr);
#endif

	/* NOTREACHED - relocate_code() does not return */
}

#define SEL_LOAD_LINUX_WRITE_FLASH_BY_SERIAL 0
#define SEL_LOAD_LINUX_SDRAM            1
#define SEL_LOAD_LINUX_WRITE_FLASH      2
#define SEL_BOOT_FLASH                  3
#define SEL_ENTER_CLI                   4
#define SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL 7
#define SEL_LOAD_BOOT_SDRAM             8
#define SEL_LOAD_BOOT_WRITE_FLASH       9

#if 0
void OperationSelect(void)
{
	printf("\nPlease choose the operation: \n");
	printf("   %d: Load system code to SDRAM via TFTP. \n", SEL_LOAD_LINUX_SDRAM);
	printf("   %d: Load system code then write to Flash via TFTP. \n", SEL_LOAD_LINUX_WRITE_FLASH);
	printf("   %d: Boot system code via Flash (default).\n", SEL_BOOT_FLASH);
#ifdef RALINK_CMDLINE
	printf("   %d: Entr boot command line interface.\n", SEL_ENTER_CLI);
#endif // RALINK_CMDLINE //
#ifdef RALINK_UPGRADE_BY_SERIAL
	printf("   %d: Load Boot Loader code then write to Flash via Serial. \n", SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL);
#endif // RALINK_UPGRADE_BY_SERIAL //
	printf("   %d: Load Boot Loader code then write to Flash via TFTP. \n", SEL_LOAD_BOOT_WRITE_FLASH);
}
#else
void OperationSelect(void)
{
	printf("\nPress '%c' or 't' to break the booting process\n", CONFIG_BREAK_BOOTING_BUTTON + '0');
#if defined(MINI_WEB_SERVER_SUPPORT)
	printf("\nPress 'x' to enter recovery web server\n");
#endif
}
#endif

int tftp_config(int type, char *argv[])
{
	char *s;
	char default_file[ARGV_LEN], file[ARGV_LEN], devip[ARGV_LEN], srvip[ARGV_LEN], default_ip[ARGV_LEN];

	printf(" Please Input new ones /or Ctrl-C to discard\n");

	memset(default_file, 0, ARGV_LEN);
	memset(file, 0, ARGV_LEN);
	memset(devip, 0, ARGV_LEN);
	memset(srvip, 0, ARGV_LEN);
	memset(default_ip, 0, ARGV_LEN);

	printf("\tInput device IP ");
	s = getenv("ipaddr");
	memcpy(devip, s, strlen(s));
	memcpy(default_ip, s, strlen(s));

	printf("(%s) ", devip);
	if (auto_load == 0)
		input_value(devip);
	setenv("ipaddr", devip);
	if (strcmp(default_ip, devip) != 0)
		modifies++;

	printf("\tInput server IP ");
	s = getenv("serverip");
	memcpy(srvip, s, strlen(s));
	memset(default_ip, 0, ARGV_LEN);
	memcpy(default_ip, s, strlen(s));

	printf("(%s) ", srvip);
	if (auto_load == 0)
		input_value(srvip);
	setenv("serverip", srvip);
	if (strcmp(default_ip, srvip) != 0)
		modifies++;

	if((type == SEL_LOAD_BOOT_SDRAM) || 
	   (type == SEL_LOAD_BOOT_WRITE_FLASH)) {
		sprintf(load_addr_str, "%8X", load_addr);
		argv[1]=load_addr_str;
		printf("\tInput Uboot filename ");
	}
	else if ((type == SEL_LOAD_LINUX_WRITE_FLASH) || 
		 (type == SEL_LOAD_LINUX_SDRAM)) {
		sprintf(load_addr_str, "%8X", load_addr);
		argv[1]=load_addr_str;
		printf("\tInput Linux Kernel filename ");
	}

	s = getenv("bootfile");
	if (s != NULL) {
		memcpy(file, s, strlen(s));
		memcpy(default_file, s, strlen(s));
	}
	printf("(%s) ", file);
	if (auto_load == 0)
		input_value(file);
	if (file == NULL)
		return 1;
	copy_filename (argv[2], file, sizeof(file));
	setenv("bootfile", file);
	if (strcmp(default_file, file) != 0)
		modifies++;

	return 0;
}

void trigger_hw_reset(void)
{
#ifdef GPIO14_RESET_MODE
        //set GPIO14 as output to trigger hw reset circuit
        RALINK_REG(RT2880_REG_PIODIR)|=1<<14; //output mode

        RALINK_REG(RT2880_REG_PIODATA)|=1<<14; //pull high
	udelay(100);
        RALINK_REG(RT2880_REG_PIODATA)&=~(1<<14); //pull low
#endif
}



int copy_image(unsigned long image_size, ulong from_image_addr, ulong to_image_addr) 
{
	int ret = 0;
#ifdef CFG_ENV_IS_IN_FLASH
	unsigned long e_end, len;
#endif
	
	
#if defined (CFG_ENV_IS_IN_NAND) || defined (CFG_ENV_IS_IN_SPI)
	u8 from_flash = 0, to_flash = 0;
	ulong load_addr = from_image_addr;

	if (from_image_addr >= CFG_FLASH_BASE)
		{
		from_flash = 1;
		from_image_addr -=CFG_FLASH_BASE;
		load_addr = CFG_SPINAND_LOAD_ADDR;
		}
	
	if (to_image_addr >= CFG_FLASH_BASE)
		{
		to_flash = 1;
		to_image_addr -=CFG_FLASH_BASE;
		}

#endif
	
	{
#if defined (CFG_ENV_IS_IN_NAND)
		printf("\nCopy Image:\nImage(0x%X) to Image(0x%X), size=0x%X\n",
				from_image_addr,
				to_image_addr, image_size);

		if (from_flash)
			ranand_read((char *)load_addr, from_image_addr, image_size);
		
		if (to_flash)
			ret = ranand_erase_write((char *)load_addr, to_image_addr, image_size);
		else
			ret = memmove(to_image_addr,  load_addr, image_size);
#elif defined (CFG_ENV_IS_IN_SPI)
		printf("\nCopy Image:\nImage(0x%X) to Image(0x%X), size=0x%X\n",
				from_image_addr,
				to_image_addr, image_size);
		if (from_flash)
			raspi_read((char *)load_addr, from_image_addr, image_size);

		if (to_flash)
			ret = raspi_erase_write((char *)load_addr, to_image_addr, image_size);
		else
			ret = memmove(to_image_addr,  load_addr, image_size);
#else //CFG_ENV_IS_IN_FLASH
		printf("\nCopy Image:\nImage(0x%X) to Image(0x%X), size=0x%X\n", from_image_addr, to_image_addr, image_size);
		e_end = to_image_addr + image_size - 1;
		if (get_addr_boundary(&e_end) != 0)
			return -1;
		printf("Erase from 0x%X to 0x%X\n", to_image_addr, e_end);
		flash_sect_erase(to_image_addr, e_end);
		memcpy(CFG_LOAD_ADDR, (void *)from_image_addr, image_size);
		ret = flash_write((uchar *)CFG_LOAD_ADDR, (ulong)to_image_addr, image_size);
#endif
	}
}


int debug_mode(void)
{
	printf("Upgrade Mode~~\n");

	
	return 0;
}

static int flash_read(char *buf, unsigned int addr, ulong length)
{
	int ret = -1;

#if defined (CFG_ENV_IS_IN_NAND)
	ret = ranand_read(buf, addr - CFG_FLASH_BASE, length);
#elif defined (CFG_ENV_IS_IN_SPI)
	ret = raspi_read(buf, addr - CFG_FLASH_BASE, length);
#else //CFG_ENV_IS_IN_FLASH
	memmove(buf,addr, length);
	ret = length;
#endif
	return ret;
}

#if defined(CHECK_DUAL_IMAGE_AND_BOOT) || defined(BOOT_NEXT_IMAGE)
//#define DEBUG_JFFS2
int env_mem_set(char * key, char * value)
{
	char * env_mem_base = (char *)CFG_BOOT_ENV_MEM_ADDR;
	int * magic = (int *)env_mem_base;
	int * len = (int *)(env_mem_base + sizeof(int));
	char * env_data = env_mem_base + sizeof(int) * 2;	
	int i = 0;
	int j = 0;

	if (key == NULL)
	{
		return -1;
	}
	
	if (*magic != ENV_MAGIC)
	{
		printf("[set]mem environment not init yet.\n");
		memset(env_mem_base, 0, CFG_BOOT_ENV_MEM_SIZE);
		*magic = ENV_MAGIC;
		*len = 1;
		env_data[0] = '\0';		
	}

	if (value && (*len + strlen(key)+ strlen(value) + 2 > CFG_BOOT_ENV_MEM_SIZE))
	{
		return -1;
	}
	
	//printf("[set]mem environment set %s=%s\n", key, value ? value : "");

	while ( i < *len)
	{
		if(strncmp(env_data + i, key, strlen(key)))
		{
			while(env_data[i] != ';' && env_data[i] != '\0')
				i++;
			i++;
		}
		else
		{
			j = strlen(key);
			if (env_data[i + j] != '=')
			{
				while(env_data[i] != ';' && env_data[i] != '\0')
					i++;
				i++;
				continue;
			}

			for (j = 0; j < strlen(key); j++)
				env_data[i + j] = '#';

			i +=  strlen(key) + 1;

			while (env_data[i] != ';' && env_data[i] != '\0')
			{
				env_data[i] = '#';
				i++;
			}
			break;
		}
	}

	if (value)
	{
		*len += sprintf(env_data + *len - 1, "%s=%s;", key, value);
	}
	*(env_data + *len - 1) = '\0';  

	return 0;
}

int env_mem_get(char * key, char * value)
{
	char * env_mem_base = (char *)CFG_BOOT_ENV_MEM_ADDR;
	int * magic = (int *)env_mem_base;
	int * len = (int *)(env_mem_base + sizeof(int));
	char * env_data = env_mem_base + sizeof(int) * 2;	
	int i = 0;
	int j = 0;

	if (key == NULL || value == NULL)
	{
		return -1;
	}

	if (*magic != ENV_MAGIC)
	{
		printf("[get]mem environment not init yet.\n");
		value[0] = '\0';
		return -1;
	}

	while ( i < *len)
	{
		if(strncmp(env_data + i, key, strlen(key)))
		{
			while(env_data[i] != ';' && env_data[i] != '\0')
				i++;
			i++;
		}
		else
		{
			i += strlen(key);
			if (env_data[i] != '=')
			{
				while(env_data[i] != ';' && env_data[i] != '\0')
					i++;
				i++;
				continue;			
			}
			i++;
			while(env_data[i] != ';' && env_data[i] != '\0')
			{
				value[j] = env_data[i];
				j++;
				i++;
			}

			value[j] = '\0';
			return 0;
		}
	}

	value[0] = '\0';
	return -1;
}

void env_mem_clear(void)
{
	char * env_mem_base = (char *)CFG_BOOT_ENV_MEM_ADDR;
	memset(env_mem_base, 0, CFG_BOOT_ENV_MEM_SIZE);
}

int check_env_commit(void)
{
	int needsave = 0;
	char buf[16];
	
	memset(buf, 0, 16);
	env_mem_get(ENV_IMAGE_BOOT, buf);
	if (buf[0])
	{
		printf("commit ENV_IMAGE_BOOT %s\n", buf);
		setenv(ENV_IMAGE_BOOT, buf);
		needsave = 1;
	}

	memset(buf, 0, 16);
	env_mem_get(ENV_IMAGE_COPY, buf);
	if (buf[0])
	{
		printf("commit ENV_IMAGE_COPY %s\n", buf);
		setenv(ENV_IMAGE_COPY, buf);
		needsave = 1;
	}
	
	if (needsave)
	{
		printf("checking mem environment need save\n");
		saveenv();
		env_mem_clear();
	}
	else
	{		
		printf("checking mem environment needn't save\n");
	}
	
	return needsave;
}

void dump_buf(char *name, unsigned char *buf, unsigned int len)
{
	int i, j;
	unsigned char ch;
	printf("\ndumping %s len is 0x%x:", name, len);

	for (i = 0; i < len; i++)
	{
		if ( (i & 15) == 0 )
		{
			if ( i != 0 )
			{
				printf(" ; ");
				for (j = 0; j < 16; j++)
				{
					ch = buf[i - (16 - j)];
					if (ch < 32 || ch >= 127)
						ch = '.';
						
					printf("%c", ch);
				}
			}
			printf("\n%08x : ", i);
		}
		printf("%02X ", buf[i]);
	}
	if ( (i & 15) != 15 )
	{
		int remain = 16 - (i & 15);
		for (j = 0; j < remain; j++)
		{
			printf("   ");
		}
		printf(" ; ");
		for (j = remain; j < 16; j++)
		{
			ch = buf[i - (16 - j)];
			if (ch < 32 || ch >= 127)
				ch = '.';
				
			printf("%c", ch);
		}
	}

	printf("\n");
}

const unsigned long crc32_table[256] = 
{	
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
};


static inline unsigned long jffs_crc32(unsigned long val, const void *ss, int len)
{ 
	const unsigned char *s = ss;
	while (--len >= 0)
	{
		val = crc32_table[(val ^ *s++) & 0xff] ^ (val >> 8);
	}
	return val;
}

/* 
 * fn		int *jffs2_modifyNode(unsigned char *jffs2_buf, unsigned int buf_len, 
 *						 char *node_name, unsigned int node_type, int fname_actual_len,unsigned char *content)
 * brief	modify a JFFS2 node in the JFFS2 filesystem buffer
 * details	if content is NULL,just find the node
 *
 * param[in]	jffs2_file : JFFS2 buffer
 * param[in]	buf_len : JFFS2 buffer length
 * param[in]	node_name :  JFFS2 node name
 * param[in/out]content : the content will be read/written from/into the buf
 * param[in]    direction : 0 - read, 1 - write
 *
 * return	file len.
 * retval	content is NULL: return a buffer pointer of the node if node can be found
 *							 return NULL if node can not be found
 *			content is not NULL: return content if node can be found
 *                               return NULL if node can not be found
 * note		
 */
static int jffs2_modifyNode(unsigned char *jffs2_buf, unsigned int buf_len, 
		char *node_name, unsigned int node_type, int fname_actual_len, 
		unsigned char *content, int direction)
{
	char *fname = node_name;
	int fname_cmp_len = strlen(fname);
	unsigned long mctime = 0;
	unsigned char *p;
	unsigned long version = 0;
	unsigned long ino = 0, sigino = 0;
	int j, chk;
	struct jffs2_raw_dirent *pdir;
	struct jffs2_raw_inode *pino;
	int ret = 0;
#ifdef DEBUG_JFFS2
	printf("finding %s type %s fname_actual_len %d in buf %p buf_len %08x\n", node_name,
		(JFFS2_NODETYPE_DIRENT == node_type) ? "DIRENT" : "INODE",
		fname_actual_len, jffs2_buf, (int)buf_len);
#endif
	if (NULL == node_name || NULL == content)
	{
		//printf("Node name MUST be valid\n");
		return ret;
	}
	/* Find the directory entry. */
	p = jffs2_buf;
	j = chk = 0;

	while( p < jffs2_buf + buf_len )
	{
		pdir = (struct jffs2_raw_dirent *) p;
#ifdef DEBUG_JFFS2
		printf("finding offset %p ,MAGIC is %x ...\n", pdir, je16_to_cpu(pdir->magic));
#endif

		if( je16_to_cpu(pdir->magic) == JFFS2_MAGIC_BITMASK )
		{
			j = 0;

			{
#ifdef DEBUG_JFFS2
				if ( je16_to_cpu(pdir->nodetype) == JFFS2_NODETYPE_DIRENT )
					printf("DIRENT name is %s, nsize is %d\n", pdir->name, (int)pdir->nsize);
#endif
			   if( je16_to_cpu(pdir->nodetype) == JFFS2_NODETYPE_DIRENT &&
				   fname_actual_len == pdir->nsize &&
				   !memcmp(fname, pdir->name, fname_cmp_len) )
			   {
				  if( je32_to_cpu(pdir->version) > version )
				  {
					 if( (ino = je32_to_cpu(pdir->ino)) != 0 )
					 {
						version = je32_to_cpu(pdir->version);
						mctime = je32_to_cpu(pdir->mctime);

						/* Setting 'done = 1' assumes there is only one
						 * version of the directory entry. This is
						 * correct as long as the file, vmlinux.lz, is
						 * not renamed, modified, copied, etc.
						 */
						if (JFFS2_NODETYPE_DIRENT == node_type)
						{
#ifdef DEBUG_JFFS2
							printf("\n============================before modifying =========================\n");
							dump_buf("dirent", p, sizeof(struct jffs2_raw_dirent));
							dump_buf("name", pdir->name, pdir->nsize);
#endif	
							if (content != NULL)
							{
								uint32_t name_crc;
								memcpy(pdir->name, content, pdir->nsize);
								name_crc = jffs_crc32(0,(unsigned char *)pdir->name, pdir->nsize);
								je32_to_cpu(pdir->name_crc) = name_crc;
#ifdef DEBUG_JFFS2
								printf("\n============================after modifying =========================\n");
								dump_buf("dirent", p, sizeof(struct jffs2_raw_dirent));
								dump_buf("name", pdir->name, pdir->nsize);
#endif	
							}
							return fname_actual_len;
						}
						break;
					 }
				  }
			   }
			}

			p += (je32_to_cpu(pdir->totlen) + 0x03) & ~0x03;
		}
		else
		{
			/* Scan to the next 16KB boundary in case the image was
			 * built with a 16KB block size.
			 */
			const unsigned long granularity = (16 * 1024) - 1;

			if( p == jffs2_buf )
				break;

			if( j == 0 )
			{
				unsigned long curlen = (unsigned long) (p - jffs2_buf);
				chk = (((curlen + granularity) & ~granularity) -
					curlen) / sizeof(long);
			}

			if( j++ > chk )
				break;

			p += sizeof(long);
		}
	}

	if( version )
	{
		unsigned long cur_isize = 0;

		unsigned long size;
		unsigned long ofs;
		unsigned long isize;
		unsigned long mtime;

		p = jffs2_buf;
		j = chk = 0;

		while( p < jffs2_buf + buf_len )
		{
			pino = (struct jffs2_raw_inode *) p;
			if( je16_to_cpu(pino->magic) == JFFS2_MAGIC_BITMASK )
			{
				j = 0;
				if(je16_to_cpu(pino->nodetype)==JFFS2_NODETYPE_INODE &&
				   je32_to_cpu(pino->ino) == ino)
				{
					size = je32_to_cpu(pino->dsize);
					ofs = je32_to_cpu(pino->offset);
					isize = je32_to_cpu(pino->isize);
					mtime = je32_to_cpu(pino->mtime);

					if( size && mtime == mctime )
					{
						if (content != NULL)
						{
							uint32_t data_crc;
							
							if (direction)
							{
								memcpy(pino->data, content + ofs, size);
								data_crc = jffs_crc32(0,(unsigned char *)pino->data, size);
								je32_to_cpu(pino->data_crc) = data_crc;
#ifdef DEBUG_JFFS2
								printf("\n============================ after modifying =========================\n");
								dump_buf("inode", p, sizeof(struct jffs2_raw_inode));
								dump_buf("data", pino->data, size);
#endif										
							}
							else
								memcpy(content + ofs, pino->data, size);
						}
						else
						{
							ret = 0;
							break;
						}
						if( (cur_isize += size) >= isize )
						{
#ifdef DEBUG_JFFS2
							printf("%s found, ino %ld datacrc 0x%08x nodecrc 0x%08x node size 0x%x data size 0x%x\n",
								node_name, ino,
								je32_to_cpu(pino->data_crc), 
								je32_to_cpu(pino->node_crc),
								sizeof(struct jffs2_raw_inode),
								(int)size
								);
							printf("\n============================before modifying =========================\n");
							dump_buf("inode", p, sizeof(struct jffs2_raw_inode));
							dump_buf("data", pino->data, size);
							printf("\ncalculated data crc 0x%08x calculated node crc 0x%08x\n",
								(int)jffs_crc32(0,(unsigned char *)pino->data, size),
								(int)jffs_crc32(0,(unsigned char *)p, sizeof(struct jffs2_raw_inode) - 8));
#endif
								
							ret = isize;

							break;
						}
					}
				}

				if(je16_to_cpu(pino->nodetype)==JFFS2_NODETYPE_INODE &&
				   je32_to_cpu(pino->ino) == sigino)
				{
					size = je32_to_cpu(pino->dsize);
					isize = je32_to_cpu(pino->isize);
					mtime = je32_to_cpu(pino->mtime);

				}

				p += (je32_to_cpu(pino->totlen) + 0x03) & ~0x03;
			}
			else
			{
				/* Scan to the next 16KB boundary in case the image was
				 * built with a 16KB block size.
				 */
				const unsigned long granularity = (16 * 1024) - 1;

				if( p == jffs2_buf )
					break;

				if( j == 0 )
				{
					unsigned long curlen = (unsigned long) (p - jffs2_buf);
					chk = (((curlen + granularity) & ~granularity) -
						curlen) / sizeof(long);
				}

				if( j++ > chk )
					break;

				p += sizeof(long);
			}
		}
	}

	return( ret );
}

static int jffs2_checkRootfsCrc(unsigned char *buf, unsigned int buf_len, unsigned int crc)
{
	unsigned int ulCrc;
	unsigned int binCrc32;
	unsigned int fs_len;
	unsigned char tag_buf[sizeof(LINUX_FILE_TAG)]; /* NOTE: sizeof(FILE_TAG) is not TAG_LEN */
	unsigned char tag_saved[sizeof(LINUX_FILE_TAG)];
	LINUX_FILE_TAG * pKern_tag = (LINUX_FILE_TAG *)tag_buf;
	char cferam_fname[] = NAND_CFE_RAM_NAME;
	int fname_actual_len = strlen(cferam_fname);
	int fname_cmp_len = strlen(cferam_fname) - 3; /* last three are digits */
	char tag_fname[] = NAND_KERNEL_TAG;
	int tag_fname_len = strlen(tag_fname);
	
	fs_len = buf_len;
	binCrc32 = crc;

	memset(tag_buf, 0, sizeof(LINUX_FILE_TAG));
	memset(tag_saved, 0, sizeof(LINUX_FILE_TAG));

	if (jffs2_modifyNode(buf, fs_len, tag_fname, JFFS2_NODETYPE_INODE, tag_fname_len, tag_buf, 0) <= 0)
	{
		printf("weird, %s was not found in the rootfs(read).\n",tag_fname);
		return -1;
	}
	memcpy(tag_saved, tag_buf, sizeof(LINUX_FILE_TAG));
	
	pKern_tag->rootfsLen = 0;
	pKern_tag->binCrc32 = 0;
	if (jffs2_modifyNode(buf, fs_len, tag_fname, JFFS2_NODETYPE_INODE, tag_fname_len, tag_buf, 1) <= 0)
	{
		printf("weird, %s was not found in the rootfs(write).\n",tag_fname);
		return -1;
	}
	/* change cferam.xxx to cferam.000 */
	cferam_fname[fname_cmp_len] = '\0';
	if (jffs2_modifyNode(buf, fs_len, 
			cferam_fname, JFFS2_NODETYPE_DIRENT, fname_actual_len, (unsigned char *)NAND_CFE_RAM_NAME, 1) <= 0)
	{
		printf("weird, %s was not found in the rootfs.\n",cferam_fname);
		return -1;
	}
	
	/* now check the CRC of the rootfs */
	ulCrc = jffs_crc32(0x00000000L, buf, fs_len);
	if (ulCrc != binCrc32)
	{
		printf("CRC of rootfs is wrong(original 0x%08x calculated 0x%08x), maybe image corrupted ?\n",
					binCrc32, ulCrc);

		return -1;
	}

	/* restore original rootfs */
	if (jffs2_modifyNode(buf, fs_len, tag_fname, JFFS2_NODETYPE_INODE, tag_fname_len, tag_saved, 1) <= 0)
	{
		printf("weird, %s was not found in the rootfs(restore tag).\n",tag_fname);
		return -1;
	}

	return 0;
}

static int getJffs2File(unsigned int start, unsigned int end, unsigned char * file, char * name)
{

    struct jffs2_raw_dirent *pdir;
    struct jffs2_raw_inode *pino;
	unsigned char * buf = (unsigned char *) CFG_NAND_LOAD_SWAP_ADDR;
	int sector_size = 0;
	int block_size = 0;
	int namelen = 0;
	int filelen = 0;
	int nameOnly = 0;
	unsigned char *p = NULL;
	int i, j, chk, done;
	unsigned long version = 0;
	unsigned long mctime = 0;
	unsigned long ino = 0;
	
	if (name == NULL)
	{
		return 0;
	}

	if (file == NULL)
	{
		nameOnly = 1;
	}

	if (start >= end)
	{
		printf("start addr %x not less then the end %x when get jffs2 file\n", start, end);
		return 0;
	}

#ifdef DEBUG_JFFS2
	printf("start:%x, end:%x, name:%s\n", start, end, name);
#endif
	//sector_size = NAND_SECTOR_SIZE;
	//if (sector_size > 1024)
	//{
	//	printf("sector size is too large %d \n", sector_size);
	//	return 0;
	//}
	block_size = CFG_BLOCKSIZE;

#ifdef DEBUG_JFFS2
	printf("sector size is %d, block size is:%d\n", sector_size, block_size);
#endif
	
	namelen = strlen(name);
	start = (start / block_size) * block_size;
	end = end % block_size ? ((end / block_size + 1) * block_size) : end;
	
	/* Find the directory entry. */
	for( i = start, done = 0; i < end && (done == 0); i+=block_size )
	{	
		if( flash_read(buf, i + CFG_FLASH_BASE, block_size) == block_size )
		{
			p = buf;
			j = chk = 0;
			while( p < buf + block_size )
			{
				pdir = (struct jffs2_raw_dirent *) p;
				
				if( je16_to_cpu(pdir->magic) == JFFS2_MAGIC_BITMASK )
				{
					j = 0;
	
					if (done == 0)
					{
#ifdef DEBUG_JFFS2
					   if (je16_to_cpu(pdir->nodetype) == JFFS2_NODETYPE_DIRENT)
					   		printf("DIRENT i:%x, off:%x, name:%s, 0x%x\n", i, p - buf, pdir->name, JFFS2_NODETYPE_DIRENT);
#endif
					   if( je16_to_cpu(pdir->nodetype) == JFFS2_NODETYPE_DIRENT &&
						   namelen == pdir->nsize &&
						  ( !memcmp(name, pdir->name, namelen) || 
						  (!memcmp(name, "cferam", 6)&& !memcmp(pdir->name, "cferam", 6))))
					   {
						  if( je32_to_cpu(pdir->version) > version )
						  {
							 if( (ino = je32_to_cpu(pdir->ino)) != 0 )
							 {
								version = je32_to_cpu(pdir->version);
								mctime = je32_to_cpu(pdir->mctime);
	
								/* Setting 'done = 1' assumes there is only one
								 * version of the directory entry. This is
								 * correct as long as the file, vmlinux.lz, is
								 * not renamed, modified, copied, etc.
								 */
								done = 1;
								filelen = 1;
								memcpy(name, pdir->name, namelen);
#ifdef DEBUG_JFFS2
								printf("find ino %d name %s\n", ino, name);
#endif
							 }
						  }
					   }
					}
	
					p += (je32_to_cpu(pdir->totlen) + 0x03) & ~0x03;
				}
				else
				{
					if (i/block_size < NAND_IMAGE_OFFSET_MAX_NUM)
					{
						break;
					}
					else
					{
						printf("not jffs2 file system when find directory.\n");
						return 0;
					}
				}
			}
		}
		else
		{
			printf("error while read at offset %x to find directory\n", i);
		}
	}

	if( version && !nameOnly)
	{
		unsigned long cur_isize = 0;

		unsigned long size;
		unsigned long ofs;
		unsigned long isize;
		unsigned long mtime;

		/* Get the file contents. */
		for( i = start, done = 0; i < end && (done == 0); i+=block_size)
		{
			if( flash_read(buf, i + CFG_FLASH_BASE, block_size) == block_size )
			{
				p = buf;
				j = chk = 0;
				while( p < buf + block_size )
				{
					pino = (struct jffs2_raw_inode *) p;
					if( je16_to_cpu(pino->magic) == JFFS2_MAGIC_BITMASK )
					{
						j = 0;

#ifdef DEBUG_JFFS2
					   if (je16_to_cpu(pino->nodetype)==JFFS2_NODETYPE_INODE)
							printf("INODE i:%x, off:%x, ino:%d, 0x%x\n", i, p - buf, pino->ino, JFFS2_NODETYPE_INODE);
#endif

						if(je16_to_cpu(pino->nodetype)==JFFS2_NODETYPE_INODE &&
						   je32_to_cpu(pino->ino) == ino)
						{
							size = je32_to_cpu(pino->dsize);
							ofs = je32_to_cpu(pino->offset);
							isize = je32_to_cpu(pino->isize);
							mtime = je32_to_cpu(pino->mtime);

				
#ifdef DEBUG_JFFS2
							printf("find the same ino %d \n", ino, name);
#endif
							if (done == 0)
							{
							   if( size && mtime == mctime )
							   {
								  memcpy(file + ofs, pino->data, size);
								  if( (cur_isize += size) >= isize )
								  {
									 done = 1;
									 filelen = isize;
								  }
							   }
							}
						}
	
						p += (je32_to_cpu(pino->totlen) + 0x03) & ~0x03;
					}
					else
					{
						if (i/block_size < NAND_IMAGE_OFFSET_MAX_NUM)
						{
							break;
						}
						else
						{
							printf("not jffs2 file system when find file.\n");
							return 0;
						}
					}
				}
			}
			else
			{
				printf("error while read at offset %x to find file\n", i);
			}
		}
	}

	return filelen;
}
#endif

#ifdef CHECK_DUAL_IMAGE_AND_BOOT
static BOOL is_image_header_broken(image_header_t *header)
{
	printf("check image header magic ==>");
	if (ntohl(header->ih_magic) != IH_MAGIC) 
		{
		printf("Failed\n");
		return TRUE;
		}
	else
		{
		printf("OK\n");
		}

	printf("check image header crc ==>");
	ulong len  = sizeof(image_header_t);
	ulong chksum = ntohl(header->ih_hcrc);
	header->ih_hcrc = 0;
	if (crc32(0, (char *)header, len) != chksum) 
		{
		printf("Failed\n");
		return TRUE;
		}

	printf("OK\n");
	return FALSE;
}

static BOOL is_image_content_broken(image_header_t *header, unsigned int addr, ulong loadaddr)
{
	/* Check data crc */
	/* Skip crc checking if there is no valid header, or it may hang on due to broken data length */
	printf("check image content crc ==>");
	ulong len = ntohl(header->ih_size);
	ulong chksum = ntohl(header->ih_dcrc);

	flash_read(loadaddr, addr + sizeof(image_header_t), len);
		
	if (crc32(0, loadaddr, len) != chksum)
	{
		printf("Failed\n");
		return TRUE;
	}

	printf("OK\n");
	return FALSE;
}

/* rewrite it. by xjb */
#if 0

static check_image_copy(image_header_t hdr[],ulong hdr_addr[], int broken[], int boot_image_index, ulong image_len[])
{
	int ret = 0;
	char addr_str[11];
	if (!broken[boot_image_index])
		{
		if (broken[1-boot_image_index])
			{
			printf("Image%d is broken!\n", 1-boot_image_index);
			erase_partition(hdr_addr[1-boot_image_index] - CFG_FLASH_BASE, image_len[1-boot_image_index]);
			copy_image(ntohl(hdr[boot_image_index].ih_size) + sizeof(image_header_t) ,
				hdr_addr[boot_image_index],hdr_addr[1-boot_image_index]);
			/* boot to another image */
			}
		}
	else
		{
		/* check another image */
		printf("Image%d is broken!\n", boot_image_index);
		/* copy operation is moved to app*/
		printf("Check Image%d\n", 1-boot_image_index);
		if (!broken[1-boot_image_index])
			broken[1-boot_image_index] = is_image_content_broken(&hdr[1-boot_image_index], hdr_addr[1-boot_image_index], CFG_SPINAND_LOAD_ADDR);
		if (!broken[1-boot_image_index])
			{
			erase_partition(hdr_addr[boot_image_index] - CFG_FLASH_BASE, image_len[boot_image_index]);
			/* just copy it! */
			copy_image(ntohl(hdr[1-boot_image_index].ih_size) + sizeof(image_header_t),
				hdr_addr[1-boot_image_index],hdr_addr[boot_image_index]);
			}
		else
			{
			printf("Image%d is broken!Return!\n", 1-boot_image_index);
			/* another image is still broken, jump to debug mode!*/
			return -1;
			}
		}

	

	/* Should we change to another one ? */

	return 0;
	
}
#if 0
static check_image_upgrade(image_header_t hdr[],ulong hdr_addr[], int broken[], int boot_image_index, ulong image_len[])
{
	
	int ret = 0;

	int i = 0;
	

	printf("\n=================================================\n");
	char *str = getenv(BOOT_IMAGE_NAME);
	int default_boot_image_index = 0;
	if (str && (strcmp(str, "1") == 0))
		default_boot_image_index = 1;
	else
		default_boot_image_index = 0;
	
	printf("check upgrade status\n");

	char *upgrade = getenv(UPGRADE_FLAG_NAME);
	if (upgrade)
	{
		/* if the default image is not the boot image. */
		if (default_boot_image_index != boot_image_index )
		{
		/* clear all upgrading related envs. */
			printf("The default image is bad, no need to check upgrade status\n");
		
			
			setenv(UPGRADE_FLAG_NAME, UPGRADE_STATUS_INTERRUPTED);
			setenv(UPGRADE_IMAGE_TRY_TIMES, NULL);
			saveenv();
			
			return -1;
		}
		str = getenv(UPGRADE_IMAGE_TRY_TIMES);
		int trytimes = 0;
		if (str)
		trytimes = (int)simple_strtoul(str, NULL, 10);
		if (strcmp(upgrade, UPGRADE_STATUS_UPGRADED_NEED_CHECK) ==0)
			{
			printf("found upgrade image.\n");
			if (broken[boot_image_index] ) 
			{
				/* The image is bad!*/
				printf("The upgrading image is error!\n");
				setenv(UPGRADE_FLAG_NAME, UPGRADE_STATUS_UP_FIRMWARE_ERROR);
				setenv(UPGRADE_IMAGE_TRY_TIMES, NULL);
				saveenv();
			}
			else
				{
				/* change status */
				setenv(UPGRADE_FLAG_NAME, UPGRADE_STATUS_CHECKED_BY_UBOOT);
				setenv(UPGRADE_IMAGE_TRY_TIMES, "1");
				saveenv();	
				}
			
			}
		else if (strcmp(upgrade,UPGRADE_STATUS_UPGRADING) == 0)
		{
		/* the upgrading process is interrupted! The image is bad ! */
			broken[boot_image_index] = TRUE;

			printf("The upgrading status is not completed!The current image is set to broken!\n");
			/* set the upgrade status to interrupted */
			upgrade = UPGRADE_STATUS_INTERRUPTED;
			setenv(UPGRADE_FLAG_NAME, upgrade);		
			/* clear upgrade timer */
			setenv(UPGRADE_IMAGE_TRY_TIMES, NULL);
			saveenv();
		}
		else if ((strcmp(upgrade,UPGRADE_STATUS_CHECKED_BY_UBOOT) == 0))
		{
			if (trytimes >= MAX_TRY_TIMES)
				{
				broken[boot_image_index] = TRUE;
				printf("The upgrading image try times exceed %d, take it as broken!\n", MAX_TRY_TIMES);
				}
				else
				{
					char times_str[3];
					sprintf(times_str, "%d", ++trytimes);
					setenv(UPGRADE_IMAGE_TRY_TIMES, times_str);
					saveenv();
					
				}
				
			if (broken[boot_image_index] ) 
			{
				/* the try of upgrading image is failed! The image is bad!*/
				printf("The upgrading image is error!\n");
				setenv(UPGRADE_FLAG_NAME, UPGRADE_STATUS_UP_FIRMWARE_ERROR);
				setenv(UPGRADE_IMAGE_TRY_TIMES, NULL);
				saveenv();
			}	
		}
		else if ((strcmp(upgrade,UPGRADE_STATUS_CHECK_OK) == 0))
			{
				/* copy upgrade image to another image! */
				if (!broken[boot_image_index] )
					{
					printf("upgrade success. copy upgrade image.\n");
					erase_partition(hdr_addr[1-boot_image_index] - CFG_FLASH_BASE, image_len[1-boot_image_index]);
					
					copy_image(ntohl(hdr[boot_image_index].ih_size)+ sizeof(image_header_t),
						hdr_addr[boot_image_index],hdr_addr[1-boot_image_index]);
					setenv(UPGRADE_FLAG_NAME, NULL);
					setenv(UPGRADE_IMAGE_TRY_TIMES, NULL);
					saveenv();
					}
				else
					{
					printf("current image broken, upgrade still failed. \n");
					setenv(UPGRADE_FLAG_NAME, UPGRADE_STATUS_UP_FIRMWARE_ERROR);
					setenv(UPGRADE_IMAGE_TRY_TIMES, NULL);
					saveenv();
					}
			}
	}
	
	
	
	printf("\n=================================================\n");
	return ret;

}
#else
static check_image_upgrade(image_header_t hdr[],ulong hdr_addr[], int broken[], int boot_image_index, ulong image_len[])
{
	
	int ret = 0;

	printf("\n=================================================\n");

	char *upgrade = getenv(UPGRADE_FLAG_NAME);
	if (upgrade)
	{
		 if (strcmp(upgrade,UPGRADE_STATUS_UPGRADING) == 0)
		{
		/* the upgrading process is interrupted! The other image is bad ! */
			broken[1-boot_image_index] = TRUE;

			printf("The upgrading status is not completed!The other image is set to broken!\n");
		}

		 printf("clear upgrade status.\n");
		 setenv(UPGRADE_FLAG_NAME, NULL);
		 saveenv();
	}
		
	printf("\n=================================================\n");
	return ret;

}

#endif

#if 1
int check_image_validation(image_header_t hdr[],ulong hdr_addr[], int broken[], int *boot_image_index, ulong image_len[])
{
	int ret = 0;
	char addr_str[11];
	unsigned long len = 0, chksum = 0;
	int i = 0;

	for (i = 0; i<2;i++)
		{
		printf("check image%d\n", i);
		flash_read((char *)&hdr[i], (unsigned int)hdr_addr[i], sizeof(image_header_t));
		broken[i] =  is_image_header_broken(&hdr[i]);
		}

	printf("\n=================================================\n");

#if 0
	char *boot0_str = getenv(BOOT_OS0_ADDR);
	char *boot1_str = getenv(BOOT_OS1_ADDR);
	
	if (!boot0_str)
		{
		sprintf(addr_str, "0x%X", hdr_addr[0]);
		setenv(BOOT_OS0_ADDR,addr_str);
		}

	if (!boot1_str)
		{
		sprintf(addr_str, "0x%X", hdr_addr[1]);
		setenv(BOOT_OS1_ADDR,addr_str);
		}

	if (!boot0_str || !boot1_str)
		{
		printf("save %s and %s into flash.\n", BOOT_OS0_ADDR, BOOT_OS1_ADDR);
		saveenv();
		}
	
	char *str = getenv(FACT_BOOT_OS_ADDR);
	if (str)
		{
		/* not boot from default addr. */
		
		printf("The former uboot said the image%d can't be booted, set it to broken!\n", *boot_image_index);
		broken[*boot_image_index] = broken;

		*boot_image_index = 1 - *boot_image_index;

		/* clear FACT_BOOT_OS_ADDR */
		printf("clear %s in flash.\n", FACT_BOOT_OS_ADDR);
		setenv(FACT_BOOT_OS_ADDR, NULL);
		saveenv();
		
		}
#endif

	char *str = getenv("bootargs");
	if (!str)
		{
		printf("save bootargs for former uboot reading mtd partitions.\n");
		saveenv();
		}
		
	if (g_uboot_data.boot_image_addr == hdr_addr[1-*boot_image_index])
	{
	/* not boot from default addr. */
	
	printf("The former uboot said the image%d can't be booted, set it to broken!\n", *boot_image_index);
	broken[*boot_image_index] = broken;

	*boot_image_index = 1 - *boot_image_index;
	
	}
	
	printf("The boot image is Image%d\n", *boot_image_index);


	/* only check the boot image to save time. */
	if (!broken[*boot_image_index])
		broken[*boot_image_index] = is_image_content_broken(&hdr[*boot_image_index], hdr_addr[*boot_image_index], CFG_SPINAND_LOAD_ADDR);
	
	printf("\n=================================================\n");

	
	return ret;
	
}
#else
int check_image_validation(void)
{
	int ret = 0;
	int broken1 = 0, broken2 = 0;
	

	
	unsigned long len = 0, chksum = 0;
	image_header_t hdr1, hdr2;
	

	
	unsigned char *hdr1_addr, *hdr2_addr;

	
	char *stable, *try;

	char *upgrade = NULL;
	hdr1_addr = (unsigned char *)CFG_KERN_ADDR;

	hdr2_addr = (unsigned char *)CFG_KERN2_ADDR;

	


#if defined (CFG_ENV_IS_IN_NAND)
	ranand_read((char *)&hdr1, (unsigned int)hdr1_addr - CFG_FLASH_BASE, sizeof(image_header_t));
	ranand_read((char *)&hdr2, (unsigned int)hdr2_addr - CFG_FLASH_BASE, sizeof(image_header_t));
#elif defined (CFG_ENV_IS_IN_SPI)
	raspi_read((char *)&hdr1, (unsigned int)hdr1_addr - CFG_FLASH_BASE, sizeof(image_header_t));
	raspi_read((char *)&hdr2, (unsigned int)hdr2_addr - CFG_FLASH_BASE, sizeof(image_header_t));
#else //CFG_ENV_IS_IN_FLASH
	memmove(&hdr1, (char *)hdr1_addr, sizeof(image_header_t));
	memmove(&hdr2, (char *)hdr2_addr, sizeof(image_header_t));
#endif

	printf("\n=================================================\n");


	printf("The boot image is Image%s\n", bootimage);
	
	printf("Check image validation:\n");

	/* Check header magic number */
	printf ("Image1 Header Magic Number --> ");
	if (ntohl(hdr1.ih_magic) != IH_MAGIC) {
		broken1 = 1;
		printf("Failed\n");
	}
	else
		printf("OK\n");

	printf ("Image2 Header Magic Number --> ");
	if (ntohl(hdr2.ih_magic) != IH_MAGIC) {
		broken2 = 1;
		printf("Failed\n");
	}
	else
		printf("OK\n");

	/* Check header crc */
	/* Skip crc checking if there is no valid header, or it may hang on due to broken header length */
	if (broken1 == 0) {
		printf("Image1 Header Checksum --> ");
		len  = sizeof(image_header_t);
		chksum = ntohl(hdr1.ih_hcrc);
		hdr1.ih_hcrc = 0;
		if (crc32(0, (char *)&hdr1, len) != chksum) {
			broken1 = 1;
			printf("Failed\n");
		}
		else
			printf("OK\n");
	}

	if (broken2 == 0) {
		printf("Image2 Header Checksum --> ");
		len  = sizeof(image_header_t);
		chksum = ntohl(hdr2.ih_hcrc);
		hdr2.ih_hcrc = 0;
		if (crc32(0, (char *)&hdr2, len) != chksum) {
			printf("Failed\n");
			broken2 = 1;
		}
		else
			printf("OK\n");
	}



	/* Check data crc */
	/* Skip crc checking if there is no valid header, or it may hang on due to broken data length */
	if (broken1 == 0) {
		printf("Image1 Data Checksum --> ");
		len = ntohl(hdr1.ih_size);
		chksum = ntohl(hdr1.ih_dcrc);
#if defined (CFG_ENV_IS_IN_NAND)
		ranand_read((char *)CFG_SPINAND_LOAD_ADDR,
				(unsigned int)hdr1_addr - CFG_FLASH_BASE + sizeof(image_header_t),
				len);
		if (crc32(0, (char *)CFG_SPINAND_LOAD_ADDR, len) != chksum)
#elif defined (CFG_ENV_IS_IN_SPI)
		raspi_read((char *)CFG_SPINAND_LOAD_ADDR,
				(unsigned int)hdr1_addr - CFG_FLASH_BASE + sizeof(image_header_t),
				len);
		if (crc32(0, (char *)CFG_SPINAND_LOAD_ADDR, len) != chksum)
#else //CFG_ENV_IS_IN_FLASH
		if (crc32(0, (char *)(hdr1_addr + sizeof(image_header_t)), len) != chksum)
#endif
		{
			broken1 = 1;
			printf("Failed\n");
		}
		else
			printf("OK\n");
	}

	
	/* we always start from image1. image2 is only used in upgrading.
	Check if upgrade flag is set. If so, copy image2 to image1. */
	upgrade = getenv(UPGRADE_FLAG_NAME);

	/* the upgrade flag is not cleared, which indicates the upgrading failed. */
	if (upgrade && (strcmp(upgrade, UPGRADE_STATUS_UPGRADING) == 0))
	{
		printf("The upgrading process is interrupted. No need to check image2. Just take image2 as error!\n");
		broken2 = 1;
	}
	else
	{
	
		/* if needed to upgrade from image2, theck it */
		/* if image1 is broken, we need to check image2 too. */
		if ((upgrade && (strcmp(upgrade,UPGRADE_STATUS_UPGRADED_NEED_CHECK) == 0)) || (broken1==1))
			{
			if (broken2 == 0) {
				printf("Image2 Data Checksum --> ");
				len  = ntohl(hdr2.ih_size);
				chksum = ntohl(hdr2.ih_dcrc);
#if defined (CFG_ENV_IS_IN_NAND)
				ranand_read((char *)CFG_SPINAND_LOAD_ADDR,
						(unsigned int)hdr2_addr - CFG_FLASH_BASE + sizeof(image_header_t),
						len);
				if (crc32(0, (char *)CFG_SPINAND_LOAD_ADDR, len) != chksum)
#elif defined (CFG_ENV_IS_IN_SPI)
				raspi_read((char *)CFG_SPINAND_LOAD_ADDR,
						(unsigned int)hdr2_addr - CFG_FLASH_BASE + sizeof(image_header_t),
						len);
				if (crc32(0, (char *)CFG_SPINAND_LOAD_ADDR, len) != chksum)
#else //CFG_ENV_IS_IN_FLASH
				if (crc32(0, (char *)(hdr2_addr + sizeof(image_header_t)), len) != chksum)
#endif
				{
					broken2 = 1;
					printf("Failed\n");
				}
				else
					printf("OK\n");
			}
		}
	}

	if (upgrade && (strcmp(upgrade,UPGRADE_STATUS_UPGRADED_NEED_CHECK) == 0))
		{
		if (broken2==0)
			{
			broken1 = 1;
			printf("The image2 is a new image. We will set image1 to broken to copy image2 to image1!\n");
			}
		else
			{
			/* the upgrade new image is bad. Should we tell the user? */
			setenv(UPGRADE_FLAG_NAME, UPGRADE_STATUS_UP_FIRMWARE_ERROR);
			saveenv();
			}
		}
	
	/* Check stable flag and try counter */
	/* The image1Stable should be initialized as 1 */
	stable = getenv("Image1Stable");
	if (!stable)
		stable = "1";
	
	printf("Image1 Stable Flag --> %s\n", !strcmp(stable, "1") ? "Stable" : "Not stable");
	try = getenv("Image1Try");
	printf("Image1 Try Counter --> %s\n", (try == NULL) ? "0" : try);
	if ((strcmp(stable, "1") != 0) && (simple_strtoul(try, NULL, 10)) > MAX_TRY_TIMES 
		&& (broken1 == 0)) {
		printf("\nImage1 is not stable and try counter > %X. Take it as a broken image.", MAX_TRY_TIMES);
		broken1 = 1;
	}

	/* copy image2 to image1 if image1 is broken */
	printf("\nImage1: %s Image2: %s\n", broken1 ? "Broken" : "OK", broken2 ? "Broken" : "OK");
	if (broken1 == 1 && broken2 == 0) {
		len = ntohl(hdr2.ih_size) + sizeof(image_header_t);
		if (len > CFG_KERN_SIZE)
			printf("\nImage1 is broken, but Image2 size(0x%X) is too big(limit=0x%X)!!\
				\nGive up copying image.\n", len, CFG_KERN_SIZE);
		else {
			printf("Image1 is borken. Copy Image2 to Image1\n");
			copy_image(2, len);
			if (upgrade && (strcmp(upgrade,UPGRADE_STATUS_UPGRADED_NEED_CHECK) == 0))
			{
				/* upgrade OK. set it to 0.*/
				setenv(UPGRADE_FLAG_NAME, NULL);

				/* set image1 to stable */
				setenv("Image1Stable", "1");
				saveenv();

			}

			if (ret == 0) {
			setenv("Image1Stable", "0");
			setenv("Image1Try", "0");
			saveenv();
				}
		}

		
	}
	else if (broken1 == 0 && broken2 == 1) {
		len = ntohl(hdr1.ih_size) + sizeof(image_header_t);
		if (len > CFG_KERN2_SIZE)
			printf("\nImage2 is broken, but Image1 size(0x%X) is too big(limit=0x%X)!!\
				\nGive up copying image.\n", len, CFG_KERN2_SIZE);
		else {
			printf("\nImage2 is borken. Copy Image1 to Image2.\n");
			copy_image(1, len);
		}
	}
	else if (broken1 == 1 && broken2 == 1)
		 debug_mode();
	else
		ret = -1;

	if (broken1 == 0)
		{
		image1_load_addr = CFG_SPINAND_LOAD_ADDR;
		}
	
	printf("\n=================================================\n");
	saveenv();
	return ret;
}

#endif
#endif

#if 0
int boot_dual_image(cmd_tbl_t *cmdtp )
{
	char *argv[4];
	char addr_str[11];

	int ret = 0;
	int broken[2] = {0,0};
	image_header_t hdr[2];
	int boot_image_index = 0;
	#if 0
	unsigned char *hdr_addr[2] = {CFG_IMAGE0_ADDR, CFG_IMAGE1_ADDR};
	ulong image_len[2] = {CFG_IMAGE0_SIZE, CFG_IMAGE1_SIZE};
	#endif
	unsigned char *hdr_addr[2] = {0, 0};
	ulong image_len[2] = {0, 0};
	char *str = getenv(ENV_IMAGE_BOOT);
	int default_boot_image_index = 0;
	if (str && (strcmp(str, "1") == 0))
		default_boot_image_index = 1;
	else
		default_boot_image_index = 0;

	mtd_partition *part = get_part(IMAGE0_PART_NAME);
	if (part)
		{
		hdr_addr[0] = part->offset + CFG_FLASH_BASE;
		image_len[0] = part->size;
		}

	part = get_part(IMAGE1_PART_NAME);
	if (part)
		{
		hdr_addr[1] = part->offset + CFG_FLASH_BASE;
		image_len[1] = part->size;
		}
	
	boot_image_index = default_boot_image_index;
	/* check image validataion */
	check_image_validation(hdr, hdr_addr, broken, &boot_image_index, image_len);


	check_image_upgrade(hdr, hdr_addr, broken, boot_image_index, image_len);

	/* do not copy image in uboot */
#if 0
	 check_image_copy(hdr, hdr_addr, broken, boot_image_index, image_len);
#endif

	/* if all image is wrong, just return! The recovery webserver will cover it */
	if (broken[boot_image_index] && broken[1-boot_image_index])
		return -1;
	
	
	
		/* if the boot image is wrong, we change to another one and restart */
	if (broken[boot_image_index])
		{
		printf("the default image %d is error, we change to another one.\n", boot_image_index);
		sprintf(addr_str, "%d", 1-boot_image_index);
		setenv(ENV_IMAGE_BOOT, addr_str);
		sprintf(addr_str, "%d", boot_image_index);
		setenv(BAD_IMAGE, addr_str);
		saveenv();
		do_reset(cmdtp, 0, 1, argv);
		}
	else 
		{
		/* prepared to boot, set some env */
		if (default_boot_image_index != boot_image_index)
			{
			sprintf(addr_str, "%d", boot_image_index);
			printf("Default boot image is not the same as boot image in fact, correct it. \n");
			setenv(ENV_IMAGE_BOOT, addr_str);
			saveenv();
			}
		if (broken[1-boot_image_index])
			{
			printf("Image %d is broken. The app should handle it.\n", 1-boot_image_index);
			sprintf(addr_str, "%d", 1-boot_image_index);
			setenv(BAD_IMAGE, addr_str);
			saveenv();
			}
		}
	

	/* skip the uboot(including os header), jump to the kernel */
	ulong load_addr = hdr_addr[boot_image_index];
	load_addr += CFG_NEXT_BOOT_OFFSET;
	
	return do_bootm_repeat(cmdtp, load_addr,load_addr + image_len[boot_image_index] - CFG_NEXT_BOOT_OFFSET, CFG_BLOCKSIZE, CFG_NEXT_BOOT_TYPE);
}

#else
static int tatoi(const char *s, int *r){
	int i = 0;
	int len = 0;
	int result = 0;

	if (s == NULL || r == NULL) return -1;

	len = strlen(s);
	for (i = 0; i < len; i++)
	{
		if (*s >= '0' && *s <= '9')
		{
			result = result * 10 + *(s++) - '0';
		}
		else
		{
			return -1;;
		}
	}

	*r = result;
	return 0;
}

int boot_dual_image(cmd_tbl_t *cmdtp )
{
	mtd_partition *part = NULL;
	//boot already load the kernel to CFG_NAND_LOAD_KERNEL_ADDR
	unsigned char * pKernel = (unsigned char *)CFG_NAND_LOAD_KERNEL_ADDR;
	unsigned int kernelLen = 0;
	unsigned char * buf = (unsigned char *)CFG_NAND_LOAD_IMAGE_ADDR;
	char *str = NULL;

	//check environment in memory and choose to commit
	check_env_commit();

	//get the image boot
	str = getenv(ENV_IMAGE_BOOT);
	if (str && (strcmp(str, "1") == 0))
		part = get_part(IMAGE1_PART_NAME);
	else
		part = get_part(IMAGE0_PART_NAME);

	if (part == NULL)
	{
		return -1;
	}

	printf("try kernel boot addr %08X length 0x%08x\n", (int)pKernel, part->size); 
	do_bootm_repeat(cmdtp, (int)pKernel, (int)pKernel + part->size, 0, CFG_NEXT_BOOT_TYPE);

	return -1;

}
#endif

#endif /* CHECK_DUAL_IMAGE_AND_BOOT */

int do_bootm_repeat (cmd_tbl_t *cmdtp, int load_addr, int max_addr, int add, int boot_type)
{
	int ret = 0;
	char *argv[4];
	char addr_str[11];
	/* try to boot standalone type(uboot) */
	
	
	do
	{
		g_next_boot_type = boot_type;
	
		argv[1] = &addr_str[0];
		sprintf(addr_str, "0x%X", load_addr);

		ret =  do_bootm(cmdtp, 0, 2, argv);

		printf("ret is %d\n", ret);

		/* if ret is -ERR_BOOTM_TRY_UBOOT, we should try the header following it. */
		if (ret == -ERR_BOOTM_TRY_UBOOT)
		{
			printf("try next header\n", ret);
			
			g_next_boot_type = IH_TYPE_STANDALONE;
			
			sprintf(addr_str, "0x%X", load_addr + sizeof(image_header_t));
			ret = do_bootm(cmdtp, 0, 2, argv);
		}
		//else if ((ret == -ERR_BOOTM_TYPE_DISMATCH || ret == -ERR_BOOTM_MAGIC_DISMATCH ) && add > 0)

		/* if do_bootm succeed, the following codes won't be executed. */
		if (add > 0)
		{
			printf("try next block\n", ret);
			load_addr += add;
		}
		else
		{
			break;
		}
	}
	while(load_addr < max_addr);
	return ret;
}


#define CHECK_RECOVERY_GAP_TIME_US 100000
int check_recovery_button(void)
{
	u32 time_us = 0;
	while (is_recovery_button_pressed() && time_us < CONFIG_PRESS_BUTTON_ENTER_RECOVERY_SECONDS * 1000000)
		{
		udelay (CHECK_RECOVERY_GAP_TIME_US);
		printf(".");
		time_us +=CHECK_RECOVERY_GAP_TIME_US;
		}

	if(time_us >= CONFIG_PRESS_BUTTON_ENTER_RECOVERY_SECONDS * 1000000)
		{
		printf("begin recovery process.\n");
		return 1;
		}

	return 0;

}
#if defined(BOOT_NEXT_IMAGE)

#if 0
int boot_main_image(cmd_tbl_t *cmdtp)
{
	char *argv[4];
	char addr_str[11];
	char boot_type_str[3];
	
	ulong boot_num = 0;

	ulong load_addr_select[3];
	ulong load_addr = 0;
	
	int load_addr_index = 0;
	

	int i = 0;

	/* try to boot standalone type(uboot) */
	sprintf(boot_type_str, "%d", IH_TYPE_STANDALONE);

	memset(load_addr_select, 0, N_CONST_ARR(load_addr_select));
	
	/* try sequency: BOOT_OS0_ADDR BOOT_OS1_ADDR CFG_NEXT_BOOT_OFFSET */
	char *str = getenv(BOOT_IMAGE_NAME);
	if (str && (strcmp(str, "1") == 0))
		boot_num = 1;
	else
		boot_num = 0;
	

	str = getenv(BOOT_OS0_ADDR);
	if (str)
		load_addr_select[boot_num] = simple_strtoul(str, NULL, 16);
	else
		load_addr_select[boot_num] = 0;

	str = getenv(BOOT_OS1_ADDR);
	if (str)
		load_addr_select[1-boot_num] = simple_strtoul(str, NULL, 16);
	else
		load_addr_select[1-boot_num] = 0;

	load_addr_select[2] = CFG_FLASH_BASE + CFG_NEXT_BOOT_OFFSET;

	while(load_addr_index < N_CONST_ARR(load_addr_select))
		{
		load_addr = 0;
		
		for (i=load_addr_index;i<N_CONST_ARR(load_addr_select);i++)
			{
			if (load_addr_select[i] !=0)
				{
				load_addr = load_addr_select[i];
				load_addr_index = i;
				break;
				}
			}
		/* can't find a load addr, return !*/
		if (load_addr ==0)
			{
			printf("can't find boot addr!\n");
			break;
			}

		/* if not boot from first boot addr, set a flag */
		if((load_addr_index != 0) && (load_addr_select[0] != 0))
			{
			/* The first addr is valid but can't be boot. Set a flag! */
			/* TODO: What about tell the next uboot with stack data rather than uboot env? */
			sprintf(addr_str, "0x%X", load_addr);
			setenv(FACT_BOOT_OS_ADDR, addr_str);
			saveenv();
			}

		/* skip the os image header, jump to the uboot */
		load_addr += sizeof(image_header_t);
		sprintf(addr_str, "0x%X", load_addr);
		
		argv[1] = &addr_str[0];
		argv[2] = "0";
		argv[3] = &boot_type_str[0];

		/* It should not return */
		do_bootm(cmdtp, 0, 4, argv);

		load_addr_index++;
		}

	return 0;
	
	
	
}

int boot_main_image(cmd_tbl_t *cmdtp)
{	
	ulong boot_num = 0;

	ulong load_addr_select[3];
	ulong load_addr = 0;
	
	int load_addr_index = 0;
	

	int i = 0;

	int ret = 0;

	

	memset(load_addr_select, 0, N_CONST_ARR(load_addr_select));
	
	/* try sequency: BOOT_IMAGE_ADDR BOOT_ANOTHER_IMAGE_ADDR CFG_NEXT_BOOT_OFFSET */
	char *str = getenv(ENV_IMAGE_BOOT);
	if (str && (strcmp(str, "1") == 0))
		boot_num = 1;
	else
		boot_num = 0;
	
#if 0
	str = getenv(BOOT_OS0_ADDR);
	if (str)
		load_addr_select[boot_num] = simple_strtoul(str, NULL, 16);
	else
		load_addr_select[boot_num] = 0;

	str = getenv(BOOT_OS1_ADDR);
	if (str)
		load_addr_select[1-boot_num] = simple_strtoul(str, NULL, 16);
	else
		load_addr_select[1-boot_num] = 0;
#endif

	int os_len = get_nand_chip_size();
	
	mtd_partition *part = get_part(IMAGE0_PART_NAME);
	if (part)
		{
		load_addr_select[boot_num] = part->offset + CFG_FLASH_BASE;
		os_len = part->size;
		}
	else
		load_addr_select[boot_num] = 0;

	part = get_part(IMAGE1_PART_NAME);
	if (part)
		load_addr_select[1-boot_num] = part->offset + CFG_FLASH_BASE;
	else
		load_addr_select[1-boot_num] = 0;

	/* only if os0 and os1 addrs are not set can CFG_NEXT_BOOT_OFFSET used */
	if (load_addr_select[0] == 0 && load_addr_select[1] ==0)
		load_addr_select[2] = CFG_FLASH_BASE + CFG_NEXT_BOOT_OFFSET;
	else
		load_addr_select[2] = 0;
	
	while(load_addr_index < N_CONST_ARR(load_addr_select))
		{
		load_addr = 0;
		for (i=load_addr_index;i<N_CONST_ARR(load_addr_select);i++)
			{
			if (load_addr_select[i] !=0)
				{
				load_addr = load_addr_select[i];
				load_addr_index = i;
				break;
				}
			}
		/* can't find a load addr, return !*/
		if (load_addr ==0)
			{
			printf("can't find boot addr!\n");
			break;
			}

		/* if not boot from valid first boot addr, set a flag */
		if((load_addr_index != 0) && (load_addr_select[0] != 0))
			{
			/* The first addr is valid but can't be boot. Set a flag! */
			/* TODO: What about tell the next uboot with stack data rather than uboot env? */
			/* DO NOT saveenv in MAIN UBOOT, because we don't know the size of env partition. ONLY READ. */
			#if 0
			sprintf(addr_str, "0x%X", load_addr);
			setenv(FACT_BOOT_OS_ADDR, addr_str);
			saveenv();
			#endif
			g_uboot_data.boot_image_addr = load_addr;
			
			}

		/* skip the os image header, jump to the uboot */
		//load_addr += sizeof(image_header_t);
		
		printf("do_bootm \n");
		
		do_bootm_repeat(cmdtp, load_addr, load_addr + os_len, CFG_BLOCKSIZE, CFG_NEXT_BOOT_TYPE);
		
		load_addr_index++;
		}

	return 0;
	
	
	
}

#else

int boot_main_image(cmd_tbl_t *cmdtp)
{
	int seq1 = -1, seq2 = -1;
	int currImage = -1;
	int ret = 1;
	LINUX_FILE_TAG * pTag = NULL;
	unsigned long binCrc32 = 0;
	char * image = NULL;
	char * needcopy = NULL;
	//bd_t *bd = gd->bd;
	mtd_partition *part = NULL, *partos0 = NULL, *partos1 = NULL;
	unsigned char * buf = (unsigned char *)CFG_NAND_LOAD_IMAGE_ADDR;
	unsigned long fs_len = 0;
	unsigned char * pKernel = (unsigned char *)CFG_NAND_LOAD_KERNEL_ADDR;
	unsigned int kernelLen = 0;
	unsigned char * pBootram = (unsigned char *)CFG_NAND_LOAD_BOOT2_ADDR;
	unsigned int bootLen = 0;
	unsigned char * pSwap = (unsigned char *)CFG_NAND_LOAD_SWAP_ADDR;
	char *devname = IMAGE0_PART_NAME;
	char filename[16];

	//init
	memset(filename, 0, 16);
	strcpy(filename, NAND_CFE_RAM_NAME);

	/* 1. check which image to load. */
	image = getenv(ENV_IMAGE_BOOT);
	if (!image || !(image[0] == '0' || image[0] == '1'))
	{
		partos0 = get_part(IMAGE0_PART_NAME);
		if (partos0)
		{
			if (getJffs2File(partos0->offset, partos0->offset + partos0->size, NULL, filename) > 0)
			{
				seq1 = (filename[7] - '0')*100 + (filename[8] - '0')*10 + (filename[9] - '0');
			}
		}
		else
		{
			printf("get Device '%s' info fail.\n", IMAGE0_PART_NAME);
		}


		partos1 = get_part(IMAGE1_PART_NAME);
		if (partos1) 
		{
			if (getJffs2File(partos1->offset, partos1->offset + partos1->size, NULL, filename) > 0)
			{
				seq2 = (filename[7] - '0')*100 + (filename[8] - '0')*10 + (filename[9] - '0');
			}
		}
		else
		{
			printf("get Device '%s' info fail.\n", IMAGE1_PART_NAME);
		}

		if (seq1 < 0 && seq2 < 0)
		{
			printf("An available image is not found, no cferam.xxx.\n");
			return -1;
		}

		if (seq1 == 0 && seq2 == 999)
			seq1 = 1000;
		if (seq2 == 0 && seq1 == 999)
			seq2 = 1000;

		if (seq1 > seq2)
		{
			currImage = 0;
			devname = IMAGE0_PART_NAME;
			part = partos0;
			setenv(ENV_IMAGE_BOOT, "0");	
			env_mem_set(ENV_IMAGE_BOOT, "0");
		}
		else
		{
			currImage = 1;
			devname = IMAGE1_PART_NAME;
			part = partos1;
			setenv(ENV_IMAGE_BOOT, "1");
			env_mem_set(ENV_IMAGE_BOOT, "1");
		}
	}
	else if (image[0] == '0')
	{
		part = get_part(IMAGE0_PART_NAME);
		currImage = 0;
		devname = IMAGE0_PART_NAME;
	}
	else if (image[0] == '1')
	{
		part = get_part(IMAGE1_PART_NAME);
		currImage = 1;
		devname = IMAGE1_PART_NAME;
	}

	if (part == NULL) {
		printf("get Device '%s' info fail.\n", IMAGE0_PART_NAME);
		return -1;
	}

	printf("boot rootfs from %s image %d.\n", devname, currImage);

	/* 2. get kernel tag file.*/

	if(getJffs2File(part->offset, part->offset + part->size, pSwap, NAND_KERNEL_TAG) > 0)
	{
		pTag = (LINUX_FILE_TAG *)pSwap;
		fs_len = pTag->rootfsLen;
		binCrc32 = pTag->binCrc32;
		printf("found kernel tag, fslen = %d, bincrc = %x\n", fs_len, binCrc32);
	}
	else
	{
		printf("An available image is not found, no kernel tag.\n");
		goto load_err;		
	}


	
	/* 2. read whole image to buf. */
	int i = 0;
	int offset = 0;
	int blockSize = CFG_BLOCKSIZE;
	int found = 0;
	unsigned char *pBuf = buf;
	struct jffs2_raw_dirent *pdir;

	for (i = 0; i < part->size; i += blockSize)
	{
		//printf("read from block i %d size %d offset %d\n", i, part->size, offset);
		//if (pBuf - buf >= fs_len)
		//{
		//	break;
		//}

		offset = part->offset + i;
		
		if( flash_read(pBuf, offset + CFG_FLASH_BASE, blockSize) == blockSize )
		{
			if (!found)
			{
				if (i/blockSize < NAND_IMAGE_OFFSET_MAX_NUM)
				{
					pdir = (struct jffs2_raw_dirent *) pBuf;
					if( je16_to_cpu(pdir->magic) == JFFS2_MAGIC_BITMASK )
					{
						pBuf += blockSize;
						found = 1;
						i += blockSize;
						break;
					}
				}
				else
				{
					printf("An available image is not found, reach max reserve space.\n");
					goto load_err;						
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			printf("error while read at offset %x\n", offset);
		}
	}

	//printf("end read from block i %d size %d offset %d fs_len %d blockSize %d\n", 
	//	i, part->size, offset, fs_len, blockSize);
	if (fs_len > blockSize)
	{
		offset = part->offset + i;
		if( flash_read(pBuf, offset + CFG_FLASH_BASE, fs_len - blockSize) != (fs_len - blockSize) )
		{
			printf("error while read image at offset %x\n", offset);
			goto load_err;	
		}
	}
#if 0
	/* 3. get kernel tag file.*/
	//if(getJffs2File(part->offset, part->offset + part->size, buf, filename) <= 0)
	if(jffs2_modifyNode(buf, part->size, NAND_KERNEL_TAG, JFFS2_NODETYPE_INODE, sizeof(NAND_KERNEL_TAG) - 1, pSwap, 0) <= 0)
	{
		printf("An available image is not found, no kernel tag.\n");
		goto load_err;		
	}
	pTag = (LINUX_FILE_TAG *)pSwap;
	fs_len = pTag->rootfsLen;
	binCrc32 = pTag->binCrc32;
#endif	
	/* 4. check crc of this image. */	
	if(jffs2_checkRootfsCrc(buf, fs_len, binCrc32) < 0)
	{
		printf("An available image is not found, illegal image crc32.\n");
		goto load_err;
	}

	/* 
	 * 5. check if or not backup image.
	 * The reason of before check crc is that 
	 * crc checking can modify image content. 
	 */
	needcopy = getenv(ENV_IMAGE_COPY);
	if (needcopy && (needcopy[0] == IMAGE_NEED_COPY))
	{
		pBuf = buf;
		if (currImage)
			devname = IMAGE0_PART_NAME;
		else
			devname = IMAGE1_PART_NAME;

		printf("Now backup image to %s.\n", devname);
		part = get_part(devname);
		if (part) 
		{
			/* erase it! */
			if (erase_partition(part->offset, part->size) == 0)
			{
				/* just copy it! */
				if (copy_image(fs_len, buf, part->offset + CFG_FLASH_BASE) >= 0)
				{
					setenv(ENV_IMAGE_COPY, IMAGE_NOT_COPY);
					env_mem_set(ENV_IMAGE_COPY, IMAGE_NOT_COPY);
					printf("backup image to %s end.\n", devname);
				}
				else
				{
					printf("error while backup image to %s.\n", devname);
				}
			}
			else
			{
				printf("error erase %s for backup image.\n", devname);
			}
		}
		else
		{
			printf("get Device '%s' part info fail for backup image.\n", devname);
		}
	}

	/* 6. load kernel to memory*/
	kernelLen = jffs2_modifyNode(buf, fs_len, NAND_FLASH_BOOT_IMAGE_LZ, JFFS2_NODETYPE_INODE, sizeof(NAND_FLASH_BOOT_IMAGE_LZ) - 1, pKernel, 0);
	if(kernelLen <= 0)
	{
		printf("An available image is not found, get linux kernel fail.\n");
		goto load_err;		
	}
	printf("copy kernel len %d fs_len %d\n", kernelLen, fs_len);

	/* 7. load boot2 to memory*/
	bootLen = jffs2_modifyNode(buf, fs_len, NAND_CFE_RAM_NAME, JFFS2_NODETYPE_INODE, sizeof(NAND_CFE_RAM_NAME) - 1, pBootram, 0);
	if(bootLen <= 0)
	{
		printf("An available image is not found, get cferam fail.\n");
		goto load_err;		
	}

	g_uboot_data.boot_image_addr = (uint32_t)pBootram;

	do_bootm_repeat(cmdtp, (int)pBootram, (int)pBootram + bootLen, 
							CFG_BLOCKSIZE, IH_TYPE_STANDALONE);
	
	//success never return
	return -1;

load_err:
	env_mem_clear();
	
	if (currImage == 0)
	{
		setenv(ENV_IMAGE_BOOT, "1");
		env_mem_set(ENV_IMAGE_BOOT, "1");
		
		buf = IMAGE_NEED_COPY;
		setenv(ENV_IMAGE_COPY, &buf);
		env_mem_set(ENV_IMAGE_COPY, &buf);
		
		ret = 1;
	}
	else if (currImage == 1)
	{
		setenv(ENV_IMAGE_BOOT, "0");
		env_mem_set(ENV_IMAGE_BOOT, "0");
		
		buf = IMAGE_NEED_COPY;
		setenv(ENV_IMAGE_COPY, &buf);
		env_mem_set(ENV_IMAGE_COPY, &buf);
		
		ret = 1;		
	}
	else
	{
		ret = -1;
	}

	return ret;
	
}
#endif

#endif

int erase_partition(ulong offset, ulong size)
{
		printf("erase partition from 0x%x, size 0x%x\n", offset, size);

		
#if defined (CFG_ENV_IS_IN_NAND)
		ranand_erase(offset, size);
#elif defined (CFG_ENV_IS_IN_SPI)
		raspi_erase(offset, size);
#else/*CFG_ENV_IS_IN_SPI*/
		flash_sect_erase(offset, offset+size-1);
#endif/*CFG_ENV_IS_IN_NAND*/
		
	
	}


int boot_next(cmd_tbl_t *cmdtp)
{
	/* jump to next boot part */
	
	uint64_t max_addr = CFG_NEXT_BOOT_OFFSET + CFG_FLASH_BASE;
	uint64_t add = 0;
#if defined (CFG_ENV_IS_IN_NAND)
	 max_addr = get_nand_chip_size() + CFG_FLASH_BASE;
	add = CFG_BLOCKSIZE;
#endif
	return do_bootm_repeat(cmdtp, CFG_NEXT_BOOT_OFFSET + CFG_FLASH_BASE, 
		max_addr, add, CFG_NEXT_BOOT_TYPE);
}

/*
Detail: we use "uboot data" to pass some data from the former uboot to the next uboot.
It's stored in load_uboot_data_addr.
Check if the magic is right. Perhaps should do more check to ensure the data is not altered by others?
By xjb@2018.6.27
*/
int LoadUbootData(void)
{
	uboot_data *ptr = ((uboot_data *)(load_uboot_data_addr));
			
	if (IH_MAGIC == ptr->magic)
		{
			g_uboot_data = *((uboot_data *)(load_uboot_data_addr));
			printf("this uboot is loaded from 0x%x\n", g_uboot_data.boot_address);
		}
	return 0;
}

/************************************************************************
 *
 * This is the next part if the initialization sequence: we are now
 * running from RAM and have a "normal" C environment, i. e. global
 * data can be written, BSS has been cleared, the stack size in not
 * that critical any more, etc.
 *
 ************************************************************************
 */

gd_t gd_data;

/* add by wanghao  */
extern void disablePhy();
int do_ethon(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	u32 index = 0;
	u32 portMatrixCtrlReg = 0x2004;

	if (strncmp(argv[0], "ethon", 5))
	{
		printf ("Usage:\n%s\n", cmdtp->usage);
		return -1;
	}

#if defined (MT7621_ASIC_BOARD) || defined (MT7621_FPGA_BOARD)
#if defined (MAC_TO_MT7530_MODE) || defined (GE_RGMII_INTERNAL_P0_AN) || defined (GE_RGMII_INTERNAL_P4_AN)
	//enable MDIO
	RALINK_REG(0xbe000060) &= ~(1 << 12); //set MDIO to Normal mode
	RALINK_REG(0xbe000060) &= ~(1 << 14); //set RGMII1 to Normal mode
	RALINK_REG(0xbe000060) &= ~(1 << 15); //set RGMII2 to Normal mode
	setup_internal_gsw();
#endif
#endif
	LANWANPartition();
#if (CONFIG_COMMANDS & CFG_CMD_NET)
	eth_initialize(gd->bd);
#endif
	 return 0;
}

U_BOOT_CMD(
	ethon, 1, 0,	do_ethon,
	"ethon   - enable ethernet.\n",
	NULL
);
/* add end  */

__attribute__((nomips16)) void board_init_r (gd_t *id, ulong dest_addr)
{
	cmd_tbl_t *cmdtp;
	ulong size;
	extern void malloc_bin_reloc (void);
#ifndef CFG_ENV_IS_NOWHERE
	extern char * env_name_spec;
#endif
	char *s, *e;
	bd_t *bd;
	int i;
	int timer1= CONFIG_BOOTDELAY;
	unsigned char BootType='3', confirm=0;
	int my_tmp;
	char addr_str[11];
#if defined (CFG_ENV_IS_IN_FLASH)
	ulong e_end;
#endif


#if defined(MT7620_ASIC_BOARD)
	/* Enable E-PHY clock */ /* TODO: remove printf()*/
	printf("enable ephy clock...");
	i = 5;
	rw_rf_reg(1, 29, &i);
	printf("done. ");
	rw_rf_reg(0, 29, &i);
	printf("rf reg 29 = %d\n", i);

	/* print SSC for confirmation */ /* TODO: remove these in formanl release*/
	u32 value = RALINK_REG(0xb0000054);
	value = value >> 4;
	if(value & 0x00000008){
		unsigned long swing = ((value & 0x00000007) + 1) * 1250;
		printf("SSC enabled. swing=%d, upperbound=%d\n", swing, (value >> 4) & 0x3);
	}else{
		printf("SSC disabled.\n");
	}

#endif

	/* add by wanghao  */
#ifdef CONFIG_TINY_UBOOT
	disablePhy();
#endif
	/* add end  */

#if defined(RT3052_ASIC_BOARD)
	void adjust_voltage(void);
	// adjust core voltage ASAP
	adjust_voltage();
#endif

#if defined (RT3052_FPGA_BOARD) || defined(RT3052_ASIC_BOARD)
#ifdef RALINK_EPHY_INIT
	void enable_mdio(int);
	// disable MDIO access ASAP
	enable_mdio(0);
#endif
#endif


	gd = id;
	gd->flags |= GD_FLG_RELOC;	/* tell others: relocation done */

	Init_System_Mode(); /*  Get CPU rate */

#if defined(MT7628_ASIC_BOARD)	/* Enable WLED share pin */
	RALINK_REG(RALINK_SYSCTL_BASE+0x3C)|= (1<<8);	
	RALINK_REG(RALINK_SYSCTL_BASE+0x64)&= ~((0x3<<16)|(0x3));
#endif	
#if defined(RT3052_ASIC_BOARD) || defined(RT3352_ASIC_BOARD) || defined(RT5350_ASIC_BOARD)
	//turn on all Ethernet LEDs around 0.5sec.
#if 0
	RALINK_REG(RALINK_ETH_SW_BASE+0xA4)=0xC;
	RALINK_REG(RALINK_ETH_SW_BASE+0xA8)=0xC;
	RALINK_REG(RALINK_ETH_SW_BASE+0xAC)=0xC;
	RALINK_REG(RALINK_ETH_SW_BASE+0xB0)=0xC;
	RALINK_REG(RALINK_ETH_SW_BASE+0xB4)=0xC;
	udelay(500000);
	RALINK_REG(RALINK_ETH_SW_BASE+0xA4)=0x5;
	RALINK_REG(RALINK_ETH_SW_BASE+0xA8)=0x5;
	RALINK_REG(RALINK_ETH_SW_BASE+0xAC)=0x5;
	RALINK_REG(RALINK_ETH_SW_BASE+0xB0)=0x5;
	RALINK_REG(RALINK_ETH_SW_BASE+0xB4)=0x5;
#endif
#endif

#if defined(RT3052_ASIC_BOARD) || defined(RT2883_ASIC_BOARD)
	void config_usbotg(void);
	config_usbotg();
#elif defined(RT3883_ASIC_BOARD) || defined(RT3352_ASIC_BOARD) || defined(RT5350_ASIC_BOARD) || defined(RT6855_ASIC_BOARD) || defined (MT7620_ASIC_BOARD) || defined(MT7628_ASIC_BOARD)
	void config_usb_ehciohci(void);
	config_usb_ehciohci();
#elif defined(MT7621_ASIC_BOARD)
	void config_usb_mtk_xhci();
	config_usb_mtk_xhci();
#endif
	void disable_pcie();
	disable_pcie();

	u32 reg = RALINK_REG(RT2880_RSTSTAT_REG);
	if(reg & RT2880_WDRST ){
		printf("***********************\n");
		printf("Watchdog Reset Occurred\n");
		printf("***********************\n");
		RALINK_REG(RT2880_RSTSTAT_REG)|=RT2880_WDRST;
		RALINK_REG(RT2880_RSTSTAT_REG)&=~RT2880_WDRST;
		trigger_hw_reset();
	}else if(reg & RT2880_SWSYSRST){
		printf("******************************\n");
		printf("Software System Reset Occurred\n");
		printf("******************************\n");
		RALINK_REG(RT2880_RSTSTAT_REG)|=RT2880_SWSYSRST;
		RALINK_REG(RT2880_RSTSTAT_REG)&=~RT2880_SWSYSRST;
		trigger_hw_reset();
	}else if (reg & RT2880_SWCPURST){
		printf("***************************\n");
		printf("Software CPU Reset Occurred\n");
		printf("***************************\n");
		RALINK_REG(RT2880_RSTSTAT_REG)|=RT2880_SWCPURST;
		RALINK_REG(RT2880_RSTSTAT_REG)&=~RT2880_SWCPURST;
		trigger_hw_reset();
	}

#ifdef CONFIG_LED_CHECK
	ralink_gpio_init();
	all_led_check(0);
#endif

#ifdef DEBUG
	debug ("Now running in RAM - U-Boot at: %08lx\n", dest_addr);
#endif
	gd->reloc_off = dest_addr - CFG_MONITOR_BASE;

	monitor_flash_len = (ulong)&uboot_end_data - dest_addr;
#ifdef DEBUG	
	debug("\n monitor_flash_len =%d \n",monitor_flash_len);
#endif	
	/*
	 * We have to relocate the command table manually
	 */
	for (cmdtp = &__u_boot_cmd_start; cmdtp !=  &__u_boot_cmd_end; cmdtp++) {
		ulong addr;

		addr = (ulong) (cmdtp->cmd) + gd->reloc_off;
#ifdef DEBUG
		printf ("Command \"%s\": 0x%08lx => 0x%08lx\n",
				cmdtp->name, (ulong) (cmdtp->cmd), addr);
#endif
		cmdtp->cmd =
			(int (*)(struct cmd_tbl_s *, int, int, char *[]))addr;

		addr = (ulong)(cmdtp->name) + gd->reloc_off;
		cmdtp->name = (char *)addr;

		if (cmdtp->usage) {
			addr = (ulong)(cmdtp->usage) + gd->reloc_off;
			cmdtp->usage = (char *)addr;
		}
#ifdef	CFG_LONGHELP
		if (cmdtp->help) {
			addr = (ulong)(cmdtp->help) + gd->reloc_off;
			cmdtp->help = (char *)addr;
		}
#endif

	}
	/* there are some other pointer constants we must deal with */
#ifndef CFG_ENV_IS_NOWHERE
	env_name_spec += gd->reloc_off;
#endif

	bd = gd->bd;
#if defined (CFG_ENV_IS_IN_NAND)
#if defined (MT7621_ASIC_BOARD) || defined (MT7621_FPGA_BOARD)
	if ((size = mtk_nand_probe()) != (ulong)0) {
		printf("nand probe fail\n");
		while(1);
	}
#else
	if ((size = ranand_init()) == (ulong)-1) {
		printf("ra_nand_init fail\n");
		while(1);
	}
#endif	
	bd->bi_flashstart = 0;
	bd->bi_flashsize = size;
	bd->bi_flashoffset = 0;
#elif defined (CFG_ENV_IS_IN_SPI)
	if ((size = raspi_init()) == (ulong)-1) {
		printf("ra_spi_init fail\n");
		while(1);
	}
	bd->bi_flashstart = 0;
	bd->bi_flashsize = size;
	bd->bi_flashoffset = 0;
#else //CFG_ENV_IS_IN_FLASH
	/* configure available FLASH banks */
	size = flash_init();

	bd->bi_flashstart = CFG_FLASH_BASE;
	bd->bi_flashsize = size;
#if CFG_MONITOR_BASE == CFG_FLASH_BASE
	bd->bi_flashoffset = monitor_flash_len;	/* reserved area for U-Boot */
#else
	bd->bi_flashoffset = 0;
#endif
#endif //CFG_ENV_IS_IN_FLASH

#if defined(MT7628_ASIC_BOARD)
	/* Enable ePA/eLNA share pin */
	{
		char ee35, ee36;
		raspi_read((char *)&ee35, CFG_FACTORY_ADDR-CFG_FLASH_BASE+0x35, 1);
		raspi_read((char *)&ee36, CFG_FACTORY_ADDR-CFG_FLASH_BASE+0x36, 1);
		if ((ee35 & 0x2) || ((ee36 & 0xc) == 0xc))
		{
			RALINK_REG(RALINK_SYSCTL_BASE+0x60)|= ((0x3<<24)|(0x3 << 6));
		}
	}

	// reset MIPS now also reset Andes
	RALINK_REG(RALINK_SYSCTL_BASE+0x38)|= 0x200;	
#endif	
	/* initialize malloc() area */
	mem_malloc_init();
	malloc_bin_reloc();

#if defined (CFG_ENV_IS_IN_NAND)
	nand_env_init();
#elif defined (CFG_ENV_IS_IN_SPI)
	spi_env_init();
#else 
#endif 

#if defined (CFG_ENV_IS_IN_NAND)
	init_partable();
#endif

#if defined(RT3052_ASIC_BOARD)
	void adjust_frequency(void);
	//adjust_frequency();
#endif
#if defined (RT3352_ASIC_BOARD)
	void adjust_crystal_circuit(void);
	adjust_crystal_circuit();
#endif
#if defined (RT3352_ASIC_BOARD) || defined (RT3883_ASIC_BOARD)
	void adjust_rf_r17(void);
	adjust_rf_r17();
#endif

	/* relocate environment function pointers etc. */
	env_relocate();

	/* board MAC address */
	s = getenv ("ethaddr");
	for (i = 0; i < 6; ++i) {
		bd->bi_enetaddr[i] = s ? simple_strtoul (s, &e, 16) : 0;
		if (s)
			s = (*e) ? e + 1 : e;
	}

	/* IP Address */
	bd->bi_ip_addr = getenv_IPaddr("ipaddr");

#if defined(CONFIG_PCI)
	/*
	 * Do pci configuration
	 */
	puts("\n  Do pci configuration  !!\n");
	pci_init();
#endif

	/** leave this here (after malloc(), environment and PCI are working) **/
	/* Initialize devices */
	devices_init ();

	jumptable_init ();

	/* Initialize the console (after the relocation and devices init) */
	console_init_r ();

	/* Initialize from environment */
	if ((s = getenv ("loadaddr")) != NULL) {
		load_addr = simple_strtoul (s, NULL, 16);
	} else {
		load_addr = CFG_LOAD_ADDR;
	}
#if (CONFIG_COMMANDS & CFG_CMD_NET)
	if ((s = getenv ("bootfile")) != NULL) {
		copy_filename (BootFile, s, sizeof (BootFile));
	}
#endif	/* CFG_CMD_NET */

#if defined(CONFIG_MISC_INIT_R)
	/* miscellaneous platform dependent initialisations */
	misc_init_r ();
#endif

	/* RT2880 Boot Loader Menu */
#if defined(RT3352_FPGA_BOARD) || defined (RT3352_ASIC_BOARD) || \
    defined(RT3883_FPGA_BOARD) || defined (RT3883_ASIC_BOARD) || \
    defined(RT5350_FPGA_BOARD) || defined (RT5350_ASIC_BOARD) 

#if defined(CFG_ENV_IS_IN_SPI) || defined (CFG_ENV_IS_IN_NAND)
	{
		int reg, boot_from_eeprom=0;
		reg = RALINK_REG(RT2880_SYSCFG_REG);
		/* Uboot Version and Configuration*/
		printf("============================================ \n");
		printf("Ralink UBoot Version: %s\n", RALINK_LOCAL_VERSION);
		printf("-------------------------------------------- \n");
		printf("%s %s %s\n",CHIP_TYPE, CHIP_VERSION, GMAC_MODE);
		boot_from_eeprom = ((reg>>18) & 0x01);
		if(boot_from_eeprom){
		    printf("DRAM_CONF_FROM: EEPROM \n");
		    printf("DRAM_SIZE: %d Mbits %s\n", DRAM_COMPONENT, DDR_INFO);
		    printf("DRAM_TOTAL_WIDTH: %d bits\n", DRAM_BUS );
		    printf("TOTAL_MEMORY_SIZE: %d MBytes\n", DRAM_SIZE);
		}else{
		int dram_width, is_ddr2, dram_total_width, total_size;
		int _x = ((reg >> 12) & 0x7); 

#if defined(RT3352_FPGA_BOARD) || defined (RT3352_ASIC_BOARD)
		int dram_size = (_x == 6)? 2048 : (_x == 5)? 1024 : (_x == 4)? 512 : (_x == 3)? 256 : (_x == 2)? 128 : \
				(_x == 1)? 64 : (_x == 0)? 16 : 0; 
#elif defined (RT5350_FPGA_BOARD) || defined (RT5350_ASIC_BOARD)
		int dram_size = (_x == 4)? 512 : (_x == 3)? 256 : (_x == 2)? 128 : \
				(_x == 1)? 64 : (_x == 0)? 16 : 0; 
#elif defined (RT3883_FPGA_BOARD) || defined (RT3883_ASIC_BOARD)
		int dram_size = (_x == 6)? 2048 : (_x == 5)? 1024 : (_x == 4)? 512 : \
				(_x == 3)? 256 : (_x == 2)? 128 : (_x == 1)? 64 : \
				(_x == 0)? 16 : 0; 
#endif
		if(((reg >> 15) & 0x1)){
		    dram_total_width = 32;
		}else{
		    dram_total_width = 16;
		}

		is_ddr2 = ((reg >> 17) & 0x1);
		if(is_ddr2){
		  if((reg >> 10) & 0x1){
			dram_width = 16;
		  }else{
			dram_width = 8;
		  }
		}else{
		  if((reg >> 10) & 0x1){
			dram_width = 32;
		  }else{
			dram_width = 16;
		  }
		}
		total_size = (dram_size*(dram_total_width/dram_width))/8;

		printf("DRAM_CONF_FROM: %s \n", boot_from_eeprom ? "EEPROM":"Boot-Strapping");
		printf("DRAM_TYPE: %s \n", is_ddr2 ? "DDR2":"SDRAM");
		printf("DRAM_SIZE: %d Mbits\n", dram_size);
		printf("DRAM_WIDTH: %d bits\n", dram_width);
		printf("DRAM_TOTAL_WIDTH: %d bits\n", dram_total_width );
		printf("TOTAL_MEMORY_SIZE: %d MBytes\n", total_size);
		}
		printf("%s\n", FLASH_MSG);
		printf("%s\n", "Date:" __DATE__ "  Time:" __TIME__ );
		printf("============================================ \n");
	}
#else
	SHOW_VER_STR();
#endif /* defined(CFG_ENV_IS_IN_SPI) || defined (CFG_ENV_IS_IN_NAND) */


#elif (defined (RT6855_ASIC_BOARD) || defined (RT6855_FPGA_BOARD) ||  \
      defined (RT6855A_ASIC_BOARD) || defined (RT6855A_FPGA_BOARD) || \
      defined (MT7620_ASIC_BOARD) || defined (MT7620_FPGA_BOARD) || \
      defined (MT7628_ASIC_BOARD) || defined (MT7628_FPGA_BOARD)) && defined (UBOOT_RAM)
	{
		unsigned long chip_mode, dram_comp, dram_bus, is_ddr1, is_ddr2, data, cfg0, cfg1, size=0;
		int dram_type_bit_offset = 0;
#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)	
		data = RALINK_REG(RALINK_SYSCTL_BASE+0x8c);
		chip_mode = ((data>>28) & 0x3)|(((data>>22) & 0x3)<<2);
		dram_type_bit_offset = 24;
#else		
		data = RALINK_REG(RALINK_SYSCTL_BASE+0x10);
		chip_mode = (data&0x0F);
		dram_type_bit_offset = 4;
#endif		
		switch((data>>dram_type_bit_offset)&0x3)			
		{
			default:
			case 0:
				is_ddr2 = is_ddr1 = 0;
				break;
#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)
#else				
			case 3:
#endif
#if defined (MT7620_ASIC_BOARD) || defined (MT7620_FPGA_BOARD) || defined (MT7628_ASIC_BOARD) || defined (MT7628_FPGA_BOARD)
				is_ddr1 = 1; 
				is_ddr2 = 0;
#else				
				is_ddr2 = is_ddr1 = 0;
#endif
				break;
#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)
			case 2:
#else				
			case 1:
#endif
				is_ddr2 = 0;
				is_ddr1 = 1;
				break;
#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)
			case 3:
#else				
			case 2:
#endif
				is_ddr2 = 1;
				is_ddr1 = 0;
				break;
		}
		
		switch((data>>dram_type_bit_offset)&0x3)
		{
			case 0:
#if defined (MT7620_ASIC_BOARD) || defined (MT7620_FPGA_BOARD) || defined (MT7628_ASIC_BOARD) || defined (MT7628_FPGA_BOARD) || \
	defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)
#else
			case 3:
#endif				
				cfg0 = RALINK_REG(RALINK_MEMCTRL_BASE+0x0);
				cfg1 = RALINK_REG(RALINK_MEMCTRL_BASE+0x4);
				data = cfg1;
				
				dram_comp = 1<<(2+(((data>>16)&0x3)+11)+(((data>>20)&0x3)+8)+1+3-20);
				dram_bus = ((data>>24)&0x1) ? 32 : 16;
				size = 1<<(2 +(((data>>16)&0x3)+11)+(((data>>20)&0x3)+8)+1-20);       	
				break;
			case 1:
			case 2:
#if defined (MT7620_ASIC_BOARD) || defined (MT7620_FPGA_BOARD) || defined (MT7628_ASIC_BOARD) || defined (MT7628_FPGA_BOARD) || \
	defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)
			case 3:
#endif				
				cfg0 = RALINK_REG(RALINK_MEMCTRL_BASE+0x40);
				cfg1 = RALINK_REG(RALINK_MEMCTRL_BASE+0x44);
				data = cfg1;
				dram_comp = 1<<(((data>>18)&0x7)+5);
			    dram_bus = 1<<(((data>>12)&0x3)+2);	
				if(((data>>16)&0x3) < ((data>>12)&0x3))
				{
					size = 1<<(((data>>18)&0x7) + 22 + 1-20); 
				}
				else
				{
					size = 1<<(((data>>18)&0x7) + 22-20);
				}	
				break;
		}
#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)		
		if ((((RALINK_REG(RALINK_SYSCTL_BASE+0x8c)>>30)&0x1)==0) && ((chip_mode==2)||(chip_mode==3))) 
		{
#if defined(ON_BOARD_DDR2)
			is_ddr2 = 1;
			is_ddr1 = 0;
#elif defined(ON_BOARD_DDR1)
			is_ddr2 = 0;
			is_ddr1 = 1;
#else
			is_ddr2 = is_ddr1 = 0;
#endif
			dram_comp = DRAM_COMPONENT;
			dram_bus = DRAM_BUS;
			size = DRAM_SIZE;
		}
#endif
		printf("============================================ \n");
		printf("Ralink UBoot Version: %s\n", RALINK_LOCAL_VERSION);
		printf("-------------------------------------------- \n");
		printf("%s %s %s\n",CHIP_TYPE, CHIP_VERSION, GMAC_MODE);
#if defined (RT6855A_ASIC_BOARD) || defined (RT6855A_FPGA_BOARD)
#if defined (RT6855A_FPGA_BOARD)
		if((!is_ddr2)&&(!is_ddr1))
		{
		printf("[SDR_CFG0=0x%08X, SDR_CFG1=0x%08X]\n", RALINK_REG(RALINK_MEMCTRL_BASE+0x0),\
								RALINK_REG(RALINK_MEMCTRL_BASE+0x4));
		}	
		else
		{		
		printf("[DDR_CFG0 =0x%08X, DDR_CFG1 =0x%08X]\n", RALINK_REG(RALINK_MEMCTRL_BASE+0x40),\
								RALINK_REG(RALINK_MEMCTRL_BASE+0x44));
		printf("[DDR_CFG2 =0x%08X, DDR_CFG3 =0x%08X]\n", RALINK_REG(RALINK_MEMCTRL_BASE+0x48),\
								RALINK_REG(RALINK_MEMCTRL_BASE+0x4C));
		printf("[DDR_CFG4 =0x%08X, DDR_CFG10=0x%08X]\n", RALINK_REG(RALINK_MEMCTRL_BASE+0x50),\
								RALINK_REG(RALINK_MEMCTRL_BASE+0x68));
		}
#endif			
		printf("DRAM_CONF_FROM: %s \n", ((RALINK_REG(RALINK_SYSCTL_BASE+0x8c)>>30)&0x1) ? \
			"From SPI/NAND": (((chip_mode==2)||(chip_mode==3)) ? "From Uboot" : "Boot-strap"));
#elif defined (MT7620_ASIC_BOARD) || defined(MT7620_FPGA_BOARD) || defined (MT7628_ASIC_BOARD) || defined(MT7628_FPGA_BOARD)
		printf("DRAM_CONF_FROM: %s \n", (((RALINK_REG(RALINK_SYSCTL_BASE+0x10)>>8)&0x1)==0) ? "From SPI/NAND": 
				(((chip_mode==2)||(chip_mode==3)) ? "From Uboot" : "Auto-detection"));
#else
		printf("DRAM_CONF_FROM: %s \n", ((RALINK_REG(RALINK_SYSCTL_BASE+0x10)>>7)&0x1) ? "From SPI/NAND":
				(((chip_mode==2)||(chip_mode==3)) ? "From Uboot" : "Auto-detection"));
#endif		
		printf("DRAM_TYPE: %s \n", is_ddr2 ? "DDR2": (is_ddr1 ? "DDR1" : "SDRAM"));
		printf("DRAM component: %d Mbits\n", dram_comp);
		printf("DRAM bus: %d bit\n", dram_bus);
		printf("Total memory: %d MBytes\n", size);
		printf("%s\n", FLASH_MSG);
		printf("%s\n", "Date:" __DATE__ "  Time:" __TIME__ );
		printf("============================================ \n");
	}
#elif (defined (MT7621_ASIC_BOARD) || defined (MT7621_FPGA_BOARD))
	{
	printf("============================================ \n");
	printf("Ralink UBoot Version: %s\n", RALINK_LOCAL_VERSION);
	printf("-------------------------------------------- \n");
#ifdef RALINK_DUAL_CORE_FUN	
	printf("%s %s %s %s\n", CHIP_TYPE, RALINK_REG(RT2880_CHIP_REV_ID_REG)>>16&0x1 ? "MT7621A" : "MT7621N", "DualCore", GMAC_MODE);
#else
	if(RALINK_REG(RT2880_CHIP_REV_ID_REG)>>17&0x1) {
		printf("%s %s %s %s\n", CHIP_TYPE, RALINK_REG(RT2880_CHIP_REV_ID_REG)>>16&0x1 ? "MT7621A" : "MT7621N", "SingleCore", GMAC_MODE);
	}else {
		printf("%s %s%s %s\n", CHIP_TYPE, RALINK_REG(RT2880_CHIP_REV_ID_REG)>>16&0x1 ? "MT7621A" : "MT7621N", "S", GMAC_MODE);
	}
#endif
	printf("DRAM_CONF_FROM: %s \n", RALINK_REG(RT2880_SYSCFG_REG)>>9&0x1 ? "Auto-Detection" : "EEPROM");
	printf("DRAM_TYPE: %s \n", RALINK_REG(RT2880_SYSCFG_REG)>>4&0x1 ? "DDR2": "DDR3");
	printf("DRAM bus: %d bit\n", DRAM_BUS);
	printf("Xtal Mode=%d OCP Ratio=%s\n", RALINK_REG(RT2880_SYSCFG_REG)>>6&0x7, RALINK_REG(RT2880_SYSCFG_REG)>>5&0x1 ? "1/4":"1/3");
	printf("%s\n", FLASH_MSG);
	printf("%s\n", "Date:" __DATE__ "  Time:" __TIME__ );
	printf("============================================ \n");
	}
#else
	SHOW_VER_STR();
#endif /* defined(RT3352_FPGA_BOARD) || defined (RT3352_ASIC_BOARD) || defined(RT3883_FPGA_BOARD) || defined (RT3883_ASIC_BOARD) */

	printf("THIS IS %s\n", CONFIG_UBOOT_NAME);

#if defined (RT2880_FPGA_BOARD) || defined (RT2880_ASIC_BOARD)
	u32 value, kk;
	value = read_32bit_cp0_register_with_select1(CP0_CONFIG);

	kk = value >> 7;
	kk = kk & 0x7;

	if(kk)
	{
		debug(" D-CACHE set to %d way \n",kk + 1);
	}
	else
	{
		debug("\n D-CACHE Direct mapped\n");
	}

	kk = value >> 16;
	kk = kk & 0x7;


	if(kk)
	{
		debug(" I-CACHE set to %d way \n",kk + 1);
	}
	else
	{
		debug("\n I-CACHE Direct mapped\n");
	}
#else
	u32 config1,lsize,icache_linesz,icache_sets,icache_ways,icache_size;
	u32 dcache_linesz,dcache_sets,dcache_ways,dcache_size;

	config1 = read_32bit_cp0_register_with_select1(CP0_CONFIG);

	if ((lsize = ((config1 >> 19) & 7)))
		icache_linesz = 2 << lsize;
	else
		icache_linesz = lsize;
	icache_sets = 64 << ((config1 >> 22) & 7);
	icache_ways = 1 + ((config1 >> 16) & 7);

	icache_size = icache_sets *
		icache_ways *
		icache_linesz;

	printf("icache: sets:%d, ways:%d, linesz:%d ,total:%d\n", 
			icache_sets, icache_ways, icache_linesz, icache_size);

	/*
	 * Now probe the MIPS32 / MIPS64 data cache.
	 */

	if ((lsize = ((config1 >> 10) & 7)))
		dcache_linesz = 2 << lsize;
	else
		dcache_linesz = lsize;
	dcache_sets = 64 << ((config1 >> 13) & 7);
	dcache_ways = 1 + ((config1 >> 7) & 7);

	dcache_size = dcache_sets *
		dcache_ways *
		dcache_linesz;

	printf("dcache: sets:%d, ways:%d, linesz:%d ,total:%d \n", 
			dcache_sets, dcache_ways, dcache_linesz, dcache_size);

#endif
	
#ifdef CONFIG_LED_CHECK
		all_led_check(1);
#endif
	debug("\n ##### The CPU freq = %d MHZ #### \n",mips_cpu_feq/1000/1000);

/*
	if(*(volatile u_long *)(RALINK_SYSCTL_BASE + 0x0304) & (1<< 24))
	{
		debug("\n SDRAM bus set to 32 bit \n");
	}
	else
	{
		debug("\nSDRAM bus set to 16 bit \n");
	}
*/
	debug(" estimate memory size =%d Mbytes\n",gd->ram_size /1024/1024 );

//#ifdef CONFIG_INIT_SWITCH
#if defined (RT3052_ASIC_BOARD) || defined (RT3052_FPGA_BOARD)  || \
    defined (RT3352_ASIC_BOARD) || defined (RT3352_FPGA_BOARD)  || \
    defined (RT5350_ASIC_BOARD) || defined (RT5350_FPGA_BOARD)  || \
    defined (MT7628_ASIC_BOARD) || defined (MT7628_FPGA_BOARD)
	rt305x_esw_init();
#elif defined (RT6855_ASIC_BOARD) || defined (RT6855_FPGA_BOARD) || \
      defined (MT7620_ASIC_BOARD) || defined (MT7620_FPGA_BOARD)
	rt_gsw_init();
#elif defined (RT6855A_ASIC_BOARD) || defined (RT6855A_FPGA_BOARD)
#ifdef FPGA_BOARD
	rt6855A_eth_gpio_reset();
#endif
	rt6855A_gsw_init();
#elif defined (MT7621_ASIC_BOARD) || defined (MT7621_FPGA_BOARD)
#if defined (MAC_TO_MT7530_MODE) || defined (GE_RGMII_INTERNAL_P0_AN) || defined (GE_RGMII_INTERNAL_P4_AN)
	//enable MDIO
	/* delete by wanghao  */
	/*RALINK_REG(0xbe000060) &= ~(1 << 12); //set MDIO to Normal mode
	RALINK_REG(0xbe000060) &= ~(1 << 14); //set RGMII1 to Normal mode
	RALINK_REG(0xbe000060) &= ~(1 << 15); //set RGMII2 to Normal mode
	setup_internal_gsw();*/
	/* add by wanghao  */
#ifdef CONFIG_TINY_UBOOT
	//disablePhy();
#endif
	/* add end  */
#endif
#elif defined (RT3883_ASIC_BOARD) && defined (MAC_TO_MT7530_MODE)
        rt3883_gsw_init();
#endif
	//LANWANPartition();/* delete by wanghao  */
			
	LoadUbootData();
//#endif/*CONFIG_INIT_SWITCH*/
/*config bootdelay via environment parameter: bootdelay */
	#if 0
	{
	    char * s;
	    s = getenv ("bootdelay");
	    timer1 = s ? (int)simple_strtol(s, NULL, 10) : CONFIG_BOOTDELAY;
	printf("boot delay %d:%s\n", timer1,s?s:"");
	
	}
	#else
	timer1 = CONFIG_BOOTDELAY;
	#endif

	#if 0
	s = getenv ("BootType");
	if(s) {
		BootType = *s;
	} else {
		char s1[2];
		memset(s1, 0, 2);
		*s1 = BootType;
		setenv("BootType", s1);
	}
	#endif
	BootType = 'b';
	OperationSelect();   
	//printf("default: '%c'\n", BootType);
	
#if defined(CONFIG_PRESS_BUTTON_ENTER_RECOVERY)
	if (check_recovery_button())
		{
		BootType = 'x';
		};
#endif
#if 0
	timer1 = 1;
#endif
	while (timer1 > 0) {
		--timer1;
		/* delay 100 * 10ms */
		for (i=0; i<100; ++i) {
			if ((my_tmp = tstc()) != 0) {	/* we got a key press	*/
				timer1 = 0;	/* no more delay	*/
				BootType = getc();
				if (BootType == 't')
				{
					BootType = '4';
					break;
				}
			}
			udelay (10000);
		}
		printf ("\b\b\b%2d ", timer1);
	}
	putc ('\n');
	{
		char *argv[4];
		int argc= 3;

		argv[2] = &file_name_space[0];
		memset(file_name_space,0,ARGV_LEN);

#if (CONFIG_COMMANDS & CFG_CMD_NET)
		//eth_initialize(gd->bd);
#endif
		
#ifdef CONFIG_LED_CHECK
		all_led_check(2);
#endif

		switch(BootType) {
	/* we do not consider those conditions. */
#if 0
		case '1':
			printf("   \n%d: System Load Linux to SDRAM via TFTP. \n", SEL_LOAD_LINUX_SDRAM);
			tftp_config(SEL_LOAD_LINUX_SDRAM, argv);           
			argc= 3;
			setenv("autostart", "yes");
			do_tftpb(cmdtp, 0, argc, argv);
			break;

		case '2':
			printf("   \n%d: System Load Linux Kernel then write to Flash via TFTP. \n", SEL_LOAD_LINUX_WRITE_FLASH);
			printf(" Warning!! Erase Linux in Flash then burn new one. Are you sure?(Y/N)\n");
			confirm = getc();
			if (confirm != 'y' && confirm != 'Y') {
				printf(" Operation terminated\n");
				break;
			}
			tftp_config(SEL_LOAD_LINUX_WRITE_FLASH, argv);
			argc= 3;
			setenv("autostart", "no");
			do_tftpb(cmdtp, 0, argc, argv);

#if defined (CFG_ENV_IS_IN_NAND)
			if (1) {
				unsigned int load_address = simple_strtoul(argv[1], NULL, 16);
				ranand_erase_write((u8 *)load_address, CFG_KERN_ADDR-CFG_FLASH_BASE, NetBootFileXferSize);
			}
#elif defined (CFG_ENV_IS_IN_SPI)
			if (1) {
				unsigned int load_address = simple_strtoul(argv[1], NULL, 16);
				raspi_erase_write((u8 *)load_address, CFG_KERN_ADDR-CFG_FLASH_BASE, NetBootFileXferSize);
			}
#else //CFG_ENV_IS_IN_FLASH
#if (defined (ON_BOARD_8M_FLASH_COMPONENT) || defined (ON_BOARD_16M_FLASH_COMPONENT)) && (defined (RT2880_ASIC_BOARD) || defined (RT2880_FPGA_BOARD) || defined (RT3052_MP1))
			//erase linux
			if (NetBootFileXferSize <= (0x400000 - (CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_FACTORY_SIZE))) {
				e_end = CFG_KERN_ADDR + NetBootFileXferSize;
				if (0 != get_addr_boundary(&e_end))
					break;
				printf("Erase linux kernel block !!\n");
				printf("From 0x%X To 0x%X\n", CFG_KERN_ADDR, e_end);
				flash_sect_erase(CFG_KERN_ADDR, e_end);
			}
			else if (NetBootFileXferSize <= CFG_KERN_SIZE) {
				e_end = PHYS_FLASH_2 + NetBootFileXferSize - (0x400000 - (CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_FACTORY_SIZE));
				if (0 != get_addr_boundary(&e_end))
					break;
				printf("Erase linux kernel block !!\n");
				printf("From 0x%X To 0x%X\n", CFG_KERN_ADDR, CFG_FLASH_BASE+0x3FFFFF);
				flash_sect_erase(CFG_KERN_ADDR, CFG_FLASH_BASE+0x3FFFFF);
				printf("Erase linux file system block !!\n");
				printf("From 0x%X To 0x%X\n", PHYS_FLASH_2, e_end);
				flash_sect_erase(PHYS_FLASH_2, e_end);
			}
#else
			if (NetBootFileXferSize <= (bd->bi_flashsize - (CFG_BOOTLOADER_SIZE + CFG_CONFIG_SIZE + CFG_FACTORY_SIZE))) {
				e_end = CFG_KERN_ADDR + NetBootFileXferSize;
				if (0 != get_addr_boundary(&e_end))
					break;
				printf("Erase linux kernel block !!\n");
				printf("From 0x%X To 0x%X\n", CFG_KERN_ADDR, e_end);
				flash_sect_erase(CFG_KERN_ADDR, e_end);
			}
#endif
			else {
				printf("***********************************\n");
				printf("The Linux Image size is too big !! \n");
				printf("***********************************\n");
				break;
			}

#ifdef DUAL_IMAGE_SUPPORT
			/* Don't do anything to the firmware upgraded in Uboot, since it may be used for testing */
			setenv("Image1Stable", "1");
			saveenv();
#endif

			//cp.linux
			argc = 4;
			argv[0]= "cp.linux";
			do_mem_cp(cmdtp, 0, argc, argv);
#endif //CFG_ENV_IS_IN_FLASH

#ifdef DUAL_IMAGE_SUPPORT
			//reset
			do_reset(cmdtp, 0, argc, argv);
#endif

			//bootm bc050000
			argc= 2;
			sprintf(addr_str, "0x%X", CFG_KERN_ADDR);
			argv[1] = &addr_str[0];
			do_bootm(cmdtp, 0, argc, argv);            
#ifdef RALINK_HTTP_UPGRADE_FUN
			NetUipLoop = 1;
			uip_main();
#endif
			break;
#endif/*if 0*/
#ifdef RALINK_CMDLINE
		case CONFIG_BREAK_BOOTING_BUTTON + '0':
		case 't':
			run_command("ethon", 0);//add by wanghao
			
			printf("   \n%d: System Enter Boot Command Line Interface.\n", SEL_ENTER_CLI);
			printf ("\n%s\n", version_string);
			/* main_loop() can retun to retry autoboot, if so just run it again. */
			for (;;) {					
				main_loop ();
			}
			break;
#endif // RALINK_CMDLINE //
#if 0
#ifdef RALINK_UPGRADE_BY_SERIAL
		case '7':
			printf("\n%d: System Load Boot Loader then write to Flash via Serial. \n", SEL_LOAD_BOOT_WRITE_FLASH_BY_SERIAL);
			argc= 1;
			setenv("autostart", "no");
			my_tmp = do_load_serial_bin(cmdtp, 0, argc, argv);
			NetBootFileXferSize=simple_strtoul(getenv("filesize"), NULL, 16);
#if defined(SMALL_UBOOT_PARTITION)
			if (NetBootFileXferSize > CFG_UBOOT_SIZE || my_tmp == 1) {
				printf("Abort: Bootloader is too big or download aborted!\n");
			}
#else
			if (NetBootFileXferSize > CFG_BOOTLOADER_SIZE || my_tmp == 1) {
				printf("Abort: Bootloader is too big or download aborted!\n");
			}
#endif
#if defined (CFG_ENV_IS_IN_NAND)
			else {
				ranand_erase_write((char *)CFG_LOAD_ADDR, 0, NetBootFileXferSize);
			}
#elif defined (CFG_ENV_IS_IN_SPI)
			else {
				raspi_erase_write((char *)CFG_LOAD_ADDR, 0, NetBootFileXferSize);
			}
#else //CFG_ENV_IS_IN_FLASH
			else {
				//protect off uboot
				flash_sect_protect(0, CFG_FLASH_BASE, CFG_FLASH_BASE+CFG_BOOTLOADER_SIZE-1);

				//erase uboot
				printf("\n Erase U-Boot block !!\n");
				printf("From 0x%X To 0x%X\n", CFG_FLASH_BASE, CFG_FLASH_BASE+CFG_BOOTLOADER_SIZE-1);
				flash_sect_erase(CFG_FLASH_BASE, CFG_FLASH_BASE+CFG_BOOTLOADER_SIZE-1);

				//cp.uboot            
				argc = 4;
				argv[0]= "cp.uboot";
				do_mem_cp(cmdtp, 0, argc, argv);                       

				//protect on uboot
				flash_sect_protect(1, CFG_FLASH_BASE, CFG_FLASH_BASE+CFG_BOOTLOADER_SIZE-1);
			}
#endif //CFG_ENV_IS_IN_FLASH

			//reset            
			do_reset(cmdtp, 0, argc, argv);
			break;
#endif // RALINK_UPGRADE_BY_SERIAL //
		case '8':
			printf("   \n%d: System Load UBoot to SDRAM via TFTP. \n", SEL_LOAD_BOOT_SDRAM);
			tftp_config(SEL_LOAD_BOOT_SDRAM, argv);
			argc= 3;
			setenv("autostart", "yes");
			do_tftpb(cmdtp, 0, argc, argv);
			break;

		case '9':
			printf("   \n%d: System Load Boot Loader then write to Flash via TFTP. \n", SEL_LOAD_BOOT_WRITE_FLASH);
			printf(" Warning!! Erase Boot Loader in Flash then burn new one. Are you sure?(Y/N)\n");
			confirm = getc();
			if (confirm != 'y' && confirm != 'Y') {
				printf(" Operation terminated\n");
				break;
			}
			tftp_config(SEL_LOAD_BOOT_WRITE_FLASH, argv);
			argc= 3;
			setenv("autostart", "no");
			do_tftpb(cmdtp, 0, argc, argv);
#if defined(SMALL_UBOOT_PARTITION)
			if (NetBootFileXferSize > CFG_UBOOT_SIZE) {
				printf("Abort: bootloader size %d too big! \n", NetBootFileXferSize);
			}
#else
			if (NetBootFileXferSize > CFG_BOOTLOADER_SIZE) {
				printf("Abort: bootloader size %d too big! \n", NetBootFileXferSize);
			}
#endif
#if defined (CFG_ENV_IS_IN_NAND)
			else {
				unsigned int load_address = simple_strtoul(argv[1], NULL, 16);
				ranand_erase_write((char *)load_address, 0, NetBootFileXferSize);
			}
#elif defined (CFG_ENV_IS_IN_SPI)
			else {
				unsigned int load_address = simple_strtoul(argv[1], NULL, 16);
				raspi_erase_write((char *)load_address, 0, NetBootFileXferSize);
			}
#else //CFG_ENV_IS_IN_FLASH
			else {
				//protect off uboot
				flash_sect_protect(0, CFG_FLASH_BASE, CFG_FLASH_BASE+CFG_BOOTLOADER_SIZE-1);

				//erase uboot
				printf("\n Erase U-Boot block !!\n");
				printf("From 0x%X To 0x%X\n", CFG_FLASH_BASE, CFG_FLASH_BASE+CFG_BOOTLOADER_SIZE-1);
				flash_sect_erase(CFG_FLASH_BASE, CFG_FLASH_BASE+CFG_BOOTLOADER_SIZE-1);

				//cp.uboot            
				argc = 4;
				argv[0]= "cp.uboot";
				do_mem_cp(cmdtp, 0, argc, argv);                       

				//protect on uboot
				flash_sect_protect(1, CFG_FLASH_BASE, CFG_FLASH_BASE+CFG_BOOTLOADER_SIZE-1);
			}
#endif //CFG_ENV_IS_IN_FLASH

			//reset            
			do_reset(cmdtp, 0, argc, argv);
			break;
#ifdef RALINK_UPGRADE_BY_SERIAL
#if defined (CFG_ENV_IS_IN_NAND) || defined (CFG_ENV_IS_IN_SPI)
		case '0':
			printf("\n%d: System Load Linux then write to Flash via Serial. \n", SEL_LOAD_LINUX_WRITE_FLASH_BY_SERIAL);
			argc= 1;
			setenv("autostart", "no");
			my_tmp = do_load_serial_bin(cmdtp, 0, argc, argv);
			NetBootFileXferSize=simple_strtoul(getenv("filesize"), NULL, 16);

#if defined (CFG_ENV_IS_IN_NAND)
			ranand_erase_write((char *)CFG_LOAD_ADDR, CFG_KERN_ADDR-CFG_FLASH_BASE, NetBootFileXferSize);
#elif defined (CFG_ENV_IS_IN_SPI)
			raspi_erase_write((char *)CFG_LOAD_ADDR, CFG_KERN_ADDR-CFG_FLASH_BASE, NetBootFileXferSize);
#endif //CFG_ENV_IS_IN_FLASH

			//reset            
			do_reset(cmdtp, 0, argc, argv);
			break;
#endif
#endif // RALINK_UPGRADE_BY_SERIAL //


#if defined (RALINK_USB) || defined (MTK_USB)
#if defined (CFG_ENV_IS_IN_NAND) || defined (CFG_ENV_IS_IN_SPI)
		case '5':
			printf("\n%d: System Load Linux then write to Flash via USB Storage. \n", 5);

			argc = 2;
			argv[1] = "start";
			do_usb(cmdtp, 0, argc, argv);
			if( usb_stor_curr_dev < 0){
				printf("No USB Storage found. Upgrade F/W failed.\n");
				break;
			}

			argc= 5;
			argv[1] = "usb";
			argv[2] = "0";
			sprintf(addr_str, "0x%X", CFG_LOAD_ADDR);
			argv[3] = &addr_str[0];
			argv[4] = "root_uImage";
			setenv("autostart", "no");
			if(do_fat_fsload(cmdtp, 0, argc, argv)){
				printf("Upgrade F/W from USB storage failed.\n");
				break;
			}

			NetBootFileXferSize=simple_strtoul(getenv("filesize"), NULL, 16);
#if defined (CFG_ENV_IS_IN_NAND)
			ranand_erase_write((char *)CFG_LOAD_ADDR, CFG_KERN_ADDR-CFG_FLASH_BASE, NetBootFileXferSize);
#elif defined (CFG_ENV_IS_IN_SPI)
			raspi_erase_write((char *)CFG_LOAD_ADDR, CFG_KERN_ADDR-CFG_FLASH_BASE, NetBootFileXferSize);
#endif //CFG_ENV_IS_IN_FLASH

			//reset            
			do_reset(cmdtp, 0, argc, argv);
			break;
#endif
#endif // RALINK_UPGRADE_BY_USB //
		case '6':
			auto_load = 1;
			printf("   \n%d: System Load Linux to SDRAM via TFTP [Automatically]. \n", SEL_LOAD_LINUX_SDRAM);
			tftp_config(SEL_LOAD_LINUX_SDRAM, argv);           
			argc= 3;
			setenv("autostart", "yes");
			do_tftpb(cmdtp, 0, argc, argv);
			auto_load = 0;
			break;

		#ifdef RALINK_HTTP_UPGRADE_FUN
			eth_initialize(gd->bd);
			NetUipLoop = 1;
			uip_main();
#endif 
#endif /* if 0*/
#if defined(MINI_WEB_SERVER_SUPPORT)
		case 'x':
			run_command("ethon", 0);//add by wanghao
			do_httpd(cmdtp, 0, 1, argv);
			break;
#endif

		default:
		case 'b':
		
		
	/* jump to next boot part */

			printf("   \n3: System Boot system code via Flash.\n");


#if defined(CHECK_DUAL_IMAGE_AND_BOOT)
			boot_dual_image(cmdtp);
#elif defined(BOOT_NEXT_IMAGE)
			// need start another image when return value > 0 
			if (boot_main_image(cmdtp) > 0)
			{
				boot_main_image(cmdtp);
			}
#else
			boot_next(cmdtp);
#endif
			/* do not break. If failed, it will start webserver */
#if defined(MINI_WEB_SERVER_SUPPORT)
			run_command("ethon", 0);//add by wanghao
			do_httpd(cmdtp, 0, 1, argv);
			break;
#endif

			break;
		} /* end of switch */   
#if 0
		do_reset(cmdtp, 0, argc, argv);
#endif

	} /* end of } */

	/* NOTREACHED - no way out of command loop except booting */
}


void hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}

#if defined (RALINK_RW_RF_REG_FUN)
#if defined (MT7620_ASIC_BOARD)
#define RF_CSR_CFG      0xb0180500
#define RF_CSR_KICK     (1<<0)
int rw_rf_reg(int write, int reg, int *data)
{
	u32	rfcsr, i = 0;

	while (1) {
		rfcsr = RALINK_REG(RF_CSR_CFG);
		if (! (rfcsr & (u32)RF_CSR_KICK) )
			break;
		if (++i > 10000) {
			puts("Warning: Abort rw rf register: too busy\n");
			return -1;
		}
	}
	rfcsr = (u32)(RF_CSR_KICK | ((reg & 0x3f) << 16)  | ((*data & 0xff) << 8));
	if (write)
		rfcsr |= 0x10;

	RALINK_REG(RF_CSR_CFG) = cpu_to_le32(rfcsr);
	i = 0;
	while (1) {
		rfcsr = RALINK_REG(RF_CSR_CFG);
		if (! (rfcsr & (u32)RF_CSR_KICK) )
			break;
		if (++i > 10000) {
			puts("Warning: still busy\n");
			return -1;
		}
	}

	rfcsr = RALINK_REG(RF_CSR_CFG);
	if (((rfcsr & 0x3f0000) >> 16) != (reg & 0x3f)) {
		puts("Error: rw register failed\n");
		return -1;
	}
	*data = (int)( (rfcsr & 0xff00) >> 8) ;
	return 0;
}
#else
#define RF_CSR_CFG      0xb0180500
#define RF_CSR_KICK     (1<<17)
int rw_rf_reg(int write, int reg, int *data)
{
	u32	rfcsr, i = 0;

	while (1) {
		rfcsr = RALINK_REG(RF_CSR_CFG);
		if (! (rfcsr & (u32)RF_CSR_KICK) )
			break;
		if (++i > 10000) {
			puts("Warning: Abort rw rf register: too busy\n");
			return -1;
		}
	}


	rfcsr = (u32)(RF_CSR_KICK | ((reg & 0x3f) << 8)  | (*data & 0xff));
	if (write)
		rfcsr |= 0x10000;

	RALINK_REG(RF_CSR_CFG) = cpu_to_le32(rfcsr);

	i = 0;
	while (1) {
		rfcsr = RALINK_REG(RF_CSR_CFG);
		if (! (rfcsr & (u32)RF_CSR_KICK) )
			break;
		if (++i > 10000) {
			puts("Warning: still busy\n");
			return -1;
		}
	}

	rfcsr = RALINK_REG(RF_CSR_CFG);

	if (((rfcsr&0x1f00) >> 8) != (reg & 0x1f)) {
		puts("Error: rw register failed\n");
		return -1;
	}
	*data = (int)(rfcsr & 0xff);

	return 0;
}
#endif
#endif

#ifdef RALINK_RW_RF_REG_FUN
#ifdef RALINK_CMDLINE
int do_rw_rf(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int write, reg, data;

	if ((argv[1][0] == 'r' || argv[1][0] == 'R') && (argc == 3)) {
		write = 0;
		reg = (int)simple_strtoul(argv[2], NULL, 10);
		data = 0;
	}
	else if ((argv[1][0] == 'w' || argv[1][0] == 'W') && (argc == 4)) {
		write = 1;
		reg = (int)simple_strtoul(argv[2], NULL, 10);
		data = (int)simple_strtoul(argv[3], NULL, 16);
	}
	else {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	rw_rf_reg(write, reg, &data);
	if (!write)
		printf("rf reg <%d> = 0x%x\n", reg, data);
	return 0;
}

U_BOOT_CMD(
	rf,     4,     1,      do_rw_rf,
	"rf      - read/write rf register\n",
	"usage:\n"
	"rf r <reg>        - read rf register\n"
	"rf w <reg> <data> - write rf register (reg: decimal, data: hex)\n"
);
#endif // RALINK_CMDLINE //
#endif

#if defined(RT3352_ASIC_BOARD)
void adjust_crystal_circuit(void)
{
	int d = 0x45;

	rw_rf_reg(1, 18, &d);
}
#endif

#if defined(RT3052_ASIC_BOARD)
/*
 *  Adjust core voltage to 1.2V using RF reg 26 for the 3052 two layer board.
 */
void adjust_voltage(void)
{
	int d = 0x25;
	rw_rf_reg(1, 26, &d);
}

void adjust_frequency(void)
{
	u32 r23;

	//read from EE offset 0x3A
#if defined (CFG_ENV_IS_IN_NAND)
	ranand_read((char *)&r23, CFG_FACTORY_ADDR-CFG_FLASH_BASE+0x3a, 1);
#elif defined (CFG_ENV_IS_IN_SPI)
	raspi_read((char *)&r23, CFG_FACTORY_ADDR-CFG_FLASH_BASE+0x3a, 1);
#else //CFG_ENV_IS_IN_FLASH
	r23 = *(volatile u32 *)(CFG_FACTORY_ADDR+0x38);
	r23 >>= 16;
#endif
	r23 &= 0xff;
	//write to RF R23
	rw_rf_reg(1, 23, &r23);
}
#endif

#if defined (RT3352_ASIC_BOARD) || defined (RT3883_ASIC_BOARD)
void adjust_rf_r17(void)
{
	u32 r17;
	u32 i;
	u32 val;
	u32 j = 0;

	//read from EE offset 0x3A
#if defined (CFG_ENV_IS_IN_NAND)
	ranand_read((char *)&r17, CFG_FACTORY_ADDR-CFG_FLASH_BASE+0x3a, 1);
#elif defined (CFG_ENV_IS_IN_SPI)
	raspi_read((char *)&r17, CFG_FACTORY_ADDR-CFG_FLASH_BASE+0x3a, 1);
#else //CFG_ENV_IS_IN_FLASH
#if defined (RT2880_ASIC_BOARD)
	r17 = *(volatile u32 *)(CFG_FACTORY_ADDR+0x3a);
#elif defined (RT3883_ASIC_BOARD)
	r17 = *(volatile u32 *)(CFG_FACTORY_ADDR+0x44);
#endif

#endif
	r17 &= 0xff;
	//printf("EE offset 0x3A is  0x%0X\n", r17);
	if((r17 == 0) || (r17 == 0xff)){
	    r17 = 0x2c;
	}

	if(r17 <= 0xf) {
		for(i=1; i<=r17; i++) {
		//write to RF R17
		val = i;
		val |= 1 << 7;
		rw_rf_reg(1, 17, &val);
		udelay(2000);
		rw_rf_reg(0, 17, &val);
		//printf("Update RF_R17 to 0x%0X\n", val);
		}	
	}
	else{
		for(i=1; i<=0xf; i++) {
		//write to RF R17
		val = i;
		val |= 1 << 7;
		rw_rf_reg(1, 17, &val);
		udelay(2000);
		rw_rf_reg(0, 17, &val);
		printf("Update RF_R17 to 0x%0X\n", val);
		}
		val = 0x1f;
		val |= 1 << 7;
		rw_rf_reg(1, 17, &val);
		udelay(2000);
		rw_rf_reg(0, 17, &val);
		printf("Update RF_R17 to 0x%0X\n", val);
		
		if(r17 <= 0x1f) {
			for(i=0x1e; i>=r17; i--) {
			//write to RF R17
			val = i;
			val |= 1 << 7;
			rw_rf_reg(1, 17, &val);
			udelay(2000);
			rw_rf_reg(0, 17, &val);
			printf("Update RF_R17 to 0x%0X\n", val);
			}
		} else if((r17 > 0x1f) && (r17 <=0x2f)){
			for(i=0x2f; i>=r17; i--) {
			//write to RF R17
			val = i;
			val |= 1 << 7;
			rw_rf_reg(1, 17, &val);
			udelay(2000);
			rw_rf_reg(0, 17, &val);
			//printf("Update RF_R17 to 0x%0X\n", val);
			}
		}else {
			val = 0x2f;
			val |= 1 << 7;
			rw_rf_reg(1, 17, &val);
			udelay(2000);
			rw_rf_reg(0, 17, &val);
			//printf("Update RF_R17 to 0x%0X\n", val);
		}
		if((r17 > 0x2f) && (r17 <= 0x3f)){
			for(i=0x3f; i>=r17; i--) {
			//write to RF R17
			val = i;
			val |= 1 << 7;
			rw_rf_reg(1, 17, &val);
			udelay(2000);
			rw_rf_reg(0, 17, &val);
			//printf("Update RF_R17 to 0x%0X\n", val);
			}
		}
		if(r17 > 0x3f){
			val = 0x3f;
			val |= 1 << 7;
			rw_rf_reg(1, 17, &val);
			udelay(2000);
			rw_rf_reg(0, 17, &val);
			//printf("Only Update RF_R17 to 0x%0X\n", val);
		}
	}
	//rw_rf_reg(0, 17, &val);
	//printf("Read RF_R17 = 0x%0X\n", val);
}
#endif

#if defined(RT3883_ASIC_BOARD) || defined(RT3352_ASIC_BOARD) || defined(RT5350_ASIC_BOARD) || defined(RT6855_ASIC_BOARD) || defined (MT7620_ASIC_BOARD) || defined (MT7628_ASIC_BOARD)
/*
 * enter power saving mode
 */
void config_usb_ehciohci(void)
{
	u32 val;
	
	val = RALINK_REG(RT2880_RSTCTRL_REG);    // toggle host & device RST bit
	val = val | RALINK_UHST_RST | RALINK_UDEV_RST;
	RALINK_REG(RT2880_RSTCTRL_REG) = val;

	val = RALINK_REG(RT2880_CLKCFG1_REG);
#if defined(RT5350_ASIC_BOARD) || defined(RT6855_ASIC_BOARD)
	val = val & ~(RALINK_UPHY0_CLK_EN) ;  // disable USB port0 PHY. 
#else
	val = val & ~(RALINK_UPHY0_CLK_EN | RALINK_UPHY1_CLK_EN) ;  // disable USB port0 & port1 PHY. 
#endif
	RALINK_REG(RT2880_CLKCFG1_REG) = val;
}
#endif /* (RT3883_ASIC_BOARD) || defined(RT3352_ASIC_BOARD)|| defined(RT5350_ASIC_BOARD) || defined(RT6855_ASIC_BOARD) || defined (MT7620_ASIC_BOARD) */


#if defined(MT7621_ASIC_BOARD)
void config_usb_mtk_xhci(void)
{
	u32	regValue;

	regValue = RALINK_REG(RALINK_SYSCTL_BASE + 0x10);
	regValue = (regValue >> 6) & 0x7;
	if(regValue >= 6) { //25Mhz Xtal
		printf("\nConfig XHCI 25M PLL \n");
		RALINK_REG(0xbe1d0784) = 0x20201a;
		RALINK_REG(0xbe1d0c20) = 0x80004;
		RALINK_REG(0xbe1d0c1c) = 0x18181819;
		RALINK_REG(0xbe1d0c24) = 0x18000000;
		RALINK_REG(0xbe1d0c38) = 0x25004a;
		RALINK_REG(0xbe1d0c40) = 0x48004a;
		RALINK_REG(0xbe1d0b24) = 0x190;
		RALINK_REG(0xbe1d0b10) = 0x1c000000;
		RALINK_REG(0xbe1d0b04) = 0x20000004;
		RALINK_REG(0xbe1d0b08) = 0xf203200;

		RALINK_REG(0xbe1d0b2c) = 0x1400028;
		//RALINK_REG(0xbe1d0a30) =;
		RALINK_REG(0xbe1d0a40) = 0xffff0001;
		RALINK_REG(0xbe1d0a44) = 0x60001;
	} else if (regValue >=3 ) { // 40 Mhz
		printf("\nConfig XHCI 40M PLL \n");
		RALINK_REG(0xbe1d0784) = 0x20201a;
		RALINK_REG(0xbe1d0c20) = 0x80104;
		RALINK_REG(0xbe1d0c1c) = 0x1818181e;
		RALINK_REG(0xbe1d0c24) = 0x1e400000;
		RALINK_REG(0xbe1d0c38) = 0x250073;
		RALINK_REG(0xbe1d0c40) = 0x71004a;
		RALINK_REG(0xbe1d0b24) = 0x140;
		RALINK_REG(0xbe1d0b10) = 0x23800000;
		RALINK_REG(0xbe1d0b04) = 0x20000005;
		RALINK_REG(0xbe1d0b08) = 0x12203200;
	
		RALINK_REG(0xbe1d0b2c) = 0x1400028;
		//RALINK_REG(0xbe1d0a30) =;
		RALINK_REG(0xbe1d0a40) = 0xffff0001;
		RALINK_REG(0xbe1d0a44) = 0x60001;
	} else { //20Mhz Xtal

		/* TODO */

	}
}

#endif

#if defined(RT3052_ASIC_BOARD) || defined(RT2883_ASIC_BOARD)
int usbotg_host_suspend(void)
{
	u32 val;
	int i, rc=0, retry_count=0;
	
	printf(".");

retry_suspend:
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
	val = val >> 10;
	val = val & 0x00000003;

	if(val == 0x3){
		if(retry_count++ < 0x100000)
			goto retry_suspend;
		printf("*** Error: D+/D- is 1/1, config usb failed.\n");
		return -1;
	}
	//printf("Config usb otg 2\n");

	val = le32_to_cpu(*(volatile u_long *)(0xB01C0400));
	//printf("1.b01c0400 = 0x%08x\n", val);
	//printf("force \"FS-LS only mode\"\n");
	val = val | (1 << 2);
	*(volatile u_long *)(0xB01C0400) = cpu_to_le32(val);
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0400));
	//printf("2.b01c0400 = 0x%08x\n", val);

    val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
    //printf("3.b01c0440 = 0x%08x\n", val);

	//printf("port power on\n");
    val = val | (1 << 12);
	*(volatile u_long *)(0xB01C0440) = cpu_to_le32(val);
    val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
    //printf("4.b01c0440 = 0x%08x\n", val);

	udelay(3000);	// 3ms

	////printf("check port connect status\n");
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
	////printf("5.b01c0440 = 0x%08x\n", val);

	////printf("port reset --set\n");
	val = val | (1 << 8);
	*(volatile u_long *)(0xB01C0440) = cpu_to_le32(val);
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
	//printf("6.b01c0440 = 0x%08x\n", val);

	udelay(10000);
	
	//printf("port reset -- clear\n");
	val = val & ~(1 << 8);
	*(volatile u_long *)(0xB01C0440) = cpu_to_le32(val);
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
	//printf("7.b01c0440 = 0x%08x\n", val);

	udelay(1000);

	//printf("port suspend --set\n");
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
	//printf("b.b01c0440 = 0x%08x\n", val);
	val = val | (1 << 7);
	/* avoid write 1 to port enable */
	val = val & 0xFFFFFFF3;

	*(volatile u_long *)(0xB01C0440) = cpu_to_le32(val);

	val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
	//printf("c.b01c0440 = 0x%08x\n", val);

	//printf(" stop pclk\n");
	*(volatile u_long *)(0xB01C0E00) = cpu_to_le32(0x1);
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0E00));
	//printf("8.b01c0e00 = 0x%08x\n", val);

	//printf("wait for suspend...");
	for(i=0; i<200000; i++){
		val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
		val = val & (1 << 7);
		if(val)
			break;			// done.
		udelay(1);
	}

	if(i==200000){
		//printf("timeout, ignore...(0x%08x)\n", val);
		rc = -1;
	}
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0440));
	//printf("val = 0x%08x\n", val);
	udelay(100000);

	val = RALINK_REG(RT2880_RSTCTRL_REG);    // reset OTG
	val = val | RALINK_OTG_RST;
	RALINK_REG(RT2880_RSTCTRL_REG) = val;
	val = val & ~(RALINK_OTG_RST);
	RALINK_REG(RT2880_RSTCTRL_REG) = val;
	udelay(200000);

	udelay(200000);

	return rc;
}


int usbotg_device_suspend(void)
{
	u32 val;
	int rc = -1, try_count;

	printf(".");

	RALINK_REG(0xB01C0E00) = 0xF;
	udelay(100000);
	val = le32_to_cpu(*(volatile u_long *)(0xB01C0808));
	//printf("B01c0808 = 0x%08x\n", val);

	RALINK_REG(0xB01C000C) = 0x40001408;    // force device mode
	udelay(50000);
	RALINK_REG(0xB01C0E00) = 0x1;           // stop pclock
	udelay(100000);

	/* Some layouts need more time. */
	for(try_count=0 ; try_count< 1000 /* ms */; try_count++)
	{
		val = le32_to_cpu(*(volatile u_long *)(0xB01C0808));
		//printf("B01C0808 = 0x%08x\n", val);
		if((val & 0x1)){
			rc = 0;
			break;
		}
		udelay(1000);
	}

	val = RALINK_REG(RT2880_RSTCTRL_REG);    // reset OTG
	val = val | RALINK_OTG_RST;
	RALINK_REG(RT2880_RSTCTRL_REG) = val;
	val = val & ~(RALINK_OTG_RST);
	RALINK_REG(RT2880_RSTCTRL_REG) = val;
	udelay(200000);

	return rc;
}

void config_usbotg(void)
{
	int i, host_rc, device_rc;	

	printf("config usb");
	for(i=0;i<2;i++){
		device_rc = usbotg_device_suspend();
		host_rc = usbotg_host_suspend();

		if(host_rc == -1 && device_rc == -1)
			continue;
		else
			break;
	}
	
	RALINK_REG(0xB01C0E00) = 0xF;        //disable USB module, optimize for power-saving
	printf("\n");
	return;
}

#endif

#if defined (RT6855A_ASIC_BOARD) || defined(RT6855A_FPGA_BOARD)	
static int watchdog_reset()
{
	unsigned int word;
	unsigned int i;

	/* check if do watch dog reset */
	if ((RALINK_REG(RALINK_HIR_REG) & 0xffff0000) == 0x40000) {
		if (!(RALINK_REG(0xbfb00080) >> 31)) {
			/* set delay counter */
			RALINK_REG(RALINK_TIMER5_LDV) = 1000;
			/* enable watch dog timer */
			word = RALINK_REG(RALINK_TIMER_CTL);
			word |= ((1 << 5) | (1 << 25));
			RALINK_REG(RALINK_TIMER_CTL) = word;
			while(1);
		}
	}

	return 0;
}
#endif

void disable_pcie(void)
{
	u32 val;
	
	val = RALINK_REG(RT2880_RSTCTRL_REG);    // assert RC RST
	val |= RALINK_PCIE0_RST;
	RALINK_REG(RT2880_RSTCTRL_REG) = val;

#if defined (MT7628_ASIC_BOARD)
	val = RALINK_REG(RT2880_CLKCFG1_REG);    // disable PCIe clock
	val &= ~RALINK_PCIE_CLK_EN;
	RALINK_REG(RT2880_CLKCFG1_REG) = val;
#endif
}

#if defined (CONFIG_DDR_CAL)
#include <asm-mips/mipsregs.h>
#include <asm-mips/cacheops.h>
#include <asm/mipsregs.h>
#include <asm/cache.h>

#if defined (CONFIG_TINY_UBOOT)
__attribute__((nomips16))
#endif
static inline void cal_memcpy(void* src, void* dst, unsigned int size)
{
	int i;
	unsigned char* psrc = (unsigned char*)src, *pdst=(unsigned char*)dst;
	for (i = 0; i < size; i++, psrc++, pdst++)
		(*pdst) = (*psrc);
	return;
}
#if defined (CONFIG_TINY_UBOOT)
__attribute__((nomips16))
#endif
static inline void cal_memset(void* src, unsigned char pat, unsigned int size)
{
	int i;
	unsigned char* psrc = (unsigned char*)src;
	for (i = 0; i < size; i++, psrc++)
		(*psrc) = pat;
	return;
}

#define pref_op(hint,addr)						\
	 __asm__ __volatile__(						\
	"       .set    push                                    \n"	\
	"       .set    noreorder                               \n"	\
	"       pref   %0, %1                                  \n"	\
	"       .set    pop                                     \n"	\
	:								\
	: "i" (hint), "R" (*(unsigned char *)(addr)))	
		
#define cache_op(op,addr)						\
	 __asm__ __volatile__(						\
	"       .set    push                                    \n"	\
	"       .set    noreorder                               \n"	\
	"       .set    mips3\n\t                               \n"	\
	"       cache   %0, %1                                  \n"	\
	"       .set    pop                                     \n"	\
	:								\
	: "i" (op), "R" (*(unsigned char *)(addr)))	

__attribute__((nomips16)) static void inline cal_invalidate_dcache_range(ulong start_addr, ulong stop)
{
	unsigned long lsize = CONFIG_SYS_CACHELINE_SIZE;
	unsigned long addr = start_addr & ~(lsize - 1);
	unsigned long aend = (stop - 1) & ~(lsize - 1);

	while (1) {
		cache_op(HIT_INVALIDATE_D, addr);
		if (addr == aend)
			break;
		addr += lsize;
	}
}	

#if defined (CONFIG_TINY_UBOOT)
__attribute__((nomips16))
#endif
static void inline cal_patgen(unsigned long* start_addr, unsigned int size, unsigned bias)
{
	int i = 0;
	for (i = 0; i < size; i++)
		start_addr[i] = ((ulong)start_addr+i+bias);
		
	return;	
}	

#define NUM_OF_CACHELINE	128
#define MIN_START	6
#define MIN_FINE_START	0xF
#define MAX_START 7
#define MAX_FINE_START	0x0
#define cal_debug debug
								
#define HWDLL_FIXED	1
#if defined (HWDLL_FIXED)								
#define DU_COARSE_WIDTH	16
#define DU_FINE_WIDTH 16
#define C2F_RATIO 8
#define HWDLL_AVG	1
#define HWDLL_LV	1
//#define HWDLL_HV	1
#define HWDLL_MINSCAN	1
#endif

#define MAX_TEST_LOOP   8								
								
__attribute__((nomips16)) void dram_cali(void)
{
#if defined(ON_BOARD_64M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x800000	
#endif
#if defined(ON_BOARD_128M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x1000000	
#endif	
#if defined(ON_BOARD_256M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x2000000	
#endif
#if defined(ON_BOARD_512M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x4000000	
#endif
#if defined(ON_BOARD_1024M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x8000000	
#endif
#if defined(ON_BOARD_2048M_DRAM_COMPONENT)
	#define DRAM_BUTTOM 0x10000000
#endif

	unsigned int * nc_addr = 0xA0000000+DRAM_BUTTOM-0x0400;
	unsigned int * c_addr = 0x80000000+DRAM_BUTTOM-0x0400;
	unsigned int min_coarse_dqs[2];
	unsigned int max_coarse_dqs[2];
	unsigned int min_fine_dqs[2];
	unsigned int max_fine_dqs[2];
	unsigned int coarse_dqs[2];
	unsigned int fine_dqs[2];
	unsigned int min_dqs[2];
	unsigned int max_dqs[2];
	unsigned int total_min_comp_dqs[2];
	unsigned int total_max_comp_dqs[2];
	unsigned int avg_min_cg_comp_dqs[2];
	unsigned int avg_max_cg_comp_dqs[2];
	unsigned int avg_min_fg_comp_dqs[2];
	unsigned int avg_max_fg_comp_dqs[2];
	unsigned int min_comp_dqs[2][MAX_TEST_LOOP];
	unsigned int max_comp_dqs[2][MAX_TEST_LOOP];
	unsigned int reg = 0, ddr_cfg2_reg = 0, dqs_dly_reg = 0;
	unsigned int reg_avg = 0, reg_with_dll = 0, hw_dll_reg = 0;
	int ret = 0;
	int flag = 0, min_failed_pos[2], max_failed_pos[2], min_fine_failed_pos[2], max_fine_failed_pos[2];
	int i,j, k;
	int dqs = 0;
	unsigned int min_coarse_dqs_bnd, min_fine_dqs_bnd, coarse_dqs_dll, fine_dqs_dll;
	unsigned int reg_minscan = 0;
	unsigned int avg_cg_dly[2],avg_fg_dly[2];
	unsigned int g_min_coarse_dqs_dly[2], g_min_fine_dqs_dly[2];
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)		
	unsigned int cid = (RALINK_REG(RALINK_SYSCTL_BASE+0xC)>>16)&0x01;
#endif

#if (NUM_OF_CACHELINE > 40)
#else	
	unsigned int cache_pat[8*40];
#endif	
	u32 value, test_count = 0;;
	u32 fdiv = 0, step = 0, frac = 0;

	value = RALINK_REG(RALINK_DYN_CFG0_REG);
	fdiv = (unsigned long)((value>>8)&0x0F);
	if ((CPU_FRAC_DIV < 1) || (CPU_FRAC_DIV > 10))
	frac = (unsigned long)(value&0x0F);
	else
		frac = CPU_FRAC_DIV;
	i = 0;
	
	while(frac < fdiv) {
		value = RALINK_REG(RALINK_DYN_CFG0_REG);
		fdiv = ((value>>8)&0x0F);
		fdiv--;
		value &= ~(0x0F<<8);
		value |= (fdiv<<8);
		RALINK_REG(RALINK_DYN_CFG0_REG) = value;
		udelay(500);
		i++;
		value = RALINK_REG(RALINK_DYN_CFG0_REG);
		fdiv = ((value>>8)&0x0F);
	}	
#if (NUM_OF_CACHELINE > 40)
#else
	cal_memcpy(cache_pat, dram_patterns, 32*6);
	cal_memcpy(cache_pat+32*6, line_toggle_pattern, 32);
	cal_memcpy(cache_pat+32*6+32, pattern_ken, 32*13);
#endif

#if defined (HWDLL_LV)
#if defined (HWDLL_HV)
	RALINK_REG(RALINK_RGCTRL_BASE+0x108) = 0x01300;
	mdelay(100);
#else
	//RALINK_REG(RALINK_RGCTRL_BASE+0x108) = 0x0F00;//0x0d00;//0x0b00;
#endif
	//cal_debug("\nSet [0x10001108]=%08X\n",RALINK_REG(RALINK_RGCTRL_BASE+0x108));
	//mdelay(100);
#endif
	
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)	
	RALINK_REG(RALINK_MEMCTRL_BASE+0x10) &= ~(0x1<<4);
#else
	RALINK_REG(RALINK_MEMCTRL_BASE+0x18) = &= ~(0x1<<4);
#endif
	ddr_cfg2_reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x48);
	dqs_dly_reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x64);
	RALINK_REG(RALINK_MEMCTRL_BASE+0x48)&=(~((0x3<<28)|(0x3<<26)));

	total_min_comp_dqs[0] = 0;
	total_min_comp_dqs[1] = 0;
	total_max_comp_dqs[0] = 0;
	total_max_comp_dqs[1] = 0;
TEST_LOOP:
	min_coarse_dqs[0] = MIN_START;
	min_coarse_dqs[1] = MIN_START;
	min_fine_dqs[0] = MIN_FINE_START;
	min_fine_dqs[1] = MIN_FINE_START;
	max_coarse_dqs[0] = MAX_START;
	max_coarse_dqs[1] = MAX_START;
	max_fine_dqs[0] = MAX_FINE_START;
	max_fine_dqs[1] = MAX_FINE_START;
	min_failed_pos[0] = 0xFF;
	min_fine_failed_pos[0] = 0;
	min_failed_pos[1] = 0xFF;
	min_fine_failed_pos[1] = 0;
	max_failed_pos[0] = 0xFF;
	max_fine_failed_pos[0] = 0;
	max_failed_pos[1] = 0xFF;
	max_fine_failed_pos[1] = 0;
	dqs = 0;

	// Add by KP, DQS MIN boundary
	reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x20);
	coarse_dqs_dll = (reg & 0xF00) >> 8;
	fine_dqs_dll = (reg & 0xF0) >> 4;
	if (coarse_dqs_dll<=8)
		min_coarse_dqs_bnd = 8 - coarse_dqs_dll;
	else
		min_coarse_dqs_bnd = 0;
	
	if (fine_dqs_dll<=8)
		min_fine_dqs_bnd = 8 - fine_dqs_dll;
	else
		min_fine_dqs_bnd = 0;
	// DQS MIN boundary
 
DQS_CAL:
	flag = 0;
	j = 0;

	for (k = 0; k < 2; k ++)
	{
		unsigned int test_dqs, failed_pos = 0;
		if (k == 0)
			test_dqs = MAX_START;
		else
			test_dqs = MAX_FINE_START;	
		flag = 0;
		do 
		{	
			flag = 0;
			for (nc_addr = 0xA0000000; nc_addr < (0xA0000000+DRAM_BUTTOM-NUM_OF_CACHELINE*32); nc_addr+=((DRAM_BUTTOM>>6)+1*0x400))
			{
				RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007474;
				wmb();
				c_addr = (unsigned int*)((ulong)nc_addr & 0xDFFFFFFF);
				cal_memset(((unsigned char*)c_addr), 0x1F, NUM_OF_CACHELINE*32);
#if (NUM_OF_CACHELINE > 40)
				cal_patgen(nc_addr, NUM_OF_CACHELINE*8, 3);
#else			
				cal_memcpy(((unsigned char*)nc_addr), ((unsigned char*)cache_pat), NUM_OF_CACHELINE*32);
#endif			
				
				if (dqs > 0)
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00000074|(((k==1) ? max_coarse_dqs[dqs] : test_dqs)<<12)|(((k==0) ? 0xF : test_dqs)<<8);
				else
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007400|(((k==1) ? max_coarse_dqs[dqs] : test_dqs)<<4)|(((k==0) ? 0xF : test_dqs)<<0);
				wmb();
				
				cal_invalidate_dcache_range(((unsigned char*)c_addr), ((unsigned char*)c_addr)+NUM_OF_CACHELINE*32);
				wmb();
				for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
				{
					if (i % 8 ==0)
						pref_op(0, &c_addr[i]);
				}		
				for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
				{
#if (NUM_OF_CACHELINE > 40)
					if (c_addr[i] != (ulong)nc_addr+i+3)
#else				
					if (c_addr[i] != cache_pat[i])
#endif				
					{	
						flag = -1;
						failed_pos = i;
						goto MAX_FAILED;
					}
				}
			}
MAX_FAILED:
			if (flag==-1)
			{
				break;
			}
			else
				test_dqs++;
		}while(test_dqs<=0xF);
		
		if (k==0)
		{	
			max_coarse_dqs[dqs] = test_dqs;
			max_failed_pos[dqs] = failed_pos;
		}
		else
		{	
			test_dqs--;
	
			if (test_dqs==MAX_FINE_START-1)
			{
				max_coarse_dqs[dqs]--;
				max_fine_dqs[dqs] = 0xF;	
			}
			else
			{	
				max_fine_dqs[dqs] = test_dqs;
			}
			max_fine_failed_pos[dqs] = failed_pos;
		}	
	}	

	for (k = 0; k < 2; k ++)
	{
		unsigned int test_dqs, failed_pos = 0;
		if (k == 0)
			test_dqs = MIN_START;
		else
			test_dqs = MIN_FINE_START;	
		flag = 0;
		do 
		{
			for (nc_addr = 0xA0000000; nc_addr < (0xA0000000+DRAM_BUTTOM-NUM_OF_CACHELINE*32); (nc_addr+=(DRAM_BUTTOM>>6)+1*0x480))
			{
				RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007474;
				wmb();
				c_addr = (unsigned int*)((ulong)nc_addr & 0xDFFFFFFF);
				RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007474;
				wmb();
				cal_memset(((unsigned char*)c_addr), 0x1F, NUM_OF_CACHELINE*32);
#if (NUM_OF_CACHELINE > 40)
				cal_patgen(nc_addr, NUM_OF_CACHELINE*8, 1);
#else			
				cal_memcpy(((unsigned char*)nc_addr), ((unsigned char*)cache_pat), NUM_OF_CACHELINE*32);
#endif				
				if (dqs > 0)
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00000074|(((k==1) ? min_coarse_dqs[dqs] : test_dqs)<<12)|(((k==0) ? 0x0 : test_dqs)<<8);
				else
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = 0x00007400|(((k==1) ? min_coarse_dqs[dqs] : test_dqs)<<4)|(((k==0) ? 0x0 : test_dqs)<<0);			
				wmb();
				cal_invalidate_dcache_range(((unsigned char*)c_addr), ((unsigned char*)c_addr)+NUM_OF_CACHELINE*32);
				wmb();
				for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
				{
					if (i % 8 ==0)
						pref_op(0, &c_addr[i]);
				}	
				for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
				{
#if (NUM_OF_CACHELINE > 40)
					if (c_addr[i] != (ulong)nc_addr+i+1)	
#else				
					if (c_addr[i] != cache_pat[i])
#endif
					{
						flag = -1;
						failed_pos = i;
						goto MIN_FAILED;
					}
				}
			}
	
MIN_FAILED:
	
			if (k==0)
			{	
				if ((flag==-1)||(test_dqs==min_coarse_dqs_bnd))
				{
					break;
				}
				else
					test_dqs--;
					
				if (test_dqs < min_coarse_dqs_bnd)
					break;	
			}
			else
			{
				if (flag==-1)
				{
					test_dqs++;
					break;
				}
				else if (test_dqs==min_fine_dqs_bnd)
				{
					break;
				}
				else
				{	
					test_dqs--;                    
				}
				
				if (test_dqs < min_fine_dqs_bnd)
					break;
				
			}
		}while(test_dqs>=0);
		
		if (k==0)
		{	
			min_coarse_dqs[dqs] = test_dqs;
			min_failed_pos[dqs] = failed_pos;
		}
		else
		{	
			if (test_dqs==MIN_FINE_START+1)
			{
				min_coarse_dqs[dqs]++;
				min_fine_dqs[dqs] = 0x0;	
			}
			else
			{	
				min_fine_dqs[dqs] = test_dqs;
			}
			min_fine_failed_pos[dqs] = failed_pos;
		}
	}

	min_comp_dqs[dqs][test_count] = (8-min_coarse_dqs[dqs])*8+(8-min_fine_dqs[dqs]);
	total_min_comp_dqs[dqs] += min_comp_dqs[dqs][test_count];
	max_comp_dqs[dqs][test_count] = (max_coarse_dqs[dqs]-8)*8+(max_fine_dqs[dqs]-8);
	total_max_comp_dqs[dqs] += max_comp_dqs[dqs][test_count];

	if (max_comp_dqs[dqs][test_count]+ min_comp_dqs[dqs][test_count] <=(2*8))
	{
		reg_minscan = 0x18180000;
		reg_with_dll = 0x88880000;
		g_min_coarse_dqs_dly[0] = g_min_coarse_dqs_dly[1] = 0;
		g_min_fine_dqs_dly[0] = g_min_fine_dqs_dly[1] = 0;
		hw_dll_reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x20);
		goto FINAL_SETUP;
	}	

	if (dqs==0)
	{
		dqs = 1;	
		goto DQS_CAL;
	}

	for (i=0 ; i < 2; i++)
	{
		unsigned int temp;
		coarse_dqs[i] = (max_coarse_dqs[i] + min_coarse_dqs[i])>>1; 
		temp = (((max_coarse_dqs[i] + min_coarse_dqs[i])%2)*4)  +  ((max_fine_dqs[i] + min_fine_dqs[i])>>1);
		if (temp >= 0x10)
		{
		   coarse_dqs[i] ++;
		   fine_dqs[i] = (temp-0x10) +0x8;
		}
		else
		{
			fine_dqs[i] = temp;
		}
	}
	reg = (coarse_dqs[1]<<12)|(fine_dqs[1]<<8)|(coarse_dqs[0]<<4)|fine_dqs[0];
	
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)
	if (cid == 1)
		RALINK_REG(RALINK_MEMCTRL_BASE+0x10) &= ~(0x1<<4);
#else
	RALINK_REG(RALINK_MEMCTRL_BASE+0x18) = &= ~(0x1<<4);
#endif
	if (cid == 1) {
		RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = reg;
		RALINK_REG(RALINK_MEMCTRL_BASE+0x48) = ddr_cfg2_reg;
	}
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)	
	if (cid == 1)
		RALINK_REG(RALINK_MEMCTRL_BASE+0x10) |= (0x1<<4);
#else
	RALINK_REG(RALINK_MEMCTRL_BASE+0x18) |= (0x1<<4);
#endif

	test_count++;


FINAL:
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)	
	if (cid==1)
#endif	
	{	
		for (j = 0; j < 2; j++)	
			cal_debug("[%02X%02X%02X%02X]",min_coarse_dqs[j],min_fine_dqs[j], max_coarse_dqs[j],max_fine_dqs[j]);
		cal_debug("\nDDR Calibration DQS reg = %08X\n",reg);
		goto EXIT;
	}
	if (test_count < MAX_TEST_LOOP)
		goto TEST_LOOP;

	for (j = 0; j < 2; j++)	
	{
		unsigned int min_count = MAX_TEST_LOOP;
		unsigned int max_count = MAX_TEST_LOOP;
		
		unsigned int tmp_min_comp_dqs = total_min_comp_dqs[j]>>3;
		unsigned int tmp_total_min_comp_dqs = total_min_comp_dqs[j];
		
		unsigned int tmp_max_comp_dqs = total_max_comp_dqs[j]>>3;
		unsigned int tmp_total_max_comp_dqs = total_max_comp_dqs[j];
		
		for (k = 0; k < MAX_TEST_LOOP; k++)
		{
			int diff_min = ((tmp_min_comp_dqs-min_comp_dqs[j][k]) > 0) ? (tmp_min_comp_dqs-min_comp_dqs[j][k]) : (min_comp_dqs[j][k]-tmp_min_comp_dqs);
			int diff_max = ((tmp_max_comp_dqs-max_comp_dqs[j][k]) > 0) ? (tmp_max_comp_dqs-max_comp_dqs[j][k]) : (max_comp_dqs[j][k]-tmp_max_comp_dqs);

			if (diff_min > 5)
			{
				//cal_debug("remove the %d min comp dqs %d (%d)\n" ,k ,min_comp_dqs[j][k],tmp_min_comp_dqs);
				tmp_total_min_comp_dqs-= min_comp_dqs[j][k];
				tmp_total_min_comp_dqs += tmp_min_comp_dqs;
				min_count--;
			}
			if (diff_max > 5)
			{
				//cal_debug("remove the %d (diff=%d) max comp dqs %d (%d)\n" ,k ,diff_max,max_comp_dqs[j][k],tmp_max_comp_dqs);
				tmp_total_max_comp_dqs-= max_comp_dqs[j][k];
				tmp_total_max_comp_dqs += tmp_max_comp_dqs;
				max_count--;
			}
		}	
		tmp_min_comp_dqs = tmp_total_min_comp_dqs>>3;
		avg_min_cg_comp_dqs[j] = 8-(tmp_min_comp_dqs>>3);
		avg_min_fg_comp_dqs[j] = 8-(tmp_min_comp_dqs&0x7);
		
		tmp_max_comp_dqs = tmp_total_max_comp_dqs>>3;
		avg_max_cg_comp_dqs[j] = 8+(tmp_max_comp_dqs>>3);
		avg_max_fg_comp_dqs[j] = 8+(tmp_max_comp_dqs&0x7);
		
	}
	//cal_debug("\n\r");
	//for (j = 0; j < 2; j++)	
	//		cal_debug("[%02X%02X%02X%02X]", avg_min_cg_comp_dqs[j],avg_min_fg_comp_dqs[j], avg_max_cg_comp_dqs[j],avg_max_fg_comp_dqs[j]);
	
	for (i=0 ; i < 2; i++)
	{
		unsigned int temp;
		coarse_dqs[i] = (avg_max_cg_comp_dqs[i] + avg_min_cg_comp_dqs[i])>>1; 
		temp = (((avg_max_cg_comp_dqs[i] + avg_min_cg_comp_dqs[i])%2)*4)  +  ((avg_max_fg_comp_dqs[i] + avg_min_fg_comp_dqs[i])>>1);
		if (temp >= 0x10)
		{
		   coarse_dqs[i] ++;
		   fine_dqs[i] = (temp-0x10) +0x8;
		}
		else
		{
			fine_dqs[i] = temp;
		}
	}

	reg = (coarse_dqs[1]<<12)|(fine_dqs[1]<<8)|(coarse_dqs[0]<<4)|fine_dqs[0];
	
#if defined (HWDLL_FIXED)
/* Read DLL HW delay */
{
	unsigned int sel_fine[2],sel_coarse[2];
	int sel_mst_coarse, sel_mst_fine;
	unsigned int avg_cg_dly[2],avg_fg_dly[2];
	
	hw_dll_reg = RALINK_REG(RALINK_MEMCTRL_BASE+0x20);
	sel_mst_coarse = (hw_dll_reg >> 8) & 0x0F;
	sel_mst_fine = (hw_dll_reg >> 4) & 0x0F;	
	for (j = 0; j < 2; j++)
	{	
		int cg_dly_adj, fg_dly_adj,sel_fine_tmp,sel_coarse_tmp;

		cg_dly_adj = coarse_dqs[j];
		fg_dly_adj = fine_dqs[j]; 	
		
		sel_fine_tmp = ((sel_mst_fine + fg_dly_adj) < 8 ) ? 0 : (sel_mst_fine + fg_dly_adj - 8);
		sel_coarse_tmp = ((sel_mst_coarse + cg_dly_adj) > DU_COARSE_WIDTH -1 + 8) ? DU_COARSE_WIDTH-1 : \
			  ((sel_mst_coarse + cg_dly_adj) < 8) ? 0 : sel_mst_coarse + cg_dly_adj -8;
		
		if (sel_fine_tmp > (DU_FINE_WIDTH-1)) {
			if (sel_coarse_tmp < (DU_COARSE_WIDTH-1)) {
				sel_fine[j] = sel_fine_tmp - C2F_RATIO;
				sel_coarse[j] = 	sel_coarse_tmp + 1;
			}
			else {
				sel_fine[j] = DU_FINE_WIDTH-1;
				sel_coarse[j] = 	sel_coarse_tmp;
			}
		}
		else if (sel_fine_tmp < 0){
			if (sel_coarse_tmp > 0) {
				sel_fine[j] = sel_fine_tmp + C2F_RATIO;
				sel_coarse[j] = 	sel_coarse_tmp - 1;
			}
			else {
				//saturate
				sel_fine[j] = 0;
				sel_coarse[j] = 	sel_coarse_tmp;
			}
		}
		else {
			sel_fine[j] = sel_fine_tmp;
			sel_coarse[j] = 	sel_coarse_tmp;
		}
		if ((sel_fine[j] & 0xf) != sel_fine[j])
		{
			sel_fine[j] = sel_fine[j] & 0xf;
		}
		if ((sel_coarse[j] & 0xf) != sel_coarse[j])
		{
			sel_coarse[j] = sel_coarse[j] & 0xf;
		}
	}
	reg_with_dll = (sel_coarse[1]<<28)|(sel_fine[1]<<24)|(sel_coarse[0]<<20)|(sel_fine[0]<<16);
	
#if defined(HWDLL_AVG)
	for (j = 0; j < 2; j++)
	{
		unsigned int avg;
		int min_coarse_dqs_dly,min_fine_dqs_dly; 
		min_coarse_dqs_dly = sel_mst_coarse - (8 - min_coarse_dqs[j]);
		min_fine_dqs_dly = sel_mst_fine - (8 -min_fine_dqs[j]);
		min_coarse_dqs_dly = (min_coarse_dqs_dly < 0) ? 0 : min_coarse_dqs_dly;
		min_fine_dqs_dly = (min_fine_dqs_dly < 0) ? 0 : min_fine_dqs_dly;
		
		
		avg_cg_dly[j] = ((min_coarse_dqs_dly<<1) + (sel_coarse[j]<<1))>>1;
		avg_cg_dly[j] = avg_cg_dly[j]&0x01 ? ((avg_cg_dly[j]>>1)+1) : (avg_cg_dly[j]>>1);
			
		avg_fg_dly[j] = ((min_fine_dqs_dly<<1) + (sel_fine[j]<<1))>>1;
		avg_fg_dly[j] = avg_fg_dly[j]&0x01 ? ((avg_fg_dly[j]>>1)+1) : (avg_fg_dly[j]>>1);
		
		g_min_coarse_dqs_dly[j] = min_coarse_dqs_dly;
		g_min_fine_dqs_dly[j] = min_fine_dqs_dly;
		if ((avg_cg_dly[j] & 0xf) != avg_cg_dly[j])
		{
			avg_cg_dly[j] = avg_cg_dly[j] & 0xf;
		}
		if ((avg_fg_dly[j] & 0xf) != avg_fg_dly[j])
		{
			avg_fg_dly[j] = avg_fg_dly[j] & 0xf;
		}
	}
	reg_avg = (avg_cg_dly[1]<<28)|(avg_fg_dly[1]<<24)|(avg_cg_dly[0]<<20)|(avg_fg_dly[0]<<16);
#endif

#if defined (HWDLL_MINSCAN)
{
	unsigned int min_scan_cg_dly[2], min_scan_fg_dly[2], adj_dly[2], reg_scan;
	
	RALINK_REG(RALINK_RGCTRL_BASE+0x108) = 0x01300;

	k=9583000;
	do {k--; }while(k>0);
		
	RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = reg_with_dll;
	RALINK_REG(RALINK_MEMCTRL_BASE+0x68) |= (0x1<<4);
	
	for (j = 0; j < 2; j++)
	{
		min_scan_cg_dly[j] = 0;
		min_scan_fg_dly[j] = 0;
	
		do
		{	
				int diff_dly;
				for (nc_addr = 0xA0000000; nc_addr < (0xA0000000+DRAM_BUTTOM-NUM_OF_CACHELINE*32); nc_addr+=((DRAM_BUTTOM>>6)+1*0x400))
				{
					
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = reg_with_dll;
					wmb();
					c_addr = (unsigned int*)((ulong)nc_addr & 0xDFFFFFFF);
					cal_memset(((unsigned char*)c_addr), 0x1F, NUM_OF_CACHELINE*32);
#if (NUM_OF_CACHELINE > 40)
					cal_patgen(nc_addr, NUM_OF_CACHELINE*8, 2);
#else			
					cal_memcpy(((unsigned char*)nc_addr), ((unsigned char*)cache_pat), NUM_OF_CACHELINE*32);
#endif			
					if (j == 0)
						reg_scan = (reg_with_dll&0xFF000000)|(min_scan_cg_dly[j]<<20)|(min_scan_fg_dly[j]<<16);
					else		
						reg_scan = (reg_with_dll&0x00FF0000)|(min_scan_cg_dly[j]<<28)|(min_scan_fg_dly[j]<<24);	
					
					RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = reg_scan;
					wmb();		
					cal_invalidate_dcache_range(((unsigned char*)c_addr), ((unsigned char*)c_addr)+NUM_OF_CACHELINE*32);
					wmb();

					for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
					{
						if (i % 8 ==0)
							pref_op(0, &c_addr[i]);
					}		
					for (i = 0; i < NUM_OF_CACHELINE*8; i ++)
					{
#if (NUM_OF_CACHELINE > 40)
						if (c_addr[i] != (ulong)nc_addr+i+2)
#else				
						if (c_addr[i] != cache_pat[i])
#endif				
						{
							goto MINSCAN_FAILED;
						}
					}
				}	
				diff_dly = (avg_cg_dly[j]*8 + avg_fg_dly[j])-(min_scan_cg_dly[j]*8+min_scan_fg_dly[j]);
				if (diff_dly < 0)
					cal_debug("diff_dly=%d\n",diff_dly);
					
				if (diff_dly < 6)
					adj_dly[j] = (avg_cg_dly[j]*8 + avg_fg_dly[j]) + (6 - diff_dly);
				else
					adj_dly[j] = (avg_cg_dly[j]*8 + avg_fg_dly[j]);

				break;
MINSCAN_FAILED:
				min_scan_fg_dly[j] ++;
				if (min_scan_fg_dly[j] > 8)
				{	
					min_scan_fg_dly[j] = 0;
					min_scan_cg_dly[j]++;
					if ((min_scan_cg_dly[j]*8+min_scan_fg_dly[j]) >= (avg_cg_dly[j]*8 + avg_fg_dly[j]))
					{
						if (j==0)
							adj_dly[0] = ((reg_with_dll>>20) &0x0F)*8 + ((reg_with_dll>>16) &0x0F);
						else
							adj_dly[1] = ((reg_with_dll>>28) &0x0F)*8 + ((reg_with_dll>>24) &0x0F);				
						break;
					}	
				}
		}while(1);		
	} /* dqs loop */
	{
		unsigned int tmp_cg_dly[2],tmp_fg_dly[2];
		for (j = 0; j < 2; j++)
		{
			if (adj_dly[j]==(avg_cg_dly[j]*8+avg_fg_dly[j]))
			{
				tmp_cg_dly[j] = avg_cg_dly[j];
				tmp_fg_dly[j] = avg_fg_dly[j];
			}
			else
			{
				tmp_cg_dly[j] = adj_dly[j]>>3;
				tmp_fg_dly[j] = adj_dly[j]&0x7;
			}
		}		
		reg_minscan = (tmp_cg_dly[1]<<28) | (tmp_fg_dly[1]<<24) | (tmp_cg_dly[0]<<20) | (tmp_fg_dly[0]<<16);
	}
}

#endif /* HWDLL_MINSCAN */

#if defined (HWDLL_LV)
	RALINK_REG(RALINK_RGCTRL_BASE+0x108) = 0x0f00;

	k=9583000;
	do {k--; }while(k>0);
#endif			

FINAL_SETUP:
#if (defined(HWDLL_AVG) && (!defined(HWDLL_MINSCAN)))
	RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = (reg_avg&0xFFFF0000)|((reg_with_dll>>16)&0x0FFFF);		
#elif defined(HWDLL_MINSCAN)
	if ((reg_minscan >> 24) > ((hw_dll_reg>>4)&0xFF))
	{
		reg_minscan &= 0xFFFFFF;
		reg_minscan |= (((hw_dll_reg>>4)&0xFF) << 24);
		printf("overwrite %x\n", reg_minscan);
	}
	if (((reg_minscan >> 16) & 0xFF) > ((hw_dll_reg>>4)&0xFF))
	{
		reg_minscan &= 0xFF00FFFF;
		reg_minscan |= (((hw_dll_reg>>4)&0xFF) << 16);
		printf("overwrite %x\n", reg_minscan);
	}
	RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = (reg_minscan&0xFFFF0000)|((reg_with_dll>>16)&0x0FFFF);		
#else	
	RALINK_REG(RALINK_MEMCTRL_BASE+0x64) = (reg_with_dll&0xFFFF0000)|(reg&0x0FFFF);
#endif		
	
	RALINK_REG(RALINK_MEMCTRL_BASE+0x68) |= (0x1<<4);
	cal_debug("\n\r");
	for (j = 0; j < 2; j++)	
			cal_debug("[%02X%02X%02X%02X]", avg_min_cg_comp_dqs[j],avg_min_fg_comp_dqs[j], avg_max_cg_comp_dqs[j],avg_max_fg_comp_dqs[j]);

	cal_debug("[%04X%02X%02X][%08X][00%04X%02X]\n", reg&0x0FFFF,\
						(g_min_coarse_dqs_dly[0]<<4)|g_min_coarse_dqs_dly[0], \
						(g_min_coarse_dqs_dly[1]<<4)|g_min_coarse_dqs_dly[1], \
						RALINK_REG(RALINK_MEMCTRL_BASE+0x64),
						(reg_avg&0xFFFF0000)>>16,
						(hw_dll_reg>>4)&0x0FF
						);
	cal_debug("DU Setting Cal Done\n");
	RALINK_REG(RALINK_MEMCTRL_BASE+0x48) = ddr_cfg2_reg;
#if defined(MT7628_FPGA_BOARD) || defined(MT7628_ASIC_BOARD)	
	RALINK_REG(RALINK_MEMCTRL_BASE+0x10) |= (0x1<<4);
#else
	RALINK_REG(RALINK_MEMCTRL_BASE+0x18) |= (0x1<<4);
#endif

#endif	
}

EXIT:

	return ;
}
#endif /* #defined (CONFIG_DDR_CAL) */
