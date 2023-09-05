NBLOCKS := 100

ifeq ($(KERNELRELEASE),)

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

all:
	gcc format_fs.c -o format_fs
	gcc debug_fs.c -o debug_fs
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm format_fs debug_fs image
	rmdir mount

endif

create-fs:
	dd bs=4096 count=$(NBLOCKS) if=/dev/zero of=image
	./format_fs image $(NBLOCKS)
	mkdir mount

debug-fs:
	./debug_fs image

mount-fs:
	mount -o loop -t aos_fs image ./mount/

umount-fs:
	umount ./mount/
