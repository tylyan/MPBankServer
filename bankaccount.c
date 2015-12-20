/*
 * bankaccount.c
 * Authors:	Emmannuel Baah
 * 		Yuk Yan
 */
#include "bankaccount.h"
#include <string.h>
#define errormessage(x) errormessage_(x, __FILE__, __LINE__)

/*
 * Create an account given a name and balance.
 * insession is initialized to 0.
 */
Account *
accountcreate( char * name )
{
	Account *	account;
	if ( (account = (Account *)malloc(sizeof(Account))) == NULL )
	{
		errormessage("Not enough memory for Account");
	}
	else
	{
		strncpy(account->accountname, name, 100);
		account->currentbalance = 0; 
		account->insession = 0;
	}
	return account;
}

/*
 * Destroy and free the memory of a given account.
 */
void
accountdestroy( Account * account )
{
	free(account);
}

/*
 * Prints the information regarding the account.
 */
void
accountprint( Account * account )
{
	char *		insession;
	if ( account == NULL )
	{
		errormessage("NULL Account");
		return;
	}
	else
	{
		if ( account->insession)
		{
			insession = "IN SERVICE";	
		}
		else
		{
			insession = "NOT IN SERVICE";
		}
		printf("-----------------------------------------------------\n");
		printf("Account name    -- %s\n", account->accountname);
		printf("Current balance -- %.2f\n", account->currentbalance);
		printf("Session status  -- %s\n", insession);
		printf("-----------------------------------------------------\n");
	}
}
