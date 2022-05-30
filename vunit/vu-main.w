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

@ Header file list
\vskip 8pt
\halign{\indent\tt # \hfil&\quad # \hfil\cr
os.h      & Van Operating system: printf(). \cr
stddef.h  & Standard type definitions: offsetof(). \cr
stdbool.h & C99 allows bool. true is 1 and false is 0. \cr
errno.h   & OS interface errors like ENOMEM=12 or EFAULT=14. \cr
limits.h  & Sizes of integral or integer types: INT\_MIN. \cr}

@ We include declarations from Van OS - os.h - and Unix.
Above there is some evidence, why we need these interfaces with
contents hints.

@<Header files@>=
#include "os.h"
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

@ This constant defines the van unit prompt.

@d  P "U>"

@ Architectures might want to move the poison pointer offset into some
well-recognized area such as 0xdead000000000000, that is also not mappable by
user-space exploits.

@d POISON_POINTER_DELTA  0

@ These are non-NULL pointers that will result in page faults under normal
circumstances, used to verify that nobody uses non-initialized list entries.

@d LIST_POISON1  ((void *) 0x100 + POISON_POINTER_DELTA)
@d LIST_POISON2  ((void *) 0x200 + POISON_POINTER_DELTA)

@ The noreturn function specifier is used to tell to the compiler that the
function will not return anything. If the program uses some return statement
inside it, the compiler will generate compile time error.

@d __noreturn  __attribute__((__noreturn__))

@ The compiler supports the ability to diagnose when the results of a function
call expression are discarded under suspicious circumstances. A diagnostic
is generated when a function or its return type is marked with the macro below.

@d __must_check  __attribute__((__warn_unused_result__))

@ The counter saturates at REFCOUNT\_SATURATED and will not move once
there. This avoids wrapping the counter and causing \lq spurious\rq\ 
use-after-free bugs.

@d REFCOUNT_SATURATED  (INT_MIN / 2)

@ This constant defines the size of a test log line in
{\sl struct kunit $\lbrace$ \dots log; \dots $\rbrace$ }.

@d KUNIT_LOG_SIZE	512

@ This constant defines the size of the kunit status\_comment element in
{\sl struct kunit $\lbrace$ \dots status\_comment; \dots $\rbrace$ }.

@d KUNIT_STATUS_COMMENT_SIZE  256

@ This constant defines the size of the test parameter buffer in {\sl kunit\_run\_tests()},
to store them in {\sl struct kunit $\lbrace$ \dots log; \dots $\rbrace$ }.

@d KUNIT_PARAM_DESC_SIZE  128

@ TAP - \pdfURL{Test Anything Protocol}{https://en.wikipedia.org/wiki/Test_Anything_Protocol} -
specifies subtest stream indentation of 4 spaces, 8 spaces for a
sub-subtest.  See the ''Subtests'' section at the inet page
\pdfURL{TAP}{https://node-tap.org/tap-protocol}

@d KUNIT_SUBTEST_INDENT     "    "
@d KUNIT_SUBSUBTEST_INDENT  "        "

@ This constant saves the source location of a line of code, see
{\sl struct kunit\_loc}.

@d KUNIT_CURRENT_LOC { .file = __FILE__, .line = __LINE__ }

@ Now we come to the layout of the |main| function. 
@<The main...@>=
void main (void)
{
	printf("Hello.\n");
}

@ printk infrastructure

\vskip 2pt\noindent
\noindent Some subsystems have their own custom printk that applies a va\_format to a
generic format, for example, to include a device number or other metadata
alongside the format supplied by the caller.

\vskip 2pt\noindent
In order to store these in the way they would be emitted by the printk
infrastructure, the subsystem provides us with the start, fixed string, and
any subsequent text in the format string.

\vskip 2pt\noindent
We take a variable argument list as pr\_fmt/dev\_fmt/etc are sometimes passed
as multiple arguments (eg: ''\%s: '', ''blah''), and we must only take the
first one.

\vskip 2pt\noindent
subsys\_fmt\_prefix must be known at compile time, or compilation will fail
(since this is a mistake). If fmt or level is not known at compile time, no
index entry will be made (since this can legitimately happen).

@d __printk_index_emit(...)  do {} while (0)

@d printk_index_wrap(_p_func, _fmt, ...) ({				@/
		__printk_index_emit(_fmt, NULL, NULL);			@/
                _p_func(_fmt, ##__VA_ARGS__); })

@ printk()

\vskip 2pt\noindent
printk() - print a kernel message.

% Macro or function format:
\vskip 2pt\noindent
\vbox{\settabs\+\indent & fmt: \ \  & \cr % sample line
\+& \sl fmt: & format string. \cr
\+& \dots:   & arguments for the format string. \cr}

\vskip 2pt\noindent
This is printk(). It can be called from any context. We want it to work.

\vskip 2pt\noindent
If printk indexing is enabled, \_printk() is called from printk\_index\_wrap.
Otherwise, printk is simply

\vskip 2pt\noindent
\#defined to \_printk.

\vskip 2pt\noindent
We try to grab the console\_lock. If we succeed, it\rq s easy - we log the
output and call the console drivers. If we fail to get the semaphore, we
place the output into the log buffer and return. The current holder
of the console\_sem will notice the new output in console\_unlock(); and will
send it to the consoles before releasing the lock.

\vskip 2pt\noindent
One effect of this deferred printing is that code which calls printk() and
then changes console\_loglevel may break. This is because console\_loglevel
is inspected when the actual printing occurs.

\vskip 2pt\noindent
See also: 

\vskip 2pt\noindent
printf(3)

\vskip 2pt\noindent
See the vsnprintf() documentation for format string extensions over C99.

@d printk(fmt, ...)  printk_index_wrap(_printk, fmt, ##__VA_ARGS__)

@d pr_fmt(fmt) "VUnit: " fmt

@d pr_info(fmt, ...)  printk(pr_fmt(fmt), ##__VA_ARGS__)

@ WARN(),  WARN\_ON() and so on can be used to report
significant kernel issues that need prompt attention if they should ever
appear at runtime.

\vskip 2pt\noindent
Do not use these macros when checking for invalid external inputs.

@d __WARN_printf(arg...)  do { fprintf(stderr, arg); } while (0)

@d WARN(condition, format...) ({					@/
	int __ret_warn_on = !! (condition);				@/
	if (unlikely(__ret_warn_on))					@/
		printf(format);			                        @/
	unlikely(__ret_warn_on); })

@d WARN_ONCE(condition, format...)  WARN(condition, format)

@d WARN_ON(condition) ({					@/
	int __ret_warn_on = !! (condition);			@/
	if (unlikely(__ret_warn_on))				@/
		__WARN_printf("assertion failed at %s:%d\n",	@/
				__FILE__, __LINE__);		@/
	unlikely(__ret_warn_on); })

@ Don\rq t use BUG() or BUG\_ON() unless there\rq s really no way out; one
example might be detecting data structure corruption in the middle
of an operation that can\rq t be backed out of.  If the (sub)system
can somehow continue operating, perhaps with reduced functionality,
it\rq s probably not BUG-worthy.

\vskip 2pt\noindent
If you\rq re tempted to BUG(), think again: is completely giving up
really the {\bf only} solution?  There are usually better options, where
users don\rq t need to reboot ASAP and can mostly shut down cleanly.

@d BUG_ON(cond)  OS_TRAP_IF(! (cond))
@d BUG()	      BUG_ON(1)

@ Comparing types.

\vskip 2pt\noindent
Are two types/vars the same type (ignoring qualifiers)?

@d __same_type(a, b)  __builtin_types_compatible_p(typeof(a), typeof(b))

@ Multiplication with overflow.

@d check_mul_overflow(a, b, d) __must_check_overflow(({	@/
	typeof(a) __a = (a);			@/
	typeof(b) __b = (b);			@/
	typeof(d) __d = (d);			@/
	(void) (&__a == &__b);			@/
	(void) (&__a == __d);			@/
	__builtin_mul_overflow(__a, __b, __d); }))

@ The {\bf volatile} is due to gcc bugs: see
\pdfURL{Extended Asm}{https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html}

\vskip 2pt\noindent
With extended asm you can read and write C variables from assembler and
perform jumps from assembler code to C labels. Extended asm syntax uses
colons (:) to delimit the operand parameters after the assembler template.
See
\pdfURL{Assembly with C}{https://gcc.gnu.org/onlinedocs/gcc/Using-Assembly-Language-with-C.html}

\vskip 2pt\noindent
In it find recommondations:
\pdfURL{Barrier Sync}{https://stackoverflow.com/questions/19965076/gcc-memory-
                barrier-sync-synchronize-vs-asm-volatile-memory}

\vskip 2pt\noindent
The first barriers does nothing at runtime. It\rq s called a SW barrier.
The second barrier would translate into a HW barrier, probably a fence
(mfence/sfence) operations if you\rq re on x86.
This instruction tells the micro code of a CPU network to make sure that
loads or stores can\rq t pass this point and must be observed in the correct
side of the sync point.

@d barrier()  __asm__ __volatile__("": : :"memory")

@ Prevent the compiler from merging or refetching reads or writes. The
compiler is also forbidden from reordering successive instances of
READ\_ONCE and WRITE\_ONCE, but only when the compiler is aware of some
particular ordering. One way to make the compiler aware of ordering is to
put the two invocations of READ\_ONCE or WRITE\_ONCE in different C
statements.

\vskip 2pt\noindent
These two macros will also work on aggregate data types like structs or
unions. If the size of the accessed data type exceeds the word size of
the machine (e.g., 32 bits or 64 bits) READ\_ONCE() and WRITE\_ONCE() will
fall back to memcpy and print a compile-time warning.

\vskip 2pt\noindent
Their two major use cases are: (1) Mediating communication between
process-level code and irq/NMI handlers, all running on the same CPU,
and (2) Ensuring that the compiler does not fold, spindle, or otherwise
mutilate accesses that either do not require ordering or that interact
with an explicit memory barrier or atomic instruction that provides the
required ordering.

@d READ_ONCE(x) ({ 			                @/
	union { typeof(x) __val; char __c[1]; } __u =	@/
		{ .__c = { 0 } };			@/
	__read_once_size(&(x), __u.__c, sizeof(x));	@/
	__u.__val;					@/
})

@d WRITE_ONCE(x, val) ({				@/
	union { typeof(x) __val; char __c[1]; } __u =	@/
		{ .__val = (val) }; 			@/
	__write_once_size(&(x), __u.__c, sizeof(x));	@/
	__u.__val;					@/
})

@ Print a refcount warning like ''underflow; use-after-free''.

@d REFCOUNT_WARN(str)  WARN_ONCE(1, "refcount_t: " str ".\n")

@ Built-in Function: {\sl long \_\_builtin\_expect (long exp, long c)}

\vskip 2pt\noindent
See
\pdfURL{Builtin\_Expect}{https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html}

\vskip 2pt\noindent
You may use \_\_builtin\_expect to provide the compiler with branch prediction
information. In general, you should prefer to use actual profile feedback for
this (-fprofile-arcs), as programmers are notoriously bad at predicting how
their programs actually perform. However, there are applications in which
this data is hard to collect.

\vskip 2pt\noindent
3The return value is the value of exp, which should be an integral expression.
The semantics of the built-in are that it is expected that $exp == c$. For
example:

\vskip 4pt\indent
\vbox{ \settabs\+ ''        '' & \cr % sample line
         \+ \bf if $(\_\_builtin\_expect(x, 0))$ \cr
         \+ & $foo()$ \cr }

\vskip 2pt\noindent
indicates that we do not expect to call foo, since we expect x to be zero.
Since you are limited to integral expressions for exp, you should use
constructions such as
 
\vskip 4pt\indent
\vbox{ \settabs\+ ''        '' & \cr % sample line
         \+ \bf if $(\_\_builtin\_expect(ptr != NULL, 1))$ \cr
         \+ & $foo(*ptr)$ \cr }

\vskip 2pt\noindent
when testing pointer or floating-point values.

\vskip 2pt\noindent
For the purposes of branch prediction optimizations, the probability that a
--builtinexpect expression is true is controlled by GCCs
builtin-expect-probability parameter, which defaults to 90%.

\vskip 2pt\noindent
You can also use --builtin-expect-with-probability to explicitly assign a
probability value to individual expressions. If the built-in is used in a
loop construct, the provided probability will influence the expected number
of iterations made by loop optimizations.

@ The attributes likely and unlikely may be applied to labels and statements
(other than declaration-statements). They may not be simultaneously applied
to the same label or statement.

\vskip 2pt\noindent
\item{1} Applies to a statement to allow the compiler to optimize for the case
where paths of execution including that statement are more likely than any
alternative path of execution that does not include such a statement.

\item{2} Applies to a statement to allow the compiler to optimize for the case
where paths of execution including that statement are less likely than any
alternative path of execution that does not include such a statement.

@d unlikely(x)	__builtin_expect(!! (x), 0)

@ static\_assert - check integer constant expression at build time

\vskip 2pt\noindent
{\sl static\_assert()} is a wrapper for the C11 \_Static\_assert, with a
little macro magic to make the message optional (defaulting to the
stringification of the tested expression).

\vskip 2pt\noindent
Contrary to {\sl BUILD\_BUG\_ON()}, {\sl static\_assert()} can be used at global
scope, but requires the expression to be an integer constant
expression (i.e., it is not enough that {\sl \_\_builtin\_constant\_p()} is
true for expr).

\vskip 2pt\noindent
Also note that {\sl BUILD\_BUG\_ON()} fails the build if the condition is
true, while {\sl static\_assert()} fails the build if the expression is
false.

@d static_assert(expr, ...) __static_assert(expr, ##__VA_ARGS__, #expr)
@d __static_assert(expr, msg, ...) _Static_assert(expr, msg)

@ container\_of - cast a member of a structure out to the containing structure.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl ptr:    & the pointer to the member. \cr
\+ & \sl type:   & the type of the container struct this is embedded in. \cr
\+ & \sl member: & the name of the member within the struct. \cr}

@d container_of(ptr, type, member) ({				        @/
	void *__mptr = (void *)(ptr);					@/
	static_assert(__same_type(*(ptr), ((type *)0)->member) ||	@/
		      __same_type(*(ptr), void),			@/
		      "pointer type mismatch in container_of()");	@/
	((type *)(__mptr - offsetof(type, member))); })

@ list\_entry - get the struct for this entry.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
% The code for the C address operation is 38: see Ascii table: mam ascii
\+ & \sl ptr:    & the {\sl \char38 struct list\_head} pointer. \cr
\+ & \sl type:   & the type of the struct this is embedded in. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_entry(ptr, type, member) @/
	container_of(ptr, type, member)

@ list\_prev\_entry - get the prev element in list.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl pos:	 & the type * to cursor. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_prev_entry(pos, member) @/
	list_entry((pos)->member.prev, typeof(*(pos)), member)

@  list\_next\_entry - get the next element in list.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl pos:	 & the type * to cursor. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_next_entry(pos, member) @/
	list_entry((pos)->member.next, typeof(*(pos)), member)

@ list\_last\_entry - get the last element from a list.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl ptr:	 & the list head to take the element from. \cr
\+ & \sl type:	 & the type of the struct this is embedded in. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_last_entry(ptr, type, member) @/
	list_entry((ptr)->prev, type, member)

@ list\_first\_entry - get the first element from a list.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl ptr:	 & the list head to take the element from. \cr
\+ & \sl type:	 & the type of the struct this is embedded in. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_first_entry(ptr, type, member) @/
	list_entry((ptr)->next, type, member)

@ list\_entry\_is \_head - test if the entry points to the head of the list.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl pos:	 & the type * to cursor. \cr
\+ & \sl head:	 & the head for your list. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_entry_is_head(pos, head, member) @/
	(&pos->member == (head))

@ list\_for\_each\_entry - iterate over list of given type.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl pos:	 & the type * to cursor. \cr
\+ & \sl head:	 & the head for your list. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_for_each_entry(pos, head, member)				@/
	for (pos = list_first_entry(head, typeof(*pos), member);	@/
	     ! list_entry_is_head(pos, head, member);			@/
	     pos = list_next_entry(pos, member))

@ list\_for\_each\_entry\_reverse - iterate backwards over list of given type.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl pos:	 & the type * to cursor. \cr
\+ & \sl head:	 & the head for your list. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_for_each_entry_reverse(pos, head, member)			@/
	for (pos = list_last_entry(head, typeof(*pos), member);		@/
	     ! list_entry_is_head(pos, head, member); 			@/
	     pos = list_prev_entry(pos, member))

@ list\_for\_each\_entry\_safe - iterate over list of given type safe against removal of list entry.

\vskip 4pt\noindent
\vbox{\settabs\+ \indent & member: \ \  & \cr % sample line
\+ & \sl pos:	 & the type * to use as a loop cursor. \cr
\+ & \sl n:	 & another type * to use as temporary storage. \cr
\+ & \sl head:	 & the head for your list. \cr
\+ & \sl member: & the name of the list\_head within the struct. \cr}

@d list_for_each_entry_safe(pos, n, head, member)			@/
	for (pos = list_first_entry(head, typeof(*pos), member),	@/
		n = list_next_entry(pos, member);			@/
	     ! list_entry_is_head(pos, head, member); 			@/
	     pos = n, n = list_next_entry(n, member))

@<The main program@>=

@* Index.
Here is the list of the identifiers used.
