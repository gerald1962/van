% wc: An example of CWEB-

@ Here is the file \.{hello.c}.

@c
@<Header files@>@/
@<The main program@>

@ We include the standard I/O declarations, since we send output to |stdout|.

@<Header files@>=
#include <stdio.h>

@ Now we come to the layout of the |main| function. 
@<The main...@>=
void main (void)
{
	printf("Hello.\n");
}

@* Index.
Here is the list of the identifiers used.
