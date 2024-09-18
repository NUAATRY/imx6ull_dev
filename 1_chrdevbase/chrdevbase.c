#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/ide.h>  //copy
/*
#include <linux/types.h>
#include <linux/delay.h>
*/

#define CHRDEVBASE_MAJOR 200  //主设备号
#define CHRDEVBASE_NAME "chrdevbase" //名字
static char readbuf[100];
static char writebuf[100];
static char kerneldata[] = {"kerneldata!!"};

static int chrdevbase_open(struct inode *inode, struct file *filp)
{
//	printk("open!!!\r\n");
	return 0;
}

static int chrdevbase_close(struct inode *inode, struct file *filp)
{
//	printk("close!!!\r\n");
	return 0;
}

static ssize_t chrdevbase_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
//	printk("write!!!\r\n");
	int ret = 0;
	ret = copy_from_user(writebuf,buf,count);
	if(ret == 0){
		printk("kernel recevdata:%s\r\n",writebuf);
	}else{
		printk("kernel recdevdata failed\r\n");
	}
	return 0;
}

static ssize_t chrdevbase_read(struct file *filp, char __user *buf,
			size_t count, loff_t *ppos)
{
	int ret = 0;
	//printk("read!!!\r\n");
	memcpy(readbuf,kerneldata,sizeof(kerneldata));
	ret = copy_to_user(buf,readbuf,count);
	if(ret == 0){
//		printk("kernel senddata ok\r\n");
	}else{
		printk("kernel senddata failed\r\n");
	}
	return 0;
}


static struct file_operations chrdevbase_fops = {
	.owner		= THIS_MODULE,
	.open		= chrdevbase_open,
	.release	= chrdevbase_close,
	.read		= chrdevbase_read,
	.write		= chrdevbase_write,
};

static int __init chrdevbase_init(void)
{
	int ret = 0;
	printk("init!!!\r\n");
	//注册字符设备
	ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME,&chrdevbase_fops);
	if(ret < 0){
		printk("init_faild!!!\r\n");
	}
	printk("chrdevbase_init()\r\n");
	return 0;
}

static void __exit chrdevbase_exit(void)
{
	printk("exit!!!\r\n");
	//注销字符设备
	unregister_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME);

	return;
}


module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hbb");
