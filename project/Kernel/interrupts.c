/* Based on code found at https://gist.github.com/maggocnx/5946907
   Modified and commented by: Luis Rivera
   
   Compile using the Makefile
*/

#ifndef MODULE
#define MODULE
#endif
#ifndef __KERNEL__
#define __KERNEL__
#endif
   
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define MSG_SIZE 50
#define CDEV_NAME "FinalP"	// "YourDevName"

MODULE_LICENSE("GPL");

unsigned long base = 0x3F200000;
unsigned long * GPFSEL0;
unsigned long * GPFSEL1;
unsigned long * GPFSEL2;
unsigned long * GPSET0;
unsigned long * GPCLR0;
unsigned long * GPEDS0;
unsigned long * GPAREN0;
unsigned long * GPAFEN0;
unsigned long * GPPUD;
unsigned long * GPPUDCLK0;


int mydev_id;	// variable needed to identify the handler
int i;

//static int dummy = 0;
static int isr_ret;

static int major; 
static char msg[MSG_SIZE] = "\0";


// Function called when the user space program reads the character device.
// Some arguments not used here.
// buffer: data will be placed there, so it can go to user space
// The global variable msg is used. Whatever is in that string will be sent to userspace.
// Notice that the variable may be changed anywhere in the module...
static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
	//Necessary in my Pi as read() is not set up as a blocking function
	//Loops until data is avaible in msg, then allows character device read
	//In order to kill usersocket process, the current read must be completed
	//e.g. a button press event or switch event needs to occur
	while(msg[0] == '\0') {
		udelay(1000);
	}

	// Whatever is in msg will be placed into buffer, which will be copied into user space
	ssize_t dummy = copy_to_user(buffer, msg, length);	// dummy will be 0 if successful

	// msg should be protected (e.g. semaphore). Not implemented here, but you can add it.
	//sprintf(msg,"\0");	// "Clear" the message, in case the device is read again.
					// This way, the same message will not be read twice.
					// Also convenient for checking if there is nothing new, in user space.
	msg[0] = '\0';

	udelay(100);


	return length;
}

// Function called when the user space program writes to the Character Device.
// Some arguments not used here.
// buff: data that was written to the Character Device will be there, so it can be used
//       in Kernel space.
// In this example, the data is placed in the same global variable msg used above.
// That is not needed. You could place the data coming from user space in a different
// string, and use it as needed...
static ssize_t device_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	ssize_t dummy;
	
	if(len > MSG_SIZE)
		return -EINVAL;
	
	// unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
	dummy = copy_from_user(msg, buff, len);	// Transfers the data from user space to kernel space
	if(len == MSG_SIZE)
		msg[len-1] = '\0';	// will ignore the last character received.
	else
		msg[len] = '\0';
	
	// You may want to remove the following printk in your final version.
	printk("Message from user space: %s\n", msg);
	
	return len;		// the number of bytes that were written to the Character Device.
}

// structure needed when registering the Character Device. Members are the callback
// functions when the device is read from or written to.
static struct file_operations fops = {
	.read = device_read, 
	.write = device_write,
};


static irqreturn_t button_isr(int irq, void *dev_id)
{
	// In general, you want to disable the interrupt while handling it.
	disable_irq_nosync(79);

	// This same handler will be called regardless of what button was pushed,
	// assuming that they were properly configured.
	// How can you determine which button was the one actually pushed?
	// Evaluate GPEDS0 for changed bit.
	//unsigned long event = *GPEDS0;

	//printk("GPEDS0 pre = %lx\n",*GPEDS0);
	*GPEDS0 &= 0xCCFFF;
	//printk("GPEDS0 post = %lx\n",*GPEDS0);

	// DO STUFF (whatever you need to do, based on the button that was pushed)
	switch(*GPEDS0) {
		case 0x1000:
			printk("Switch 1 switched\n");
			sprintf(msg,"Switch 1 switched"); 
			break;
		case 0x2000:
			printk("Switch 2 switched\n");
			sprintf(msg,"Switch 2 switched");
			break;
		case 0x10000:
			printk("Button 1 pushed\n");
			sprintf(msg,"Button 1 pushed");
			break;
		case 0x20000:
			printk("Button 2 pushed\n");
			sprintf(msg,"Button 2 pushed");
			break;
	}

	// IMPORTANT: Clear the Event Detect status register before leaving.	
	*GPEDS0 = *GPEDS0 | 0xFFFFFFFF;

	printk("Interrupt handled\n");

	enable_irq(79);		// re-enable interrupt
	
    return IRQ_HANDLED;
}


int int_init(void)
{
	GPFSEL0 	= (unsigned long *) ioremap(base, 4096);
	GPFSEL1 	= GPFSEL0 + 0x4/4;
	GPFSEL2 	= GPFSEL0 + 0x8/4;
	GPSET0		= GPFSEL0 + 0x1C/4;
	GPCLR0		= GPFSEL0 + 0x28/4; //for cleanup
	GPEDS0	 	= GPFSEL0 + 0x40/4; //Event detect Status high = detected
	GPAREN0 	= GPFSEL0 + 0x7C/4; //rising edge detect enable
	GPAFEN0		= GPFSEL0 + 0x88/4; //falling edge detection
	GPPUD 		= GPFSEL0 + 0x94/4;
	GPPUDCLK0	= GPFSEL0 + 0x98/4;

	// Don't forget to configure all ports connected to the push buttons as inputs.
	// Push btns are BCM 16-20, speaker is BCM 6
	*GPFSEL0 	= *GPFSEL0 | 0b00000000000001000000000000000000;
	*GPFSEL1 	= *GPFSEL1 | 0b00000000000000000000000000000000;
	*GPFSEL2 	= *GPFSEL2 | 0b00000000000000000000000000000000;
	
	// You need to configure the pull-downs for all those ports. There is
	// a specific sequence that needs to be followed to configure those pull-downs.
	// The sequence is described on page 101 of the BCM2835-ARM-Peripherals manual.
	// You can use  udelay(100);  for those 150 cycles mentioned in the manual.
	// It's not exactly 150 cycles, but it gives the necessary delay.
	// WiringPi makes it a lot simpler in user space, but it's good to know
	// how to do this "by hand".
	*GPPUD 		= *GPPUD | 0b00000000000000000000000000000001;
	udelay(100);
	*GPPUDCLK0 	= *GPPUDCLK0 | 0b00000000000000000000000011000000;
	udelay(100);
	*GPPUD 		= *GPPUD & 0b00000000000000000000000000000001;
	*GPPUDCLK0 	= *GPPUDCLK0 & 0b00000000000000000000000011000000;

		
	// Enable (Async) Rising Edge / Falling Edge Detection
	*GPAREN0 	= *GPAREN0 | 0b00000000000000110011000000000000;
	*GPAFEN0 	= *GPAFEN0 | 0b00000000000000000011000000000000;
	isr_ret = request_irq(79, button_isr, IRQF_SHARED, "Button_handler", &mydev_id);
	printk("Button Detection enabled.\n");

	// register the Characted Device and obtain the major (assigned by the system)
	major = register_chrdev(0, CDEV_NAME, &fops);
	if (major < 0) {
     		printk("Registering the character device failed with %d\n", major);
	     	return major;
	}

	//Char dev installation instructions
	printk("Lab6_cdev_kmod example, assigned major: %d\n", major);
	printk("Create Char Device (node) with: sudo mknod /dev/%s c %d 0\n", CDEV_NAME, major);
	printk("Change permissions of Char Dev with: sudo chmod go+w /dev/%s\n", CDEV_NAME);
	
	return 0;
}

void int_exit(void)
{
	// Good idea to clear the Event Detect status register here, just in case.
	*GPEDS0 	= *GPEDS0 | 0xFFFFFFFF;	

	// Disable (Async) Rising Edge detection for all 5 GPIO ports.
	*GPAREN0 	= *GPAREN0 & 0x33000;

	*GPCLR0		= *GPCLR0 | 0b111100;

	// Remove the interrupt handler; you need to provide the same identifier
    free_irq(79, &mydev_id);
    printk("Button Detection disabled.\n");


  	// Once unregistered, the Character Device won't be able to be accessed,
	// even if the file /dev/YourDevName still exists. Give that a try...
	unregister_chrdev(major, CDEV_NAME);
	printk("Char Device /dev/%s unregistered.\n", CDEV_NAME);
	
}

// Notice this alternative way to define your init_module()
// and cleanup_module(). "timer_init" will execute when you install your
// module. "timer_exit" will execute when you remove your module.
// You can give different names to those functions.
module_init(int_init);
module_exit(int_exit);