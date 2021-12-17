#include "os.h"

#define P  "pot> "

int main(void)
{
	/* XXX */
#if 1
	char *buf;
	int dev_id, n;
	
	os_init();
	os_trace_button(0);
	dev_id = os_open("/python");

	for(;;) {
		buf = NULL;
		n = os_sync_zread(dev_id, &buf, 1024);		
		OS_TRAP_IF(n < 1 || buf == NULL);
		
		printf("%s received: [b:\"%s\", s:%d]\n", P, buf, n);

		if (os_strcmp(buf, "That's it.") == 0)
			break;
	}
	
	n = os_sync_zread(dev_id, NULL, 0);
	OS_TRAP_IF(n > 0);
		
	os_close(dev_id);
	os_exit();
#else
	char buf[512];
	int dev_id, n;
	
	os_init();
	os_trace_button(0);
	dev_id = os_open("/python");

	for(;;) {
		n = os_sync_read(dev_id, buf, 512);		
		OS_TRAP_IF(n < 1);
		
		printf("%s received: [b:\"%s\", s:%d]\n", P, buf, n);

		if (os_strcmp(buf, "That's it.") == 0)
			break;
	}
		
	os_close(dev_id);
	os_exit();
#endif
	return 0;
}
