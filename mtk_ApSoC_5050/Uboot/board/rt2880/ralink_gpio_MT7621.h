#ifndef __RALINK_GPIO_7628_H__
#define __RALINK_GPIO_7628_H__

#define OUTPUT_HIGH 1
#define OUTPUT_LOW 0
 
#define INPUT_HIGH 1
#define INPUT_LOW 0

#define DIR_OUTPUT 1
#define DIR_INPUT 0


#define BIT(n)          (1 << (n))
#define SET_BIT(x, n)   (x |= BIT(n))
#define CLR_BIT(x, n)   (x &= ~(BIT(n)))

typedef unsigned int u32;

int get_gpio_reg_bit(u32 gpio, u32 *data_reg, u32 *bit, u32 *dir_reg);

int set_gpio_data(u32 gpio, u32 data);
int get_gpio_data(u32 gpio, u32 *data);
int set_gpio_dir(u32 gpio, u32 dir);

#endif