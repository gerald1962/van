/*
	Simple udp client:
	gcc udp_client.c && ./a.out
*/
#include<stdio.h>	//printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<unistd.h> //close
#include <errno.h>  // errno
#include<arpa/inet.h>
#include<sys/socket.h>

#define BUFLEN 512	//Max length of buffer

//#define SERV_ADDR  "127.0.0.1"
#define SERV_ADDR   "192.168.178.96"
#define SERV_PORT  62058	//The port on which to send data

//#define CLI_ADDR  "127.0.0.1"
#define CLI_ADDR   "10.0.2.15"
#define CLI_PORT   58062	//The port on which to receive data

void die(char *s)
{
	perror(s);
	exit(1);
}

int main(void)
{
	struct sockaddr_in cli_addr, serv_addr;
	ssize_t n;
	int s, i, slen=sizeof(serv_addr), err;
	char buf[BUFLEN];
	char message[BUFLEN];

	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		die("socket");
	}

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_PORT);
	
	if (inet_aton(SERV_ADDR , &serv_addr.sin_addr) == 0) 
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}
	
	memset((char *) &cli_addr, 0, sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_port = htons(CLI_PORT);
	
	if (inet_aton(CLI_ADDR , &cli_addr.sin_addr) == 0) 
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	//bind socket to port
	if( bind(s , (struct sockaddr*)&cli_addr, sizeof(cli_addr) ) == -1)
	{
		die("bind");
	}

	while(1)
	{
		printf("Enter message : ");
		fgets(message, BUFLEN, stdin);

		//send the message
		if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
		{
			die("sendto()");
		}
		
		//receive a reply and print it
		//clear the buffer by filling null, it might have previously received data
		memset(buf,'\0', BUFLEN);
		
		//try to receive some data, this is a blocking call
		if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &serv_addr, &slen) == -1)
		{
			die("recvfrom()");
		}
		
		//print details of the client/peer and the data received
		printf("Received packet from %s:%d\n", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
		printf("Data: %s\n" , buf);
	}

	close(s);
	return 0;
}
