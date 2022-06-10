@ The next few sections contain stuff from the file |"lcommon.w"| that must
be included in both |"lit.w"| and |"log.w"|. It appears in
file |"lcommon.h"|, which is also included in |"lcommon.w"| to propagate
possible changes from this \.{COMMON} interface consistently.

@i lextern.h

@ \.{LIT} has a fairly straightforward outline.  It operates in
three phases: First it inputs the source file and stores cross-reference
data, then it inputs the source once again and produces the \TEX/ output
file, finally it sorts and outputs the index.

@ {\sl main()} - is called by the C library by recognizing the in-built keyword
{\sl main}. The way for running another program on \.{Linux} involves first
calling {\sl fork()}, which creates a new process as a copy of the first one,
and then calling {\sl exec()} to replace this copy (of the shell) with the
actual program to run.

Richie and Kernighan write: "\ldots {\sl main} is a special function. Our program
begins executing at the beginning of {\sl main}. This means that every program
must have a {\sl main} somewhere and will usually call other functions to help perform its
job."

In the \.{C99} standard is defined: "The function called at program startup is
named {\sl main}. \ldots It shall be defined with a return type of {\bf int} and
\ldots or with two parameters (referred to here as argc and argv)."

\vskip 4pt
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
% The code for the C address operation is 38: see Ascii table: mam ascii
\+ & \sl ptr:    & the {\sl \char38 struct list\_head} pointer. \cr
\+ & \sl type:   & the type of the struct this is embedded in. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

\vskip 4pt\indent
{\bf return} xxx.

@c
void main(void)
{
        printf("Hi Herbert and Renate.\n");
}

@* Index.
