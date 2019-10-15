/* Based on example from: http://tuxthink.blogspot.com/2011/02/kernel-thread-creation-1.html
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
#include <linux/kthread.h>	// for kthreads
#include <linux/sched.h>	// for task_struct
#include <linux/time.h>		// for using jiffies 
#include <linux/timer.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/interrupt.h>

#define HPER1		1073	//Bb4 1000000 / 466.16 / 2
#define HPER2		851		//D5
#define HPER3		716		//F5
#define HPER4		536		//Bb5
#define HPER5		426		//D6

MODULE_LICENSE("GPL");

unsigned long base = 0x3F200000;
unsigned long * GPFSEL0;
unsigned long * GPFSEL1;
unsigned long * GPFSEL2;
unsigned long * GPSET0;
unsigned long * GPCLR0;
unsigned long * GPEDS0;
unsigned long * GPAREN0;
unsigned long * GPPUD;
unsigned long * GPPUDCLK0;

int targetBtn;
int mydev_id;	// variable needed to identify the handler
int i;
int interval = HPER1;
static int isr_ret;


// structure for the kthread.
static struct task_struct *kthread1;

static irqreturn_t button_isr(int irq, void *dev_id)
{
	// In general, you want to disable the interrupt while handling it.
	disable_irq_nosync(79);

	// This same handler will be called regardless of what button was pushed,
	// assuming that they were properly configured.
	// How can you determine which button was the one actually pushed?
	// Evaluate GPEDS0 for changed bit.
	//unsigned long event = *GPEDS0;
	
	// one of the events has to happen, so GPEDS0 has to be at least 2^16
	for(i=16; i<21; i++) {
		//bit shift for powers of 2
		//printk("for: %d %d %d\n",i,*GPEDS0,2<<i);
		if(*GPEDS0 < (2<<i)) {
			targetBtn = i;
			break;
		}
	}

	// DO STUFF (whatever you need to do, based on the button that was pushed)
	switch(targetBtn) {
		case 16:
			printk("Button 16 pushed\n"); 
			//printk("%d\n",HPER1);
			interval = HPER1;
			break;
		case 17:
			printk("Button 17 pushed\n");
			//printk("%d\n",HPER2);
			interval = HPER2;
			break;
		case 18:
			printk("Button 18 pushed\n");
			//printk("%d\n",HPER3);
			interval = HPER3;
			break;
		case 19:
			printk("Button 19 pushed\n");
			//printk("%d\n",HPER4);
			interval = HPER4;
			break;
		case 20:
			printk("Button 20 pushed\n");
			//printk("%d\n",HPER5);
			interval = HPER5;
			break;
	}

	// IMPORTANT: Clear the Event Detect status register before leaving.	
	*GPEDS0 	= *GPEDS0 & 0b00000000000111110000000000000000;	

	printk("Interrupt handled\n");	
	enable_irq(79);		// re-enable interrupt
	
    return IRQ_HANDLED;
}


// Function to be associated with the kthread; what the kthread executes.
int kthread_fn(void *ptr)
{
	unsigned long j0, j1;
	//int count = 0;

	printk("In kthread1\n");
	j0 = jiffies;		// number of clock ticks since system started;
						// current "time" in jiffies
	j1 = j0 + 10*HZ;	// HZ is the number of ticks per second, that is
						// 1 HZ is 1 second in jiffies
	
	while(time_before(jiffies, j1))	// true when current "time" is less than j1
        schedule();		// voluntarily tell the scheduler that it can schedule
						// some other process
	
	printk("Before loop\n");
	
	// The ktrhead does not need to run forever. It can execute something
	// and then leave.
	while(1)
	{
		//msleep(1000);	// good for > 10 ms
		udelay(interval);
		*GPSET0 |= 0b100000; 	
		udelay(interval);
		*GPCLR0 |= 0b100000;
		//msleep_interruptible(1000); // good for > 10 ms
		//udelay(unsigned long usecs);	// good for a few us (micro s)
		//usleep_range(unsigned long min, unsigned long max); // good for 10us - 20 ms
		
		
		// In an infinite loop, you should check if the kthread_stop
		// function has been called (e.g. in clean up module). If so,
		// the kthread should exit. If this is not done, the thread
		// will persist even after removing the module.
		if(kthread_should_stop()) {
			do_exit(0);
		}
				
		// comment out if your loop is going "fast". You don't want to
		// printk too often. Sporadically or every second or so, it's okay.
		//printk("Count: %d\n", ++count);
	}
	
	return 0;
}

int thread_init(void)
{
	char kthread_name[11]="my_kthread";	// try running  ps -ef | grep my_kthread
										// when the thread is active.
	printk("In init module\n");

	GPFSEL0 	= (unsigned long *) ioremap(base, 4096);
	GPFSEL1 	= GPFSEL0 + 0x4/4;
	GPFSEL2 	= GPFSEL0 + 0x8/4;
	GPSET0		= GPFSEL0 + 0x1C/4;
	GPCLR0		= GPFSEL0 + 0x28/4; //for cleanup?
	GPEDS0	 	= GPFSEL0 + 0x40/4; //Event detect Status high = detected
	GPAREN0 	= GPFSEL0 + 0x7C/4; //rising edge detect enable
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
	*GPPUDCLK0 	= *GPPUDCLK0 | 0b00000000000000000000011111000000;
	udelay(100);
	*GPPUD 		= *GPPUD & 0b00000000000000000000000000000001;
	*GPPUDCLK0 	= *GPPUDCLK0 & 0b00000000000000000000011111000000;

		
	// Enable (Async) Rising Edge detection for all 5 GPIO ports.
	*GPAREN0 	= *GPAREN0 | 0b00000000000111110000000000000000;
	isr_ret = request_irq(79, button_isr, IRQF_SHARED, "Button_handler", &mydev_id);
	printk("Button Detection enabled.\n");
    	    
    kthread1 = kthread_create(kthread_fn, NULL, kthread_name);
	
    if((kthread1))	// true if kthread creation is successful
    {
        printk("Inside if\n");
		// kthread is dormant after creation. Needs to be woken up
        wake_up_process(kthread1);
    }

    return 0;
}

void thread_cleanup(void) {
	int ret;
	// the following doesn't actually stop the thread, but signals that
	// the thread should stop itself (with do_exit above).
	// kthread should not be called if the thread has already stopped.
	ret = kthread_stop(kthread1);

	// Good idea to clear the Event Detect status register here, just in case.
	*GPEDS0 	= *GPEDS0 | 0b00000000000000000000000000000000;	

	// Disable (Async) Rising Edge detection for all 5 GPIO ports.
	*GPAREN0 	= *GPAREN0 | 0b00000000000000000000000000000000;

	*GPCLR0 = *GPCLR0 | 0b111100;

	// Remove the interrupt handler; you need to provide the same identifier
    free_irq(79, &mydev_id);
    printk("Button Detection disabled.\n");
								
	if(!ret)
		printk("Kthread stopped\n");
}

// Notice this alternative way to define your init_module()
// and cleanup_module(). "thread_init" will execute when you install your
// module. "thread_cleanup" will execute when you remove your module.
// You can give different names to those functions.
module_init(thread_init);
module_exit(thread_cleanup);
