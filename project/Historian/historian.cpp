#include <stdio.h>
#include <sqlite3.h> 
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
#include <sstream>
#include <vector>

#define MSG_SIZE 40        // message size
#define PACKAGE_SIZE 100

using namespace std;

//slave = 0, master = 1
int status = 0;
int myrandnum = 0;
char message[MSG_SIZE];

struct ifaddrs *ifaddr;

struct Package {
    double timestamp;
    char RTUid[50];
    char status[100];
    char voltage[50];
    char event[50];
};

int run = 1;

sqlite3 *db;

//Error handling
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

//SOCKET 2
int sock2, length2, n2;
int boolval2 = 1;       // for a socket option
socklen_t fromlen2;
struct sockaddr_in server2;
struct sockaddr_in addr2;


/* Converts string from socket to Package format
 */
Package string_to_package(char in[PACKAGE_SIZE]) {

    //Parse out data in string
    string msg = string(in);
    int prev = 0;
    vector<string> strvect;
    for(int i=0;i<PACKAGE_SIZE; i++) {
        if(msg[i] == ',') {
            strvect.push_back(msg.substr(prev,i-prev));
            prev = i+1;
        }
    }

    //Save into new Package struct
    Package temppack;
    temppack.timestamp  = atof(strvect[0].c_str());
    strncpy(temppack.RTUid,(char*)strvect[1].c_str(),50);
    strncpy(temppack.status,(char*)strvect[2].c_str(),50);
    strncpy(temppack.voltage,(char*)strvect[3].c_str(),50);
    strncpy(temppack.event,(char*)strvect[4].c_str(),50);
    //cout << temppack.timestamp << " " << temppack.RTUid << " " << temppack.status << " " << temppack.voltage << " " << temppack.event << endl;

    return temppack;
}


/* Function that SQL calls when output is necessary in order to display records.
 */
static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for(i = 0; i<argc; i++) {
        //printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        printf("%s | ", argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}


/* Opens DB and creates a Table FPLog.
 */
int open_db() {
    char *zErrMsg = 0;
    int rc;

    rc = sqlite3_open("4220Final.db", &db);

    if( rc ) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    } else {
        fprintf(stderr, "Opened database successfully\n");
    }

    // Create SQL statement
    char* sql = "CREATE TABLE FPLog(" \
      "TIME                 DOUBLE      NOT NULL," \
      "RTUID                TEXT    NOT NULL," \
      "STATUS               TEXT," \
      "VOLTAGE              TEXT,"
      "EVENT                TEXT );";

    // Execute SQL statement
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

    if( rc != SQLITE_OK ){
        fprintf(stdout,"Table already exists.\n");
    } else {
        fprintf(stdout, "Table created successfully\n");
    }

    return 0;
}


/* Inserts record into the FPLog table. 
 */
void *write_to_db(void *ptr) {
    char *zErrMsg = 0;
    int rc;

    char *msg = (char*)ptr;
    Package temppkg = string_to_package(msg);

    /* Create SQL statement */
    char sql[200];
    sprintf(sql,"INSERT INTO FPLog (TIME,RTUID,STATUS,VOLTAGE,EVENT) VALUES (%f,'%s','%s','%s','%s');",temppkg.timestamp, temppkg.RTUid, temppkg.status, temppkg.voltage, temppkg.event);

    /* Execute SQL statement */
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } 
    return NULL;
}


/* Get ip address - Found on stack overflow
 */
char* getmycorrectip() {
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    snprintf(ifr.ifr_name, IFNAMSIZ, "enp0s25");
    ioctl(fd, SIOCGIFADDR, &ifr);
    char *ip =  inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    //printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    close(fd);
    return ip;
}


/* Sends string over socket2
 */
void send_led_command(string com) {
    
    server2.sin_addr.s_addr = inet_addr("128.206.19.255");
    n2 = sendto(sock2, com.c_str(), com.size(), 0, (struct sockaddr *)&server2, fromlen2);
    if (n2 < 0)
        cout << "Error sending: " << com; 
}



/* Main menu display thread for user input 
 */
void *menu(void *ptr) {
    int exit = 0;
    while(exit == 0) {
        cout << endl << endl;
        cout << "1. To view log: \n\tLOG [# records]\n\n";
        cout << "2. To change LEDs on RTUs:\n\tLED [COLOR] [0/1]\n\n";
        cout << "3. To exit:\n\tQ\n\n";

        //convert to all-caps
        char input[50];
        cin.getline(input,50);
        char res[50];
        for(int i=0; i<50; i++) {
            res[i] = toupper(input[i]);
        }

        //Strings are easier
        string result = string(res);

        //Action - display # of most recent log entries
        if(result.substr(0,3) == "LOG") {
            char sql[200];
            char *zErrMsg = 0;
            int rc;

            //create SQL query and execute it
            sprintf(sql,"SELECT * FROM FPLog ORDER BY TIME DESC LIMIT %s",result.substr(4,result.size()-4).c_str());
            cout << sql << endl;
            rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
            if( rc != SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            }
        }
        //Send string to RTUs
        else if(result.substr(0,3) == "LED") {
            send_led_command(result);
        }
        //Quit
        else if(result.substr(0,1) == "Q") {
            //exit menu loop AND main process
            exit = 1;
            run = 0;
        } else {
            system("clear");
            cout << "\nTry Again...\n";
        }
    }
}



int main(int argc, char *argv[]) {

    system("clear");
    int sock, length, n;
    int boolval = 1;       // for a socket option
    socklen_t fromlen;
    struct sockaddr_in server;
    struct sockaddr_in addr;


    //******************************************************************
    //***************************SOCKET SETUP***************************
    //Port 25566 - Recieving Packages from RTUs
    char *ip = getmycorrectip();
    cout << endl << "Device IP: " << ip << endl;

    sock = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
    if (sock < 0)
        error("Opening socket");

    length = sizeof(server);        // length of structure
    bzero(&server,length);       // sets all values to zero. memset() could be used
    server.sin_family = AF_INET;    // symbol constant for Internet domain
    server.sin_addr.s_addr = INADDR_ANY;     // IP address of the machine on which
                           // the server is running
    server.sin_port = htons(25566);  // port number

    // binds the socket to the address of the host and the port number
    if (bind(sock, (struct sockaddr *)&server, length) < 0)
        error("binding");

    // change socket permissions to allow broadcast
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0)
    {
        printf("error setting socket options\n");
        exit(-1);
    }

    fromlen = sizeof(struct sockaddr_in); // size of structure


    if(open_db()) {
        cout << "Error opening DB" << endl;
    }


    //******************************************************************
    //**************************SOCKET2 SETUP*************************** 
    //Port 25565 - Sending LED commands   
    ip = getmycorrectip();
    //cout << endl << "Device IP: " << ip << endl;
    sock2 = socket(AF_INET, SOCK_DGRAM, 0); // Creates socket. Connectionless.
    if (sock2 < 0)
        error("Opening socket");

    length2 = sizeof(server2);        // length of structure
    bzero(&server2,length2);       // sets all values to zero. memset() could be used
    server2.sin_family = AF_INET;    // symbol constant for Internet domain
    server2.sin_addr.s_addr = INADDR_ANY;     // IP address of the machine on which
                           // the server is running
    server2.sin_port = htons(25565);  // port number

    // binds the socket to the address of the host and the port number
    if (bind(sock2, (struct sockaddr *)&server2, length2) < 0)
        error("binding");

    // change socket permissions to allow broadcast
    if (setsockopt(sock2, SOL_SOCKET, SO_BROADCAST, &boolval2, sizeof(boolval2)) < 0)
    {
        printf("error setting socket options\n");
        exit(-1);
    }

    fromlen2 = sizeof(struct sockaddr_in); // size of structure



    //******************************************************************
    //*************************MENU THREAD****************************** 
    pthread_t menuthread;
    pthread_create(&menuthread,NULL,menu,NULL);




    //******************************************************************
    //***************************MAIN LOOP******************************
    //Receives Packages from RTUs
    while (run) {
        //Package temppkg;
        char msg[PACKAGE_SIZE];

        // receive from a client
        n = recvfrom(sock, &msg, sizeof(msg), 0, (struct sockaddr *)&addr, &fromlen);
        if (n < 0)
            error("recvfrom"); 

        //printf("Received a datagram: %s\n",msg);

        //Deal with info in separate thread, free up Process to catch more packages
        pthread_t writethread;
        pthread_create(&writethread,NULL,write_to_db,(void*)&msg);
    }

  return 0;
}
