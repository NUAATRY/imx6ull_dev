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
#include <linux/semaphore.h>  //信号量  互斥体
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define LED_CNT 1
#define LED_NAME "dtsplatform_led"
#define LED_ON 1
#define LED_OFF 0

struct gpioled_dev{
	dev_t devid; 
	int major;
	int minor;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;
	int led_gpio;
};

struct gpioled_dev gpioled;


static int led_open(struct inode *inode, struct file *file)
{
	file->private_data = &gpioled;
	return 0;
}

static ssize_t led_write(struct file *file, const char __user *buf,
				size_t count, loff_t *off)
{
	unsigned char status;
	int ret;
	struct gpioled_dev *dev = file->private_data;
	ret = copy_from_user(&status,buf,count);
	if(ret < 0){
		printk("kernel write failed!!!");
		return -1;
	}
	printk("device write%d\r\n",status);
	if(status == LED_ON){
		gpio_set_value(dev->led_gpio, 0);
	}else if(status == LED_OFF){
		gpio_set_value(dev->led_gpio, 1);
	}
	return 0;
}

static const struct file_operations gpioled_fops = {
	.owner	= THIS_MODULE,
	.open	= led_open,
	.write	= led_write,
};


static int led_probe(struct platform_device *dev)
{
    int ret;
    printk("led driver and device matched\r\n");
	gpioled.nd = of_find_node_by_path("/gpioled");
	if(gpioled.nd == NULL){
		printk("gpioled node cant not find!!\r\n");
		ret = -1;
		goto fail_node;
	}else{
		printk("gpioled node found!!\r\n");
	}
	gpioled.led_gpio = of_get_named_gpio(gpioled.nd,"led-gpio",0);
	if(gpioled.led_gpio < 0){
		printk("cant not get led-gpio\r\n");
		ret = -1;
		goto fail_node;
	}
	printk("led-gpio-num=%d\r\n",gpioled.led_gpio);

	ret = gpio_direction_output(gpioled.led_gpio,1);
	if(ret < 0){
		printk("can`t set gpio!!!\r\n");
	}

	if(gpioled.major){
		gpioled.devid = MKDEV(gpioled.major,gpioled.minor);
		ret = register_chrdev_region(gpioled.devid, LED_CNT, LED_NAME);
	}else{
		ret = alloc_chrdev_region(&gpioled.devid,0,LED_CNT,LED_NAME);
		gpioled.major = MAJOR(gpioled.devid);
		gpioled.minor = MINOR(gpioled.devid);
		printk("alloc_chrdev_region major=%d minor=%d\r\n",gpioled.major, gpioled.minor);
	}
	if (ret < 0) {
		printk("Could not register\r\n");
		goto fail_devid;
	}
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &gpioled_fops);
	ret = cdev_add(&gpioled.cdev,gpioled.devid,LED_CNT);
	if(ret < 0){
		printk("Could not cdev\r\n");
		goto fail_cdev;
	}
	gpioled.class = class_create(THIS_MODULE,LED_NAME);
	if(IS_ERR(gpioled.class)){
		ret = PTR_ERR(gpioled.class);
		goto fail_class;
	}
	gpioled.device = device_create(gpioled.class,NULL,gpioled.devid,NULL,LED_NAME);
	if(IS_ERR(gpioled.device)){
		ret = PTR_ERR(gpioled.device);
		goto fail_device;
	}

	return 0;
fail_device:
	class_destroy(gpioled.class);
fail_class:
	cdev_del(&gpioled.cdev);
fail_cdev:
	unregister_chrdev_region(gpioled.devid,LED_CNT);
fail_devid:
fail_node:
	return ret;
}

static int led_remove(struct platform_device *dev)
{
    printk("led driver and device removed\r\n");
	gpio_set_value(gpioled.led_gpio,1);
	printk("gpioled_exit\r\n");
	cdev_del(&gpioled.cdev);
	unregister_chrdev_region(gpioled.devid,LED_CNT);

	device_destroy(gpioled.class,gpioled.devid);
	class_destroy(gpioled.class);
    return 0;
}


static struct of_device_id led_of_match[] = {
    { .compatible = "hbb-gpioled", },
	{ /* sentinel */ }
};


static struct platform_driver led_dirver ={
    .driver = {
        .name = "led_dirver",   /* platform的驱动名称，用于与非设备树的外设匹配 */
        .of_match_table = led_of_match,
    },
    .probe = led_probe,
    .remove = led_remove,
};

static int __init leddriver_init(void)
{
    return platform_driver_register(&led_dirver);
}

static void __exit leddriver_exit(void)
{
    platform_driver_unregister(&led_dirver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("hubinbin");