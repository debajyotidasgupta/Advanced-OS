
# Assignment 2
# ------------------------------------------
# subject:        Advanced Operating Systems
# year:           2021 - 2022
# ------------------------------------------
# Name:           Debajyoti Dasgupta
# Roll No:        18CS30051
# ------------------------------------------
# Kernel Version: 5.11.0-37-generic
# System:         Ubuntu 20.04.3 LTS

obj-m+=18CS30051_Assign_2.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
