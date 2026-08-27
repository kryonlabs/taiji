#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

ssize_t
getdelim(char **linep, size_t *cap, int delim, FILE *f)
{
	char *p, *np;
	size_t n, size;
	int c;

	if(linep == 0 || cap == 0 || f == 0){
		errno = EINVAL;
		return -1;
	}
	p = *linep;
	size = *cap;
	if(p == 0 || size == 0){
		size = 128;
		p = malloc(size);
		if(p == 0){
			errno = ENOMEM;
			return -1;
		}
	}
	n = 0;
	while((c = fgetc(f)) != EOF){
		if(n + 1 >= size){
			size *= 2;
			np = realloc(p, size);
			if(np == 0){
				*linep = p;
				*cap = size/2;
				errno = ENOMEM;
				return -1;
			}
			p = np;
		}
		p[n++] = c;
		if(c == delim)
			break;
	}
	if(n == 0 && c == EOF){
		*linep = p;
		*cap = size;
		return -1;
	}
	p[n] = 0;
	*linep = p;
	*cap = size;
	return n;
}

ssize_t
getline(char **linep, size_t *cap, FILE *f)
{
	return getdelim(linep, cap, '\n', f);
}
