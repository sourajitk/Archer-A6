#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>

#include <asm/mach-ralink/rt_mmap.h>
#include "i2c-mtk.h"


static int delay_timing = PERI_CLK / (MAX_MODE_SPEED * 2);

s32 i2c_set_speed(mt_i2c *i2c)
{
	u32 clk_div = 0;
		
	clk_div = PERI_CLK / i2c->speed;
	delay_timing = clk_div / 2;
	
	spin_lock(&i2c->lock);

#if defined (CONFIG_RALINK_MT7621) || defined (CONFIG_RALINK_MT7628)
    REG_CMD(I2C_SM0CTL0_REG) = SM0_ODRAIN_PULL | SM0_VSYNC_PULSE | SM0_WAIT_H | SM0_EN | (clk_div << 16);
#if defined (CONFIG_I2C_MANUAL_MODE)
    REG_CMD(I2C_AUTOMODE_REG) = 0;
#else
	REG_CMD(I2C_AUTOMODE_REG) = 1;
#endif
#else
	REG_CMD(I2C_CLKDIV_REG) = CLKDIV_VALUE;
#endif
	spin_unlock(&i2c->lock);
	
	return 0;
}

void mt_i2c_init_hw(mt_i2c *i2c)
{
	u32 val;
#if defined (CONFIG_RALINK_MT7621) || defined (CONFIG_RALINK_MT7628)	
	i2c->speed = MAX_MODE_SPEED;
#else
	i2c->speed = ST_MODE_SPEED;
#endif
	spin_lock(&i2c->lock);
	REG_CMD(GPIO_MODE_REG) &= I2C_MODE;	

    // reset i2c block 
    val = REG_CMD(RSTCTRL_REG);
	val |= RALINK_I2C_RST;
    REG_CMD(RSTCTRL_REG) = val;

	val = val & ~RALINK_I2C_RST;
    REG_CMD(RSTCTRL_REG) = val;

	udelay(300);

	REG_CMD(I2C_CONFIG_REG) = CFG_DEFAULT;
	spin_unlock(&i2c->lock);
	
	i2c_set_speed(i2c);
}

#ifdef CONFIG_I2C_MANUAL_MODE
/* Currently only support 1 byte address EEPROM for MANUAL mode */
static void manual_mode_read(struct i2c_msg* msg)
{
	int i, cnt;
	u32 reg, data0, data1;

	REG_CMD(I2C_SM0CTL1_REG) = SM0_MODE_START;	/* re-start */
	REG_CMD(I2C_SM0D0_REG) = (msg->addr << 1) | READ_CMD;	/* Write address for read */
	udelay(delay_timing);

	REG_CMD(I2C_SM0CTL1_REG) = SM0_MODE_WRITE_DATA;
	udelay(delay_timing);
	
	for (cnt = 0; cnt < msg->len; cnt += (2 * SM0_DATA_COUNT)) {
		if ((msg->len - cnt) > (2 * SM0_DATA_COUNT))
			reg = SM0_MODE_READ_ACK | SM0_MAX_PGLEN;
		else
			reg = SM0_MODE_READ_NACK | SM0_PGLEN((msg->len - cnt) - 1);

		REG_CMD(I2C_SM0CTL1_REG) = reg; /* read data */
		udelay(delay_timing);

		data0 = REG_CMD(I2C_SM0D0_REG);
		data1 = REG_CMD(I2C_SM0D1_REG);

		for (i = 0; i < (msg->len - cnt); i++) {
			if (i < SM0_DATA_COUNT)
				msg->buf[cnt + i] = (data0 >> SM0_DATA_SHIFT(i)) & 0xff;
			else
				msg->buf[cnt + i] = (data1 >> SM0_DATA_SHIFT(i)) & 0xff;
		}
	}

	REG_CMD(I2C_SM0CTL1_REG) = SM0_MODE_STOP;	/* stop */
}

static void manual_mode_write(struct i2c_msg* msg)
{
	int i, cnt;
	u32 data0, data1, reg;
	
	REG_CMD(I2C_SM0CTL1_REG) = SM0_MODE_START;	/* Start */
	REG_CMD(I2C_SM0D0_REG) = (msg->addr << 1) | WRITE_CMD;	/* write address for write */
	udelay(delay_timing);

	REG_CMD(I2C_SM0CTL1_REG) = SM0_MODE_WRITE_DATA;
	udelay(delay_timing);
	
	for (cnt = 0; cnt < msg->len; cnt += (2 * SM0_DATA_COUNT)) {
		data0 = data1 = 0;
		for (i = 0; i < (msg->len - cnt); i++) {
			if (i < SM0_DATA_COUNT)
				data0 |= msg->buf[cnt + i] << SM0_DATA_SHIFT(i);
			else
				data1 |= msg->buf[cnt + i] << SM0_DATA_SHIFT(i);
		}
		REG_CMD(I2C_SM0D0_REG) = data0;
		REG_CMD(I2C_SM0D1_REG) = data1;
		udelay(delay_timing);
	
		if ((msg->len - i) >= (2 * SM0_DATA_COUNT))
			reg = SM0_MODE_WRITE_DATA | SM0_MAX_PGLEN;
		else
			reg = SM0_MODE_WRITE_DATA | SM0_PGLEN((msg->len - cnt) - 1);
		
		REG_CMD(I2C_SM0CTL1_REG) = reg; /* write data */
		udelay(delay_timing);

		while(!(REG_CMD(I2C_SM0CTL0_REG) & SM0_ACK));
	}

	if ((msg + 1)->flags == I2C_M_RD)
		return;
	else	
		REG_CMD(I2C_SM0CTL1_REG) = SM0_MODE_STOP;	/* stop */
}

static s32 mtk_i2c_master_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg* msgs, s32 num) 
{
	int left_num = num;

	while (left_num) {
		if (msgs->flags & I2C_M_TEN) {
			printk("10 bits addr not supported\n");
			return -EINVAL;
		}

		if (msgs->len > MAX_SIZE) {
			printk("please reduce data length\n");
			return -EINVAL;
		}
		
		if (msgs->flags & I2C_M_RD)
			manual_mode_read(msgs);
		else
			manual_mode_write(msgs);
		
		msgs++;
		left_num--;
	}

	msleep(1);
	return num - left_num;
}
#else
static s32 mtk_i2c_wait_tx_done(void)
{
	while(!(REG_CMD(I2C_STATUS_REG) & SDOEMPTY));
	return 0;
}

static s32 mtk_i2c_wait_rx_done(void)
{
	while(!(REG_CMD(I2C_STATUS_REG) & DATARDY));
	return 0;
}

static s32 mtk_i2c_wait(void)
{
	while(REG_CMD(I2C_STATUS_REG) & DATABUSY);
	return 0;
}

static s32 mtk_i2c_handle_msg(struct i2c_adapter *i2c_adap, struct i2c_msg* msg) 
{
	s32 i = 0, j = 0, pos = 0;
	s32 nblock = msg->len / READ_BLOCK;
	s32 rem = msg->len % READ_BLOCK;

	if (msg->flags & I2C_M_TEN) {
		printk("10 bits addr not supported\n");
		return -EINVAL;
	}

	if (msg->len > MAX_SIZE) {
		printk("please reduce data length\n");
		return -EINVAL;
	}

	if ((msg->flags & I2C_M_RD)) {
		for(i = 0; i < nblock; i++) {
			mtk_i2c_wait();
			REG_CMD(I2C_BYTECNT_REG) = READ_BLOCK-1;
			REG_CMD(I2C_STARTXFR_REG) = READ_CMD;
			for(j=0; j < READ_BLOCK; j++) {
				mtk_i2c_wait_rx_done();
				msg->buf[pos++] = REG_CMD(I2C_DATAIN_REG);
			}
		}

		mtk_i2c_wait();
		REG_CMD(I2C_BYTECNT_REG) = rem-1;
		REG_CMD(I2C_STARTXFR_REG) = READ_CMD;
		for(i = 0; i < rem; i++) {
			mtk_i2c_wait_rx_done();
			msg->buf[pos++] = REG_CMD(I2C_DATAIN_REG);
		}
	} else {
		rem = msg->len;
		mtk_i2c_wait();
		REG_CMD(I2C_BYTECNT_REG) = rem-1;
		for(i = 0; i < rem; i++) {
			REG_CMD(I2C_DATAOUT_REG) = msg->buf[i];
			if(i == 0)
				REG_CMD(I2C_STARTXFR_REG) = WRITE_CMD;		
			mtk_i2c_wait_tx_done();
		}
		msleep(2);
	}

	return 0;
}


static s32 mtk_i2c_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, s32 num)
{
	s32 im = 0;
	s32 ret = 0;
 
	REG_CMD(I2C_DEVADDR_REG) = msgs[0].addr;
	REG_CMD(I2C_ADDR_REG) = 0;
	
	for (im = 0; ret == 0 && im != num; im++) {
		ret = mtk_i2c_handle_msg(adap, &msgs[im]);
	}

	if(ret)
		return ret;

	return im;   
}
#endif

static u32 mtk_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm mtk_i2c_algo = {
	.master_xfer	= mtk_i2c_master_xfer,
	.functionality	= mtk_i2c_func,
};


static s32 mt_i2c_remove(struct platform_device *pdev)
{
	mt_i2c *i2c = platform_get_drvdata(pdev);

	if (i2c) {
		platform_set_drvdata(pdev, NULL);
		i2c_del_adapter(&i2c->adap);
		kfree(i2c);
	}
	return 0;
}

static s32 mt_i2c_probe(struct platform_device *pdev)
{
	struct resource *res;
	s32 ret;
	mt_i2c *i2c = NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	if (!request_mem_region(res->start, resource_size(res), pdev->name))
		return -ENOMEM;
		
	if (NULL == (i2c = kzalloc(sizeof(mt_i2c), GFP_KERNEL)))
		return -ENOMEM;

	i2c->id	 				= pdev->id;
	i2c->dev 				= &i2c->adap.dev;
	i2c->adap.dev.parent	= &pdev->dev;
	i2c->adap.nr			= i2c->id;	
	i2c->adap.owner			= THIS_MODULE;
	i2c->adap.class 		= I2C_CLASS_HWMON | I2C_CLASS_SPD;
	i2c->adap.timeout 		= HZ;
	i2c->adap.algo 			= &mtk_i2c_algo;
	i2c->adap.algo_data		= NULL;

	snprintf(i2c->adap.name, sizeof(i2c->adap.name), I2C_DRV_NAME);
	
	spin_lock_init(&i2c->lock);

	mt_i2c_init_hw(i2c);

	i2c_set_adapdata(&i2c->adap, i2c);
	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret) {
		dev_err(&pdev->dev, "failed to add i2c bus to i2c core\n");
		goto free;
	}
	platform_set_drvdata(pdev, i2c);

	return ret;

free:
	i2c_del_adapter(&i2c->adap);
	kfree(i2c);
	return ret;
}

static struct resource i2c_resources[] = {
	{
		.start		= RALINK_I2C_BASE,
		.end		= RALINK_I2C_BASE + 255,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device mtk_i2c_device = {
	.name			= I2C_DRV_NAME,
	.id				= 0,
	.num_resources	= ARRAY_SIZE(i2c_resources),
	.resource		= i2c_resources,
};

static struct platform_driver mt_i2c_driver = {
	.probe		= mt_i2c_probe,
	.remove		= mt_i2c_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= I2C_DRV_NAME,
	},
};

static s32 __init mt_i2c_init(void)
{
	platform_device_register(&mtk_i2c_device);
	return platform_driver_register(&mt_i2c_driver);
}

static void __exit mt_i2c_exit(void)
{
	platform_driver_unregister(&mt_i2c_driver);
}

module_init(mt_i2c_init);
module_exit(mt_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek I2C Host Driver");
