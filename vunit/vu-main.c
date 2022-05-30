#define P "U>"
#define POISON_POINTER_DELTA 0
#define LIST_POISON1 ((void*) 0x100+POISON_POINTER_DELTA) 
#define LIST_POISON2 ((void*) 0x200+POISON_POINTER_DELTA) 
#define __noreturn __attribute__((__noreturn__) ) 
#define __must_check __attribute__((__warn_unused_result__) ) 
#define REFCOUNT_SATURATED (INT_MIN/2)  \

#define KUNIT_LOG_SIZE 512
#define KUNIT_STATUS_COMMENT_SIZE 256
#define KUNIT_PARAM_DESC_SIZE 128
#define KUNIT_SUBTEST_INDENT "    "
#define KUNIT_SUBSUBTEST_INDENT "        "
#define KUNIT_CURRENT_LOC {.file= __FILE__,.line= __LINE__} \

#define __printk_index_emit(...) do{}while(0)  \

#define printk_index_wrap(_p_func,_fmt,...) ({ \
__printk_index_emit(_fmt,NULL,NULL) ; \
_p_func(_fmt,##__VA_ARGS__) ; \
})  \

#define printk(fmt,...) printk_index_wrap(_printk,fmt,##__VA_ARGS__)  \

#define pr_fmt(fmt) "VUnit: "fmt \

#define pr_info(fmt,...) printk(pr_fmt(fmt) ,##__VA_ARGS__)  \

#define __WARN_printf(arg...) do{fprintf(stderr,arg) ;}while(0)  \

#define WARN_ON(condition) ({ \
int __ret_warn_on= !!(condition) ; \
if(unlikely(__ret_warn_on) )  \
__WARN_printf("assertion failed at %s:%d\n", \
__FILE__,__LINE__) ; \
unlikely(__ret_warn_on) ; \
})  \

#define BUG_ON(cond) OS_TRAP_IF(!(cond) ) 
#define BUG() BUG_ON(1) 
#define __same_type(a,b) __builtin_types_compatible_p(typeof(a) ,typeof(b) )  \

#define check_mul_overflow(a,b,d) __must_check_overflow(({ \
typeof(a) __a= (a) ; \
typeof(b) __b= (b) ; \
typeof(d) __d= (d) ; \
(void) (&__a==&__b) ; \
(void) (&__a==__d) ; \
__builtin_mul_overflow(__a,__b,__d) ; \
}) )  \

#define barrier() __asm__ __volatile__("":::"memory")  \

#define WARN(condition,format...) ({ \
int __ret_warn_on= !!(condition) ; \
if(unlikely(__ret_warn_on) )  \
printf(format) ; \
unlikely(__ret_warn_on) ; \
})  \

#define WARN_ONCE(condition,format...) WARN(condition,format)  \

#define READ_ONCE(x) ({ \
union{typeof(x) __val;char __c[1];}__u=  \
{.__c= {0}}; \
__read_once_size(&(x) ,__u.__c,sizeof(x) ) ; \
__u.__val; \
})  \

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

/*10:*/
#line 272 "vu-main.w"

void main(void)
{
printf("Hello.\n");
}

/*:10*/
#line 5 "vu-main.w"


/*:1*/
