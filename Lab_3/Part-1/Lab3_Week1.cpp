#include <stdlib.h>
#include <stdio.h>
#include <wiringPi.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <sys/timerfd.h>
#include <iostream>
#include <unistd.h>
#include "ece4220lab3.h"

#define MY_PRIORITY 51

#define LEDRED	8
#define LEDGRN	7
#define LEDYEL	9
#define LEDBLU	21
#define BTN		27

//
int Create_RealTime_Process(int vsec=0, int vnsec=0, int isec=0, int insec=0) {
	struct itimerspec new_value;
	struct sched_param param;
	int tfd = timerfd_create(CLOCK_MONOTONIC,0);
	new_value.it_value.tv_sec = vsec;
	new_value.it_value.tv_nsec = vnsec;
	new_value.it_interval.tv_sec = isec;
	new_value.it_interval.tv_nsec = insec;
	timerfd_settime(tfd,0,&new_value,NULL);
	param.sched_priority = MY_PRIORITY;
	sched_setscheduler(0, SCHED_RR, &param);
	return tfd;
}

//
void *Go_Traffic_Light(void *ptr) {
	int tfd = Create_RealTime_Process();	

	while(1) {
		digitalWrite(LEDRED,1);
		sleep(1);
		digitalWrite(LEDRED,0);
		digitalWrite(LEDGRN,1);
		sleep(1);
		digitalWrite(LEDGRN,0);

		if(check_button()) {
			digitalWrite(LEDYEL,1);
			sleep(2);
			digitalWrite(LEDYEL,0);
			clear_button();
		}
	}
	pthread_exit(0);
}

int main(){

	wiringPiSetup();
	
	pinMode(LEDRED,OUTPUT);
	pinMode(LEDGRN,OUTPUT);
	pinMode(LEDYEL,OUTPUT);
	pinMode(BTN,INPUT);

	pullUpDnControl(BTN,PUD_DOWN);

	digitalWrite(LEDRED,0);
	digitalWrite(LEDGRN,0);
	digitalWrite(LEDYEL,0);

	pthread_t thread;
	pthread_create(&thread, NULL, Go_Traffic_Light, NULL);

	pthread_join(thread,NULL);
    return 0;
}