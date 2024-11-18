KERNELDIR := /home/hbb/linux/kernel/linux-imx-rel_imx_4.1.15_2.1.0_ga
CURRENT_PATH := $(shell pwd)

my_name := noblockio_dev
obj-m := $(patsubst %,%.o,$(my_name))


build : kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

debug :
	@echo $(CURRENT_PATH)
	@echo $(obj-m)