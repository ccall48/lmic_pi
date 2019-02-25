#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h> 
   
int send_to_socket(int port, const char * host, const char * buffer) 
{ 
    struct sockaddr_in address; 
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   
    memset(&serv_addr, '0', sizeof(serv_addr)); 
   
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(port); 
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, host, &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 
    send(sock , buffer, strlen(buffer) , 0 ); 
    return 0; 
} 


int main(int argc, char *argv[])
{
    int opt;
    char buffer[1024], host[255];
    strcpy(buffer, "");
    unsigned int port = 0;
    while((opt = getopt(argc, argv, "p:h:a:d:n:s:e:x:")) != -1)
    {
        switch(opt)
        {
            case 'p':
                {
                port = atoi(optarg);
                }
                break;

           case 'h':
                {
                strcpy(host, optarg);
                }
                break;
 
            default:
                {
                char optbuf[2];
                optbuf[0] = opt;
                optbuf[1] = 0;
                strcat(buffer, optbuf);
                strcat(buffer, ":");
                strcat(buffer, optarg);
                strcat(buffer, ":");
                }
                break;
        }
    }

    printf("SENDING=%s\n", buffer);
    send_to_socket(port, host, buffer);

    return 0;
}
