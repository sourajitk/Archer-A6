#include <common.h>



#include "ralink_gpio_reg.h"
#include "ralink_gpio_MT7621.h"


typedef enum
{
    GPIO_BUTTON_WIFI = 7,
	GPIO_LED_POWER = 18,
	GPIO_LED_2G = 16,
    GPIO_LED_5G = 15,
    GPIO_LED_INTERNET_WHITE = 14,
    GPIO_LED_INTERNET_ORANGE = 13,
    GPIO_LED_LAN = 9,
    GPIO_LED_WPS = 5,
    GPIO_LED_USB = 0,
    GPIO_POWER_USB = 11,
    GPIO_BUTTON_BOARD = 4,
    GPIO_LED_BOARD = 3,
    GPIO_BUTTON_WPS = 10,
    GPIO_BUTTON_RESET = 8
}
GPIO_NAME_t;

void ralink_gpio_init(void)
{
    u32 gpiomode;
	
	gpiomode = le32_to_cpu(*(volatile u32 *)(RALINK_REG_GPIOMODE));
	/* set gpio mode */
    /*
     * GPIO10: CTS2_N
     * GPIO18: WDT_RST_N
     * GPIO16: JTCLK
     * GPIO15: JTMS
     * GPIO13: JTDO
     * GPIO14: JTDI
     * GPIO09: RTS2_N
     * GPIO05: RTS3_N
     * GPIO0:  GPIO0
     * GPIO11: TXD2
     * GPIO04: I2C_SCLK 
     * GPIO03: I2C_SD
     * GPIO7:  TXD3
     * GPIO8:  RXD3
     */
     	/* overwrite UART3_MODE UART2_MODE JTAG_MODE WDT_MODE I2C_MODE */
    gpiomode &= ~((3*RALINK_GPIOMODE_WDT) |  (3*RALINK_GPIOMODE_UART2) 
                | (3*RALINK_GPIOMODE_UART3) | (1*RALINK_GPIOMODE_JTAG) | (1*RALINK_GPIOMODE_I2C));

	gpiomode |= (1*RALINK_GPIOMODE_WDT) | (1*RALINK_GPIOMODE_UART2) 
				| (1*RALINK_GPIOMODE_UART3) | (1*RALINK_GPIOMODE_JTAG) | (1*RALINK_GPIOMODE_I2C);
	*(volatile u32 *)(RALINK_REG_GPIOMODE) = cpu_to_le32(gpiomode);

    /* set gpio dir */
	set_gpio_dir(GPIO_LED_POWER, DIR_OUTPUT);
    set_gpio_data(GPIO_LED_POWER, OUTPUT_LOW);
    
	set_gpio_dir(GPIO_LED_2G, DIR_OUTPUT);
    set_gpio_data(GPIO_LED_2G, OUTPUT_HIGH);
     
	set_gpio_dir(GPIO_LED_5G, DIR_OUTPUT);
    set_gpio_data(GPIO_LED_5G, OUTPUT_HIGH);
     
    /* high valid */
    set_gpio_dir(GPIO_LED_INTERNET_WHITE, DIR_OUTPUT);
    set_gpio_data(GPIO_LED_INTERNET_WHITE, OUTPUT_HIGH);
     
    set_gpio_dir(GPIO_LED_INTERNET_ORANGE, DIR_OUTPUT);
    set_gpio_data(GPIO_LED_INTERNET_ORANGE, OUTPUT_LOW);
     
    set_gpio_dir(GPIO_LED_LAN, DIR_OUTPUT);
    set_gpio_data(GPIO_LED_LAN, OUTPUT_HIGH);
     
    set_gpio_dir(GPIO_LED_WPS, DIR_OUTPUT);
    set_gpio_data(GPIO_LED_WPS, OUTPUT_HIGH);
     
    set_gpio_dir(GPIO_LED_USB, DIR_OUTPUT);
    set_gpio_data(GPIO_LED_USB, OUTPUT_HIGH);

	set_gpio_dir(GPIO_POWER_USB, DIR_OUTPUT);
	set_gpio_data(GPIO_POWER_USB, OUTPUT_LOW);

   
    set_gpio_dir(GPIO_LED_BOARD, DIR_OUTPUT);
    //set_gpio_data(GPIO_LED_BOARD, OUTPUT_HIGH);
    set_gpio_data(GPIO_LED_BOARD, OUTPUT_LOW);
	
    set_gpio_dir(GPIO_BUTTON_WIFI, DIR_INPUT);
    set_gpio_dir(GPIO_BUTTON_BOARD, DIR_INPUT);
    
    set_gpio_dir(GPIO_BUTTON_WPS, DIR_INPUT);
    set_gpio_dir(GPIO_BUTTON_RESET, DIR_INPUT);
    
}

void all_led_check(int step)
{
	if(0 == step)
    {
	    all_led_on();
	}

	//mdelay(1000);

	else if(1 == step)
	{
		/* led internet should change another color */
		/* high valid , orange*/
	    set_gpio_data(GPIO_LED_INTERNET_ORANGE, OUTPUT_HIGH);
	     
	    /* low valid , white*/
	    set_gpio_data(GPIO_LED_INTERNET_WHITE, OUTPUT_HIGH);
	}
	//mdelay(1000);

	else if(2 == step)
	{
		/* all led off but power led and board led on */
		/* board led controls all led, so you can't disable it, or power led will be disabled too. */
	   	set_gpio_data(GPIO_LED_2G, OUTPUT_HIGH);
	     
	    set_gpio_data(GPIO_LED_5G, OUTPUT_HIGH);
	     
	    /* high valid , orange*/
	    set_gpio_data(GPIO_LED_INTERNET_ORANGE, OUTPUT_LOW);
	     
	    /* low valid , white*/
	    set_gpio_data(GPIO_LED_INTERNET_WHITE, OUTPUT_HIGH);
	     
	    set_gpio_data(GPIO_LED_LAN, OUTPUT_HIGH);
	     
	    set_gpio_data(GPIO_LED_WPS, OUTPUT_HIGH);
	     
	    set_gpio_data(GPIO_LED_USB, OUTPUT_HIGH);
	     
	     
	}
}

void all_led_on()
{
    /* set all led on */
    set_gpio_data(GPIO_LED_POWER, OUTPUT_LOW);
    
    set_gpio_data(GPIO_LED_2G, OUTPUT_LOW);
     
    set_gpio_data(GPIO_LED_5G, OUTPUT_LOW);
     
    /* high valid , orange*/
    set_gpio_data(GPIO_LED_INTERNET_ORANGE, OUTPUT_LOW);
     
    /* low valid , white*/
    set_gpio_data(GPIO_LED_INTERNET_WHITE, OUTPUT_LOW);
     
    set_gpio_data(GPIO_LED_LAN, OUTPUT_LOW);
     
    set_gpio_data(GPIO_LED_WPS, OUTPUT_LOW);
     
    set_gpio_data(GPIO_LED_USB, OUTPUT_LOW);
     
     
    set_gpio_data(GPIO_LED_BOARD, OUTPUT_LOW);
}

/* this function is called when uboot end */
void all_led_off()
{
    /* set all led off */
    set_gpio_data(GPIO_LED_POWER, OUTPUT_HIGH);
    
    set_gpio_data(GPIO_LED_2G, OUTPUT_HIGH);
     
    set_gpio_data(GPIO_LED_5G, OUTPUT_HIGH);
     
    /* high valid , orange*/
    set_gpio_data(GPIO_LED_INTERNET_ORANGE, OUTPUT_LOW);
     
    /* low valid , white*/
    set_gpio_data(GPIO_LED_INTERNET_WHITE, OUTPUT_HIGH);
     
    set_gpio_data(GPIO_LED_LAN, OUTPUT_HIGH);
     
    set_gpio_data(GPIO_LED_WPS, OUTPUT_HIGH);
     
    set_gpio_data(GPIO_LED_USB, OUTPUT_HIGH);
     
    set_gpio_data(GPIO_LED_BOARD, OUTPUT_HIGH);
}

/* called by firmware recovery checking */
int is_recovery_button_pressed()
{
	u32 data;
	get_gpio_data(GPIO_BUTTON_RESET, &data);
	if (data == 0)
		return 1;

	return 0;
}


int power_led_blink_ratelimit(u32 *count, u32 period)
{
	if (!count)
		return -1;

	if (period <=1)
		period = 2;
		
	if (*count >= period )
		{
		set_gpio_data(GPIO_LED_POWER, OUTPUT_LOW);
		}
	else if (*count == period/2 )
		{
		set_gpio_data(GPIO_LED_POWER, OUTPUT_HIGH);
		}

	if (*count >= period)
		*count = 0;
	else
		(*count) ++;
	
	return 0;
}
