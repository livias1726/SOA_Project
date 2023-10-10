PUT := 174
GET := 177
INV := 178

all:
	gcc test_single_sys.c ./user/single_syscalls.c ./user/utils.c -o test_single_sys
	gcc test_multi_sys.c ./user/multi_syscalls.c ./user/utils.c -lpthread -o test_multi_sys
	gcc test_dev.c ./user/device_ops.c ./user/utils.c -lpthread -o test_dev
	gcc super_test.c ./user/device_ops.c ./user/multi_syscalls.c ./user/utils.c -lpthread -o super_test

clean:
	rm test_single_sys
	rm test_multi_sys
	rm test_dev
	rm super_test

run-single:
	./test_single_sys $(PUT) $(GET) $(INV)

run-multi:
	./test_multi_sys $(PUT) $(GET) $(INV)

run-dev:
	./test_dev

run-super:
	./super_test $(PUT) $(GET) $(INV)