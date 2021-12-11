#include <stdio.h>
#include <string.h>

int s_write(char *buf, int count)
{
	printf("buf=\"%s\", count=%d\n", buf, count);
	return count;
}

int s_read(char *buf, int count)
{
	strcpy(buf, "Welcome");
	return strlen(buf) + 1;
}
