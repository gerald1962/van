#include "os.h"

#define AIO

#define P  "vat> "

static struct {
	int    dev_id;   /* Van dvice id. */
	int    cycles;   /* Number of the test cycles. */
	int    count;    /* Current test counter. */
	sem_t  suspend;  /* Waiting for the end of the test. */
} v;

#if defined(AIO)
/* Async. data sink. */
static int aio_read_cb(int dev_id, char *buf, int count)
{
	return count;
}

/* Async. data generator. */
static int aio_write_cb(int dev_id, char *buf, int count)
{
	int n;

	if (v.count < v.cycles) {
		/* The generator continues to run. */
		n = snprintf(buf, OS_BUF_SIZE, "%d", v.count);
		printf("%s sent: [b:\"%s\", s:%d]\n", P, buf, n);
		v.count++;
		return n;
	}
	
	if (v.count == v.cycles) {
		/* The generator sends the last message. */
		n = snprintf(buf, OS_BUF_SIZE, "That's it.");
		printf("%s sent: [b:\"%s\", s:%d]\n", P, buf, n);
		v.count++;
		return n;
	}

	/* v.count > v.cycles */

	/* The generator said its goodbyes. */
	os_sem_release(&v.suspend);
	return 0;
}
#endif
	
int main(void)
{
#if defined(AIO)
	os_aio_cb_t aio;

	/* Test cycles. */
	v.cycles = 99999;

	/* Van OS. */
	os_init();

	/* Main control semaphore. */
	os_sem_init(&v.suspend, 0);

	/* Disable the user-friendly OS - FOS - trace and create the van server device. */
	os_trace_button(0);
	v.dev_id = os_open("/van");

	/* Use the async. FOS I/O operations. */
	aio.write_cb = aio_write_cb;
	aio.read_cb  = aio_read_cb;
	os_aio_action(v.dev_id, &aio);

	/* Trigger the FOS to invoke the async. callbacks. */
	os_aio_write(v.dev_id);
	os_aio_read(v.dev_id);

	/* Wait for end of the test. */
	os_sem_wait(&v.suspend);

	/* Free the used FOS resources. */
	os_close(v.dev_id);
	os_sem_delete(&v.suspend);	
	os_exit();
	
	return 0;
#else
	char buf[32];
	int dev_id, i, n;
	
	os_init();
	os_trace_button(0);
	dev_id = os_open("/van");
	
	for (i = 0; i < 999999; i++) {
		n = snprintf(buf, 32, "%d", i);
		os_sync_write(dev_id, buf, n + 1);

		printf("%s sent: [b:\"%s\", s:%d]\n", P, buf, n);
	}

	n = snprintf(buf, 32, "That's it.");
	os_sync_write(dev_id, buf, n + 1);		
	
	printf("%s sent: [b:\"%s\", s:%d]\n", P, buf, n);
		
	os_close(dev_id);
	os_exit();
	
	return 0;
#endif
}
