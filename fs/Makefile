NBLOCKS := 10

ifeq ($(KERNELRELEASE),)

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

all:
	gcc format_fs.c -o format_fs
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm format_fs

endif

create-fs:
	dd bs=4096 count=100 if=/dev/zero of=image
	./format_fs image $(NBLOCKS)
	mkdir mount

mount-fs:
	mount -o loop -t aos_fs image ./mount/

umount-fs:
	umount ./mount/
