#include "os.h"

#define P  "pot> "

int main(void)
{
	char buf[OS_BUF_SIZE];
	int dev_id, done, i, n, count;
	
	os_init();
	os_trace_button(0);
	dev_id = os_open("/python");

	for(done = 0, i = 0; ! done; i++) {
		n = os_read(dev_id, buf, OS_BUF_SIZE);		
		OS_TRAP_IF(n < 1);
		
		printf("%s received: [b:\"%s\", s:%d]\n", P, buf, n);

		if (os_strcmp(buf, "That's it.") == 0) {
			done = 1;
		}
		else {
			count = strtol(buf, NULL, 10);
			OS_TRAP_IF(i != count);
		}
		
		os_write(dev_id, buf, n);
	}
		
	os_close(dev_id);
	os_exit();

	return 0;
}
