#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>  //copy
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h> 
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define MISCBEEP_MINOR 144
#define MISCBEEP_NAME "miscbeep"
#define BEEP_ON 1
#define BEEP_OFF 0

struct beep_dev{
	dev_t devid; 
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;
	int beep_gpio;
};

struct beep_dev beepdev;

static int beep_open(struct inode *inode, struct file *file)
{
	file->private_data = &beepdev; /* 设置私有数据 */
	return 0;		
}

static ssize_t beep_write(struct file *file, const char __user *buf,
				size_t count, loff_t *off)
{
	unsigned char status;
	int ret;
	struct beep_dev *dev = file->private_data;
	ret = copy_from_user(&status,buf,count);
	if(ret < 0){
		printk("kernel write failed!!!");
		return -1;
	}
	printk("device write%d\r\n",status);
	if(status == BEEP_ON){
		gpio_set_value(dev->beep_gpio, 0);
	}else if(status == BEEP_OFF){
		gpio_set_value(dev->beep_gpio, 1);
	}
	return 0;
}


static const struct file_operations beep_fops = {
	.owner	= THIS_MODULE,
	.open	= beep_open,
	.write	= beep_write,
};


static struct miscdevice beep_miscdevice = {
	.minor	= MISCBEEP_MINOR,
	.name	= MISCBEEP_NAME,
	.fops	= &beep_fops,
};



static int imscbeep_probe(struct platform_device *dev)
{
	int ret = 0;
	printk("beep driver and device was matched\r\n");
	beepdev.nd = dev->dev.of_node;
	if(beepdev.nd == NULL) {
		printk("beep node not find!\r\n");
		return -EINVAL;
	}
	beepdev.beep_gpio = of_get_named_gpio(beepdev.nd,"beep-gpio",0);
	if(beepdev.beep_gpio < 0) {
		printk("can't get beep-gpio");
		return -EINVAL;
	}
	gpio_request(beepdev.beep_gpio,"beep");
	gpio_direction_output(beepdev.beep_gpio,1);

	ret = misc_register(&beep_miscdevice);


	return 0;
}

static int imscbeep_remove(struct platform_device *dev)
{
	gpio_set_value(beepdev.beep_gpio,1);
	gpio_free(beepdev.beep_gpio);
	misc_deregister(&beep_miscdevice);
	return 0;
}


static const struct of_device_id beep_of_match[] = {
	{.compatible	= "hbb-beep"},
	{				}
};


static struct platform_driver beep_driver = {
	.driver = {
		.name	= "beep_driver",
		.of_match_table	= beep_of_match,
	},
	.probe	= imscbeep_probe,
	.remove	= imscbeep_remove,
};


static int __init miscbeep_init(void)
{
	return platform_driver_register(&beep_driver);
}


static void __exit miscbeep_exit(void)
{
	platform_driver_unregister(&beep_driver);
}

module_init(miscbeep_init);
module_exit(miscbeep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("hubinbin");