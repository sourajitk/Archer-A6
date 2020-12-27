#ifndef _GPIO_KEYS_H
#define _GPIO_KEYS_H

struct device;

struct gpio_keys_button {
	/* Configuration parameters */
	unsigned int code;	/* input event code (KEY_*, SW_*) */
	int gpio;		/* -1 if this key does not support gpio */
	int active_low;
	const char *desc;
	unsigned int type;	/* input event type (EV_KEY, EV_SW, EV_ABS) */
	int wakeup;		/* configure the button as a wake-up source */
	int debounce_interval;	/* debounce ticks interval in msecs */
	bool can_disable;
	int value;		/* axis value for EV_ABS */
	unsigned int irq;	/* Irq number in case of interrupt keys */
};

struct gpio_keys_platform_data {
	struct gpio_keys_button *buttons;
	int nbuttons;
	unsigned int poll_interval;	/* polling interval in msecs -
					   for polling driver only */
	unsigned int rep:1;		/* enable input subsystem auto repeat */
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	const char *name;		/* input device name */
};


#ifdef CONFIG_PRODUCT_ARCHER_A10
#define GPIO_BUTTON_WIFI 	4
#define GPIO_BUTTON_BOARD 	8
#define GPIO_BUTTON_WPS		0
#define GPIO_BUTTON_RESET	3
#elif defined(CONFIG_PRODUCT_ARCHER_C6U)
#define GPIO_BUTTON_WIFI 	10
#define GPIO_BUTTON_BOARD 	10 // LEO_TODO
#define GPIO_BUTTON_WPS		10
#define GPIO_BUTTON_RESET	8
#else
#define GPIO_BUTTON_WIFI 	7
#define GPIO_BUTTON_BOARD 	4
#define GPIO_BUTTON_WPS		10
#define GPIO_BUTTON_RESET	8
#endif


struct gpio_keys_button tp_gpio_keys[]=
{
		{		
			.desc = "reset",		
			.gpio = GPIO_BUTTON_RESET,
			.active_low = 1,
		},
		{		
			.desc = "wifi",		
			.gpio = GPIO_BUTTON_WIFI,
			.active_low = 1,
		},
		{		
			.desc = "wps",		
			.gpio = GPIO_BUTTON_WPS,
			.active_low = 1,
		},
		{		
			.desc = "board",		
			.gpio = GPIO_BUTTON_BOARD,
			.active_low = 0,
		},

};



#endif
