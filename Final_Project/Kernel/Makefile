obj-m += interrupts.o
#obj-m += kthread_test.o
#obj-m += hrtimer_test.o
#obj-m += ece4220lab6_isr.o
#obj-m += Lab6_cdev_kmod.o
#ccflags-y := -std=gnu99 -Wno-declaration-after-statement


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean