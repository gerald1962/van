#include "os.h"

#define P  "pot> "

int main(void)
{
	char *buf;
	int dev_id, n;
	
	os_init();
	os_trace_button(0);
	dev_id = os_open("/python");

	for(;;) {
		buf = NULL;
		n = os_sync_pread(dev_id, &buf, 1024);		
		OS_TRAP_IF(n < 1 || buf == NULL);
		
		printf("%s received: [b:\"%s\", s:%d]\n", P, buf, n);

		if (os_strcmp(buf, "That's it.") == 0)
			break;
	}
	
	n = os_sync_pread(dev_id, NULL, 0);
	OS_TRAP_IF(n > 0);
		
	os_close(dev_id);
	os_exit();
	
	return 0;
}
