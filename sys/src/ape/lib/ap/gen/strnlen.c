#include <string.h>

size_t
strnlen(const char *s, size_t maxlen)
{
	size_t n;

	for(n = 0; n < maxlen && s[n] != 0; n++)
		;
	return n;
}
