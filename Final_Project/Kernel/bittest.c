#include<stdio.h>


int main() {
	
	unsigned long test;
	test = 0x10001;
	printf("test: %lx\n",test);

	unsigned long t1;
	t1 = test & 0x33000;
	unsigned long t2;
	t2 = test & 0xFFFFF;

	printf("t1: %lx\n",t1);
	printf("t2: %lx\n",t2);


	return 0;
}