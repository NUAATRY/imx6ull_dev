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
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/unaligned.h>
#include <linux/irq.h>
#include "gt9147_dev.h"


struct gt9147_dev{
	int irq_pin,reset_pin;					/* 中断和复位IO		*/
	int irqnum;								/* 中断号    		*/
	int irqtype;							/* 中断类型         */
	int max_x;								/* 最大横坐标   	*/
	int max_y; 								/* 最大纵坐标		*/
	void *private_data;						/* 私有数据 		*/
	struct input_dev *input;				/* input结构体 		*/
	struct i2c_client *client;				/* I2C客户端 		*/
};

static struct gt9147_dev gt9147dev;

const u8 irq_table[] = {IRQ_TYPE_EDGE_RISING, IRQ_TYPE_EDGE_FALLING, IRQ_TYPE_LEVEL_LOW, IRQ_TYPE_LEVEL_HIGH};


static int gt9147_read_regs(struct gt9147_dev *dev, u16 reg, void *val, int len)
{
    int ret;
    u8 regdata[2];
    struct i2c_msg msg[2];
    struct i2c_client *client = (struct i2c_client *)dev->client;
    regdata[0] = reg >> 8;
    regdata[1] = reg & 0xff;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].buf = &regdata[0];
    msg[0].len = 2;

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

static int gt9147_write_regs(struct gt9147_dev *dev, u16 reg, u8 *buf, u8 len) 
{
    u8 b[256];
    struct i2c_msg msg;
    struct i2c_client *client = (struct i2c_client *) dev->client;
    b[0] = reg >> 8; //寄存器首地址 
    b[1] = reg & 0xff;
    memcpy(&b[2],buf,len); //将要写入的数据拷贝到数组 b 里面
    msg.addr = client->addr; //gt9147 地址 
    msg.flags = 0;
    msg.buf = b; //要写入的数据缓冲区 
    msg.len = len + 2; // 要写入的数据长度
    return i2c_transfer(client->adapter, &msg, 1);
}

static irqreturn_t gt9147_irq_handler(int irq, void *dev_id)
{
    int touch_num = 0;
    int input_x, input_y;
    int id = 0;
    int ret = 0;
    u8 data;
    u8 touch_data[BUFFER_SIZE];
    u16 touch_index = 0;
    int pos = 0;
    int report_num = 0;
    int i;
    static u16 last_index = 0;
    struct gt9147_dev *dev = dev_id;
    ret = gt9147_read_regs(dev, GT_GSTID_REG, &data, 1);
    if (data == 0x00)  {     // 没有触摸数据，直接返回
        goto fail;
    } else {                 //统计触摸点数据 
        touch_num = data & 0x0f;
    }
    if(touch_num) {         //有触摸按下
        //读取具体的触摸寄存器
        gt9147_read_regs(dev, GT_TP1_REG, touch_data, BUFFER_SIZE);
        id = touch_data[0];
        touch_index |= (0x01<<id);
        for(i = 0;i < 5; i++){
            if(touch_index |= (0x01 << i)){
                input_x  = touch_data[pos + 1] | (touch_data[pos + 2] << 8);
                input_y  = touch_data[pos + 3] | (touch_data[pos + 4] << 8);

                input_mt_slot(dev->input, id); //产生ABS_MT_SLOT 事件 报告是哪个触摸点的坐标 
		        input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, true); // 指定手指触摸  连续触摸
		        input_report_abs(dev->input, ABS_MT_POSITION_X, input_x);  // 上报触摸点坐标信息 
		        input_report_abs(dev->input, ABS_MT_POSITION_Y, input_y);  // 上报触摸点坐标信息
                report_num++;
                if(report_num < touch_num){
                    pos += 8;
                    id = touch_data[pos];
                    touch_index |= (0x01<<id);
                }
            }
            else{
                input_mt_slot(dev->input, i);
                input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, false);   // 关闭手指触摸 
            }
        }
    } else if(last_index){                // 触摸释放
        for(i = 0;i < 5; i++){
            if(last_index & (0x01 << i)){
                input_mt_slot(dev->input, i);
                input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, false);
            }
        }
    }
    last_index = touch_index;
	input_mt_report_pointer_emulation(dev->input, true);
    input_sync(dev->input);

    data = 0x00;                //向0X814E寄存器写0
    gt9147_write_regs(dev, GT_GSTID_REG, &data, 1);

fail:
	return IRQ_HANDLED;
}


static int gt9147_ts_irq(struct i2c_client *client, struct gt9147_dev *dev)
{
	int ret = 0;
	//2，申请中断,client->irq就是IO中断，
	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
					gt9147_irq_handler, irq_table[dev->irqtype] | IRQF_ONESHOT,
					client->name, &gt9147dev);
	if (ret) {
		dev_err(&client->dev, "Unable to request touchscreen IRQ.\n");
		return ret;
	}
	return 0;
}

static int gt9147_read_firmware(struct i2c_client *client, struct gt9147_dev *dev)
{
	int ret = 0, version = 0;
	u16 id = 0;
	u8 data[7]={0};
	char id_str[5];
	ret = gt9147_read_regs(dev, GT_PID_REG, data, 6);
	if (ret) {
		dev_err(&client->dev, "Unable to read PID.\n");
		return ret;
	}
	memcpy(id_str, data, 4);
	id_str[4] = 0;
    if (kstrtou16(id_str, 10, &id))
        id = 0x1001;
	version = get_unaligned_le16(&data[4]);
	dev_info(&client->dev, "ID %d, version: %04x\n", id, version);
	switch (id) {    // 由于不同的芯片配置寄存器地址不一样需要判断一下
    case 1151:
    case 1158:
    case 5663:
    case 5688:    //读取固件里面的配置信息
        ret = gt9147_read_regs(dev, GT_1xx_CFGS_REG, data, 7);  
		break;
    default:
        ret = gt9147_read_regs(dev, GT_9xx_CFGS_REG, data, 7);
		break;
    }
	if (ret) {
		dev_err(&client->dev, "Unable to read Firmware.\n");
		return ret;
	}
	dev->max_x = (data[2] << 8) + data[1];
	dev->max_y = (data[4] << 8) + data[3];
	dev->irqtype = data[6] & 0x3;
	printk("X_MAX: %d, Y_MAX: %d, TRIGGER: 0x%02x\r\n", dev->max_x, dev->max_y, dev->irqtype);
	return 0;
}

static int gt9147_ts_reset(struct i2c_client *client, struct gt9147_dev *dev)
{
	int ret = 0;

    //申请复位IO
	if (gpio_is_valid(dev->reset_pin)) {  		
		//申请复位IO，并且默认输出高电平 
		ret = devm_gpio_request_one(&client->dev,	
					dev->reset_pin, GPIOF_OUT_INIT_HIGH,
					"gt9147 reset");
		if (ret) {
			return ret;
		}
	}
    //申请中断IO
	if (gpio_is_valid(dev->irq_pin)) {  		
		// 申请复位IO，并且默认输出高电平
		ret = devm_gpio_request_one(&client->dev,	
					dev->irq_pin, GPIOF_OUT_INIT_HIGH,
					"gt9147 int");
		if (ret) {
			return ret;
		}
	}

    //4、初始化GT9147，要严格按照GT9147时序要求 
    gpio_set_value(dev->reset_pin, 0); // 复位GT9147
    msleep(10);
    gpio_set_value(dev->reset_pin, 1); // 停止复位GT9147
    msleep(10);
    gpio_set_value(dev->irq_pin, 0);    // 拉低INT引脚
    msleep(50);
    gpio_direction_input(dev->irq_pin); //INT引脚设置为输入
	return 0;
}

static int gt9147_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    u8 data;
    printk("gt9147_probe!!!\r\n");
    gt9147dev.client = client;

 	// 1，获取设备树中的中断和复位引脚
	gt9147dev.irq_pin = of_get_named_gpio(client->dev.of_node, "interrupt-gpios", 0);
	gt9147dev.reset_pin = of_get_named_gpio(client->dev.of_node, "reset-gpios", 0);

	// 2，复位GT9147
	ret = gt9147_ts_reset(client, &gt9147dev);
	if(ret < 0) {
		goto fail;
    }
    // 3，初始化GT9147
    data = 0x02;
    gt9147_write_regs(&gt9147dev, GT_CTRL_REG, &data, 1); //软复位
    mdelay(100);
    data = 0x0;
    gt9147_write_regs(&gt9147dev, GT_CTRL_REG, &data, 1); //停止软复位
    mdelay(100);

    // 4,初始化GT9147，读取固件
	ret = gt9147_read_firmware(client, &gt9147dev);
	if(ret != 0) {
		printk("Fail !!! check !!\r\n");
		goto fail;
    }

    // 5，input设备注册 
	gt9147dev.input = devm_input_allocate_device(&client->dev);
	if (!gt9147dev.input) {
		ret = -ENOMEM;
		goto fail;
	}
	gt9147dev.input->name = client->name;
	gt9147dev.input->id.bustype = BUS_I2C;
	gt9147dev.input->dev.parent = &client->dev;

	__set_bit(EV_KEY, gt9147dev.input->evbit);
	__set_bit(EV_ABS, gt9147dev.input->evbit);
	__set_bit(BTN_TOUCH, gt9147dev.input->keybit);

	input_set_abs_params(gt9147dev.input, ABS_X, 0, gt9147dev.max_x, 0, 0);
	input_set_abs_params(gt9147dev.input, ABS_Y, 0, gt9147dev.max_y, 0, 0);
	input_set_abs_params(gt9147dev.input, ABS_MT_POSITION_X,0, gt9147dev.max_x, 0, 0);
	input_set_abs_params(gt9147dev.input, ABS_MT_POSITION_Y,0, gt9147dev.max_y, 0, 0);	     
	ret = input_mt_init_slots(gt9147dev.input, MAX_SUPPORT_POINTS, 0);
	if (ret) {
		goto fail;
	}

	ret = input_register_device(gt9147dev.input);
	if (ret)
		goto fail;

    // 6，最后初始化中断 
	ret = gt9147_ts_irq(client, &gt9147dev);
	if(ret < 0) {
		goto fail;
	}
    return 0;
fail:
	return ret;
}


static int gt9147_remove(struct i2c_client *client)
{
    printk("gt9147_remove!!!\r\n");
    input_unregister_device(gt9147dev.input);
    return 0;
}

static const struct of_device_id gt9147_of_match[] = {
    {.compatible = "hbb,gt9147"},
    {       }
};   

static const struct i2c_device_id gt9147_id[] = {
    {"gt9147",0},/* 我们用的是of_device_id,i2c_device_id可以写的不对，但是必须要有*/
};

static struct i2c_driver gt9147_driver = {
    .probe = gt9147_probe,
    .remove = gt9147_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "gt9147",
        .of_match_table = gt9147_of_match,
    },
    .id_table = gt9147_id,
};

static int __init gt9147_init(void)
{
    return i2c_add_driver(&gt9147_driver);
}

static void __exit gt9147_exit(void)
{
    i2c_del_driver(&gt9147_driver);
}

module_init(gt9147_init);
module_exit(gt9147_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("hubinbin");
