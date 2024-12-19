modname = pcibus2
modsource = pcibus2

all: $(modname).ko

KDIR = /lib/modules/$(shell uname -r)/build
ccflags-y += $(CFLAGS) -Wno-int-conversion

obj-m += $(modname).o
ifneq ($(modname), $(modsource))
	$(modname)-objs = $(modsource).o
endif

$(modname).ko: $(modsource).c
	make -C $(KDIR) M=$(PWD)

clean:
	make -C $(KDIR) M=$(PWD) clean

insmod: $(modname).ko
	sudo rmmod $(modname).ko || :
	sudo insmod $(modname).ko

rmmod: $(modname).ko
	sudo rmmod $(modname).ko
