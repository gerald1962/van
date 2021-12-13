#include "os.h"                                                    // (1)

#define BUF_LEN  32
typedef struct {                                                   // (2)
        OS_QUEUE_MSG_HEAD;                                         // (3)
        char buf[BUF_LEN];                                         // (4)
} hello_msg_t;                                                     // (5)

static char *hello = "Hello battery 1";                            // (6)
static sem_t suspend;                                              // (7)

static void hello_exec(os_queue_elem_t *m)                         // (8)
{
        hello_msg_t *msg;                                          // (9)

        msg = (hello_msg_t *) m;                                   // (10)
	printf("%s\n", msg->buf);                                  // (11)
	os_sem_release(&suspend);                                  // (12)
}

static void hello_send(void *bat)                                  // (13)
{
        hello_msg_t msg;                                           // (14)
	
	os_memset(&msg, 0, sizeof(msg));                           // (15)
	msg.param = bat;                                           // (16)
	msg.cb    = hello_exec;                                    // (17)
	os_strcpy(msg.buf, BUF_LEN, hello);                        // (18)
	os_queue_send(bat, (os_queue_elem_t *) &msg, sizeof(msg)); // (19)
}

int main(void)                                                     // (20)
{
        void *bat;                                                 // (21)
	
        os_init();                                                 // (22)
	os_sem_init(&suspend, 0);                                  // (23)
	bat = os_thread_create("bat", OS_THREAD_PRIO_FOREG, 2);    // (24)
	hello_send(bat);                                           // (25)
	
	os_sem_wait(&suspend);                                     // (26)

	os_thread_destroy(bat);                                    // (27)
	os_sem_delete(&suspend);                                   // (28)
	os_exit();                                                 // (29)
	
	printf("Goodby battery 1\n");                              // (30)
	return 0;                                                  // (31)
}
