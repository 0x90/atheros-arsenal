obj-m := jaldi.o
jaldi-objs :=  \
		main.o \
		ahb.o \
		pci.o \
		phy.o \
		eeprom.o \
		eeprom_def.o \
		init.o \
		hw.o \
		debug.o

PWD :=	$(shell pwd)

default:
	$(MAKE) -C $(KLIB_BUILD) M=$(PWD) modules
