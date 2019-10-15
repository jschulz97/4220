/* 	Name       : 	server_udp_broadcast.c
	Author     : 	Luis A. Rivera
	Description: 	Simple server (broadcast)
					ECE4220/7220		*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <sys/ioctl.h>
#include <net/if.h>
#include <fcntl.h>
#include <pthread.h>
#include <wiringPi.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <vector>
#include <sstream>
#include <iomanip>

#define CHAR_DEV "/dev/FinalP" // "/dev/YourDevName"
#define MSG_SIZE 50

#define LEDRED 2
#define LEDYEL 3
#define LEDGRN 4

//Make sure to change when on second RTU
#define RTUID "RTU0"

using namespace std;

struct Package {
    double timestamp;
    string RTUid;
    string status;
    uint16_t voltage;
    string event;
};

vector<Package> packvect;

//slave = 0, master = 1
int status = 0;
int myrandnum = 0;
char message[MSG_SIZE];
int sent = 0;
uint16_t currentvoltage;

struct ifaddrs *ifaddr;

int fppipe;

int ledred = 0,ledgreen = 0,ledyellow = 0;

//Error handling
void error(const char *msg)
{
    perror(msg);
    exit(0);
}


/* Creates a realtime process for the process that calls.
 */
int Create_RealTime_Process(int priority=50, int vsec=0, int vnsec=10, int isec=0, int insec=0) {
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


/* Adds event package to Package vector
 */
int add_event_to_packages(double time,string rtuid,string status,string msg) {
    Package temppack;
    temppack.timestamp = time;
    temppack.RTUid = rtuid;
    temppack.status = status;
    temppack.voltage = currentvoltage;
    temppack.event = msg;

    packvect.push_back(temppack);

    cout << "Added to packvect: " << msg << endl;

    return 0;   
}


/* Turns Package object into cstring to send through socket
 */
const char* package_to_string(Package pkg) {
    stringstream str;
    str << setprecision(15) << pkg.timestamp << "," << pkg.RTUid << "," << pkg.status << "," << (int)pkg.voltage << "," << pkg.event << ",";

    return str.str().c_str();
}


// /* Write to character device FinalP
//  */
// int write_to_chardev(char buffer[MSG_SIZE]) {
//     int cdev_id, dummy;

//     // Open the Character Device for writing
//     if((cdev_id = open(CHAR_DEV, O_WRONLY)) == -1) {
//         printf("Cannot open device %s\n", CHAR_DEV);
//         exit(1);
//     }

//     dummy = write(cdev_id, buffer, sizeof(buffer));
//     if(dummy != sizeof(buffer)) {
//           printf("Write failed, leaving...\n");
//     }

//     close(cdev_id); // close the device.
//     return 0;     // dummy used to prevent warning messages...
// }


/* Read from character device FinalP thread
 * Gets button and switch interrupts from kernel module
 */
void *read_from_chardev(void *ptr) {
    int cdev_id, dummy;
    char buffer[MSG_SIZE];

    // Open the Character Device for reading
    if((cdev_id = open(CHAR_DEV, O_RDONLY)) == -1) {
        printf("Cannot open device %s\n", CHAR_DEV);
        exit(1);
    }

    while(1) {
        dummy = read(cdev_id, buffer, sizeof(buffer));
        if(dummy != sizeof(buffer)) {
              printf("Read failed, leaving...\n");
        }

        //cout << "Msg from kernel: " << buffer << endl;
        //Get current time of incident 
        timeval timeofday;
        gettimeofday(&timeofday, 0);
        double time = timeofday.tv_sec + timeofday.tv_usec/1000000.0;
        string msg = buffer;
        //Add event to Package vector
        add_event_to_packages(time,RTUID,"",msg);
        //usleep(100);
    }
    //close(cdev_id); // close the device.
}


/* Get correct IP Address
 */
char* getmycorrectip() {
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    snprintf(ifr.ifr_name, IFNAMSIZ, "wlan0");
    ioctl(fd, SIOCGIFADDR, &ifr);
    char *ip =  inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    //printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    close(fd);
    return ip;
}


/* Thread to read from named pipe from adc_wiringPi.c 
 * Reads ADC value
 */
void *read_from_pipe(void *ptr) {
    fppipe = open("/tmp/fppipe",O_RDONLY);
    if(fppipe == -1) {
        printf("Failed to open pipe fppipe\n");
    }

    //Counter to ensure event packages are not generated constantly 
    //for when voltage errors occur
    int counter = 0;

    while(1) {
        int dummy = read(fppipe,&currentvoltage,sizeof(&currentvoltage));
        if(counter == 0 && (currentvoltage > 550 || currentvoltage < 400)) {
            timeval timeofday;
            gettimeofday(&timeofday, 0);
            double time = timeofday.tv_sec + timeofday.tv_usec/1000000.0;

            double result = double(currentvoltage)/1023*3.3;
            stringstream msg;
            msg << "Voltage outside normal operating capacity: " << result << "V";
            add_event_to_packages(time,RTUID,"Error",msg.str());

            counter = 1000;
        }

        if(counter > 0) 
            counter--;
    }
}


/* Sends update packages as well as all event packages every second
 */
void *send_update_packages(void *ptr) {   
    //Create real time process that is allowed to execute every second
    uint64_t period;
    int pid = Create_RealTime_Process(99,0,10,1,0);

    //Create socket - Port 25566 for Sending Packages to Historian
    int sock, length, n;
    int boolval = 1;            // for a socket option
    socklen_t fromlen;
    struct sockaddr_in server;
    struct sockaddr_in addr;

    char *ip = getmycorrectip();
    //cout << endl << "Device IP: " << ip << endl;

    sock = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
    if (sock < 0)
        error("Opening socket");

    length = sizeof(server);            // length of structure
    bzero(&server,length);          // sets all values to zero. memset() could be used
    server.sin_family = AF_INET;        // symbol constant for Internet domain
    server.sin_addr.s_addr = INADDR_ANY;        // IP address of the machine on which
                                    // the server is running
    server.sin_port = htons(25566); // port number

    // binds the socket to the address of the host and the port number
    if (bind(sock, (struct sockaddr *)&server, length) < 0)
        error("binding");

    // change socket permissions to allow broadcast
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0)
    {
        printf("error setting socket options\n");
        exit(-1);
    }

    fromlen = sizeof(struct sockaddr_in);   // size of structure

    //Loop for sending Packages every second
    while(1) {
        //Controlled by RT scheduler
        read(pid,&period,sizeof(period));
        cout << "Sending update packages...\n";

        //Get current time
        timeval timeofday;
        gettimeofday(&timeofday, 0);
        double time = timeofday.tv_sec + timeofday.tv_usec/1000000.0;   

        //Create status update package
        Package temppack;
        temppack.timestamp = time;
        temppack.RTUid = RTUID;
        stringstream str;
        str << "RED LED " << ledred << " YELLOW LED " << ledyellow << " GREEN LED " << ledgreen;
        temppack.status = str.str();
        temppack.voltage = currentvoltage;
        temppack.event = "Status Update";

        //Convert package to string for socket use
        const char *msg = package_to_string(temppack);

        //Broadcast
        server.sin_addr.s_addr = inet_addr("128.206.19.255"); 

        n = sendto(sock, msg, 100, 0, (struct sockaddr *)&server, fromlen);
        if (n  < 0)
            cout << "Error sending update package.\n";
        else
            printf("\nsuccess\n");

        //Send all error packages
        while(packvect.size() > 0) {
            temppack = packvect.front();
            msg = package_to_string(temppack);
            n = sendto(sock, msg, 100, 0, (struct sockaddr *)&server, fromlen);
            if (n  < 0)
                cout << "Error sending package through socket:" << endl << msg << endl;
            
            packvect.erase(packvect.begin());
        }
    }
}




int main(int argc, char *argv[]) {

    //******************************************************************
    //***************************SOCKET SETUP***************************
    //For receiving LED commands - Port 25565
    int sock, length, n;
    int boolval = 1;			// for a socket option
    char buffer[MSG_SIZE];
    socklen_t fromlen;
    struct sockaddr_in server;
    struct sockaddr_in addr;
    struct ifaddrs *ifaddr;


    char *ip = getmycorrectip();
    cout << endl << "Device IP: " << ip << endl;

    sock = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
    if (sock < 0)
        error("Opening socket");

    length = sizeof(server);			// length of structure
    bzero(&server,length);			// sets all values to zero. memset() could be used
    server.sin_family = AF_INET;		// symbol constant for Internet domain
    server.sin_addr.s_addr = INADDR_ANY;		// IP address of the machine on which
  									// the server is running
    server.sin_port = htons(25565);	// port number

    // binds the socket to the address of the host and the port number
    if (bind(sock, (struct sockaddr *)&server, length) < 0)
        error("binding");

    // change socket permissions to allow broadcast
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0)
  	{
  		printf("error setting socket options\n");
  		exit(-1);
  	}

    fromlen = sizeof(struct sockaddr_in);	// size of structure

    


    //********************************************************************
    //***************************WIRINGPI SETUP***************************
    int res = wiringPiSetupGpio();
    pinMode(LEDRED,OUTPUT);
    pinMode(LEDYEL,OUTPUT);
    pinMode(LEDGRN,OUTPUT);

    digitalWrite(LEDRED,0);
    digitalWrite(LEDYEL,0);
    digitalWrite(LEDGRN,0);



    //****************************************************************
    //*******************THREAD AND TIMER SETUP***********************     
    pthread_t pipethread,kernelread,packageupdate;
    pthread_create(&pipethread,NULL,read_from_pipe,NULL);
    pthread_create(&kernelread,NULL,read_from_chardev,NULL);
    pthread_create(&packageupdate,NULL,send_update_packages,NULL);

    


    //****************************************************************
    //*************************LED LISTEN LOOP************************ 
    while (1) {
       	// bzero: to "clean up" the buffer. The messages aren't always the same length...
       	bzero(buffer,MSG_SIZE);		// sets all values to zero. memset() could be used

       	// receive from a client
       	n = recvfrom(sock, buffer, MSG_SIZE, 0, (struct sockaddr *)&addr, &fromlen);
       	if (n < 0)
      	  error("recvfrom"); 

       	printf("Received a datagram. It says: %s\n", buffer);

        //Get time
        timeval timeofday;
        gettimeofday(&timeofday, 0);
        double time = timeofday.tv_sec + timeofday.tv_usec/1000000.0;

       	//Handle keywords
       	if(strncmp(buffer,"LED RED",7) == 0) {
       		printf("\nLED RED\n");

            if(strncmp(buffer,"LED RED 1",9) == 0) {
                //Write to GPIO
                digitalWrite(LEDRED,1);
                //Add the event to event list
                add_event_to_packages(time,RTUID,"","RED LED ON");
                //Global variable to mirror pin
                ledred = 1;
            }
            else if (strncmp(buffer,"LED RED 0",9) == 0) {
                digitalWrite(LEDRED,0);
                add_event_to_packages(time,RTUID,"","RED LED OFF");
                ledred = 0;
            }

       	} else if(strncmp(buffer,"LED YELLOW",10) == 0) {
            printf("\nLED YELLOW\n");

            if(strncmp(buffer,"LED YELLOW 1",12) == 0) {
                digitalWrite(LEDYEL,1);
                add_event_to_packages(time,RTUID,"","YELLOW LED OFF");
                ledyellow = 1;
            }
            else if (strncmp(buffer,"LED YELLOW 0",12) == 0) {
                digitalWrite(LEDYEL,0);
                add_event_to_packages(time,RTUID,"","YELLOW LED OFF");
                ledyellow = 0;
            }

        } else if(strncmp(buffer,"LED GREEN",9) == 0) {
            printf("\nLED GREEN\n");

            if(strncmp(buffer,"LED GREEN 1",11) == 0) {
                ledgreen = 1;
                digitalWrite(LEDGRN,1);
                add_event_to_packages(time,RTUID,"","GREEN LED ON");
            }
            else if (strncmp(buffer,"LED GREEN 0",11) == 0){
                digitalWrite(LEDGRN,0);
                add_event_to_packages(time,RTUID,"","GREEN LED OFF");
                ledgreen = 0;
            }

        }
    }

  return 0;
}
