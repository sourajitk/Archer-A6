#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
//#include <mach/sync_write.h>
//#include <mach/dbg_dump.h>
#include <linux/kallsyms.h>
#include <linux/init.h>

static struct platform_device mt76xx_i2s_device = 
{    
	.name 	= "mt76xx-i2s",
    	.id 	= -1,
};

static struct platform_device mt76xx_pcm_platform = 
{    
	.name 	= "mt76xx-pcm",
    	.id 	= -1,
};

static struct platform_device *mt7623_audio_devices[] __initdata = {
	&mt76xx_i2s_device,
	&mt76xx_pcm_platform,
};

/*
 * mt7623_audio_init: initialize driver.
 */

static int __init mt7623_audio_init(void)
{
	int ret = 0;

	printk("\n\n ********** %s **********\n\n", __func__);	
  	ret = platform_add_devices(mt7623_audio_devices, ARRAY_SIZE(mt7623_audio_devices));
  	if (ret) {
      		pr_err("\n\n ###### Fail to register mt7623_audio_devices ######\n\n");
      		return ret;
  	}  
	
  	return ret;
}


arch_initcall(mt7623_audio_init);
