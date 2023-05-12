#include "syscall.h"

int main(void)
{
	write(1, "Hello, world!\n", 14);
	
	int n;
	char buffer[64];
	n = read(0, buffer, 64);
	write(1, "Input: ", 8);
	write(1, buffer, n);
	exit(0);
	return 0;
}
