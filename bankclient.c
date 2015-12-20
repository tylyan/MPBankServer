/*
 * bankclient.c
 * Authors:	Emmanuel Baah
 * 		Yuk Yan
 *
 * Executable will start a bank server.  The server will initialize a global
 * data set of bank accounts, and then wait for client connections.  Upon
 * successful client connections, the bank will spawn new threads (or processes)
 * to communicate with the client.  
 *
 * Every 20 seconds, the bank will print out the information of all exixting accounts.
 */
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define PORT_NUMBER "3499"

static pthread_attr_t	kernel_attr;
static char		buff[512];

/***************************************************************************/
/* THREADS								   */
/***************************************************************************/

void *
commandinput_thread( void * sdptr )
{
	int		sockfd;

	sockfd = *(int *) sdptr;
	free(sdptr); // covenant

	while( 1 )
	{
		bzero(buff, sizeof(buff));
		fgets(buff, sizeof(buff), stdin);
		write(sockfd, buff, sizeof(buff));
		sleep(2);
	}
	printf("Normal end\n");
	exit(0);
}

void *
responseoutput_thread( void * sdptr )
{
	int		sockfd;

	sockfd = *(int *) sdptr;
	free(sdptr); // covenant

	while( read(sockfd, buff, sizeof(buff)) != 0 )
	{
		write(1, buff, sizeof(buff));		
	}
	printf("Connection to the server has been lost!\n");
	
	_exit(0);
	return 0;
}

/***************************************************************************/
/* MAIN									   */
/***************************************************************************/

int
main(int argvc, char ** argv)
{
	struct addrinfo		hints,
				* result;
	int			sockfd, error;
	int			* sdptr, * sdptr2;
	pthread_t		tid;
	
	hints.ai_flags = 0;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;

	if ( (error = getaddrinfo(argv[1], PORT_NUMBER , &hints, &result)) != 0 )
	{
	//	errormessage("getaddrinfo() failed");
		printf("getaddrinfo() failed\n");
		return 0;
	}
	else if ( (sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == -1 )
	{
	//	errormessage("socket() failed");
		printf("sockfd() failed\n");
		return 0;
	}
	else if ( (error = connect(sockfd, result->ai_addr, result->ai_addrlen)) != 0)
	{
		do
		{
			printf("Failed to connect()\n");
			printf("Will try to connect again in 3 seconds\n");
			sleep(3);
		} while((error = connect(sockfd, result->ai_addr, result->ai_addrlen)) != 0);
	}
	/* Spawn new thread to communicate with server */

	if ( (sdptr = (int *)malloc(sizeof(int))) == NULL )
	{
		printf("malloc() failed\n");
		return 0;
	}
	else if ( (sdptr2 = (int *)malloc(sizeof(int))) == NULL )
	{
		printf("malloc() failed\n");
		return 0;
	}
	else
	{
		*sdptr = sockfd;
		*sdptr2 = sockfd;
		if ( pthread_attr_init( &kernel_attr ) != 0)
		{
			printf("pthread_attri_init() failed\n");
			return 0;
		}
		else if ( pthread_attr_setscope( &kernel_attr, PTHREAD_SCOPE_SYSTEM ) != 0 )
		{
			printf("pthread_attr_setscope() failed\n");
			return 0;
		}
		else if ( pthread_create( &tid, &kernel_attr, commandinput_thread, sdptr) != 0 )
		{
			printf("pthread_create() failed\n");
			return 0;
		}
		else if ( pthread_create( &tid, &kernel_attr, responseoutput_thread, sdptr2) != 0 )
		{
			printf("pthread_create() failed\n");
			return 0;
		}
		else
		{
			printf("Connection successful\n");
			printf("Welcome to the bank of JuJu.\n");
			pthread_exit(0);
		}
	}
	
	printf("Normal end\n");
	return 0;
}
