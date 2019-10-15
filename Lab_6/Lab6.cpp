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

#define CHAR_DEV "/dev/Lab6" // "/dev/YourDevName"
#define MSG_SIZE 50

using namespace std;

//slave = 0, master = 1
int status = 0;
int myrandnum = 0;
char message[MSG_SIZE];

struct ifaddrs *ifaddr;


//Error handling
void error(const char *msg)
{
    perror(msg);
    exit(0);
}


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


//Gets ip address - Found on stack overflow
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


int main(int argc, char *argv[]) {
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

    while (1) {
       	// bzero: to "clean up" the buffer. The messages aren't always the same length...
       	bzero(buffer,MSG_SIZE);		// sets all values to zero. memset() could be used

       	// receive from a client
       	n = recvfrom(sock, buffer, MSG_SIZE, 0, (struct sockaddr *)&addr, &fromlen);
       	if (n < 0)
      	  error("recvfrom"); 

       	printf("Received a datagram. It says: %s\n", buffer);

       	//Handle keywords
       	if(strncmp(buffer,"WHOIS",5) == 0) {
       		printf("\nWHOIS CONFIRMED\n");
          if(status == 1) {
            //Send to all boards that I am master
            //addr.sin_addr.s_addr = inet_addr("128.206.19.255");    // broadcast address

            char message[MSG_SIZE];
            sprintf(message,"Jeff on board %s is master",ip); 

            n = sendto(sock, message, MSG_SIZE, 0, (struct sockaddr *)&addr, fromlen);
            if (n  < 0)
              error("sendto");
            else
              printf("\nsuccess\n");
          }

       	} else if(strncmp(buffer,"VOTE",4) == 0) {
       		printf("\nVOTE CONFIRMED\n");

            addr.sin_addr.s_addr = inet_addr("128.206.19.255");    // broadcast address

            //Generate random num
            myrandnum = rand() % 10 + 1;

            //Create char* message
            bzero(message,MSG_SIZE);
            sprintf(message,"# %s %d",ip,myrandnum); 

            //Send message, if succesful, preemptively set status = 1
            n = sendto(sock, message, MSG_SIZE, 0, (struct sockaddr *)&addr, fromlen);
            if (n  < 0)
                error("sendto");
            else {
                printf("\nsuccess\n");
                status = 1;
            }

       	} else if(strncmp(buffer,"#",1) == 0) {
            //Handles all votes
            //Status was set to 1 by vote confirmation
            //Parse out vote number and last ip
            int numpos, i=8;
            while(buffer[i] != ' ')
                i++;

            //Get target number
            numpos = i+1;
            int random = atoi(&buffer[numpos]);

            //If my random num is beaten, reset status
            if(random > myrandnum) {
                status = 0;
            } else if(random == myrandnum) {
                if(strncmp(buffer,message,15) > 0)
                    status = 0;
            }

        } else if(strncmp(buffer,"@",1) == 0) {
            printf("\nNOTE RECIEVED\n");
            if(status == 0) {
               //Since slave, send note to chardev
               n = write_to_chardev(buffer);
               if (n  < 0)
                    error("sendto");
               else
                    printf("\nsuccess\n");
            } 
        }
    }

  return 0;
}
