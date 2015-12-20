#ifndef BANKSERVER_H
#define BANKSERVER_H
#define errormessage(x) errormessage_(x, __FILE__, __LINE__)
/*
 * bankserver.h
 */
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include "errormessage.c"
#include "bankaccount.c"

struct Bank_{
	int			numaccounts;
	Account			accounts[20];
	pthread_mutex_t		bankmutex;
};
typedef struct Bank_ Bank;

/*
 * Initializes a bank struct.
 */
int
initBank( Bank * bank );

/*
 * Prints the information regarding all open bank accounts.
 */
void
printBank( Bank * bank );
/*
 * Opens a bank account with the given name.
 * If bank is full or name already exists, return -1.
 *
 * Returns 0 on success.
 */
int
openaccount( char * name );

/*
 * Given a name, returns the ID (or index) of the account.
 *
 * Returns -1 if not found.
 */
int
getIDfromname( char * accountname );

/*
 * Credits the bank account with the given amount.
 */
int
creditaccount( float amount, char * accountname );

/*
 * Debits the bank account with the given amount.
 */
int
debitaccount( float amount, char * accountname );

/*
 * Returns the current balance for the given bank account.
 */
float
accountbalance( char * accountname );

/*
 * Parses the buffer and populates pointers as needed
 */
int
parseBuffer( char* buff , char * argument);


#endif
