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

@* Index.
