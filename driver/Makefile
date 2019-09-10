Q                := @
PWD              ?= $(shell pwd)
KERNEL_SRC       ?= /lib/modules/`uname -r`/build
INCLUDE_DIR      ?= $(PWD)/include

CFILES           := main.c triple_parse.c tx.c
TARGET           := usb2cansocketcan.ko
obj-m            := usb2cansocketcan.o
usb2cansocketcan-y := $(CFILES:.c=.o)
ccflags-y        := -I$(INCLUDE_DIR) -std=gnu99 -Wno-declaration-after-statement


default:
	$(MAKE) -C$(KERNEL_SRC) M=$(PWD) modules
	$(Q)cp $(TARGET) ../

clean:
	$(MAKE) -C$(KERNEL_SRC) M=$(PWD) clean
	$(Q)rm -f ../$(TARGET)
