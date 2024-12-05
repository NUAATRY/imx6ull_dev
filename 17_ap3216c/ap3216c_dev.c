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
#include <linux/timer.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "ap3216c_dev.h"

#define AP3216C_CNT     1
#define AP3216C_NAME    "ap3216c"

struct ap3216c_dev{
    dev_t devid;
    int major;
    int minor;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    void *private_data;
    unsigned short ir,als,ps;
};

static struct ap3216c_dev ap3216cdev;



static int ap3216c_read_regs(struct ap3216c_dev *dev, unsigned char reg, void *val, int len)
{
    int ret;
    struct i2c_msg msg[2];
    struct i2c_client *client = (struct i2c_client *)dev->private_data;
    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].buf = &reg;
    msg[0].len = 1;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].buf = val;
    msg[1].len = len;

    ret = i2c_transfer(client->adapter,msg,2);

    if(ret == 2){
        return 0;
    }else{
        printk("i2c read error\r\n");
        return -EREMOTEIO;
    }
}

static int ap3216c_write_regs(struct ap3216c_dev *dev, u8 reg, u8 *buf, u8 len) 
{
    u8 b[256];
    struct i2c_msg msg;
    struct i2c_client *client = (struct i2c_client *) dev->private_data;
    b[0] = reg; /* 寄存器首地址 */
    memcpy(&b[1],buf,len); /* 将要写入的数据拷贝到数组 b 里面 */
    msg.addr = client->addr; /* ap3216c 地址 */ 
    msg.flags = 0;
    msg.buf = b; /* 要写入的数据缓冲区 */ 
    msg.len = len + 1; /* 要写入的数据长度 */ 
    return i2c_transfer(client->adapter, &msg, 1);
}


static unsigned char ap3216c_read_reg(struct ap3216c_dev *dev, u8 reg)
{ 
    u8 data = 0;
    ap3216c_read_regs(dev, reg, &data, 1);
    return data;
#if 0 
    struct i2c_client *client = (struct i2c_client *) dev->private_data;
    return i2c_smbus_read_byte_data(client, reg); 
#endif 
}

static void ap3216c_write_reg(struct ap3216c_dev *dev, u8 reg, u8 data)
{ 
    u8 buf = 0;
    buf = data; 
    ap3216c_write_regs(dev, reg, &buf, 1);
}


void ap3216c_readdata(struct ap3216c_dev *dev)
{
    unsigned char i =0;
    unsigned char buf[6];
    for(i = 0; i < 6; i++){
        buf[i] = ap3216c_read_reg(dev, AP3216C_IRDATALOW + i);
    } 
    if(buf[0] & 0X80) /* IR_OF 位为 1,则数据无效 */ 
        dev->ir = 0;
    else /* 读取 IR 传感器的数据 */ 
        dev->ir = ((unsigned short)buf[1] << 2) | (buf[0] & 0X03); 
        dev->als = ((unsigned short)buf[3] << 8) | buf[2];/* ALS 数据 */
    if(buf[4] & 0x40) /* PS_OF 位为 1,则数据无效 */ 
        dev->ps = 0;
    else /* 读取 PS 传感器的数据 */ 
        dev->ps = ((unsigned short)(buf[5] & 0X3F) << 4) | (buf[4] & 0X0F);
}


static int ap3216c_open(struct inode *inode, struct file *file)
{
//    u8 data = 0;
    file->private_data = &ap3216cdev;
    ap3216c_write_reg(&ap3216cdev, AP3216C_SYSTEMCONG, 0x04);
    mdelay(50); /* AP3216C 复位最少 10ms */
    ap3216c_write_reg(&ap3216cdev, AP3216C_SYSTEMCONG, 0X03);
//    mdelay(50); /* AP3216C 复位最少 10ms */    
//    ap3216c_read_regs(&ap3216cdev, AP3216C_SYSTEMCONG, &data, 1);
//    printk("data: %d", data);
    return 0;
}

static ssize_t ap3216c_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
    short data[3];
    int ret;
    struct ap3216c_dev *dev = (struct ap3216c_dev *)file->private_data;
    ap3216c_readdata(dev);
    data[0] = dev->ir;
    data[1] = dev->als;
    data[2] = dev->ps;
    ret = copy_to_user(buf,data,sizeof(data));
    return ret;
}

int ap3216c_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations ap3216c_ops = {
    .owner = THIS_MODULE,
    .open = ap3216c_open,
    .read = ap3216c_read,
    .release = ap3216c_release,
};





static int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    printk("ap3216c_probe!!!\r\n");
    if(ap3216cdev.major){
        ap3216cdev.devid = MKDEV(ap3216cdev.major, 0);
        register_chrdev_region(ap3216cdev.devid,AP3216C_CNT,AP3216C_NAME);
    }else{
        alloc_chrdev_region(&ap3216cdev.devid,0,AP3216C_CNT,AP3216C_NAME);
        ap3216cdev.major = MAJOR(ap3216cdev.devid);
        ap3216cdev.minor = MINOR(ap3216cdev.devid);
        printk("ap3216c major %d minor %d!!!\r\n",ap3216cdev.major,ap3216cdev.minor);
    }
    cdev_init(&ap3216cdev.cdev,&ap3216c_ops);
    cdev_add(&ap3216cdev.cdev,ap3216cdev.devid,AP3216C_CNT);

    ap3216cdev.class = class_create(THIS_MODULE,AP3216C_NAME);
    ap3216cdev.device = device_create(ap3216cdev.class,NULL,ap3216cdev.devid,NULL,AP3216C_NAME);

    ap3216cdev.private_data = client;

    return 0;
}


static int ap3216c_remove(struct i2c_client *client)
{
    printk("ap3216c_remove!!!\r\n");
    cdev_del(&ap3216cdev.cdev);
    unregister_chrdev_region(ap3216cdev.devid,AP3216C_CNT);
    device_destroy(ap3216cdev.class,ap3216cdev.devid);
    class_destroy(ap3216cdev.class);    
    return 0;
}

static const struct of_device_id ap3216c_of_match[] = {
    {.compatible = "hbb,ap3216c"},
    {       }
};   

static const struct i2c_device_id ap3216c_id[] = {
    {"ap3216c",0},/* 我们用的是of_device_id,i2c_device_id可以写的不对，但是必须要有*/
};

static struct i2c_driver ap3216c_driver = {
    .probe = ap3216c_probe,
    .remove = ap3216c_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "ap3216c",
        .of_match_table = ap3216c_of_match,
    },
    .id_table = ap3216c_id,
};

static int __init ap3216c_init(void)
{
    return i2c_add_driver(&ap3216c_driver);
}

static void __exit ap3216c_exit(void)
{
    i2c_del_driver(&ap3216c_driver);
}

module_init(ap3216c_init);
module_exit(ap3216c_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("hubinbin");
