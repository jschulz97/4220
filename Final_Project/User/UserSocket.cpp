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

#define CHAR_DEV "/dev/FinalP" // "/dev/YourDevName"
#define MSG_SIZE 50

#define LEDRED 2
#define LEDYEL 3
#define LEDGRN 4

using namespace std;

typedef struct Package {
    string timestamp;
    string RTUid;
    string status;
    string voltage;
    string event;
} package[10];


//slave = 0, master = 1
int status = 0;
int myrandnum = 0;
char message[MSG_SIZE];
int sent = 0;

struct ifaddrs *ifaddr;

int fppipe;


//Error handling
void error(const char *msg)
{
    perror(msg);
    exit(0);
}


/* Write to character device FinalP
 */
int write_to_chardev(char buffer[MSG_SIZE]) {
    int cdev_id, dummy;

    // Open the Character Device for writing
    if((cdev_id = open(CHAR_DEV, O_WRONLY)) == -1) {
        printf("Cannot open device %s\n", CHAR_DEV);
        exit(1);
    }

    dummy = write(cdev_id, buffer, sizeof(buffer));
    if(dummy != sizeof(buffer)) {
          printf("Write failed, leaving...\n");
    }

    close(cdev_id); // close the device.
    return 0;     // dummy used to prevent warning messages...
}


/* Read from character device FinalP
 * Gets BTN and Switch interrupts from kernel module
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

        cout << "Msg from kernel: " << buffer << endl;
        usleep(100000);
    }

    close(cdev_id); // close the device.
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


/* Read from named pipe from adc_wiringPi.c 
 * Reads ADC value
 */
void *read_from_pipe(void *pipe) {
    while(1) {
        uint16_t value;
        int dummy = read(*(int*)pipe,&value,sizeof(&value));
        //printf("Read from pipe: %d\n",value);
        usleep(9000); 
    }
}


int main(int argc, char *argv[]) {


    //******************************************************************
    //***************************SOCKET SETUP***************************
    int sock, length, n;
    int boolval = 1;			// for a socket option
    socklen_t fromlen;
    struct sockaddr_in server;
    struct sockaddr_in addr;
    char buffer[MSG_SIZE];	// to store received messages or messages to be sent.


    char *ip = getmycorrectip();
    cout << endl << "Device IP: " << ip << endl;

    if (argc < 2) {
        printf("usage: %s port\n", argv[0]);
        exit(0);
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
    if (sock < 0)
        error("Opening socket");

    length = sizeof(server);			// length of structure
    bzero(&server,length);			// sets all values to zero. memset() could be used
    server.sin_family = AF_INET;		// symbol constant for Internet domain
    server.sin_addr.s_addr = INADDR_ANY;		// IP address of the machine on which
  									// the server is running
    server.sin_port = htons(atoi(argv[1]));	// port number

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
    cout << "wiring pi setup: " << res << endl;
    pinMode(LEDRED,OUTPUT);
    pinMode(LEDYEL,OUTPUT);
    pinMode(LEDGRN,OUTPUT);

    digitalWrite(LEDRED,0);
    digitalWrite(LEDYEL,0);
    digitalWrite(LEDGRN,0);






    //****************************************************************
    //***************************PIPE SETUP*************************** 
    fppipe = open("/tmp/fppipe",O_RDONLY);
    if(fppipe == -1) {
        printf("Failed to open pipe fppipe\n");
    }

    pthread_t pipethread,kernelread;
    pthread_create(&pipethread,NULL,read_from_pipe,(void*)&fppipe);
    pthread_create(&kernelread,NULL,read_from_chardev,NULL);





    //****************************************************************
    //***************************MAIN LOOP**************************** 
    while (1) {
       	// bzero: to "clean up" the buffer. The messages aren't always the same length...
       	bzero(buffer,MSG_SIZE);		// sets all values to zero. memset() could be used

       	// receive from a client
       	n = recvfrom(sock, buffer, MSG_SIZE, 0, (struct sockaddr *)&addr, &fromlen);
       	if (n < 0)
      	  error("recvfrom"); 

       	printf("Received a datagram. It says: %s\n", buffer);


       	//Handle keywords
       	if(strncmp(buffer,"LED 1",5) == 0) {
       		printf("\nLED 1\n");
            //Send to all boards that I am master
            //addr.sin_addr.s_addr = inet_addr("128.206.19.255");    // broadcast address

            // char message[MSG_SIZE];
            // sprintf(message,"Jeff on board %s is master",ip); 

            // n = sendto(sock, message, MSG_SIZE, 0, (struct sockaddr *)&addr, fromlen);
            // if (n  < 0)
            //   error("sendto");
            // else
            //   printf("\nsuccess\n");

            if(strncmp(buffer,"LED 1 ON",8) == 0)
                digitalWrite(LEDRED,1);
            else if (strncmp(buffer,"LED 1 OFF",8) == 0)
                digitalWrite(LEDRED,0);

       	} else if(strncmp(buffer,"LED 2",5) == 0) {
            printf("\nLED 2\n");

            if(strncmp(buffer,"LED 2 ON",8) == 0)
                digitalWrite(LEDYEL,1);
            else if (strncmp(buffer,"LED 2 OFF",8) == 0)
                digitalWrite(LEDYEL,0);

        } else if(strncmp(buffer,"LED 3",5) == 0) {
            printf("\nLED 3\n");

            if(strncmp(buffer,"LED 3 ON",8) == 0)
                digitalWrite(LEDGRN,1);
            else if (strncmp(buffer,"LED 3 OFF",8) == 0)
                digitalWrite(LEDGRN,0);

        }
    }

  return 0;
}
