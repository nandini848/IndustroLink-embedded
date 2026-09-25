KERNEL_DIR := /home/nandini_mn/buildroot/output/build/linux-6.18.7

obj-m += industrolink_driver.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
