KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

DRVRparm := $(DRVRparm)
DRVRparm := 0

obj-m := refcount.o

all: 	clean run
	@make -s clean

run: load refcountapp
	echo "Hello World" > /dev/refcount;
	cat < /dev/refcount;
	echo "Here is some input" > /dev/refcount;
	cat < /dev/refcount;
	echo "Input is good" > /dev/refcount;
	cat < /dev/refcount;
	echo "Hi" > /dev/refcount;
	cat < /dev/refcount;
	cat /proc/refcount;

load: compile
	-su -c "{ insmod ./refcount.ko DRVRparm=$(DRVRparm);} || \
		{ echo DRVRparm is not set;} ";

compile:
	$(MAKE) -C $(KDIR) M=$(PWD) modules


refcountapp:
	-gcc -o refcountapp refcountapp.c;

unload:
	-su -c "rmmod refcount; rm -fr /dev/refcount;"

clean: unload
	-@rm -fr *.o refcount*.o refcount*.ko .refcount*.* refcount*.*.* refcountapp .tmp_versions .[mM]odule* [mM]o*

