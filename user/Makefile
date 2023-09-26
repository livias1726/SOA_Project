all:
	gcc single_syscalls.c utils.c -o single_syscalls
	gcc multi_syscalls.c utils.c -lpthread -o multi_syscalls
	gcc device_ops.c -lpthread -o device_ops

clean:
	rm single_syscalls
	rm multi_syscalls
	rm device_ops

run-single:
	./single_syscalls 174 177 178

run-multi:
	./multi_syscalls 174 177 178

run-dev:
	./device_ops