#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <fstream>
#include <sys/timerfd.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include <cstring>

#define MY_PRIORITY 51

using namespace std;

char buffer[100];
char strarray[20][100];

//Prints contents of strarray
void print_array() {
	cout << endl;
	for(int i=0; i<20; i++) {
		cout << strarray[i] << endl;
	}
}	

//Reads in first.txt, real-time thread
void *Read1(void *ptr) {
	ifstream first("first.txt",ios::in);
	if(first == NULL) {
		cout << "\nError opening file\n";
		exit(1);
	}
	struct itimerspec new_value;
	struct sched_param param;
	int tfd = timerfd_create(CLOCK_MONOTONIC,0);
	new_value.it_value.tv_sec = 1;
	new_value.it_value.tv_nsec = 0;
	new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_nsec = 40000000;
	timerfd_settime(tfd,0,&new_value,NULL);
	param.sched_priority = MY_PRIORITY;
	sched_setscheduler(0, SCHED_FIFO, &param);
	uint64_t num_periods = 0;		
	
	for(int i=0; i<8; i++) {
		read(tfd, &num_periods, sizeof(num_periods));
		if(num_periods > 1) {
			puts("MISSED WINDOW");
			exit(1);
		}
		//cout << "1\n";
		first.getline(buffer,100);
	}
}

//Reads in second.txt, real-time thread
void *Read2(void *ptr) {
	ifstream second("second.txt",ios::in);
	if(second == NULL) {
		cout << "\nError opening file\n";
		exit(1);
	}
	struct itimerspec new_value;
	struct sched_param param;
	int tfd = timerfd_create(CLOCK_MONOTONIC,0);
	new_value.it_value.tv_sec = 1;
	new_value.it_value.tv_nsec = 20000000;
	new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_nsec = 40000000;
	timerfd_settime(tfd,0,&new_value,NULL);
	param.sched_priority = MY_PRIORITY;
	sched_setscheduler(0, SCHED_FIFO, &param);
	uint64_t num_periods = 0;		
	
	for(int i=0; i<8; i++) {
		read(tfd, &num_periods, sizeof(num_periods));
		if(num_periods > 1) {
			puts("MISSED WINDOW");
			exit(1);
		}
		//cout << "2\n";
		second.getline(buffer,100);
	}
}

//Writes out to first.txt, real-time thread
void *Write(void *ptr) {
	ofstream third("third.txt",ios::out);
	if(third == NULL) {
		cout << "\nError opening file\n";
		exit(1);
	}
	struct itimerspec new_value;
	struct sched_param param;
	int tfd = timerfd_create(CLOCK_MONOTONIC,0);
	new_value.it_value.tv_sec = 1;
	new_value.it_value.tv_nsec = 10000000;
	new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_nsec = 20000000;
	timerfd_settime(tfd,0,&new_value,NULL);
	param.sched_priority = MY_PRIORITY;
	sched_setscheduler(0, SCHED_FIFO, &param);
	uint64_t num_periods = 0;		
	
	for(int i=0; i<16; i++) {
		read(tfd, &num_periods, sizeof(num_periods));
		if(num_periods > 1) {
			puts("MISSED WINDOW");
			exit(1);
		}
		strcpy((char *)&strarray[i],(char*)&buffer);
	}
	print_array();
}


int main(void) {
	pthread_t read1,read2,write;
	
	pthread_create(&read1,NULL,Read1,NULL);
	pthread_create(&read2,NULL,Read2,NULL);
	pthread_create(&write,NULL,Write,NULL);
	pthread_join(read1,NULL);
	pthread_join(read2,NULL);
	pthread_join(write,NULL);
	cout << "Joined" << endl;
	return 0;
}
