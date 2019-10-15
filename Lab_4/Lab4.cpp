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
#include <iomanip>
#include <vector>
using namespace std;

typedef struct {
	double time;
	int gps_coor;
} gps_struct;

gps_struct gpsstr;

typedef struct {
	double b1time;
	double b1gps;
	double b2time;
	double b2gps;
	double pushtime;
	double pushgps;
} timing_info;

//int printcount = 0;

vector<timing_info> print_vector;

sem_t printsem;

//Prints information given from Thread 0 child
void *Go_Thread_1(void *ptr) {
	//cout << "\nThread 1\n";
	sem_wait(&printsem);
	
	timing_info ti = *(new timing_info);
	timing_info tiold = print_vector.back();	
	
	ti.b1time 	= tiold.b1time;
	ti.b1gps 	= tiold.b1gps;
	ti.b2time 	= tiold.b2time;
	ti.b2gps 	= tiold.b2gps;
	ti.pushtime = tiold.pushtime;
	ti.pushgps 	= tiold.pushgps;
	
	//Destroy
	print_vector.pop_back();
	
	//print
	cout << endl;
	cout << "Previous GPS Coordinate:       " << setprecision(5) << ti.b1gps << "\t\t" << "Time: " << 
		setprecision(15) << ti.b1time << endl;
	cout << "Next GPS Coordinate:           " << setprecision(5) << ti.b2gps << "\t\t" << "Time: " << 
		setprecision(15) << ti.b2time << endl;
	cout << "Interpolated GPS Coordinate:   " << setprecision(5) << ti.pushgps << "\t\t" << "Time: " << 
		setprecision(15) << ti.pushtime << endl;

	//delete(&ti);

	sem_post(&printsem);
}


//Recieves Bound 1 and push information, waits for second tick and interpolates.
void *Go_Thread_0_Child(void *ptr) {
	//cout << "\nThread 0 Child\n";
	
	timing_info ti = *(new timing_info);
	gps_struct gpsstr2 = * (gps_struct*) ptr;

	double 	bound1time 	= gpsstr.time;
	double 	bound1gps	= (double)gpsstr.gps_coor;

	while(gpsstr.gps_coor == bound1gps) {
		usleep(100);
	}
	
	//Interpolate
	double 	pushtime 	= gpsstr2.time;

	double 	bound2time 	= gpsstr.time;
	double 	bound2gps 	= (double)gpsstr.gps_coor;

	double 	interpGPS 	= ((pushtime - bound1time)/(bound2time - bound1time))*(bound2gps - bound1gps) + bound1gps;

	ti.b1time 	= bound1time;
	ti.b1gps 	= bound1gps;
	ti.b2time 	= bound2time;
	ti.b2gps 	= bound2gps;
	ti.pushtime = pushtime;
	ti.pushgps 	= interpGPS;

	//Queue up printing information. Processor may not schedule this new thread right away
	// and data could be overwritten in a simple global variable pipe.
	print_vector.insert(print_vector.begin(),ti);
	//printcount++;
	//cout << "\ninsert printcount: " << printcount << endl;

	pthread_t thread1;
	pthread_create(&thread1,NULL,Go_Thread_1,NULL);
	//delete(&gpsstr);
	usleep(10);
}


//Reads btn push timestamp from Process-2, Recieves GPS and timestamp from Process-1, 
// creates child thread to wait for next time stamp
void *Go_Thread_0(void *ptr) {
	double 	timestamp; 
	gps_struct 	gpsstr2 = *(new gps_struct);

	int N_pipe2 = open("/tmp/N_pipe2",O_RDONLY);

	if(N_pipe2 != -1) {
		while(1) {	
			//cout << "\nThread 0\n";
			read(N_pipe2,&timestamp,sizeof(timestamp));

			gpsstr2.time = (double)timestamp;
			gpsstr2.gps_coor = 0;
						
			pthread_t thread0Ch;
			pthread_create(&thread0Ch,NULL,Go_Thread_0_Child,(void *)&gpsstr2);
			
			//To ensure child thread is run immediately
			usleep(10);
		}
	} else {
		cerr << endl << "Could not open N_pipe2" << endl; 
	}
}


//Recieves GPS information from N_pipe1, sends to thread0
int main() {
	sem_init(&printsem,0,1);
	pthread_t thread0;
	pthread_create(&thread0,NULL,Go_Thread_0,NULL);
	timeval timeofday;

	//Process 1: Recieves GPS data through N_pipe1
	//Thread0 read data from N_pipe2 and store GPS data before and after instance 
	int N_pipe1 = open("/tmp/N_pipe1",O_RDONLY);

	int gps;
	gpsstr.gps_coor = 0;

	if(N_pipe1 != -1) {
		while(1) {
			read(N_pipe1,&gps,sizeof(gps));
			gettimeofday(&timeofday, 0);
			double time = timeofday.tv_sec + timeofday.tv_usec/1000000.0;
			gpsstr.time = time;
			gpsstr.gps_coor = gps;
			
			//To ensure thread 0 is run immediately
			usleep(10);
		}
	} else {
		cerr << endl << "Could not open N_pipe1" << endl;
	}

	return 0;
}
