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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include "ece4220lab3.h"

using namespace std;


//Creates a realtime process for the process that calls.
int Create_RealTime_Process(int priority=50, int vsec=0, int vnsec=0, int isec=0, int insec=0) {
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


int main(void) {
	//Process 2: Waits for button 1
	//When button is pushed, collect time stamp and pass through N_pipe2 to thread0
	timeval timeofday;
	uint64_t period;
	clear_button();

	system("mkfifo /tmp/N_pipe2");
	
	//Repeating process NEEDS initial delay. WHY???
	int pid = Create_RealTime_Process(99,0,10,0,60000000);
	int N_pipe2 = open("/tmp/N_pipe2",O_WRONLY);

	if(N_pipe2 != -1) {
		while(1) {
			read(pid,&period,sizeof(period));
			if(check_button()) {
				gettimeofday(&timeofday, 0); 
				double time = timeofday.tv_sec + timeofday.tv_usec/1000000.0;
				cout << "Button Pushed" << endl;
				
				//Send to N_pipe2
				write(N_pipe2,&time,sizeof(time));
				clear_button();
				usleep(10000);
			}
		}
	} else {
		cerr << endl << "Could not open N_pipe2" << endl;
	}
	return 0;
}
