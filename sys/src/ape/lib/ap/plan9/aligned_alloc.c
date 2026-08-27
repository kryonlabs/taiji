#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

typedef struct Aligned Aligned;
struct Aligned {
	Aligned		*next;
	void		*ptr;
	void		*base;
};

static Aligned *alignedlist;

void
_alignedfree(void *ptr, void **base)
{
	Aligned **l, *a;

	for(l = &alignedlist; (a = *l) != 0; l = &a->next){
		if(a->ptr != ptr)
			continue;
		*l = a->next;
		*base = a->base;
		return;
	}
	*base = ptr;
}

int
posix_memalign(void **memptr, size_t alignment, size_t size)
{
	uintptr_t p, q;
	void *base;
	Aligned *a;

	if(memptr == 0)
		return EINVAL;
	if(alignment < sizeof(void*) || (alignment & (alignment-1)) != 0)
		return EINVAL;
	base = malloc(size + alignment - 1 + sizeof(Aligned));
	if(base == 0)
		return ENOMEM;
	p = (uintptr_t)base + sizeof(Aligned);
	q = (p + alignment - 1) & ~(uintptr_t)(alignment - 1);
	a = (Aligned*)(q - sizeof(Aligned));
	a->ptr = (void*)q;
	a->base = base;
	a->next = alignedlist;
	alignedlist = a;
	*memptr = (void*)q;
	return 0;
}

void *
aligned_alloc(size_t alignment, size_t size)
{
	void *p;
	int r;

	if(alignment == 0 || (size % alignment) != 0){
		errno = EINVAL;
		return 0;
	}
	r = posix_memalign(&p, alignment, size);
	if(r != 0){
		errno = r;
		return 0;
	}
	return p;
}
