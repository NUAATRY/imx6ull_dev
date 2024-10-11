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
#include <linux/semaphore.h>  //信号量  互��体
#include <linux/timer.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define TIMER_CNT 1
#define TIMER_NAME "timer"
#define CLOSE_CMD  			_IO(0XEF, 0X1)   /* 关闭定时器 */
#define OPEN_CMD   			_IO(0XEF, 0X2)   /* 打开定时器 */
#define SETPERIOD_CMD  		_IO(0XEF, 0X3)   /* 设置定时器周期命令 */   //虽然此处没有写arg，但是最后如果有arg参数还是可以传进来

#define LED_ON 1
#define LED_OFF 0

struct timer_dev{
	dev_t devid; 
	int major;
	int minor;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;
	int led_gpio;
	int timeperiod;  			//定义一个周期
	struct timer_list timer;  //定义一个定时器
	spinlock_t lock;
};

struct timer_dev timerdev;

static int led_init(void)
{
	int ret;
	timerdev.nd = of_find_node_by_path("/gpioled");
	if(timerdev.nd == NULL){
		printk("timerdev node cant not find!!\r\n");
		ret = -1;
		goto fail_node;
	}else{
		printk("timerdev node found!!\r\n");
	}
	timerdev.led_gpio = of_get_named_gpio(timerdev.nd,"led-gpio",0);
	if(timerdev.led_gpio < 0){
		printk("cant not get led-gpio\r\n");
		ret = -1;
		goto fail_node;
	}
	printk("led-gpio-num=%d\r\n",timerdev.led_gpio);
	gpio_request(timerdev.led_gpio,"led");
	ret = gpio_direction_output(timerdev.led_gpio,1);
	if(ret < 0){
		printk("can`t set gpio!!!\r\n");
	}
	return 0;
fail_node:
	return ret;
	
}


static int timer_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	file->private_data = &timerdev;
	timerdev.timeperiod = 1000;
	ret = led_init();
	if(ret < 0){
		return ret;
	}
	return 0;
}


static long timer_unlocked_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	unsigned long flags;
	int timerperiod;
	struct timer_dev *dev = filep->private_data;


	switch (cmd) {
	case CLOSE_CMD:
		del_timer_sync(&dev->timer);
		break;
	case OPEN_CMD:
		spin_lock_irqsave(&dev->lock, flags);
		timerperiod = dev->timeperiod;
		spin_unlock_irqrestore(&dev->lock, flags);
		mod_timer(&dev->timer,jiffies+msecs_to_jiffies(timerperiod));
		break;
	case SETPERIOD_CMD:
		spin_lock_irqsave(&dev->lock, flags);
		dev->timeperiod = arg;
		spin_unlock_irqrestore(&dev->lock, flags);
		mod_timer(&dev->timer,jiffies+msecs_to_jiffies(dev->timeperiod));
		break;
	default:
		break;
	}
	return 0;
}

void timer_function(unsigned long arg)
{
	struct timer_dev *dev = (struct timer_dev *)arg;  //此处需要注意传进来的是地址
	static int sta =1;
	int timerperiod;
	unsigned long flags;
	sta = !sta;
	gpio_set_value(dev->led_gpio,sta);
	spin_lock_irqsave(&dev->lock, flags);
	timerperiod = dev->timeperiod;
	spin_unlock_irqrestore(&dev->lock, flags);
	mod_timer(&dev->timer,jiffies+msecs_to_jiffies(timerperiod));
}




static const struct file_operations timer_fops = {
	.owner	= THIS_MODULE,
	.open	= timer_open,
	.unlocked_ioctl	= timer_unlocked_ioctl,
};

static int __init timer_init(void)
{
	int ret;
	/* 初始化原子变量 */
	spin_lock_init(&timerdev.lock);

	if(timerdev.major){
		timerdev.devid = MKDEV(timerdev.major,timerdev.minor);
		ret = register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
	}else{
		ret = alloc_chrdev_region(&timerdev.devid,0,TIMER_CNT,TIMER_NAME);
		timerdev.major = MAJOR(timerdev.devid);
		timerdev.minor = MINOR(timerdev.devid);
		printk("alloc_chrdev_region major=%d minor=%d\r\n",timerdev.major, timerdev.minor);
	}
	if (ret < 0) {
		printk("Could not register\r\n");
		goto fail_devid;
	}
	timerdev.cdev.owner = THIS_MODULE;
	cdev_init(&timerdev.cdev, &timer_fops);
	ret = cdev_add(&timerdev.cdev,timerdev.devid,TIMER_CNT);
	if(ret < 0){
		printk("Could not cdev\r\n");
		goto fail_cdev;
	}
	timerdev.class = class_create(THIS_MODULE,TIMER_NAME);
	if(IS_ERR(timerdev.class)){
		ret = PTR_ERR(timerdev.class);
		goto fail_class;
	}
	timerdev.device = device_create(timerdev.class,NULL,timerdev.devid,NULL,TIMER_NAME);
	if(IS_ERR(timerdev.device)){
		ret = PTR_ERR(timerdev.device);
		goto fail_device;
	}
	
	//初始化timer，设置定时器处理函数，还未设置周期，所以不会激活定时器
	init_timer(&timerdev.timer);
	timerdev.timer.function = timer_function;
	timerdev.timer.data = (unsigned long)&timerdev;  //设置要传递给 timer_function 函数的参数为 timerdev 的地址
	return 0;
fail_device:
	class_destroy(timerdev.class);
fail_class:
	cdev_del(&timerdev.cdev);
fail_cdev:
	unregister_chrdev_region(timerdev.devid,TIMER_CNT);
fail_devid:
	return ret;
}


static void __exit timer_exit(void)
{
	gpio_set_value(timerdev.led_gpio,1);
	del_timer_sync(&timerdev.timer);

	printk("timer_exit\r\n");
	cdev_del(&timerdev.cdev);
	unregister_chrdev_region(timerdev.devid,TIMER_CNT);

	device_destroy(timerdev.class,timerdev.devid);
	class_destroy(timerdev.class);
}





module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");	
MODULE_AUTHOR("hbb");