@ Literate Programming @.\it{Literate Programming}@> \hfill \break
Literate programming is a programming paradigm introduced by Donald Knuth in
which a computer program is given an explanation of its logic in a natural
language, such as English, see https://en.wikipedia.org/wiki/Literate_programming

\vskip 4pt \noindent
Knuth means, that a programmer at any time is able to understand,
what you do and therefore it should be able to lef t ideas
as comments except you get low. I refere to "Literate Programming", see
{\tt knuth\_lit.pdf}: "Instead of imagining that our
main task is to instruct a computer what to do, let us
concentrate rather on explaining to human beings what
we want a computer to do."

Und hier sind meine Gedanken:
Und zudem soll zu jdem x-beliebigen Zeitpunkt
der Autor, der erste Verantwortliche, mir erklären, was jedes Statement bedeutet oder ein Nacherfolger, der n-Verantwortliche, für den die Vorgänger-Bedingung gilt.

@ Generator @.\it{C-Generator}@> \hfill \break
The {\bf ctangle} program converts the {\bf CWEB} source document {\tt hello.w}
into the {\bf C} program {\tt hello.c} that may be compiled in the usual way.
The output file includes{ \tt \#line} specifications so that debugging can be
done in terms of the {\bf CWEB} source file, see {\tt man ctangle}.

\vskip 4pt \noindent
The command line should have two names on it. The first is {\bf ctangle} and the
second is taken as the {\bf CWEB} input file {\tt hello.w}.

\vskip 4pt \noindent
{\tt ctangle hello.w}

@ Compilation \hfill \break
Compiling {\sl main} or a C program is a multi-stage process. At an overview
level, the process can be split into four separate stages: preprocessing,
compilation, assembly, and linking, \hfill \break
see {\tt https://2150.medium.com/what-gcc-main-c-means-3d5e5e3e4d76}:

\vskip 4pt \noindent
I'll walk through each of the four stages of compiling the C program {\bf lit}:

@ Preprocessor @.\it{Preprocessor}@> \hfill \break
The first stage of compilation is called preprocessing. In this stage,
lines starting with a \# character are interpreted by the preprocessor as
preprocessor commands. These commands form a simple macro language with its own
syntax and semantics. This language is used to reduce repetition in source code
by providing functionality to inline files, define macros, and to conditionally
omit code.

\vskip 4pt \noindent
Before interpreting commands, the preprocessor does some initial processing.
his includes joining continued lines (lines ending with a \) and stripping comments.

\vskip 4pt \noindent
To print the result of the preprocessing stage, pass the {\sl -E} option to {\bf gcc}:

\vskip 4pt \noindent
{\tt gcc -E hello.c}

@ Compilation  @.\it{Compilation}@> \hfill \break
The second stage the preprocessed code is translated to assembly instructions
specific to the target processor architecture. These form an intermediate human
readable language.

\vskip 4pt \noindent
The existence of this step allows for C code to contain inline assembly
instructions and for different assemblers to be used.

\vskip 4pt \noindent
Some compilers also supports the use of an integrated assembler, in which the
compilation stage generates machine code directly, avoiding the overhead of
generating the intermediate assembly instructions and invoking the assembler.

\vskip 4pt \noindent
To save the result of the compilation stage, pass the {\sl -S} option to {\bf gcc}:

\vskip 4pt \noindent
{\tt gcc \-S hello.c}

@ Assembler  @.\it{Assembler}@> \hfill \break
During this stage, an assembler is used to translate the assembly instructions
to object code. The output consists of actual instructions to be run by the target processor.

\vskip 4pt \noindent
To save the result of the assembly stage, pass the {\sl -c} option to {\bf gcc}:

\vskip 4pt \noindent
{\tt gcc \-c hello.c}

\vskip 4pt \noindent
Running the above command will create a file named {\tt hello.o}, containing the
object code of the program. The contents of this file is in a binary format and
an be inspected using hexdump or od by running either one of the following commands:

\vskip 4pt \noindent
{\tt 
hexdump hello.o
od \-c hello.o
}

@ Linking  @.\it{Linking}@> \hfill \break
The object code generated in the assembly stage is composed of machine
instructions that the processor understands but some pieces of the program are
out of order or missing. To produce an executable program, the existing pieces
save to be rearranged and the missing ones filled in. This process is called
linking. The linker will arrange the pieces of object code so that functions in
some pieces can successfully call functions in other ones. It will also add
pieces containing the instructions for library functions used by the program.
In the ase of the {\bf hello} program, the linker will add the object code for
the {\sl printf} function.

\vskip 4pt \noindent
The result of this stage is the final executable program.

\vskip 4pt \noindent
{\tt gcc \-o hello hello.c}

@ User header \hfill \break
The next few sections contain stuff from the file |"lextern.w"| that must
be included in both |"lit.w"| and |"log.w"|. It appears in
file |"lextern.h"|, which is also included in |"lextern.w"| to propagate
possible changes from this \.{EXTERN} interface consistently.

@i lextern.h

@ Program \hfill \break
\.{LIT} has a fairly straightforward outline.  It operates in
three phases: First it inputs the source file and stores cross-reference
data, then it inputs the source once again and produces the \TEX/ output
file, finally it sorts and outputs the index.

\vskip 2pt
@  Main function \hfill \break
A function definition has this form, see {\bf KR}:Rechercheb
Rechercheb
 \vskip 2pt \noindent
{\tt
return-type function-name(parameter declarations, or void) \hfill \break
$\lbrace$ \hfill \break
\indent declarations \hfill \break
\indent statements \hfill \break
$\rbrace$
}

\vskip 4pt \noindent
The function {\sl main} is called by the operation system. Each call passes two
arguments to main, which each time returns an integer.

\vskip 2pt \noindent
{\sl main()} - The function {\sl main} is called by the operation system. Each call passes two
arguments to main, which each time returns an integer.

\vskip 2pt \noindent    
The way for running another program on \.{Linux} involves first
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

\item{1} xxx
\item{x} yyy

@c
void main(int ac, char **av)
{
        argc = ac; /* 1 */
        argv = av; /* 2 */
        
        printf("Hi Herbert and Renate.\n");
}

@* Index.
