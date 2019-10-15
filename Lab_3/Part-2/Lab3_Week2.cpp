#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <wiringPi.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <sys/timerfd.h>
#include <iostream>
#include <unistd.h>
#include <semaphore.h>
#include "ece4220lab3.h"

#define LEDRED	8
#define LEDGRN	7
#define LEDYEL	9
#define LEDBLU	21
#define BTN		27

using namespace std;

sem_t sem;

//
int Create_RealTime_Process(int priority=99, int vsec=0, int vnsec=0, int isec=0, int insec=0) {
	struct itimerspec new_value;
	struct sched_param param;
	int tfd = timerfd_create(CLOCK_MONOTONIC,0);
	new_value.it_value.tv_sec = vsec;
	new_value.it_value.tv_nsec = vnsec;
	new_value.it_interval.tv_sec = isec;
	new_value.it_interval.tv_nsec = insec;
	timerfd_settime(tfd,0,&new_value,NULL);
	param.sched_priority = priority;
	sched_setscheduler(0, SCHED_RR, &param);
	return tfd;
}

/*
Case 1:	1=2=L 	All functions as normal. Ped Light thread is called at its turn in RR.
Case 2:	1=2>L 	Ped Light thread is never called.
Case 3: 1=2<L 	Ped light called immediately after the light its pushed on.
Case 4: 1>2>L 	By the time higher priority 1 and 2 are each done, they call each other. Ped light never called.
Case 5: 1<2=L	Ped light called in turn
Case 6: 1>2=L	Ped light is called successfully
*/

//Traffic light 1 thread. RED
void *Go_Traffic_Light1(void *ptr) {
	int tfd = Create_RealTime_Process(51);	
	
	while(1) {
		sem_wait(&sem);
		cout << "TL1\n";
		digitalWrite(LEDRED,1);
		sleep(1);
		digitalWrite(LEDRED,0);
		sem_post(&sem);
		usleep(100);
	}
	pthread_exit(0);
}

//Traffic Light 2 thread. GREEN
void *Go_Traffic_Light2(void *ptr) {
	int tfd = Create_RealTime_Process(51);	
	
	while(1) {
		sem_wait(&sem);
		cout << "TL2\n";
		digitalWrite(LEDGRN,1);
		sleep(1);
		digitalWrite(LEDGRN,0);
		sem_post(&sem);
		usleep(100);
	}
	pthread_exit(0);
}

//Pedestrian Light thread. YELLOW
void *Go_Ped_Light(void *ptr) {
	int tfd = Create_RealTime_Process(51);	
	
	while(1) {
		sem_wait(&sem);
		if(check_button()) {
			cout << "\tPL\n";
			digitalWrite(LEDYEL,1);
			sleep(1);
			digitalWrite(LEDYEL,0);
			clear_button();
		}
		sem_post(&sem);
		usleep(100);
	}
	pthread_exit(0);
}

int main(){

	wiringPiSetup();
	
	pinMode(LEDRED,OUTPUT);
	pinMode(LEDGRN,OUTPUT);
	pinMode(LEDYEL,OUTPUT);
	pinMode(LEDBLU,OUTPUT);
	pinMode(BTN,INPUT);

	//cout << "\n" << sched_get_priority_max(SCHED_RR);
	//cout << "\n" << sched_get_priority_min(SCHED_RR) << endl;
	pullUpDnControl(BTN,PUD_DOWN);

	digitalWrite(LEDRED,0);
	digitalWrite(LEDGRN,0);
	digitalWrite(LEDYEL,0);
	digitalWrite(LEDBLU,0);

	//Make sure flag is not still set from kernel
	clear_button();
	
	//Initialize semaphore
	sem_init(&sem,0,1);

	pthread_t t1,t2,pl;
	pthread_create(&t1, NULL, Go_Traffic_Light1, NULL);
	pthread_create(&t2, NULL, Go_Traffic_Light2, NULL);
	pthread_create(&pl, NULL, Go_Ped_Light, NULL);

	pthread_join(t1,NULL);
	pthread_join(t2,NULL);
	pthread_join(pl,NULL);
    return 0;
}
