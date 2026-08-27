#include <stdlib.h>
#include <string.h>

char *
strndup(const char *s, size_t maxlen)
{
	size_t n;
	char *p;

	n = strnlen(s, maxlen);
	p = malloc(n + 1);
	if(p == 0)
		return 0;
	memmove(p, s, n);
	p[n] = 0;
	return p;
}
