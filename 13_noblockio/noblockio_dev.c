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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>



#define IMX6UIRQ_CNT 1
#define IMX6UIRQ_NAME "imx6uirq"
#define KEY_CNT 1
#define KEY_NAME "key"
#define INVAKEY 	0X00


struct key_irq{
	int gpio;
	int irqnum;
	unsigned char keyvalue;    //按键值
	char name[10];
	irqreturn_t (*handler)(int,void *);
};


struct imx6uirq_dev{
	dev_t devid;
	int major;
	int minor;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	struct device_node *nd;
	unsigned char curkeynum;
	atomic_t keyvalue;
	atomic_t keyrelease;
	struct key_irq keyirq[KEY_CNT];  /* 按键描述数组 */
	struct timer_list timer;
	wait_queue_head_t r_wait; /*读等待队列头*/
};

struct imx6uirq_dev imx6uirqdev;


static irqreturn_t key0_handler(int irq, void *dev_id)
{
	unsigned char i = 0;
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *)dev_id;
	dev->timer.data = (volatile long)dev_id;
	for(i = 0; i < KEY_CNT; i++){
		if(irq == dev->keyirq[i].irqnum)
		{
			dev->curkeynum = i;
			break;
		}
	}
	mod_timer(&dev->timer,jiffies + msecs_to_jiffies(10));
	return IRQ_RETVAL(IRQ_HANDLED);
}

void timer_function(unsigned long arg)
{
	unsigned char value;
	struct imx6uirq_dev *dev = (struct imx6uirq_dev *)arg;
	struct key_irq *subkey_irq;
	subkey_irq = &dev->keyirq[dev->curkeynum];
	value = gpio_get_value(subkey_irq->gpio);
	if(value == 0){
		atomic_set(&dev->keyvalue,subkey_irq->keyvalue);
	}
	else{
		atomic_set(&dev->keyvalue,0x80 | subkey_irq->keyvalue);
		atomic_set(&dev->keyrelease, 1);
	}
	if(atomic_read(&dev->keyrelease)){  /*完成一次按键过程*/
		wake_up_interruptible(&dev->r_wait);  /*唤醒等待队列*/
	}
}


static int key_nd_init(void)
{
	int ret = 0;
	unsigned char i = 0;
	imx6uirqdev.nd = of_find_node_by_path("/key");
	if(imx6uirqdev.nd == NULL){
		printk("key node cant not find!!\r\n");
		ret = -1;
		goto fail_node;
	}else{
		printk("key node found!!\r\n");
	}

	/* 提取 GPIO */
	for(i = 0 ; i < KEY_CNT; i++){
		imx6uirqdev.keyirq[i].gpio = of_get_named_gpio(imx6uirqdev.nd,"key-gpio",i);
		if(imx6uirqdev.keyirq[i].gpio < 0){
			printk("cant not get key[%d] gpio\r\n",i);
			ret = -1;
			goto fail_node;
		}
		printk("key[%d] gpio-num=%d\r\n",i,imx6uirqdev.keyirq[i].gpio);
	}


	/* 初始化 key 所使用的 IO，并且获取中断号 */
	for(i = 0; i < KEY_CNT ; i++){
		memset(imx6uirqdev.keyirq[i].name,0,sizeof(imx6uirqdev.keyirq[i].name));
		sprintf(imx6uirqdev.keyirq[i].name,"key%d",i);
		gpio_request(imx6uirqdev.keyirq[i].gpio,imx6uirqdev.keyirq[i].name);
		gpio_direction_input(imx6uirqdev.keyirq[i].gpio);
#if 0  /* 此处从节点获取中断号，有两种方式，第二种方式只能用于gpio*/
		imx6uirqdev.keyirq[i].irqnum = irq_of_parse_and_map(imx6uirqdev.node,i);
#endif
		imx6uirqdev.keyirq[i].irqnum = gpio_to_irq(imx6uirqdev.keyirq[i].gpio);
		printk("key%d:gpio=%d, irqnum=%d\r\n",i, imx6uirqdev.keyirq[i].gpio, imx6uirqdev.keyirq[i].irqnum);
	}
	/* 设置成中断模式，申请中断 */
	imx6uirqdev.keyirq[0].handler = key0_handler;
	for(i = 0; i < KEY_CNT; i++) {
		ret = request_irq(imx6uirqdev.keyirq[i].irqnum,
						imx6uirqdev.keyirq[i].handler,
						IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
						imx6uirqdev.keyirq[i].name,&imx6uirqdev);
		if(ret < 0){
			printk("irq %d request failed!\r\n",imx6uirqdev.keyirq[i].irqnum);
			goto fail_requestirq;
		}
		
		imx6uirqdev.keyirq[i].keyvalue = i+1;
	}
	init_timer(&imx6uirqdev.timer);
	imx6uirqdev.timer.function = timer_function;

	/*初始化等待队列头*/
	init_waitqueue_head(&imx6uirqdev.r_wait);
	return 0;

fail_requestirq:
fail_node:
	return ret;
}

static int imx6uirq_open(struct inode *inode, struct file *file)
{
	file->private_data = &imx6uirqdev;
	return 0;
}

static ssize_t imx6uirq_read(struct file *file, char __user *buf,
				size_t count, loff_t *off)
{
	unsigned char key_value;
	unsigned char key_releasevalue;
	int ret;
	struct imx6uirq_dev *dev = file->private_data;
	if(file->f_flags & O_NONBLOCK){
		if(atomic_read(&dev->keyrelease) == 0){
			return -EAGAIN;
		}
	}else{
		ret = wait_event_interruptible(dev->r_wait,atomic_read(&dev->keyrelease));
		if(ret){
			goto wait_error;
		}
	}
	key_value = atomic_read(&dev->keyvalue);
	key_releasevalue = atomic_read(&dev->keyrelease);
	if(key_releasevalue){
		if(key_value & 0x80){
			key_value &= ~0x80;
			ret = copy_to_user(buf,&key_value,sizeof(key_value));
			if(ret < 0){
			goto data_error;
			}
		}else{
			goto data_error;
		}
		atomic_set(&dev->keyrelease,0);
	}else{
		goto data_error;
	}
	return 0;
wait_error:
	return ret;
data_error:
	return -EINVAL;
}



unsigned int imx6uirq_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct imx6uirq_dev *dev = filp->private_data;
	poll_wait(filp, &dev->r_wait, wait);
	if(atomic_read(&dev->keyrelease)) { /* 按键按下 */ 
		mask = POLLIN | POLLRDNORM; /* 返回 PLLIN */
	}
	return mask;
}

static const struct file_operations imx6uirq_fops = {
	.owner	= THIS_MODULE,
	.open	= imx6uirq_open,
	.read	= imx6uirq_read,
	.poll   = imx6uirq_poll,
};

static int __init imx6uirq_init(void)
{
	int ret;

	if(imx6uirqdev.major){
		imx6uirqdev.devid = MKDEV(imx6uirqdev.major,imx6uirqdev.minor);
		ret = register_chrdev_region(imx6uirqdev.devid, KEY_CNT, KEY_NAME);
	}else{
		ret = alloc_chrdev_region(&imx6uirqdev.devid,0,KEY_CNT,KEY_NAME);
		imx6uirqdev.major = MAJOR(imx6uirqdev.devid);
		imx6uirqdev.minor = MINOR(imx6uirqdev.devid);
		printk("alloc_chrdev_region major=%d minor=%d\r\n",imx6uirqdev.major, imx6uirqdev.minor);
	}
	if (ret < 0) {
		printk("Could not register\r\n");
		goto fail_devid;
	}
	imx6uirqdev.cdev.owner = THIS_MODULE;
	cdev_init(&imx6uirqdev.cdev, &imx6uirq_fops);
	ret = cdev_add(&imx6uirqdev.cdev,imx6uirqdev.devid,IMX6UIRQ_CNT);
	if(ret < 0){
		printk("Could not cdev\r\n");
		goto fail_cdev;
	}
	imx6uirqdev.class = class_create(THIS_MODULE,IMX6UIRQ_NAME);
	if(IS_ERR(imx6uirqdev.class)){
		ret = PTR_ERR(imx6uirqdev.class);
		goto fail_class;
	}
	imx6uirqdev.device = device_create(imx6uirqdev.class,NULL,imx6uirqdev.devid,NULL,IMX6UIRQ_NAME);
	if(IS_ERR(imx6uirqdev.device)){
		ret = PTR_ERR(imx6uirqdev.device);
		goto fail_device;
	}
	//初始化按键及其原子变量
	atomic_set(&imx6uirqdev.keyrelease,0);

	ret = key_nd_init();
	if(ret < 0){
		return ret;
	}

	return 0;
fail_device:
	class_destroy(imx6uirqdev.class);
fail_class:
	cdev_del(&imx6uirqdev.cdev);
fail_cdev:
	unregister_chrdev_region(imx6uirqdev.devid,IMX6UIRQ_CNT);
fail_devid:
	return ret;
}


static void __exit imx6uirq_exit(void)
{
	unsigned int i = 0;

	/*释放中断和IO口*/
	for(i = 0; i < KEY_CNT;i++){
		free_irq(imx6uirqdev.keyirq[i].irqnum,&imx6uirqdev);
		gpio_free(imx6uirqdev.keyirq[i].gpio);
	}
	printk("key_exit\r\n");
	cdev_del(&imx6uirqdev.cdev);
	unregister_chrdev_region(imx6uirqdev.devid,IMX6UIRQ_CNT);

	device_destroy(imx6uirqdev.class,imx6uirqdev.devid);
	class_destroy(imx6uirqdev.class);
}





module_init(imx6uirq_init);
module_exit(imx6uirq_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("hbb");