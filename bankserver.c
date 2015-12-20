/*
 * bankserver.c
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
#include "bankserver.h"
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>

#define PORT_NUMBER "3499"
#define KEY_PATHNAME "bankserver.c"
#define KEY_ID 2

Bank			* bank;
static pthread_attr_t	kernel_attr;
static char		buff[512];

/***************************************************************************/
/* SIGNAL HANDLERS							   */
/***************************************************************************/

/*
 * Signal handler for SIGINT.
 *
 * When invoked, the bank server will shutdown.
 */
static void
sigint_handler( int signo )
{
	printf("SIGINT invoked... server shutting down.\n");
	_exit( 0 );
}

/*
 * Signal handler for SIGCHLD
 *
 * When invoked, this handler will wait() and clean up a child process.
 */
static void
sigchld_handler( int signo )
{
	printf("SIGCHLD_HANDLER: Child process ended, parent PID %d wait()ing now.\n", getpid());
	wait(NULL);
}

/*
 * Initializes signal handlers.
 */
static void
init_sighandlers()
{
	struct sigaction	action;

	action.sa_flags = 0;
	action.sa_handler = sigint_handler;
	sigemptyset( &action.sa_mask );
	sigaction(SIGINT, &action, 0 );

	action.sa_flags = SA_SIGINFO | SA_RESTART;
	action.sa_handler = sigchld_handler;
	sigemptyset( &action.sa_mask );
	sigaction(SIGCHLD, &action, 0);
}

void
init_addrinfo( struct addrinfo * aiptr )
{
	(*aiptr).ai_flags = AI_PASSIVE;
	(*aiptr).ai_family = AF_INET;
	(*aiptr).ai_socktype = SOCK_STREAM;
	(*aiptr).ai_protocol = 0;
	(*aiptr).ai_addrlen = 0;
	(*aiptr).ai_canonname = NULL;
	(*aiptr).ai_next = NULL;
}

/***************************************************************************/
/* UTILITIES 								   */
/***************************************************************************/

/*
 * Parses the buffer and populates the argument pointer as needed.
 *
 * Returns -1 on error.
 * Returns 0 for open account. Argument is populated with account name.
 * Returns 1 for start account. Argument is populated with account name.
 * Returns 2 for credit account. Argument is populated with amount.
 * Returns 3 for debit account. Argument is populated with amount.
 * Returns 4 for balance. Argument is not populated.
 * Returns 5 for finish. Argument is not populated.
 * Returns 6 for exit. Argument is not populated.
 */
int
parseBuffer( char* buff , char * argument){
	int		end, length,
			i, truelength, inter, rv;
	char		* arg1;

	arg1 = NULL;
	end = length = i = truelength = inter = rv = 0;

	/*Read first argument*/

	for( length = 0 ; ; length++ )
	{
		if( buff[length] == '\0' )
		{
			end = 1;
			if( (arg1 = (char *)malloc(sizeof(char)*length)) == NULL)
			{
		//		errormessage("malloc() failed");
				return -1;
			}
			strncpy( arg1, buff, length);
			arg1[length-1] = '\0';			
			break;		
		}
		else if( buff[length] == ' ')
		{
			if( (arg1 = (char *)malloc((sizeof(char)*length) + 1)) == NULL)
			{
		//		errormessage("malloc() failed");
				return -1;
			}
			strncpy( arg1, buff, length+1);
			arg1[length] = '\0';
			break;
		}
	}
	if(arg1 == NULL){
		printf("arg1 is null\n");
		return -2;
	}


	inter = length+1;

	/*If second argument exists, read it now, and set value of argument to the read argument*/

	if(end != 1)
	{
//		printf("parseBuffer: end != 1\n");
		truelength = 0;
//		length = 0;
		for( length++; ; truelength++)
		{
			if( buff[length] == '\n'){
//				printf("new line character detected yo. length:%d , truelength:%d\n",length,truelength);
			}
			if( buff[length] == '\0' ){
//				printf("null character detected yo. length:%d , truelength:%d\n",length,truelength);
//				printf("parseBuffer: buff[length] == '\\0'\n");

//				if( ((*argument) = (char *)malloc(sizeof(char)*truelength)) == NULL)
//				{
			//		errormessage("malloc() failed");
//					return -1;
//				}
				strncpy( argument, (&buff[inter]), truelength);
				(argument)[truelength-1] = '\0';			
				break;		
			}
			length++;
		}		
	}

	if( strcmp(arg1, "open") == 0)
	{
		rv = 0;
	}
	else if( strcmp(arg1, "start") == 0)
	{
		rv = 1;
	}
	else if( strcmp(arg1, "credit") == 0)
	{
		rv = 2;
	}
	else if( strcmp(arg1, "debit") == 0)
	{
		rv = 3;
	}
	else if( strcmp(arg1, "balance") == 0)
	{
		rv = 4;
	}
	else if( strcmp(arg1, "finish") == 0)
	{
		rv = 5;
	}
	else if( strcmp(arg1, "exit") == 0)
	{
		rv = 6;
	}
	else
	{
		rv = -1;
	}
	free(arg1);
	return rv;
}
/***************************************************************************/
/* BANK ACCOUNT FUNCTIONS						   */
/***************************************************************************/

/*
 * Initializes a bank struct in shared memory.
 *
 * Returns a pointer to the shared memory segment.
 */
Bank *
initshmBank()
{
	key_t		key;
	int		shmid, id, i;
	const char	* path = KEY_PATHNAME;
	char		* test;
	Bank		* bank;

	id = KEY_ID;

	if( (key = ftok( path, id )) == -1 )
	{
		errormessage("ftok() failed");
		return 0;
	}
	else if ( (shmid = shmget( key, sizeof(Bank), 0666 | IPC_CREAT | IPC_EXCL )) == -1 )
	{
		if( errno == EEXIST )
		{
			/* Shared memory already exists */
			printf("Shared memory segment already exists\n");
			if ( (shmid = shmget(key, 0, 0666)) == -1 )
			{
				errormessage("shmget() failed");
				return 0;
			}
			else
			{
				if ( (test = shmat(shmid, 0, 0)) == (char *) -1 )
				{
					errormessage("shmat() failed");
					return 0;
				}
				else
				{
					bank = (Bank *) test;
					return bank;
				}
			}	
		}
		else
		{
			errormessage("shmget() failed");
			return 0;
		}
	}
	else{
		printf("Sucessfully created shared memory segment.\n");
		if ( (test = shmat(shmid, 0, 0)) == (char *) -1 )
		{
			errormessage("shmat() failed");
			return 0;
		}
		else{
			bank = (Bank *) test;
			bank->numaccounts = 0;
			if ( pthread_mutex_init( &bank->bankmutex, NULL ) != 0 )
			{
				errormessage("pthread_mutex_init() failed");
				return 0;
			}	
			for ( i = 0; i < 20; i++ )
			{
				/* accountname is left empty, strlen(accountname) == 0 means empty account */
				bank->accounts[i].currentbalance = 0.0;
				bank->accounts[i].insession = 0;  
				if ( pthread_mutex_init( &bank->accounts[i].clientsession_mutex, NULL ) != 0 )
				{
					errormessage("pthread_mutex_init() failed");
					return 0;
				}	
				else if ( pthread_mutex_init( &bank->accounts[i].updateinfo_mutex, NULL ) != 0 )
				{
					errormessage("pthread_mutex_init() failed");
					return 0;
				}	
			}	
			printf("Bank initialized.\n");
			return bank;
		}
	}

}
/*
 * Initializes a bank struct.
 */
int
initBank( Bank * bank )
{
	int	i;
	bank->numaccounts = 0;
	if ( pthread_mutex_init( &bank->bankmutex, NULL ) != 0 )
	{
		errormessage("pthread_mutex_init() failed");
		return 0;
	}	
	for ( i = 0; i < 20; i++ )
	{
		/* accountname is left empty, strlen(accountname) == 0 means empty account */
		bank->accounts[i].currentbalance = 0.0;
		bank->accounts[i].insession = 0;  
		if ( pthread_mutex_init( &bank->accounts[i].clientsession_mutex, NULL ) != 0 )
		{
			errormessage("pthread_mutex_init() failed");
			return 0;
		}	
		else if ( pthread_mutex_init( &bank->accounts[i].updateinfo_mutex, NULL ) != 0 )
		{
			errormessage("pthread_mutex_init() failed");
			return 0;
		}	
	}	
	printf("Bank initialized.\n");
	return 0;
}

/*
 * Prints information regarding all open bank accounts.
 */
void
printBank( Bank * bank )
{
	int	i;
	pthread_mutex_lock( &bank->bankmutex ); //Printing bank, lock.
	if ( bank->numaccounts == 0 )
	{
		printf("There are no open accounts at the moment.\n");
	}
	else
	{
		/* loop through and lock update info every account */
		for( i = 0; i < bank->numaccounts; i++ )
		{
			//Printing info, lock accounts from updating.
			pthread_mutex_lock(&bank->accounts[i].updateinfo_mutex);
		}
		for( i = 0; i < bank->numaccounts; i++ )
		{
			accountprint(&bank->accounts[i]);
		}

		/* loop through and unlock update info every account */
		for( i = 0; i < bank->numaccounts; i++ )
		{
			//Done printing info, unlock accounts for updating.
			pthread_mutex_unlock(&bank->accounts[i].updateinfo_mutex);
		}
	}
	pthread_mutex_unlock( &bank->bankmutex ); //Done printing, unlock.
}

/*
 * Opens a bank account with the given name.
 * If bank is full or name already exists, return -1.
 *
 * Returns 0 on success.
 */
int
openaccount( char * name)
{
	int	i;
	if ( bank->numaccounts == 20 )
	{
		printf("Could not create account: Bank is full.\n");
		return -1;
	}
	else
	{
		for ( i = 0; i < bank->numaccounts; i ++ )
		{
			if ( strcmp(bank->accounts[i].accountname, name) == 0 )
			{
				printf("An account with that name already exists.\n");
				return -2;
			}
		}
		
		pthread_mutex_lock( &bank->bankmutex ); //Adding account, lock.
		if ( strncpy(bank->accounts[bank->numaccounts].accountname, name, 100) == NULL )
		{
			errormessage("Could not create account");
			return -3;
		}
		else
		{
			printf("Account %d: %s successfully created.\n", (bank->numaccounts + 1), name);
			bank->numaccounts++;
		}
		pthread_mutex_unlock( &bank->bankmutex ); //Done adding, unlock.
	}
	return 0;
}

/* Given a name, returns the ID (or index) of the account.
 *
 * Returns -1 if not found.
 */
int
getIDfromname( char * accountname )
{
	int	i;
	if (accountname == NULL)
	{
		return -1;
	}	
	else
	{
		for ( i = 0; i < bank->numaccounts; i++ )
		{
			if ( strcmp(bank->accounts[i].accountname, accountname) == 0 )
			{
				return i;
			}
		} 
//		printf("Account does not exist.\n");
		return -1;
	}
}

/*
 * Credits the bank account with the given amount.
 *
 * Returns 0 on success, -1 otherwise.
 */
int
creditaccount( float amount, char * accountname )
{
	int i;
	
	if ( (i = getIDfromname(accountname)) == -1 )
	{
		return -1;
	}
	else if ( amount < 0 )
	{
		printf("Cannot credit a negative amount.\n");
		return -1;
	}
	else
	{
		pthread_mutex_lock( &bank->accounts[i].updateinfo_mutex );
		bank->accounts[i].currentbalance += amount;
		printf("Credit successful, current balance: %.2f\n", bank->accounts[i].currentbalance);
		pthread_mutex_unlock( &bank->accounts[i].updateinfo_mutex );
	}
	return 0;
}

/*
 * Debits the bank account with the given amount.
 */
int
debitaccount( float amount, char * accountname )
{
	int i;
	
	if ( (i = getIDfromname(accountname)) == -1 )
	{
		return -1;
	}
	else if ( amount > bank->accounts[i].currentbalance )
	{
		printf("Insufficient funds.\n");
		return -2;
	}
	else
	{
		pthread_mutex_lock( &bank->accounts[i].updateinfo_mutex );
		bank->accounts[i].currentbalance -= amount;
		printf("Debit successful, current balance: %.2f\n", bank->accounts[i].currentbalance);
		pthread_mutex_unlock( &bank->accounts[i].updateinfo_mutex );
	}
	return 0;
}

/*
 * Returns the current balance for the given bank account.
 */
float
accountbalance( char * accountname )
{
	int i;
	
	if ( (i = getIDfromname(accountname)) == -1 )
	{
		return -1;
	}
	else
	{
		pthread_mutex_lock( &bank->accounts[i].updateinfo_mutex );
		printf("Current balance for %s: %.2f\n", accountname, bank->accounts[i].currentbalance);
		pthread_mutex_unlock( &bank->accounts[i].updateinfo_mutex );
		return bank->accounts[i].currentbalance;
	}
	return 0;
}

/***************************************************************************/
/* THREADS								   */
/***************************************************************************/

/*
 * Thread that prints all bank accounts every 20 seconds.
 */
void *
printaccounts_thread( void * ignore )
{
	pthread_detach( pthread_self() );
	while(1)
	{
		printBank(bank);
		sleep(20);
	}
}

/*
 * Client session thread. Argument is pointer to socket descriptor.
 *
 * This thread fork()s for every connection.
 */
void *
client_service_thread( void * sdptr )
{
	int			sd, rv, id;
	float		balance;
	char			  argument[256];
	char			  currAccount[100];
	char			  balancefloat[100];
	char			  errorstatement[60];
	int			asflag;
	//char			* func = "client service thread";

	pthread_detach( pthread_self() ); // don't wait for me

	strcpy(errorstatement,"There was an error in your request. Please try again");

	sd = *(int *) sdptr; // get that argument
	free(sdptr); // covenant

	id = 0;
	rv = 0;
	asflag = 0;

	bzero( argument, sizeof(argument));
	bzero( currAccount, sizeof(currAccount));
	bzero( balancefloat, sizeof(balancefloat));

	printf("Connection established\n");
	bzero(buff, sizeof(buff));
	write(sd, "Enter command: ", sizeof("Enter command: "));
	while(read(sd,buff,sizeof(buff)) != 0)
	{
		bzero( argument, sizeof(argument));
		write(1, "client entered:", sizeof("client entered:"));
		write(1, buff, sizeof(buff));
		rv = parseBuffer( buff, argument );
		switch (rv)
		{
			case 0: // open account - requires argument
				if( asflag != 1 )
				{
					if( ( id = openaccount( argument ) ) == -1 )
					{
						write(sd, "Could not create account: Bank is full.\n", sizeof("Could not create account: Bank is full.\n"));
					}
					else if( id == -2)
					{
						write(sd, "An account with that name already exists.\n", sizeof("An account with that name already exists.\n"));
					}
					else if( id == -3)
					{
						write(sd, "Could not create account", sizeof("Could not create account"));
					}
					else
					{
						write(sd, "Account successfully opened for: ", sizeof("Account successfully opened for: "));
						write(sd, argument, sizeof(argument));
						write(sd, "\n", sizeof("\n"));						
					}
			//		free(argument);
				}
				else
				{
					printf("Currently in session\n");
					write(sd, "Account currently in session\n", sizeof("Account currently in session\n"));
					write(sd, "\n", sizeof("\n"));
				}
				break;
			case 1: // start account - requires argument, sets account started flag.
				if( asflag != 1 )
				{
					if( ( id = getIDfromname( argument ) ) == -1)
					{
						write(sd, "Account does not exist.\n", sizeof("Account does not exist.\n"));
					}
					else
					{
						//if( bank->accounts[id].insession == 1){
							//
						//	while(bank->accounts[id].insession == 1)
							while (pthread_mutex_trylock(&bank->accounts[id].clientsession_mutex) != 0)
							{
							printf("Currently in session\n");
							write(sd, "Account currently in session\n", sizeof("Account currently in session\n"));
							sleep(2);
							write(sd, "Trying to connect again\n", sizeof("Trying to connect again\n"));
							}
						//}

						asflag = 1;
						strcpy(currAccount , argument);
				//		currAccount[(strlen(argument))] = "\0";
						
						bank->accounts[id].insession = 1;

						printf("Session starting for: \n");
						write(sd, "Session starting for: ", sizeof("Session starting for: "));
						write(sd, argument, sizeof(argument));
						write(sd, "\n", sizeof("\n"));						
					}

			//		free(argument);
				}	
				else
				{
					printf("Currently in session\n");
					write(sd, "Account currently in session\n", sizeof("Account currently in session\n"));
					write(sd, "\n", sizeof("\n"));
				}	
				break;
			case 2: // credit account - requires argument and account started flag.
				if( asflag != 1 )
				{
					printf("Need to be in session\n");
					write(sd, "Account must be in session first\n", sizeof("Account must be in session first\n"));
					write(sd, "\n", sizeof("\n"));
				}
				else
				{
					if( (id = creditaccount( atof( argument ), currAccount ) ) == -1 )
					{
						write(sd, "Crediting went wrong\n", sizeof( "Crediting went wrong\n" ));
					}
					else
					{
						printf("Crediting account\n");
						write(sd, "Crediting account: $", sizeof("Crediting account: $"));
						write(sd, argument, sizeof(argument));
						write(sd, "\n", sizeof("\n"));
					}
			//		free(argument);
				}
				break;
			case 3: // debit account - requires argument and account started flag.
				if( asflag != 1 )
				{
					printf("Need to be in session\n");
					write(sd, "Account must be in session first\n", sizeof("ccount must be in session first\n"));
					write(sd, "\n", sizeof("\n"));
				}
				else
				{
					if( (id = debitaccount( atof( argument ), currAccount ) ) == -1 )
					{
						write(sd, "Debiting went wrong\n", sizeof( "Debiting went wrong\n" ));
						write(sd, "\n", sizeof("\n"));
					}
					else if(id == -2)
					{
						write(sd, "Insufficient funds.\n", sizeof("Insufficient funds.\n"));
						write(sd, "\n", sizeof("\n"));
					}
					else
					{
						printf("Debiting account\n");
						write(sd, "Debiting account: $", sizeof("Debiting account: $"));
						write(sd, argument, sizeof(argument));
						write(sd, "\n", sizeof("\n"));						
					}					
			//		free(argument);
				}
				break;
			case 4: // account balance - requires argument and account started flag.
				if( asflag != 1 )
				{
					printf("Need to be in session\n");
					write(sd, "Account must be in session first\n", sizeof("ccount must be in session first\n"));
					write(sd, "\n", sizeof("\n"));
				}
				else
				{
					if( (balance = accountbalance( currAccount ) ) == -1)
					{
						write(sd, "Checking account balance went wrong\n", sizeof("Checking account balance went wrong\n") );
					}
					else
					{
						printf("Printing account balance\n");
						write(sd, "Printing account balance: $", sizeof("Printing account balance: $"));
						sprintf(balancefloat,"%.2f", balance);
						write(sd, balancefloat, sizeof(balancefloat));
						write(sd, "\n", sizeof("\n"));
						bzero(balancefloat, sizeof(balancefloat));						
					}

				}
				break;
			case 5: // finish - requires acount started flags, resets flag.
				if( asflag != 1 )
				{
					printf("Need to be in session\n");
					write(sd, "Account must be in session first\n", sizeof("ccount must be in session first\n"));
					write(sd, "\n", sizeof("\n"));
				}
				else
				{
					if( ( id = getIDfromname( currAccount ) ) == -1)
					{
						write(sd, "Something went wrong with finish\n", sizeof("Something went wrong with finish\n"));
						write(sd, "\n", sizeof("\n"));
					}
					else
					{
						if ( pthread_mutex_unlock( &bank->accounts[id].clientsession_mutex ) != 0 )
						{
							write(sd,"pthread_mutex_unlock() failed\n", sizeof("pthread_mutex_unlock() failed\n"));
							write(sd, "\n", sizeof("\n"));
							return 0;
						}
						else
						{ 
							bank->accounts[id].insession = 0;
							printf("Ending session now\n");
							write(sd, "Ending session now\n", sizeof("Ending session now\n"));
							write(sd, "\n", sizeof("\n"));
							asflag = 0;
							bzero(currAccount, sizeof(currAccount));							
						}
						
					}
				}
				break;
						

			case 6: // exit - can be called whenever, writes 0 to client.
		/*		if ( phtread_mutex_unlock( &bank->accounts[id].clientsession_mutex ) != 0 )
				{
					write(sd,"pthread_mutex_unlock() failed", sizeof("pthread_mutex_unlock() failed"));
					return 0;
				}	 */			
				if( ( id = getIDfromname( currAccount ) ) != -1)
				{
					//Calling exit while inside a session
					bank->accounts[id].insession = 0;
					printf("Ending session now\n");
					write(sd, "Ending session now\n", sizeof("Ending session now\n"));
					asflag = 0;
					bzero(currAccount, sizeof(currAccount));
				}
				write(sd, "Exiting. Thank you for using the bank of JuJu\n", sizeof("Exiting. Thank you for using the bank of JuJu\n"));
				exit(0);			
			default: // error, report back to client
//				write(sd, errorstatement, sizeof(buff));
				write(sd, "There was an error processing your request\n", sizeof("There was an error processing your request\n"));
				write(sd, "\n", sizeof("\n"));
				break;
		}
		bzero(buff,sizeof(buff));
		write(sd, "Enter command: ", sizeof("Enter command: "));
	}	
		bzero(buff,sizeof(buff)); 
	exit(0);	

}

/*
 * Fork() thread.  This fork()ing thread is responsible for fork()ing only.
 *
 * This thread fork()s for every connection.
 */
void *
forking_thread( void * fdptr )
{
	int			fd;
	int			* sdptr;
	pid_t			pid;
	pthread_t		tid;
	//char			* func = "client service thread";

	pthread_detach( pthread_self() ); // don't wait for me

	fd = *(int *) fdptr; // get that argument
	free(fdptr); // covenant

	if( (pid = fork()) == -1 )
	{
		errormessage("fork() failed");
		return 0;
	}
	else if( pid == 0 )
	/*** CHILD PROCESS ***/
	{
//		printf("CHILD: Child process created with PID: %d\n", getpid());
//		printf("CHILD [PID - %d]: Parent process with [PID - %d]\n", getpid(), getppid());
		/* Spawn a client-session thread and then exit this thread? */
//		printf("CHILD: I should be doing client stuff blah blah\n");
		if ( (sdptr = (int *) malloc(sizeof(int))) == NULL )
		{
			errormessage("malloc() failed");
			return 0;
		}
		else if ( *sdptr = fd, pthread_create(&tid, &kernel_attr, client_service_thread, sdptr) != 0 )
		{
			errormessage("pthread_create() failed");
			return 0;
		}
		else
		{
		//	printf("CHILD: client_service_thread successfully created\n");
			pthread_exit(0);
		}
	}
	else
	/*** PARENT PROCESS ***/
	{
//		printf("PARENT: Parent process with PID: %d\n", getpid());
		printf("[PID - %d]: Created child process with [PID - %d]\n", getpid(), pid);
		close(fd);
//		printf("PARENT: Exiting this thread now\n");
		pthread_exit(0);
		/* Spawn a new thread to wait for child process */
	}
	return 0;
}

/*
 * Session acceptor thread.
 *
 * This thread waits for incoming connections.
 * Every connection established spawns a client session thread.
 */
void *
session_acceptor_thread( void * ignore )
{
	struct addrinfo		hints,
				* result;
	struct sockaddr_in	senderAddr;
	int			sockfd, fd, on, error;
	int 			*fdptr;
	socklen_t		size;
	pthread_t		tid;
	//char			* func = "session acceptor thread";

	pthread_detach( pthread_self() );
	init_addrinfo(&hints);
	on = 1;

	if ( (error = getaddrinfo(NULL, PORT_NUMBER, &hints, &result)) != 0 )
	{
		errormessage("getaddrinfo() failed");
		return 0;
	}
	else if ( (sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == -1 )
	{
		errormessage("socket() failed");
		return 0;
	}
	else if ( (error = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) != 0 )
	{
		errormessage("setsockopt() failed");
		return 0;
	}
	else if ( (error = bind(sockfd, result->ai_addr, result->ai_addrlen)) != 0 )
	{
		errormessage("bind() failed");
		return 0;
	} 
	else if ( (error = listen(sockfd, 100)) != 0 )
	{
		errormessage("listen() failed");
		return 0;
	}
	else
	{
		size = sizeof(senderAddr);
		printf("Bank server is now waiting for connections...\n");
		while( (fd = accept(sockfd, (struct sockaddr *) &senderAddr, &size)) != -1)
		{
			if ( fd < 0 )
			{
				errormessage("accept() failed");
				return 0;
			}
			else
			{
				fdptr = (int *)malloc(sizeof(int));
				*fdptr = fd;	
				printf("======================\n");
				printf("Connection established\n");
				printf("======================\n");
				if( pthread_create( &tid, &kernel_attr, forking_thread, fdptr) != 0 )
				{
					errormessage("pthread_create() failed");
					return 0;
				}
				else	
				{
				//	printf("client_service_thread created\n");
				}
			}
		}
	}
	return 0;
}

/***************************************************************************/
/* MAIN									   */
/***************************************************************************/

int
main()
{
	pthread_t		tid;
	//char			* func = "server main";

	/* Initialize signal handlers */
	init_sighandlers();

		/*** Real main stuff ***/
	if( (bank = initshmBank()) == NULL )
	{
		errormessage("Failed to inittialize bank");
		return 0;
	}
	else if( pthread_attr_init( &kernel_attr ) != 0 )
	{
		errormessage("pthread_attr_init() failed");
		return 0;
	}	
	else if( pthread_attr_setscope( &kernel_attr, PTHREAD_SCOPE_SYSTEM) != 0)
	{
		errormessage("pthread_attr_setscope() failed");
		return 0;
	}	
	else if ( pthread_create( &tid, &kernel_attr, session_acceptor_thread, 0) != 0)
	{
		errormessage("pthread_create() failed");
		return 0;
	}
	else if ( pthread_create( &tid, &kernel_attr, printaccounts_thread, 0) != 0)
	{
		errormessage("pthread_create() failed");
		return 0;
	}
	else
	{
		printf("Thread successfully created.\n");
		pthread_exit(0);
	}
		
	printf("Normal end\n");
	return 0;
}
