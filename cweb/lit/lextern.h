% Type styles: see texbook.pdf
% \bf use boldface type
% \it use italic type
% \rm use roman type
% \sl use slanted type
% \tt use typewriter type

% \def
% There’s a special way of making a macro definition global. Normally you define
% a macro using either the \def command ...
%
% \par
% This command ends a paragraph and puts TEX into vertical mode, ready
% to add more items to the page.
%
% \parindent [ <token-list> parameter ]
% This parameter specifies the amount by which the first line of each paragraph
% is to be indented.
%
% \hangindent [ <dimen> parameter]
% These two parameters jointly specify “hanging indentation” for a paragraph.
% The hanging indentation indicates to TEX that certain lines of the paragraph
% should be indented
% 
% \textindent like \item, but doesn’t do hanging indentation.

\def\decl #1 #2 { \par \hskip #1 pt
                  \hangindent #2 \parindent}

@ Substitution \hfill \break
Ritchie writes in "The Development of the C Language", see
{\tt c\_development.html}: "$\ldots$ but the most important was the introduction
of the preprocessor $\ldots$ The preprocessor performs macro substitution, using
conventions distinct from the rest of the language. $\ldots$"

\vskip 2pt \noindent
Here's what Wittgenstein says in the {\bf TLP},  see {\tt tlp.pdf}: "6.24 The
method by which mathematics arrives at its equations is the method of
substitution. For equations express the substitutability of two expressions, and
we proceed from a number of equations to new equations, replacing expressions by
others in accordance with the equations."

\vskip 2pt \noindent
Stallman defines {\sl header file} in "The C Preprocessor", see {\tt cpp.pdf}:
A {\sl header file} is a file containing C declarations and macro definitions
(see Chapter 3 [Macros], page 13) to be shared between several source files. You
request the use of a header file in your program by including it, with the C
preprocessing directive {\bf \#include}.

@ System Header \hfill \break
The system header {\tt <stdio.h>} declares three types, several macros, and
many functions for performing input and output, see {\bf ISO/IEC 9899:TC3}.

\vskip 2pt \noindent
To use the {\sl printf()} function we must include {\tt <stdio.h>}.
{\sl printf} is the name of one of the main C output functions, and stands for
"print formatted". The {\sl printf} format string is a control parameter used by
a class of functions in the input/output libraries of C and many other
programming languages. The string is written in a simple template language:
characters are usually copied literally into the function's output, but format
specifiers, which start with a % character, indicate the location and method to
translate a piece of data (such as a number) to characters, see
{\tt https://en.wikipedia.org/wiki/Printf\_format\_string}.

\vskip 2pt \noindent
The output function {\sl printf} translates internal values to characters:

\vskip 2pt \noindent
{\tt int printf(char *format, arg1, arg2, ...); }

\vskip 2pt \noindent
{\sl printf} converts, formats, and prints its arguments on the
format. It returns the number of characters printed, see {\bf KR}.

@c
#include <stdio.h>

@ External variables \hfill \break
Because external variables are globally accessible, they can be used instead of
argument lists to communicate data between functions. Furthermore, because
external variables remain in existence permanently, rather than appearing and
disappearing as functions are called and exited, they retain their values even
ater the functions that set them ve returned, see {\bf KR}.

Declaration of the global variables or function simply declares that the
variable or function exists, but the memory is not allocated for them.

\vskip 2pt
\decl 1 3.25 {\sl argc:} copy of |ac| parameter to |main|.

\decl 1 3.25 {\sl argv:}  copy of |av| parameter to |main|

@c
extern int argc;
extern char **argv;
