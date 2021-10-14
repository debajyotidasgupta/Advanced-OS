MODULE_NAME = 18CS10066

obj-m += ${MODULE_NAME}.o
$(MODULE_NAME)-y := deque.o

TESTS = tests/bin/race tests/bin/user

all: ${MODULE_NAME}.ko ${TESTS}

${MODULE_NAME}.ko: deque.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules


.PHONY: clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm tests/bin/*

.PHONY: tests
tests: ${TESTS}
	sudo insmod ${MODULE_NAME}.ko

	@echo "Testing Multiprocess accesses"
	-./tests/bin/race
	sleep 2

	@echo "Testing User Program"
	-./tests/bin/user
	sleep 1


	sudo rmmod ${MODULE_NAME}

tests/bin/race: tests/race.cpp
	g++ tests/race.cpp -o tests/bin/race -lc -lm -lpthread

tests/bin/user: tests/user.c
	gcc tests/user.c -o tests/bin/user -lc -lm