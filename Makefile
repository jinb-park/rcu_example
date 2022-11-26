obj-m := list_rcu_example.o 
KDIR := /lib/modules/$(shell uname -r)/build

ccflags-y += -Wall
ccflags-y += -Werror

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
