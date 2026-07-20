#include "inc.h"

/*
 * The panel shipped as a separate program, lpanel(1), is the supported
 * panel for lola sessions.  These stubs keep older lola panel call sites and
 * command-line options harmless without carrying a second panel/menu
 * implementation.
 */

int
panelenabled(void)
{
	return 0;
}

void
panelreset(void)
{
}

void
panelsetedge(char*)
{
}

Rectangle
panelworkrect(void)
{
	return screen->r;
}

void
paneldraw(void)
{
}

int
panelmouse(Mousectl*)
{
	return 0;
}

void
panelinit(void)
{
}
