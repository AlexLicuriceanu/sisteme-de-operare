/**
 * SO
 * Lab #5, Memory Management
 *
 * Task #8, Linux
 *
 * Endianess
 */
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"

int main(void)
{
	int i;
	unsigned int n = 0xDEADBEEF;
	unsigned char *w = &n;

	/* TODO - use w to show all bytes of n in order */
	for (int i=0; i<sizeof(int); i++) {
		printf("%X", *w);
		w++;
	}
	printf("\n");
	return 0;
}
