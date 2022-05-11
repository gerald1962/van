@ Here is the experimental prototype of the Van unit test system.

@c
@<Header files@>@/
@<The main program@>

@ License \hfil\break
The GNU General Public License (GPL) is designed for engineers shipping
products with GPL-licensed software:
\vskip 8pt
\noindent {\tt SPDX-License-Identifier: GPL-2.0} \hfil\break
Van unit test system. \hfil\break
Copyright (C) 2022 Gerald Schueller {\tt <gerald.schueller@@web.de>}

@ We include declarations from Van OS - os.h - and Unix.
Below there is some evidence, why we need these interfaces with
justification.

@<Header files@>=
#include "os.h"
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

@ Header file grounds:
\vskip 8pt
\halign{\indent\tt # \hfil&\quad # \hfil\cr
os.h      & Van Operating system: printf(). \cr
stddef.h  & Standard type definitions: offsetof(). \cr
stdbool.h & C99 allows bool. true is 1 and false is 0. \cr
errno.h   & OS interface errors like ENOMEM=12 or EFAULT=14. \cr
limits.h  & Sizes of integral or integer types: INT\_MIN. \cr}

@ Constant Definitions.
\vskip 8pt
@d P "U>"
@d POISON_POINTER_DELTA  0
@d LIST_POISON1  ((void *) 0x100 + POISON_POINTER_DELTA)
@d LIST_POISON2  ((void *) 0x200 + POISON_POINTER_DELTA)
@d __noreturn  __attribute__((__noreturn__))

@d __must_check  __attribute__((__warn_unused_result__))

@d REFCOUNT_SATURATED  (INT_MIN / 2)
@d KUNIT_LOG_SIZE	512

@d KUNIT_STATUS_COMMENT_SIZE  256

@d KUNIT_PARAM_DESC_SIZE  128

@d KUNIT_SUBTEST_INDENT     "    "
@d KUNIT_SUBSUBTEST_INDENT  "        "

@d KUNIT_CURRENT_LOC { .file = __FILE__, .line = __LINE__ }

@ Constants Explanations.
\vskip 8pt
\noindent {\sl P:} \hfil\break\indent
Van unit prompt. \hfil\break

\noindent {\sl POISON\_POINTER\_DELTA:} \hfil\break\indent
Architectures might want to move the poison pointer offset
into some well-recognized area such as \hfil\break\indent
0xdead000000000000, that is also not mappable by user-space exploits. \hfil\break

\noindent {\sl LIST\_POISON1:} \hfil\break
\noindent {\sl LIST\_POISON2:} \hfil\break\indent
These are non-NULL pointers that will result in page faults
under normal circumstances, used to verify \hfil\break\indent
that nobody uses non-initialized list entries. \hfil\break

@ Now we come to the layout of the |main| function. 
@<The main...@>=
void main (void)
{
	printf("Hello.\n");
}

@* Index.
Here is the list of the identifiers used.
