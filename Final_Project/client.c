#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <ctype.h>
#include <time.h> 
#include <sys/time.h>
void usage();
// ./client -a 127.0.0.1 -p PORT -s 768 -d 979
int main(int argc, char *argv[])
{
    int opt,m=0;
    unsigned int port;
    const char* ipAdress;
    char* s,*d;
    if(argc!=9){
        fprintf(stderr, "%s. Entered count: %d\n","Argument count should be 9",argc );
        usage();
        exit(EXIT_FAILURE);
    }
    while((opt = getopt (argc, argv,"a:p:s:d:")) != -1){
        switch(opt){
            case 'a':
                ipAdress=optarg;
                break;
            case 'p':
                m=0;
                for (; m<strlen(optarg); m++){
                    if (!isdigit(optarg[m])){
                        printf("\n -p %s argument not unsigned integer. It should be integer. Program finished\n",optarg);
                        usage();
                        exit(EXIT_FAILURE);
                    }
                }
                port=atoi(optarg);
                break;
            case 's':
                s=optarg;
                m=0;
                for (; m<strlen(s); m++){
                    if (!isdigit(s[m])){
                        printf("\n -s %s argument not unsigned integer. It should be integer. Program finished\n",optarg);
                        usage();
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            case 'd':
                d=optarg;
                m=0;
                for (; m<strlen(optarg); m++){
                    if (!isdigit(optarg[m])){
                        printf("\n -d %s argument not unsigned integer. It should be integer. Program finished\n",optarg);
                        usage();
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case '?':
                    if (optopt == 'a'){
                        fprintf (stderr, "Option -%c requires an ip adress.\n", optopt);
                        usage();
                        exit(EXIT_FAILURE);
                    }
                    else if (optopt == 'p'){
                        fprintf (stderr, "Option -%c equires a port number.\n", optopt);
                        usage();
                        exit(EXIT_FAILURE);
                    }
                    else if (optopt == 's'){
                        fprintf (stderr, "Option -%c requires a start node number.\n", optopt);
                        usage();
                        exit(EXIT_FAILURE);
                    }
                    else if (optopt == 'd'){
                        fprintf (stderr, "Option -%c requires a end node number.\n", optopt);
                        usage();
                        exit(EXIT_FAILURE);
                    }
                    else {
                        fprintf (stderr,"Unknown option character %c\n", optopt);
                        usage();
                        exit(EXIT_FAILURE);
                    }
                    break;
            default:
                usage();
                exit(EXIT_FAILURE);
                break; 
        }
    }
    int sockfd = 0, readedByte = 0;
    char responseBuff[1024];
    struct sockaddr_in serv_addr; 

    memset(responseBuff, '0',sizeof(responseBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    } 
    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); 
    serv_addr.sin_addr.s_addr = inet_addr(ipAdress);
    if(inet_pton(AF_INET, ipAdress, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    } 
    printf("Client (%d) connecting to %s:%d\n",getpid(),ipAdress,port );
    int connectfd=0;

    if((connectfd=connect(sockfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0){
       printf("\n Error : Connect Failed \n");
       return 1;
    } 

    struct timeval  tm1, tm2;
    gettimeofday(&tm1, NULL);

    printf("Client (%d) connected and requesting a path from node %s to %s\n",getpid(),s,d);
    strcat(s,"-");
    strcat(s,d);
    write(sockfd, s, strlen(s)); 

    readedByte = read(sockfd, responseBuff, sizeof(responseBuff)-1);
    responseBuff[readedByte] = 0;
    if(readedByte < 0){
        printf("\n Read error! Program finished. \n");
        exit(EXIT_FAILURE);
    }
    gettimeofday(&tm2, NULL);
    double totalTime=(double) (tm2.tv_usec - tm1.tv_usec) / 1000000 +(double) (tm2.tv_sec - tm1.tv_sec);  
    printf("Server’s response to (%d): %s, arrived in %.1fseconds. \n\n",getpid(),responseBuff,totalTime);
    close(connectfd);
    return 0;
}

void usage(){
    printf("usage:\n");
    printf("You should enter like this arguments: ./client -a 127.0.0.1 -p PORT -s 768 -d 979\n");
    printf("-a: IP address of the machine running the server\n");
    printf("-p: port number at which the server waits for connections\n");
    printf("-s: source node of the requested path\n");
    printf("-d: destination node of the requested path\n");
}
