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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define KEY_CNT 1
#define KEY_NAME "key"

#define KEY_VALUE  	0X01
#define INVAKEY 	0X00
struct key_dev{
	dev_t devid; 
	int major;
	int minor;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;
	int key_gpio;
	atomic_t keyvalue;    //按键值
};

struct key_dev keydev;


static int key_nd_init(void)
{
	int ret = 0;
	keydev.nd = of_find_node_by_path("/key");
	if(keydev.nd == NULL){
		printk("key node cant not find!!\r\n");
		ret = -1;
		goto fail_node;
	}else{
		printk("key node found!!\r\n");
	}
	keydev.key_gpio = of_get_named_gpio(keydev.nd,"key-gpio",0);
	if(keydev.key_gpio < 0){
		printk("cant not get key_gpio\r\n");
		ret = -1;
		goto fail_node;
	}
	printk("key_gpio-num=%d\r\n",keydev.key_gpio);

	gpio_request(keydev.key_gpio,"key0");
	gpio_direction_input(keydev.key_gpio);
	return 0;
fail_node:
	return ret;
}

static int key_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	file->private_data = &keydev;

	ret = key_nd_init();
	if(ret < 0){
		return ret;
	}
	return 0;
}

static ssize_t key_read(struct file *file, char __user *buf,
				size_t count, loff_t *off)
{
	unsigned char value;
	int ret;
	struct key_dev *dev = file->private_data;
	if(gpio_get_value(dev->key_gpio) == 0){
		while(!gpio_get_value(dev->key_gpio));
		atomic_set(&dev->keyvalue,KEY_VALUE);
	}else {
		atomic_set(&dev->keyvalue,INVAKEY);
	}

	value = atomic_read(&dev->keyvalue);
	ret = copy_to_user(buf,&value,sizeof(value));
	return ret;
}


static const struct file_operations key_fops = {
	.owner	= THIS_MODULE,
	.open	= key_open,
	.read	= key_read,
};

static int __init my_key_init(void)
{
	int ret;
	//初始化原子变量
	atomic_set(&keydev.keyvalue,INVAKEY);

	if(keydev.major){
		keydev.devid = MKDEV(keydev.major,keydev.minor);
		ret = register_chrdev_region(keydev.devid, KEY_CNT, KEY_NAME);
	}else{
		ret = alloc_chrdev_region(&keydev.devid,0,KEY_CNT,KEY_NAME);
		keydev.major = MAJOR(keydev.devid);
		keydev.minor = MINOR(keydev.devid);
		printk("alloc_chrdev_region major=%d minor=%d\r\n",keydev.major, keydev.minor);
	}
	if (ret < 0) {
		printk("Could not register\r\n");
		goto fail_devid;
	}
	keydev.cdev.owner = THIS_MODULE;
	cdev_init(&keydev.cdev, &key_fops);
	ret = cdev_add(&keydev.cdev,keydev.devid,KEY_CNT);
	if(ret < 0){
		printk("Could not cdev\r\n");
		goto fail_cdev;
	}
	keydev.class = class_create(THIS_MODULE,KEY_NAME);
	if(IS_ERR(keydev.class)){
		ret = PTR_ERR(keydev.class);
		goto fail_class;
	}
	keydev.device = device_create(keydev.class,NULL,keydev.devid,NULL,KEY_NAME);
	if(IS_ERR(keydev.device)){
		ret = PTR_ERR(keydev.device);
		goto fail_device;
	}

	return 0;
fail_device:
	class_destroy(keydev.class);
fail_class:
	cdev_del(&keydev.cdev);
fail_cdev:
	unregister_chrdev_region(keydev.devid,KEY_CNT);
fail_devid:
	return ret;
}


static void __exit my_key_exit(void)
{
	gpio_free(keydev.key_gpio);
	printk("key_exit\r\n");
	cdev_del(&keydev.cdev);
	unregister_chrdev_region(keydev.devid,KEY_CNT);

	device_destroy(keydev.class,keydev.devid);
	class_destroy(keydev.class);
}





module_init(my_key_init);
module_exit(my_key_exit);
MODULE_LICENSE("GPL");	
MODULE_AUTHOR("hbb");