ifeq ($(KERNELRELEASE),)

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

all:
	gcc makefs.c -o makefs
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm makefs

endif

create-fs:
	dd bs=4096 count=100 if=/dev/zero of=image
	./makefs image
	mkdir mount

mount-fs:
	mount -o loop -t aos_fs image ./mount/

umount-fs:
	umount ./mount/
