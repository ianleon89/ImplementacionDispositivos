
obj-m += char_01.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc test_module.c -o test_module


clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm test_module
