@ Knuth means, that a programmer at any time is able to understand,
what you do and therefore it should be able to left ideas
as comments except you get low. I refere to "Literate Programming", see
{\tt knuth\_lit.pdf}: "Instead of imagining that our
main task is to instruct a computer what to do, let us
concentrate rather on explaining to human beings what
we want a computer to do."

@ The next few sections contain stuff from the file |"lcommon.w"| that must
be included in both |"lit.w"| and |"log.w"|. It appears in
file |"lcommon.h"|, which is also included in |"lcommon.w"| to propagate
possible changes from this \.{COMMON} interface consistently.

@i lextern.h

@ \.{LIT} has a fairly straightforward outline.  It operates in
three phases: First it inputs the source file and stores cross-reference
data, then it inputs the source once again and produces the \TEX/ output
file, finally it sorts and outputs the index.

\vskip 2pt
@ {\sl main()} - is called by the C library by recognizing the in-built keyword
{\sl main}. The way for running another program on \.{Linux} involves first
calling {\sl fork()}, which creates a new process as a copy of the first one,
and then calling {\sl exec()} to replace this copy (of the shell) with the
actual program to run. 

\vskip 2pt \noindent
Richie and Kerninghan write: "$\ldots$ {\sl main} is a special function. Our program
begins executing at the beginning of {\sl main}. This means that every program
must have a {\sl main} somewhere and will usually call other functions to help perform its
job."

\vskip 2pt \noindent
In the \.{C99} standard is defined: "The function called at program startup is
named {\sl main}. $\ldots$ It shall be defined with a return type of {\bf int} and
$\ldots$ or with two parameters (referred to here {\sl ac} and {\sl av})."

\vskip 2pt
\decl 1 3.25 {\sl ac:} { \rm If the value of {\sl ac} is greater than zero, the
array members $av[0]$ through $av[ac-1]$ inclusive shall contain pointers to
strings, which are given by the host environment prior to program startup. }

\decl 1 3.25 {\sl av:} { \rm If the value of {\sl ac} is greater than zero, the
string pointed to by $av[0]$ represents the program name. If the value of
{\sl ac} is greater than one, the strings pointed to by $av[1]$ through
$av[ac-1]$ represent the program parameters. }

\vskip 2pt
\decl 1 3.25 {\bf return} { \rm $\ldots$ from the initial call to the
{\sl main} function is equivalent to calling the {\sl exit} function with the
value returned by the {\sl main} function as its argument; reaching the end of
{\sl main} $\rbrace$ that terminates the {\sl main} function returns a value
compatible with \bf int. }

@c
void main(int ac, char **av)
{
        printf("Hi Herbert and Renate.\n");
}

@* Index.
