/*
	Simple udp server:
	gcc udp_server.c && ./a.out
*/
#include<stdio.h>	//printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<unistd.h> //close
#include <errno.h>  // errno
#include<arpa/inet.h>
#include<sys/socket.h>

#define BUFLEN 512	//Max length of buffer

//#define SERV_ADDR "127.0.0.1"
#define SERV_ADDR  "192.168.178.96"
#define SERV_PORT  62058	//The port on which to receive data

//#define CLI_ADDR   "127.0.0.1"
#define CLI_ADDR   "192.168.178.1"
#define CLI_PORT   58062	//The port on which to send data

void die(char *s)
{
	perror(s);
	exit(1);
}

int main(void)
{
	struct sockaddr_in si_me, si_other;
	ssize_t sent_len;
	int s, rv, i, slen = sizeof(si_other) , recv_len, err;
	char buf[BUFLEN];
	
	//create a UDP socket
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		die("socket");
	}

	memset((char *) &si_other, 0, sizeof(si_other));
#if 1
	si_other.sin_family = AF_INET;
#else
	si_other.sin_family = AF_UNSPEC;
#endif
	si_other.sin_port = htons(CLI_PORT);
	
	//only allow a specific client IP address
	if (inet_aton(CLI_ADDR , &si_other.sin_addr) == 0) 
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	// zero out the structure
	memset((char *) &si_me, 0, sizeof(si_me));	
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(SERV_PORT);

	if (inet_aton(SERV_ADDR , &si_me.sin_addr) == 0) 
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	//bind socket to port
	if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
	{
		die("bind");
	}

#if 0
	rv = connect(s, (struct sockaddr *) &si_other, slen);
	if (rv != 0)
		die("connect");
#endif

	//keep listening for data
	while(1)
	{
		printf("Waiting for data...");
		fflush(stdout);
		
		//try to receive some data, this is a blocking call
		memset(buf, 0, BUFLEN);
#if 1
		if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
		{
			die("recvfrom()");
		}
		
		//print details of the client/peer and the data received
		printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
#else
		recv_len = recv(s, buf, BUFLEN, 0);
		if (recv_len == -1)
			die("recv()");
#endif
		printf("Data: %s\n" , buf);
		
		//now reply the client with the same data
#if 1
		if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == -1)
			die("sendto()");
#else
		rv = send(s, buf, recv_len, 0);
		if (rv == -1)
			die("send()");
#endif
	}

	close(s);
	return 0;
}
