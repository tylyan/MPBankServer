/*
 * errormessage.c
 * Author:	Emmanuel Baah
 * 		Yuk Yan
 */
#include "errormessage.h"

/*
 * Print out an error message for the given file and line number
 */
void
errormessage_( char * message, const char * file, int line )
{
	if ( message == NULL )
	{
		printf("\x1b[1;31mError in file %s line %d: Unspecified\n\x1b[0m", file, line);
	}
	else
	{
		printf("\x1b[1;31mError in file %s line %d: %s\n\x1b[0m", file, line, message);
	}
}
