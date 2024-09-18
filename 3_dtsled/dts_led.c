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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define LED_NEME "dtsled"
#define LED_CNT   1
#define LED_ON  1
#define LED_OFF 0

static void __iomem *IMX_CCM_CCGR1;
static void __iomem *IMX_SW_MUX_GPIO1_IO03;
static void __iomem *IMX_SW_PAD_GPIO1_IO03;
static void __iomem *IMX_GPIO1_DR;
static void __iomem *IMX_GPIO1_GDIR;


struct led_device {
	dev_t devid;
	int major; //
	int minor; //
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;
};
struct led_device led_device;


void led_switch(unsigned char sta)
{
	unsigned int val = 0;
	if(sta == LED_ON){
		val = readl(IMX_GPIO1_DR);
		val &= ~(1 << 3);
		writel(val,IMX_GPIO1_DR);
	}else if(sta == LED_OFF){
		val = readl(IMX_GPIO1_DR);
		val |= (1 << 3);
		writel(val,IMX_GPIO1_DR);
	}
}



static int dtsled_open(struct inode *inode, struct file *file)
{
	file->private_data = &led_device;
	return 0;
}

ssize_t dtsled_read(struct file *file, char __user * buf,
		      size_t len, loff_t * ppos)
{
	return 0;
}



ssize_t dtsled_write(struct file *file, const char __user *data,
		       size_t len, loff_t *ppos)
{
	unsigned char databuf;
	unsigned char ledstat;
	int ret;
	ret = copy_from_user(&databuf,data,len);
	if(ret < 0){
		printk("kernel write failed\r\n");
		return -ENODATA;
	}
	ledstat = databuf;
	led_switch(ledstat);
	return 0;
}


static int dtsled_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations dtsled_fops = {
	.owner = THIS_MODULE,
	.open = dtsled_open,
    .release = dtsled_release,
    .read	= dtsled_read,
	.write = dtsled_write,
};


// 驱动入口注册
static int __init dtsled_init(void)
{
	int ret = 0;

	u32 val = 0;
	struct property *prop;
	u32 regdata[10];
	const char *str;

	led_device.nd = of_find_node_by_path("/dtsled");
	if (!led_device.nd) {
        printk("dtsled: can't find node by path\n");
        return -1;
    }else{
		printk("dtsled: find node\n");
	}
	prop = of_find_property(led_device.nd,"compatible",NULL);
	if(!prop){
		printk("dtsled: can't find compatible property\n");
        return -1;
	}else{
		printk("dtsled: compatible %s\n",prop->value);
	}
	ret = of_property_read_string(led_device.nd,"status",&str);
	if(ret < 0){
		printk("dtsled: can't read status property\n");
        return -1;
	}else{
		printk("dtsled: status %s\n",str);
	}
	ret = of_property_read_u32_array(led_device.nd,"reg",regdata,10);
	if(ret < 0){
		printk("dtsled: can't read reg property\n");
        return -1;
	}else {
		u8 i =0;
		printk("reg data:\r\n");
		for(i = 0;i < 10; i++){
			printk("0x%x ",regdata[i]);
		}
		printk("\r\n");
	}

#if 0
	IMX_CCM_CCGR1 = ioremap(regdata[0],regdata[1]);
	IMX_SW_MUX_GPIO1_IO03 = ioremap(regdata[2],regdata[3]);
	IMX_SW_PAD_GPIO1_IO03 = ioremap(regdata[4],regdata[5]);
	IMX_GPIO1_DR = ioremap(regdata[6],regdata[7]);
	IMX_GPIO1_GDIR = ioremap(regdata[8],regdata[9]);
#else
	IMX_CCM_CCGR1 = of_iomap(led_device.nd,0);
	IMX_SW_MUX_GPIO1_IO03 = of_iomap(led_device.nd,1);
	IMX_SW_PAD_GPIO1_IO03 = of_iomap(led_device.nd,2);
	IMX_GPIO1_DR = of_iomap(led_device.nd,3);
	IMX_GPIO1_GDIR = of_iomap(led_device.nd,4);
#endif
	
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

	if(led_device.major){
		led_device.devid = MKDEV(led_device.major, 0);
		ret = register_chrdev_region(led_device.devid,LED_CNT,LED_NEME);

	}else{
		ret = alloc_chrdev_region(&led_device.devid,0,LED_CNT,LED_NEME);
		led_device.major = MAJOR(led_device.devid);
		led_device.minor = MINOR(led_device.devid);
	}
	if(ret < 0){
		printk("dtsled: can't register device\n");
		goto fail_devid;
	}
	printk("led_device:major %d,minor %d\r\n",led_device.major,led_device.minor);


	led_device.cdev.owner = THIS_MODULE;
	cdev_init(&led_device.cdev,&dtsled_fops);
	ret = cdev_add(&led_device.cdev , led_device.devid, LED_CNT);
	if(ret < 0){
        printk("dtsled: can't add cdev\r\n");
        goto fail_cdev;
    }
	led_device.class = class_create(THIS_MODULE,LED_NEME);
	if(IS_ERR(led_device.class)) {
		ret = PTR_ERR(led_device.class);
		printk("dtsled: can't create class\r\n");
		goto fail_class;
	}

	led_device.device = device_create(led_device.class,NULL,led_device.devid,NULL,LED_NEME);
	if(IS_ERR(led_device.device)) {
		ret = PTR_ERR(led_device.device);
		printk("dtsled: can't create device\r\n");
		goto fail_device;
	}
	return 0;

fail_device:
	class_destroy(led_device.class);
fail_class:
    cdev_del(&led_device.cdev);
fail_cdev:
    unregister_chrdev_region(led_device.devid, LED_CNT);
fail_devid:
    return ret;
}

static void __exit dtsled_exit(void)
{

	iounmap(IMX_CCM_CCGR1);
	iounmap(IMX_SW_MUX_GPIO1_IO03);
	iounmap(IMX_SW_PAD_GPIO1_IO03);
	iounmap(IMX_GPIO1_GDIR);
	iounmap(IMX_GPIO1_DR);

	device_destroy(led_device.class, led_device.devid);
	class_destroy(led_device.class);
	cdev_del(&led_device.cdev);
	unregister_chrdev_region(led_device.devid, LED_CNT);
	printk("dtsled: module exit\n");
}


module_init(dtsled_init);
module_exit(dtsled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hbb");

