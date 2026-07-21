#include <u.h>
#include <libc.h>

void
main(void)
{
	execl("/bin/explorer", "explorer", "/lib/controlpanel", nil);
	sysfatal("exec explorer: %r");
}
