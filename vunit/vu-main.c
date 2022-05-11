#define P "U>"
#define POISON_POINTER_DELTA 0
#define LIST_POISON1 ((void*) 0x100+POISON_POINTER_DELTA) 
#define LIST_POISON2 ((void*) 0x200+POISON_POINTER_DELTA) 
#define __noreturn __attribute__((__noreturn__) )  \

#define __must_check __attribute__((__warn_unused_result__) )  \

#define REFCOUNT_SATURATED (INT_MIN/2) 
#define KUNIT_LOG_SIZE 512 \

#define KUNIT_STATUS_COMMENT_SIZE 256 \

#define KUNIT_PARAM_DESC_SIZE 128 \

#define KUNIT_SUBTEST_INDENT "    "
#define KUNIT_SUBSUBTEST_INDENT "        " \

#define KUNIT_CURRENT_LOC {.file= __FILE__,.line= __LINE__} \
 \

/*1:*/
#line 3 "vu-main.w"

/*3:*/
#line 19 "vu-main.w"

#include "os.h"
#include <stddef.h> 
#include <stdbool.h> 
#include <errno.h> 
#include <limits.h> 

/*:3*/
#line 4 "vu-main.w"

/*8:*/
#line 94 "vu-main.w"

void main(void)
{
printf("Hello.\n");
}

/*:8*/
#line 5 "vu-main.w"


/*:1*/
