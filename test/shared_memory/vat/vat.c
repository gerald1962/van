#include "os.h"

#define P  "vat> "

int main(void)
{
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
}
