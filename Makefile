ifneq ($(KERNELRELEASE),)
obj-m := phy2virt.o 
else
KDIR :='/home/Work/Linux/Ti/AM572x/SDK_3_1_6/Linux/source/linux-4.4.19'
#KDIR:=/lib/modules/$(shell uname -r)/build
#KBUILD_EXTMOD := "/root/Desktop/SSS/work/driver/I2C_uart/SC16IS/new" 
PWD := $(shell pwd) 

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: modules clean
 endif
