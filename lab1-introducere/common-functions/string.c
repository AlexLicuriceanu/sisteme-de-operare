#include "string.h"

unsigned long strlen(const char *s)
{
	unsigned long len;

	for (len = 0; *s != '\0'; s++, len++)
		;

	return len;
}

char *strcpy(char *dest, const char *src)
{
	char *d;

	for (d = dest; *src != '\0'; src++, d++)
		*d = *src;
	/* Also place NUL byte. */
	*d = *src;

	return dest;
}

char *my_strcat(char *dest, char *src) {
	char *d = dest + strlen(dest);
	while (*src != '\0') {
		*d++ = *src++;
	}
	*d = '\0';
	return dest;
}
