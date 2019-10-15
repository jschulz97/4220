## 4220 Final Project README
## Jeff Schulz

In order to run and test my SCADA system:

RTU:
	1. Unzip source files.
	2. Perform "chmod +x" on:
		a. User/gadc.sh
		b. User/guser.sh
	3. Run each of these scripts (e.g. ./gadc.sh)
	4. "Make" the kernel module interrupts.c by typing "make" inside Kernel/
	5. Install kernel module: "sudo insmod interrupts.ko"
	6. Type "dmesg"
	7. Follow instructions printed to create and change permissions on the Character Device
	8. Run ./adc in one terminal window
	9. Run ./usersocket in another terminal window

Historian:
	1. Unzip source files.
	2. Perform "chmod +x" on :
		a. Historian/ghist.sh
	3. Run script "./ghist.sh"
	4. Run Historian "./historian"