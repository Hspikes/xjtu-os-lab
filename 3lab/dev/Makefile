
ifneq ($(KERNELRELEASE),)
obj-m := glo_pro.o # 编译成外部模块
else
KERNELDIR := /lib/modules/$(shell uname -r)/build # 定义一个变量，指向内核目录
PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules # 编译内核模块

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean # 清理编译产生的文件
	rm -f read write

all : read write 
.PHONY : all
	gcc -o $@ $<
read: read.c
	gcc -o $@ $<
write: write.c
	gcc -o $@ $<

	
endif