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

#define SERVER "127.0.0.1"
#define BUFLEN 512	//Max length of buffer
#define PORT 5862	//The port on which to send data

void die(char *s)
{
	perror(s);
	exit(1);
}

int main(void)
{
	struct sockaddr_in si_other;
	ssize_t n;
	int s, i, slen=sizeof(si_other), err;
	char buf[BUFLEN];
	char message[BUFLEN];

	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		die("socket");
	}

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(PORT);
	
	if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
	{
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	while(1)
	{
		printf("Enter message : ");
		fgets(message, BUFLEN, stdin);

#if 0
		//send the message
		if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen)==-1)
		{
			die("sendto()");
		}
		
		//receive a reply and print it
		//clear the buffer by filling null, it might have previously received data
		memset(buf,'\0', BUFLEN);
		
		//try to receive some data, this is a blocking call
		if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen) == -1)
		{
			die("recvfrom()");
		}
		
		puts(buf);
#else
		//send the message, this is a non blocking call
		n = sendto(s, message, strlen(message) , MSG_DONTWAIT , (struct sockaddr *) &si_other, slen);
		err = errno;
		printf("sendto done: n = %ld, errno = %d\n", n, err);

		//test number of the sent characters or the send status
		if (n == -1)
		{
			die("sendto()");
		}
		
		//receive a reply and print it
		//clear the buffer by filling null, it might have previously received data
		memset(buf,'\0', BUFLEN);
		
		//try to receive some data, this is a non blocking call
		n = recvfrom(s, buf, BUFLEN, MSG_DONTWAIT , (struct sockaddr *) &si_other, &slen);
		err = errno;
		printf("recvfrom done: n = %ld, errno = %d\n", n, err);

		//in non blocking mode it is no mistake, if the data are not received immediately
		if (n == -1 && err != EAGAIN)
		{
			die("recvfrom()");
		}

		//test the length of the received data
		if (n > 0)
			puts(buf);

		//wait a certain amount of time since now the internet accesses run in non blocking mode
		usleep(100);
#endif
		
	}

	close(s);
	return 0;
}
