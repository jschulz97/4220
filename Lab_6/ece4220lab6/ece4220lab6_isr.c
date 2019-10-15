/* ece4220lab6_isr.c
 * ECE4220/7220
 * Author: Luis Alberto Rivera
 
 Basic steps needed to configure GPIO interrupt and attach a handler.
 Check chapter 6 of the BCM2835 ARM Peripherals manual for details about
 the GPIO registers, how to configure, set, clear, etc.
 
 Note: this code is not functional. It shows some of the actual code that you need,
 but some parts are only descriptions, hints or recommendations to help you
 implement your module.
 
 You can compile your module using the same Makefile provided. Just add
 obj-m += YourModuleName.o
 */

#ifndef MODULE
#define MODULE
#endif
#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define HPER1		1073 	//Bb4 1000000 / 466.16 / 2
#define HPER2		851		//D5
#define HPER3		716		//F5
#define HPER4		536		//Bb5
#define HPER5		426		//D6

//500,000 microsec = .5 sec
#define SOUNDLEN1 	500000 / HPER1 / 2  
#define SOUNDLEN2 	500000 / HPER2 / 2
#define SOUNDLEN3 	500000 / HPER3 / 2
#define SOUNDLEN4 	500000 / HPER4 / 2
#define SOUNDLEN5 	500000 / HPER5 / 2

MODULE_LICENSE("GPL");

/* Declare your pointers for mapping the necessary GPIO registers.
   You need to map:
   
   - Pin Event detect status register(s)
   - Rising Edge detect register(s) (either synchronous or asynchronous should work)
   - Function selection register(s)
   - Pin Pull-up/pull-down configuration registers
   
   Important: remember that the GPIO base register address is 0x3F200000, not the
   one shown in the BCM2835 ARM Peripherals manual.
*/

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
int i;

int mydev_id;	// variable needed to identify the handler

// Interrupt handler function. Tha name "button_isr" can be different.
// You may use printk statements for debugging purposes. You may want to remove
// them in your final code.
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
			printk("%d\n",HPER1);
			printk("%d\n",SOUNDLEN1);
			for(i=0; i<SOUNDLEN1; i++) {
				*GPSET0 |= 0b100000;
				udelay(HPER1);
				*GPCLR0 |= 0b100000;
				udelay(HPER1);
			}
			break;
		case 17:
			printk("Button 17 pushed\n");
			printk("%d\n",HPER2);
			printk("%d\n",SOUNDLEN2);
			for(i=0; i<SOUNDLEN2; i++) {
				*GPSET0 |= 0b100000;
				udelay(HPER2);
				*GPCLR0 |= 0b100000;
				udelay(HPER2);
			}
			break;
		case 18:
			printk("Button 18 pushed\n");
			printk("%d\n",HPER3);
			printk("%d\n",SOUNDLEN3);
			for(i=0; i<SOUNDLEN3; i++) {
				*GPSET0 |= 0b100000;
				udelay(HPER3);
				*GPCLR0 |= 0b100000;
				udelay(HPER3);
			}
			break;
		case 19:
			printk("Button 19 pushed\n");
			printk("%d\n",HPER4);
			printk("%d\n",SOUNDLEN4);
			for(i=0; i<SOUNDLEN4; i++) {
				*GPSET0 |= 0b100000;
				udelay(HPER4);
				*GPCLR0 |= 0b100000;
				udelay(HPER4);
			}
			break;
		case 20:
			printk("Button 20 pushed\n");
			printk("%d\n",HPER5);
			printk("%d\n",SOUNDLEN5);
			for(i=0; i<SOUNDLEN5; i++) {
				*GPSET0 |= 0b100000;
				udelay(HPER5);
				*GPCLR0 |= 0b100000;
				udelay(HPER5);
			}
			break;
	}

	// IMPORTANT: Clear the Event Detect status register before leaving.	
	*GPEDS0 	= *GPEDS0 & 0b00000000000111110000000000000000;	

	printk("Interrupt handled\n");	
	enable_irq(79);		// re-enable interrupt
	
    return IRQ_HANDLED;
}

int init_module()
{
	int dummy = 0;

	// Map GPIO registers
	// Remember to map the base address (beginning of a memory page)
	// Then you can offset to the addresses of the other registers
	unsigned long base = 0x3F200000;
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
			
	// Request the interrupt / attach handler (line 79, doesn't match the manual...)
	// The third argument string can be different (you give the name)
	dummy = request_irq(79, button_isr, IRQF_SHARED, "Button_handler", &mydev_id);

	printk("Button Detection enabled.\n");
	return 0;
}

// Cleanup - undo whatever init_module did
void cleanup_module()
{
	// Good idea to clear the Event Detect status register here, just in case.
	*GPEDS0 	= *GPEDS0 | 0b00000000000000000000000000000000;	

	// Disable (Async) Rising Edge detection for all 5 GPIO ports.
	*GPAREN0 	= *GPAREN0 | 0b00000000000000000000000000000000;

	*GPCLR0 = *GPCLR0 | 0b111100;

	// Remove the interrupt handler; you need to provide the same identifier
    free_irq(79, &mydev_id);
	
	printk("Button Detection disabled.\n");
}
