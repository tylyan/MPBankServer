#ifndef ERRORMESSAGE_H
#define ERRORMESSAGE_H
/*
 * errormessage.h
 */
#include <stdio.h>
#include <stdlib.h>

/*
 * Print out an error message for the given file and line number
 */
void
errormessage_( char * message, const char * file, int line );


#endif
