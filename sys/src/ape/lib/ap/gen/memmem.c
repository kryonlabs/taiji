#include <string.h>

void *
memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen)
{
	const unsigned char *h, *n;
	size_t i;

	if(needlelen == 0)
		return (void*)haystack;
	if(haystacklen < needlelen)
		return 0;
	h = haystack;
	n = needle;
	for(i = 0; i <= haystacklen - needlelen; i++)
		if(h[i] == n[0] && memcmp(h+i, n, needlelen) == 0)
			return (void*)(h+i);
	return 0;
}
