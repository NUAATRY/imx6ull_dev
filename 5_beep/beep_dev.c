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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define BEEP_CNT 1
#define BEEP_NAME "beep"
#define BEEP_ON 1
#define BEEP_OFF 0

struct beep_dev{
	dev_t devid; 
	int major;
	int minor;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;
	int beep_gpio;
};

struct beep_dev beep;

static int beep_open(struct inode *inode, struct file *file)
{
	file->private_data = &beep;
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

int beep_release(struct inode *inode, struct file *file)
{
	return 0;

}

static const struct file_operations beep_fops = {
	.owner	= THIS_MODULE,
	.open	= beep_open,
	.write	= beep_write,
	.release = beep_release,
};

static int __init gpioled_init(void)
{
	int ret;
	beep.nd = of_find_node_by_path("/beep");
	if(beep.nd == NULL){
		printk("beep node cant not find!!\r\n");
		ret = -1;
		goto fail_node;
	}else{
		printk("beep node found!!\r\n");
	}
	beep.beep_gpio = of_get_named_gpio(beep.nd,"beep-gpio",0);
	if(beep.beep_gpio < 0){
		printk("cant not get beep-gpio\r\n");
		ret = -1;
		goto fail_node;
	}
	printk("beep-gpio-num=%d\r\n",beep.beep_gpio);

	ret = gpio_direction_output(beep.beep_gpio,1);
	if(ret < 0){
		printk("can`t set gpio!!!\r\n");
	}

	if(beep.major){
		beep.devid = MKDEV(beep.major,beep.minor);
		ret = register_chrdev_region(beep.devid, BEEP_CNT, BEEP_NAME);
	}else{
		ret = alloc_chrdev_region(&beep.devid,0,BEEP_CNT,BEEP_NAME);
		beep.major = MAJOR(beep.devid);
		beep.minor = MINOR(beep.devid);
		printk("alloc_chrdev_region major=%d minor=%d\r\n",beep.major, beep.minor);
	}
	if (ret < 0) {
		printk("Could not register\r\n");
		goto fail_devid;
	}
	beep.cdev.owner = THIS_MODULE;
	cdev_init(&beep.cdev, &beep_fops);
	ret = cdev_add(&beep.cdev,beep.devid,BEEP_CNT);
	if(ret < 0){
		printk("Could not cdev\r\n");
		goto fail_cdev;
	}
	beep.class = class_create(THIS_MODULE,BEEP_NAME);
	if(IS_ERR(beep.class)){
		ret = PTR_ERR(beep.class);
		goto fail_class;
	}
	beep.device = device_create(beep.class,NULL,beep.devid,NULL,BEEP_NAME);
	if(IS_ERR(beep.device)){
		ret = PTR_ERR(beep.device);
		goto fail_device;
	}
	return 0;
fail_device:
	class_destroy(beep.class);
fail_class:
	cdev_del(&beep.cdev);
fail_cdev:
	unregister_chrdev_region(beep.devid,BEEP_CNT);
fail_devid:
fail_node:
	return ret;
}


static void __exit gpioled_exit(void)
{
	gpio_set_value(beep.beep_gpio,1);
	printk("beep_exit\r\n");
	cdev_del(&beep.cdev);
	unregister_chrdev_region(beep.devid,BEEP_CNT);

	device_destroy(beep.class,beep.devid);
	class_destroy(beep.class);
}





module_init(gpioled_init);
module_exit(gpioled_exit);
MODULE_LICENSE("GPL");	
MODULE_AUTHOR("hbb");