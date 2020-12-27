#ifndef I2C_MTK_H
#define I2C_MTK_H


#define I2C_DRV_NAME        "mt-i2c"

#define DATARDY				0x04
#define SDOEMPTY			0x02
#define DATABUSY			0x01
#define REG_CMD(x)			(*((volatile u32 *)(x)))
#define READ_BLOCK			16
#define MAX_SIZE			64
#define SM0_DATA_COUNT		4						/* Serial interface master data max byte count  */
#define SM0_DATA_SHIFT(x)	((x & 0x3) << 3)		/* Serial data shift */


/* controller speed limitation*/
#define ST_MODE_SPEED		100	/* khz */
#define FS_MODE_SPEED		400	/* khz */
#define MAX_MODE_SPEED		500	/* khz */


/* Instruction codes */
#define READ_CMD			0x1
#define WRITE_CMD			0x0

/* SYSCTL register */
#define RSTCTRL_REG			(RALINK_SYSCTL_BASE + 0x34)
#define GPIO_MODE_REG		(RALINK_SYSCTL_BASE + 0x60)
#define I2C_REG_BASE		(RALINK_I2C_BASE)


/* I2C register */
#define I2C_CONFIG_REG		(I2C_REG_BASE + 0x00)
#define I2C_CLKDIV_REG		(I2C_REG_BASE + 0x04)
#define I2C_DEVADDR_REG		(I2C_REG_BASE + 0x08)
#define I2C_ADDR_REG		(I2C_REG_BASE + 0x0C)
#define I2C_DATAOUT_REG		(I2C_REG_BASE + 0x10)
#define I2C_DATAIN_REG		(I2C_REG_BASE + 0x14)
#define I2C_STATUS_REG		(I2C_REG_BASE + 0x18)
#define I2C_STARTXFR_REG	(I2C_REG_BASE + 0x1C)
#define I2C_BYTECNT_REG		(I2C_REG_BASE + 0x20)
#define I2C_AUTOMODE_REG	(I2C_REG_BASE + 0x28)
#define I2C_SM0CTL0_REG		(I2C_REG_BASE + 0x40)
#define I2C_SM0CTL1_REG		(I2C_REG_BASE + 0x44)
#define I2C_SM0D0_REG		(I2C_REG_BASE + 0x50)
#define I2C_SM0D1_REG		(I2C_REG_BASE + 0x54)


/* platform setting */
#if defined (CONFIG_RALINK_MT7621)
#define I2C_MODE			~0x4
#define PERI_CLK			50000					/* source clock */
#elif defined (CONFIG_RALINK_MT7628)
#define I2C_MODE			~(0x3 << 20)
#define PERI_CLK			40000					/* source clock */
#else
#define I2C_MODE			~0x1
#define CLKDIV_VALUE		333
#define PERI_CLK			50000					/* source clock */
#endif


/* I2C_SM0_CTL register bit field */
#define SM0_ODRAIN_PULL		(0x1 << 31)				/* The output is pulled hight by SIF master 0 */
#define SM0_VSYNC_PULSE		(0x1 << 28)				/* Allow triggered in VSYNC pulse */
#define SM0_WAIT_H			(0x1 << 6)				/* Output H when SIF master 0 is in WAIT state */
#define SM0_EN				(0x1 << 1)				/* Enable SIF master 0 */


/* I2C_SM1_CTL register bit field */
#define SM0_ACK				(0xff << 16)			/* Acknowledge of 8 bytes of data */
#define SM0_TRI				(0x1 << 0)				/* Trigger serial interface */
#define SM0_MODE_START		((0x1 << 4) | SM0_TRI)	/* SIF master mode start */
#define SM0_MODE_WRITE_DATA	((0x2 << 4) | SM0_TRI)	/* SIF master mode write data */
#define SM0_MODE_STOP		((0x3 << 4) | SM0_TRI)	/* SIF master mode stop */
#define SM0_MODE_READ_NACK 	((0x4 << 4) | SM0_TRI)	/* SIF master mode read data with no ack for final byte */
#define SM0_MODE_READ_ACK	((0x5 << 4) | SM0_TRI)	/* SIF master mode read data with ack */
#define SM0_PGLEN(x)		((x) << 8)				/* Page length */
#define SM0_MAX_PGLEN		(0x7 << 8)				/* Max page length */


/* I2C_CFG register bit field */
#define CFG_ADDRLEN_8		(0x7 << 5)				/* 8 bits */
#define CFG_DEVADLEN_7		(0x6 << 2)				/* 7 bits */
#define CFG_ADDRDIS			(0x1 << 1)				/* Disable address transmission*/
#define CFG_DEFAULT			(CFG_ADDRLEN_8 | CFG_DEVADLEN_7 | CFG_ADDRDIS)


typedef struct mt_i2c_t {
	struct i2c_adapter  adap;
	struct device   	*dev;
	u32 speed;
	u16 id;
	spinlock_t lock;
} mt_i2c;


int i2c_set_speed(mt_i2c *i2c);
void mt_i2c_init_hw(mt_i2c *i2c);
#endif /* I2C_MTK_H */
