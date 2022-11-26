obj-m := list_rcu_example.o 

# Please set your kernel directory before build
KDIR := /lib/modules/$(shell uname -r)/build

ccflags-y += -Wall
ccflags-y += -Werror

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
