#ifndef BANKACCOUNT_H
#define BANKACCOUNT_H
/*
 * bankaccount.h
 */
#include <stdlib.h>
#include <pthread.h>

/*
 * A struct representing a bank account
 */
struct Account_ {
	char			accountname[100];
	float			currentbalance;
	unsigned int		insession:1;	
	pthread_mutex_t		clientsession_mutex;
	pthread_mutex_t		updateinfo_mutex;
};

typedef struct Account_ Account;

/*
 * Create an account given a name and balance.
 * insession is initialized to 0.
 */
Account *
accountcreate( char * name );

/*
 * Destroy and free the memory of a given account.
 */
void
accountdestroy( Account * account );

/*
 * Prints the information regarding the account.
 */
void
accountprint( Account * account );
#endif

