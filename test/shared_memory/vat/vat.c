#include "os.h"

#define P  "vat>"

static struct {
	int    dev_id;   /* Van dvice id. */
	int    cycles;   /* Number of the test cycles. */
	int    sent;     /* Current send counter. */
	int    received; /* Current receive counter. */
	sem_t  suspend;  /* Waiting for the end of the test. */
} v;

/* Async. data sink. */
static int aio_read_cb(int dev_id, char *buf, int count)
{
	int n;
	
	/* Entry condition. */
	OS_TRAP_IF(dev_id != v.dev_id || buf == NULL || count < 1);

	if (v.received <= v.cycles) {
		printf("%s receivd: [b:\"%s\", s:%d]\n", P, buf, count);

		/* Convert and test the received counter. */
		if (v.received < v.cycles) {
			n = strtol(buf, NULL, 10);
			OS_TRAP_IF(n != v.received);
		}
		
		v.received++;
		return count;
	}

	/* It is done. */
	os_sem_release(&v.suspend);
	return count;
}

/* Async. data generator. */
static int aio_write_cb(int dev_id, char *buf, int count)
{
	int n;

	/* Entry condition. */
	OS_TRAP_IF(dev_id != v.dev_id || buf == NULL || count != OS_BUF_SIZE);
	
	if (v.sent < v.cycles) {
		/* The generator continues to run. */
		n = snprintf(buf, OS_BUF_SIZE, "%d", v.sent);
		n++;
		printf("%s sent:    [b:\"%s\", s:%d]\n", P, buf, n);
		v.sent++;
		return n;
	}
	
	if (v.sent == v.cycles) {
		/* The generator sends the last message. */
		n = snprintf(buf, OS_BUF_SIZE, "That's it.");
		n++;
		printf("%s sent:    [b:\"%s\", s:%d]\n", P, buf, n);
		v.sent++;
		return n;
	}

	/* v.count > v.cycles */

	/* The generator said its goodbyes. */
	os_sem_release(&v.suspend);
	return 0;
}
	
int main(void)
{
	os_aio_cb_t aio;

	/* Test cycles. */
	v.cycles = 99999;

	/* Van OS. */
	os_init(1);

	/* Main control semaphore. */
	os_sem_init(&v.suspend, 0);

	/* Disable the user-friendly OS - FOS - trace and create the van server
	 * device. */
	os_trace_button(0);
	v.dev_id = os_open("/van_py");

	/* Use the async. FOS I/O operations. */
	aio.write_cb = aio_write_cb;
	aio.read_cb  = aio_read_cb;
	os_aio_action(v.dev_id, &aio);

	/* Trigger the FOS to invoke the async. callbacks. */
	os_aio_write(v.dev_id);
	os_aio_read(v.dev_id);

	/* Wait for the end of the test. */
	while(v.sent <= v.cycles && v.received <= v.cycles)
		os_sem_wait(&v.suspend);

	/* Free the used FOS resources. */
	os_close(v.dev_id);
	os_sem_delete(&v.suspend);	
	os_exit();
	
	return 0;
}
