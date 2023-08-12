SUBDIRS := fs
SCT := $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)
KERNELDIR := /lib/modules/$(shell uname -r)/build
FSDIR := ./fs
PWD := $(shell pwd)

obj-m := aos.o
aos-objs := aos_man.o aos_syscall.o lib/scth.o fs/aos_fs.o fs/file.o fs/dir.o

all:
	for n in $(SUBDIRS); do $(MAKE) -C $$n || exit 1; done
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	for n in $(SUBDIRS); do $(MAKE) -C $$n clean; done

create-fs:
	make -C $(FSDIR) create-fs

mount-fs:
	make -C $(FSDIR) mount-fs

umount-fs:
	make -C $(FSDIR) umount-fs

insmod:
	insmod aos.ko the_syscall_table=$(SCT)

rmmod:
	rmmod aos