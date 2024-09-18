#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>  //copy
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h> 
#include <linux/fs.h>
#include <linux/device.h>

#define LED_CNT		1
#define LED_NAME	"led"
#define LEDOFF 		0
#define LEDON 		1


#define CCM_CCGR1_BASE		 	(0X020C406C) 
#define SW_MUX_GPIO1_IO03_BASE 	(0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE 	(0X020E02F4)
#define GPIO1_DR_BASE 			(0X0209C000)
#define GPIO1_GDIR_BASE 		(0X0209C004)

static void __iomem *IMX_CCM_CCGR1;
static void __iomem *IMX_SW_MUX_GPIO1_IO03;
static void __iomem *IMX_SW_PAD_GPIO1_IO03;
static void __iomem *IMX_GPIO1_DR;
static void __iomem *IMX_GPIO1_GDIR;


struct led_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int major;
	int minor;
};

struct led_dev led;

void led_switch(unsigned char sta)
{
	unsigned int val = 0;
	if(sta == LEDON){
		val = readl(IMX_GPIO1_DR);
		val &= ~(1 << 3);
		writel(val,IMX_GPIO1_DR);
	}else if(sta == LEDOFF){
		val = readl(IMX_GPIO1_DR);
		val |= (1 << 3);
		writel(val,IMX_GPIO1_DR);
	}
}




static ssize_t led_read(struct file *filp, char __user *buf,
			size_t count, loff_t *ppos)
{
	struct led_dev *dev = (struct led_dev *)filp->private_data;
	return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	unsigned char databuf;
	unsigned ledstat;
	int ret;
	ret = copy_from_user(&databuf,buf,count);
	if(ret < 0){
		printk("kernel write failed\r\n");
		return -1;
	}
	ledstat = databuf;
	led_switch(ledstat);
	return 0;
}

static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &led;
	return 0;
}

static int led_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations led_fops = {
	.owner		= THIS_MODULE,
	.read		= led_read,
	.write		= led_write,
	.open		= led_open,
	.release	= led_close,
};



static int __init led_init(void)
{
	u32 val = 0;
	int ret = 0;
	//初始化LED
	IMX_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE,4);
	IMX_SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE,4);
	IMX_SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE,4);
	IMX_GPIO1_DR = ioremap(GPIO1_DR_BASE,4);
	IMX_GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE,4);
	val = readl(IMX_CCM_CCGR1); //时能GPIO1的时钟
	val &= ~(3 << 26);
	val |= (3 << 26);
	writel(val,IMX_CCM_CCGR1);
	writel(5,IMX_SW_MUX_GPIO1_IO03);
	writel(0x10b0,IMX_SW_PAD_GPIO1_IO03);
	val = readl(IMX_GPIO1_GDIR);
	val &= ~(1 << 3);
	val |= (1 << 3);
	writel(val,IMX_GPIO1_GDIR);
	val = readl(IMX_GPIO1_DR);
	val |= (1 << 3);
	writel(val,IMX_GPIO1_DR);
	//注册字符设备驱动
	if(led.major){
		led.devid = MKDEV(led.major,0);
		ret = register_chrdev_region(led.devid,LED_CNT,LED_NAME);
	}else{
		ret = alloc_chrdev_region(&led.devid,0,LED_CNT,LED_NAME);
		led.major = MAJOR(led.devid);
		led.minor = MINOR(led.devid);
	}
	if(ret < 0){
		printk("led_devid chrdev_region err!\r\n");
		goto fail_devid;
	}
	printk("led major=%d,minor=%d\r\n",led.major,led.minor);
	//初始化cdev
	led.cdev.owner = THIS_MODULE;
	cdev_init(&led.cdev,&led_fops);
	ret = cdev_add(&led.cdev,led.devid,LED_CNT);
	if(ret < 0){
		printk("led cdev_add err!\r\n");
		goto fail_cdev;
	}
	//创建类
	led.class = class_create(THIS_MODULE,LED_NAME);
	if (IS_ERR(led.class)){
		ret = PTR_ERR(led.class);
		printk("led class_creat err!\r\n");
		goto fail_class;
	}
	//创建设备
	led.device = device_create(led.class,NULL,led.devid,NULL,LED_NAME);
	if (IS_ERR(led.device)) {
		ret = PTR_ERR(led.device);
		printk("led device_creat err!\r\n");
		goto fail_device;
	}
	return 0;
fail_device:
	class_destroy(led.class);
fail_class:
	cdev_del(&led.cdev);
fail_cdev:
	unregister_chrdev_region(led.devid,LED_CNT);
fail_devid:
	return ret;
}


static void __exit led_exit(void)
{
	//取消映射
	iounmap(IMX_CCM_CCGR1);
	iounmap(IMX_SW_MUX_GPIO1_IO03);
	iounmap(IMX_SW_PAD_GPIO1_IO03);
	iounmap(IMX_GPIO1_GDIR);
	iounmap(IMX_GPIO1_DR);
	//注销字符设备
	
	device_destroy(led.class,led.devid);
	class_destroy(led.class);
	cdev_del(&led.cdev);
	unregister_chrdev_region(led.devid,LED_CNT);
	printk("led exit\r\n");
}


module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("hbb");