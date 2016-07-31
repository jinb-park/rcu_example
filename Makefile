obj-m := list_rcu_example.o 

# Please set your kernel directory before build
KDIR:=""

ccflags-y += -Wall
ccflags-y += -Werror

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod *.symvers *.order *.mod.c

