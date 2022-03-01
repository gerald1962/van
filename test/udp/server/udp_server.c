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

#define CLIENT "127.0.0.1"
#define BUFLEN 512	//Max length of buffer
#define PORT 5862	//The port on which to listen for incoming data

void die(char *s)
{
	perror(s);
	exit(1);
}

int main(void)
{
	struct sockaddr_in si_me, si_other;
	ssize_t sent_len;
	int s, i, slen = sizeof(si_other) , recv_len, err;
	char buf[BUFLEN];
	
	//create a UDP socket
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		die("socket");
	}
	
	// zero out the structure
	memset((char *) &si_me, 0, sizeof(si_me));
	
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
#if 0
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
#else
	//only allow a specific client IP address
	if (inet_aton(CLIENT , &si_me.sin_addr) == 0) 
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}
#endif	
	//bind socket to port
	if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
	{
		die("bind");
	}
	
	//keep listening for data
	while(1)
	{
#if 0
		printf("Waiting for data...");
		fflush(stdout);
		
		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
		{
			die("recvfrom()");
		}
		
		//print details of the client/peer and the data received
		printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
		printf("Data: %s\n" , buf);
		
		//now reply the client with the same data
		if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == -1)
		{
			die("sendto()");
		}
#else
		//try to receive some data, this is a non blocking call
		recv_len = recvfrom(s, buf, BUFLEN, MSG_DONTWAIT, (struct sockaddr *) &si_other, &slen);

		//in non blocking mode it is no mistake, if the data are not received immediately
		err = errno;
		if (recv_len == -1 && err != EAGAIN)
		{
			die("recvfrom()");
		}

		//have we received something
		if (recv_len > 0)
		{
			//print details of the client/peer and the data received
			printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
			printf("Data: %s\n" , buf);
		
			//now reply the client with the same data, this is a non blocking call
			sent_len = sendto(s, buf, recv_len, MSG_DONTWAIT, (struct sockaddr*) &si_other, slen);
			err = errno;
			printf("sendto done: sent_len = %ld, errno = %d\n", sent_len, err);

			//test number of the sent characters or the send status
			if (sent_len == -1)
			{
				die("sendto()");
			}
		}
		
		//wait a certain amount of time since now the internet accesses run in non blocking mode
		usleep(100);
#endif
	}

	close(s);
	return 0;
}
