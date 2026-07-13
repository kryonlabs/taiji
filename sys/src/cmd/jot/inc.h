#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
//#include <cursor.h>
#include <frame.h>
//#include <fcall.h>
//#include <9p.h>
//#include <complete.h>
#include <plumb.h>

typedef uchar bool;
enum {
	FALSE = 0,
	TRUE = 1,

	Kscrolloneup = KF|0x20,
	Kscrollonedown = KF|0x21
};

#define ALT(c, v, t) (Alt){ c, v, t, nil, nil, 0 }

#define CTRL(c) ((c)&0x1F)


extern Rune *snarf;
extern int nsnarf;
extern int snarfversion;
extern int snarffd;
enum { MAXSNARF = 100*1024 };
void putsnarf(void);
void getsnarf(void);
void setsnarf(char *s, int ns);

typedef struct Text Text;
struct Text
{
	Frame;
	Rectangle scrollr, lastsr;
	Image *i;
	Rune *r;
	uint nr;
	uint maxr;
	uint org;	/* start of Frame's text */
	uint q0, q1;	/* selection */
	uint qh;	/* host point, output here */

	/* not entirely happy with this in here */
	bool rawmode;
	Rune *raw;
	int nraw;

	int posx;
	int selflip;
};

void xinit(Text *x, Rectangle textr, Rectangle scrollr, Font *ft, Image *b, Image **cols);
void xsetrects(Text *x, Rectangle textr, Rectangle scrollr);
void xclear(Text *x);
void xredraw(Text *x);
void xfullredraw(Text *x);
uint xinsert(Text *x, Rune *r, int n, uint q0);
void xfill(Text *x);
void xdelete(Text *x, uint q0, uint q1);
void xsetselect(Text *x, uint q0, uint q1);
void xselect(Text *x, Mousectl *mc);
void xscrdraw(Text *x);
void xscroll(Text *x, Mousectl *mc, int but);
void xscrolln(Text *x, int n);
void xtickupdn(Text *x, int d);
void xshow(Text *x, uint q0);
void xplacetick(Text *x, uint q);
void xtype(Text *x, Rune r);
int xninput(Text *x);
void xaddraw(Text *x, Rune *r, int nr);
void xlook(Text *x);
void xsnarf(Text *x);
void xcut(Text *x);
void xpaste(Text *x);
void xsend(Text *x);
int xplumb(Text *w, char *src, char *dir, int maxsize);

#define	runemalloc(n)		malloc((n)*sizeof(Rune))
#define	runerealloc(a, n)	realloc(a, (n)*sizeof(Rune))
#define	runemove(a, b, n)	memmove(a, b, (n)*sizeof(Rune))
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))

void panic(char *s);
void *emalloc(ulong size);
void *erealloc(void *p, ulong size);
char *estrdup(char *s);


typedef struct Timer Timer;
struct Timer
{
	int		dt;
	int		cancel;
	Channel	*c;	/* chan(int) */
	Timer	*next;
};
void timerinit(void);
Timer *timerstart(int dt);
void timerstop(Timer *t);
void timercancel(Timer *t);




typedef struct RKeyboardctl RKeyboardctl;
struct RKeyboardctl
{
	Keyboardctl;
	int kbdfd;
};
RKeyboardctl *initkbd(char *file, char *kbdfile);
