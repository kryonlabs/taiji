#include "inc.h"
#include <bio.h>

/*
 * Windows-2000 style widgets shared by the shell: popup menus,
 * modal text dialogs, the Run box, the calendar, the Ctrl+Alt+Del
 * security screen, the window system menu and the Alt+Tab switcher.
 *
 * Everything here runs on the mouse thread, like menuhit and the
 * shut-down dialog, and parks the keyboard thread via modalbegin().
 */

enum {
	Itemh = 20,
	Menupad = 26,
};

static Image *dface;
static Image *dhilite;
static Image *dshadow;
static Image *ddark;
static Image *dactive;
static Image *dnavy;
static Image *dtipback;

static void
dlgcolors(void)
{
	if(dface != nil)
		return;
	dface = getcolor("3d_face", 0xC0C0C0FF);
	dhilite = getcolor(nil, 0xFFFFFFFF);
	dshadow = getcolor("3d_shadow1", 0x808080FF);
	ddark = getcolor("3d_shadow2", 0x000000FF);
	dactive = getcolor("titlebar_active", 0x000080FF);
	dnavy = getcolor("titlebar_active", 0x000080FF);
	dtipback = getcolor("tipback", 0xFFFFE1FF);
}

/*
 * Park the keyboard (and mouse) thread the way the logon screen does,
 * so a modal dialog can read /dev/kbd and the mouse itself. Any stale
 * releases from a previous dialog are drained first.
 */
void
modalbegin(void)
{
	ulong junk;

	while(nbrecv(logonchan, &junk) == 1)
		;
	inlogon = TRUE;
}

void
modalend(void)
{
	inlogon = FALSE;
	sendul(logonchan, 1);
	sendul(logonchan, 1);
}

static void
dlgtitle(Rectangle r, char *s)
{
	Rectangle t;

	t = Rect(r.min.x+3, r.min.y+3, r.max.x-3, r.min.y+24);
	draw(screen, t, dnavy, nil, ZP);
	string(screen, Pt(t.min.x+8, t.min.y+(Dy(t)-font->height)/2),
		dhilite, ZP, font, s);
}

static void
dlgframe(Rectangle r, char *title)
{
	draw(screen, r, dface, nil, ZP);
	winborder(screen, r, ddark, dhilite);
	winborder(screen, insetrect(r, 1), dshadow, dface);
	if(title != nil)
		dlgtitle(r, title);
}

static void
dlgsunken(Rectangle r)
{
	winborder(screen, r, dshadow, dhilite);
	draw(screen, insetrect(r, 1), dhilite, nil, ZP);
}

/* ---- generic popup menu ---- */

static int
menuitemdisabled(char *s)
{
	return s == nil || s[0] == '-' && s[1] == 0 || s[0] == '!';
}

int
winmenuhit(Mousectl *mc, Rectangle anchor, char **items, int n, int defsel)
{
	Image *backup;
	Rectangle r, ir;
	Point pt;
	int i, w, h, sel, hover;
	static Alt alts[2];

	dlgcolors();
	drainmouse(mc, nil);

	w = 0;
	for(i = 0; i < n; i++){
		if(items[i] == nil)
			continue;
		h = stringwidth(font, items[i][0] == '!' ? items[i]+1 : items[i]);
		if(h > w)
			w = h;
	}
	w += Menupad + 12;
	h = n*Itemh + 6;
	r = Rect(anchor.min.x, anchor.min.y, anchor.min.x+w, anchor.min.y+h);
	if(r.max.x > screen->r.max.x)
		r = rectaddpt(r, Pt(screen->r.max.x-r.max.x, 0));
	if(r.max.y > screen->r.max.y)
		r = rectaddpt(r, Pt(0, screen->r.max.y-r.max.y));
	if(r.min.x < screen->r.min.x)
		r = rectsubpt(r, Pt(screen->r.min.x-r.min.x, 0));
	if(r.min.y < screen->r.min.y)
		r = rectsubpt(r, Pt(0, screen->r.min.y-r.min.y));

	backup = allocimage(display, r, screen->chan, 0, -1);
	if(backup)
		draw(backup, r, screen, nil, r.min);
	draw(screen, r, dface, nil, ZP);
	winborder(screen, r, dhilite, ddark);
	sel = -1;

	for(i = 0; i < n; i++){
		ir = Rect(r.min.x+3, r.min.y+3+i*Itemh, r.max.x-3, r.min.y+3+(i+1)*Itemh);
		pt = Pt(ir.min.x+Menupad-16, ir.min.y+(Itemh-font->height)/2);
		if(items[i] != nil && items[i][0] == '-' && items[i][1] == 0){
			line(screen, Pt(ir.min.x+4, ir.min.y+Itemh/2),
				Pt(ir.max.x-4, ir.min.y+Itemh/2), 0, 0, 0, dshadow, ZP);
			continue;
		}
		draw(screen, ir, dface, nil, ZP);
		if(items[i][0] == '!')
			string(screen, pt, dshadow, ZP, font, items[i]+1);
		else
			string(screen, pt, ddark, ZP, font, items[i]);
		if(i == defsel && defsel >= 0 && !menuitemdisabled(items[i])){
			draw(screen, ir, dactive, nil, ZP);
			string(screen, pt, dhilite, ZP, font, items[i]);
			sel = i;
		}
	}
	flushimage(display, 1);

	for(;;){
		alts[0] = ALT(mc->c, &mc->Mouse, CHANRCV);
		alts[1].op = CHANEND;
		switch(alt(alts)){
		case 0:
			hover = -1;
			if(ptinrect(mc->xy, insetrect(r, 3)))
				hover = (mc->xy.y - (r.min.y+3)) / Itemh;
			if(hover >= n)
				hover = -1;
			if(hover >= 0 && menuitemdisabled(items[hover]))
				hover = -1;
			if(hover != sel){
				/* unselect old, select new */
				if(sel >= 0){
					ir = Rect(r.min.x+3, r.min.y+3+sel*Itemh, r.max.x-3, r.min.y+3+(sel+1)*Itemh);
					draw(screen, ir, dface, nil, ZP);
					string(screen, Pt(ir.min.x+Menupad-16, ir.min.y+(Itemh-font->height)/2),
						ddark, ZP, font, items[sel][0]=='!' ? items[sel]+1 : items[sel]);
				}
				sel = hover;
				if(sel >= 0){
					ir = Rect(r.min.x+3, r.min.y+3+sel*Itemh, r.max.x-3, r.min.y+3+(sel+1)*Itemh);
					draw(screen, ir, dactive, nil, ZP);
					string(screen, Pt(ir.min.x+Menupad-16, ir.min.y+(Itemh-font->height)/2),
						dhilite, ZP, font, items[sel]);
				}
				flushimage(display, 1);
			}
			if(mc->buttons == 0)
				break;
			if(mc->buttons & 1){
				drainmouse(mc, nil);
				if(sel < 0)
					goto Done;
				goto Done;
			}
			if(mc->buttons & 4){
				drainmouse(mc, nil);
				sel = -1;
				goto Done;
			}
			if(mc->buttons & 2){
				drainmouse(mc, nil);
				goto Done;
			}
			break;
		}
	}

Done:
	if(backup){
		draw(screen, r, backup, nil, r.min);
		freeimage(backup);
		flushimage(display, 1);
	}
	return sel;
}

/* ---- modal keyboard helper (same protocol as the logon screen) ---- */

static int
dlgrune(char *buf, int *np, int nbuf, Rune r)
{
	if(r == '\n' || r == '\r')
		return 1;
	if(r == Kesc)
		return 2;
	if(r == Kbs || r == Kdel){
		if(*np > 0){
			while(*np > 0 && (buf[*np-1] & 0xC0) == 0x80)
				(*np)--;
			if(*np > 0)
				(*np)--;
			buf[*np] = 0;
		}
		return 0;
	}
	if(r >= ' ' && r < 0x7f && *np < nbuf-2){
		buf[(*np)++] = r;
		buf[*np] = 0;
		return 0;
	}
	return 0;
}

/*
 * Handle one keyboard event string; appends printable runes to buf.
 * Returns 1 on Enter, 2 on Esc, 3 on Up, 4 on Down, 0 otherwise.
 */
static char kdown[128];

static int
dlgkey(char *ks, char *buf, int *np, int nbuf)
{
	Rune r;
	char *p;
	int n, rc;

	rc = 0;
	if(ks[0] == 'c'){
		p = ks+1;
		while(*p){
			n = chartorune(&r, p);
			p += n;
			if(r == Kup){
				rc = 3;
				continue;
			}
			if(r == Kdown){
				rc = 4;
				continue;
			}
			n = dlgrune(buf, np, nbuf, r);
			if(n)
				rc = n;
		}
		free(ks);
		return rc;
	}
	if(ks[0] != 'k' && ks[0] != 'K'){
		free(ks);
		return 0;
	}
	if(ks[0] == 'k'){
		p = ks+1;
		while(*p){
			n = chartorune(&r, p);
			p += n;
			if(r == Kshift || r == Kctl || r == Kalt)
				continue;
			if(utfrune(kdown, r) != nil)
				continue;
			if(r == Kup){
				rc = 3;
				continue;
			}
			if(r == Kdown){
				rc = 4;
				continue;
			}
			n = dlgrune(buf, np, nbuf, r);
			if(n)
				rc = n;
		}
	}
	n = strlen(ks+1);
	if(n >= sizeof kdown)
		n = sizeof kdown-1;
	memmove(kdown, ks+1, n);
	kdown[n] = 0;
	free(ks);
	return rc;
}

/* ---- shared entry field drawing ---- */

static void
dlgentrydraw(Rectangle r, char *buf, int blink)
{
	int w, cx;
	Rectangle cl;

	dlgsunken(r);
	cl = insetrect(insetrect(r, 1), 1);
	draw(screen, cl, dhilite, nil, ZP);
	stringn(screen, Pt(cl.min.x+4, cl.min.y+(Dy(cl)-font->height)/2),
		ddark, ZP, font, buf, strlen(buf));
	w = stringnwidth(font, buf, strlen(buf));
	if(blink){
		cx = cl.min.x+4+w;
		draw(screen, Rect(cx, cl.min.y+3, cx+1, cl.max.y-3), ddark, nil, ZP);
	}
}

/* ---- generic text entry dialog ---- */

int
wintextdlg(char *title, char *prompt, char *buf, int nbuf)
{
	Rectangle dlg, entry, ok, cancel;
	Image *backup;
	char *ks;
	int blink, done, rc, n;

	dlgcolors();
	n = strlen(buf);
	done = 0;
	rc = 0;

	dlg = Rect((screen->r.min.x+screen->r.max.x)/2-190,
		(screen->r.min.y+screen->r.max.y)/2-90,
		(screen->r.min.x+screen->r.max.x)/2+190,
		(screen->r.min.y+screen->r.max.y)/2+90);
	entry = Rect(dlg.min.x+20, dlg.min.y+58, dlg.max.x-20, dlg.min.y+86);
	ok = Rect(dlg.max.x-180, dlg.max.y-52, dlg.max.x-100, dlg.max.y-28);
	cancel = Rect(dlg.max.x-90, dlg.max.y-52, dlg.max.x-16, dlg.max.y-28);

	backup = allocimage(display, dlg, screen->chan, 0, -1);
	if(backup)
		draw(backup, dlg, screen, nil, dlg.min);

	modalbegin();
	kdown[0] = 0;
	blink = 1;

Redraw:
	dlgframe(dlg, title);
	string(screen, Pt(dlg.min.x+20, dlg.min.y+34), ddark, ZP, font, prompt);
	dlgentrydraw(entry, buf, blink);
	panelbutton(ok, "OK", -1, 0);
	panelbutton(cancel, "Cancel", -1, 0);
	flushimage(display, 1);

	while(!done){
		blink = 1;
		while(nbrecv(kbctl->c, &ks) == 1){
			switch(dlgkey(ks, buf, &n, nbuf)){
			case 1:
				done = 1;
				rc = 1;
				break;
			case 2:
				done = 1;
				rc = 0;
				break;
			default:
				break;
			}
			if(done)
				break;
			goto Redraw;
		}
		if(done)
			break;
		/* wait for mouse or blink */
		{
			Timer *timer;
			static Alt alts[3];
			timer = timerstart(400);
			alts[0] = ALT(timer->c, nil, CHANRCV);
			alts[1] = ALT(mctl->c, &mctl->Mouse, CHANRCV);
			alts[2].op = CHANEND;
			switch(alt(alts)){
			case 0:
				blink = !blink;
				goto Redraw;
			case 1:
				timercancel(timer);
				if((mctl->buttons & 1) && ptinrect(mctl->xy, ok)){
					drainmouse(mctl, nil);
					done = 1;
					rc = 1;
				}else if((mctl->buttons & 1) && ptinrect(mctl->xy, cancel)){
					drainmouse(mctl, nil);
					done = 1;
					rc = 0;
				}
				break;
			}
		}
	}
	if(backup){
		draw(screen, dlg, backup, nil, dlg.min);
		freeimage(backup);
		flushimage(display, 1);
	}
	modalend();
	return rc;
}

/* ---- Run dialog ---- */

void
rundlg(void)
{
	Rectangle dlg, icon, entry, ok, cancel, browse;
	Image *backup;
	char buf[256];
	char *hist[16];
	char *ks, *home, *path, *line;
	int nhist, hcur, blink, done, launchit, i, n;
	Biobuf *bp;

	dlgcolors();
	buf[0] = 0;
	n = 0;
	nhist = 0;
	launchit = 0;

	home = getenv("home");
	if(home != nil){
		path = smprint("%s/lib/runhist", home);
		if(path != nil){
			bp = Bopen(path, OREAD);
			if(bp != nil){
				while(nhist < 16 && (line = Brdline(bp, '\n')) != nil){
					line[Blinelen(bp)-1] = 0;
					if(line[0] == 0)
						continue;
					hist[nhist++] = estrdup(line);
				}
				Bterm(bp);
			}
			free(path);
		}
		free(home);
	}
	hcur = nhist;

	dlg = Rect((screen->r.min.x+screen->r.max.x)/2-210,
		(screen->r.min.y+screen->r.max.y)/2-85,
		(screen->r.min.x+screen->r.max.x)/2+210,
		(screen->r.min.y+screen->r.max.y)/2+85);
	icon = Rect(dlg.min.x+18, dlg.min.y+38, dlg.min.x+50, dlg.min.y+70);
	entry = Rect(dlg.min.x+20, dlg.min.y+86, dlg.max.x-20, dlg.min.y+114);
	ok = Rect(dlg.max.x-266, dlg.max.y-54, dlg.max.x-186, dlg.max.y-30);
	cancel = Rect(dlg.max.x-176, dlg.max.y-54, dlg.max.x-96, dlg.max.y-30);
	browse = Rect(dlg.max.x-86, dlg.max.y-54, dlg.max.x-16, dlg.max.y-30);

	backup = allocimage(display, dlg, screen->chan, 0, -1);
	if(backup)
		draw(backup, dlg, screen, nil, dlg.min);

	modalbegin();
	kdown[0] = 0;
	blink = 1;
	done = 0;

Redraw:
	dlgframe(dlg, "Run");
	draw(screen, insetrect(icon, 1), dface, nil, ZP);
	paneldrawicon(insetrect(icon, 2), Irun);
	string(screen, Pt(dlg.min.x+58, dlg.min.y+34), ddark, ZP, font,
		"Type the name of a program, folder or document,");
	string(screen, Pt(dlg.min.x+58, dlg.min.y+50), ddark, ZP, font,
		"and TaijiOS will open it for you.");
	dlgentrydraw(entry, buf, blink);
	panelbutton(ok, "OK", -1, 0);
	panelbutton(cancel, "Cancel", -1, 0);
	panelbutton(browse, "Browse...", -1, 0);
	flushimage(display, 1);

	while(!done){
		blink = 1;
		while(nbrecv(kbctl->c, &ks) == 1){
			switch(dlgkey(ks, buf, &n, sizeof buf)){
			case 1:
				done = 1;
				launchit = 1;
				break;
			case 2:
				done = 1;
				launchit = 0;
				break;
			case 3:	/* Up: older history */
				if(nhist > 0){
					hcur = hcur <= 0 ? 0 : hcur-1;
					if(hcur < nhist){
						n = strlen(hist[hcur]);
						memmove(buf, hist[hcur], n+1);
					}
				}
				break;
			case 4:	/* Down: newer history */
				if(nhist > 0){
					hcur = hcur >= nhist-1 ? nhist-1 : hcur+1;
					n = strlen(hist[hcur]);
					memmove(buf, hist[hcur], n+1);
				}
				break;
			}
			if(done)
				break;
			goto Redraw;
		}
		if(done)
			break;
		{
			Timer *timer;
			static Alt alts[3];
			timer = timerstart(400);
			alts[0] = ALT(timer->c, nil, CHANRCV);
			alts[1] = ALT(mctl->c, &mctl->Mouse, CHANRCV);
			alts[2].op = CHANEND;
			switch(alt(alts)){
			case 0:
				blink = !blink;
				goto Redraw;
			case 1:
				timercancel(timer);
				if(mctl->buttons & 1){
					drainmouse(mctl, nil);
					if(ptinrect(mctl->xy, ok)){
						done = 1;
						launchit = 1;
					}else if(ptinrect(mctl->xy, cancel)){
						done = 1;
						launchit = 0;
					}else if(ptinrect(mctl->xy, browse)){
						strcpy(buf, "explorer /bin");
						done = 1;
						launchit = 1;
					}
				}
				break;
			}
		}
	}

	if(backup){
		draw(screen, dlg, backup, nil, dlg.min);
		freeimage(backup);
		flushimage(display, 1);
	}
	modalend();

	if(launchit && buf[0] != 0){
		/* record in history: dedupe, newest first */
		home = getenv("home");
		if(home != nil){
			path = smprint("%s/lib/runhist", home);
			if(path != nil){
				bp = Bopen(path, OWRITE|OTRUNC);
				if(bp != nil){
					Bprint(bp, "%s\n", buf);
					for(i = 0; i < nhist; i++){
						if(strcmp(hist[i], buf) == 0)
							continue;
						Bprint(bp, "%s\n", hist[i]);
					}
					Bterm(bp);
				}
				free(path);
			}
			free(home);
		}
		panellaunch(buf);
	}
	for(i = 0; i < nhist; i++)
		free(hist[i]);
}

/* ---- calendar (double click on the clock) ---- */

static char *monthname[] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"
};

void
calendardlg(void)
{
	Rectangle dlg, grid, prevb, nextb, closeb;
	Image *backup;
	Tm *tm;
	vlong sec;
	int mon, year, today, tomon, toyear;
	int first, ndays, i, x, y, day, cellw, cellh, done, sel;
	char buf[64];
	char *wdays[] = { "S", "M", "T", "W", "T", "F", "S" };

	dlgcolors();
	tm = localtime(time(0));
	tomon = tm->mon;
	toyear = tm->year+1900;
	today = tm->mday;
	mon = tomon;
	year = toyear;
	sel = 0;

	dlg = Rect((screen->r.min.x+screen->r.max.x)/2-140,
		(screen->r.min.y+screen->r.max.y)/2-120,
		(screen->r.min.x+screen->r.max.x)/2+140,
		(screen->r.min.y+screen->r.max.y)/2+120);
	grid = Rect(dlg.min.x+16, dlg.min.y+62, dlg.max.x-16, dlg.max.y-64);
	prevb = Rect(dlg.min.x+16, dlg.min.y+32, dlg.min.x+34, dlg.min.y+52);
	nextb = Rect(dlg.max.x-34, dlg.min.y+32, dlg.max.x-16, dlg.min.y+52);
	closeb = Rect(dlg.max.x-90, dlg.max.y-46, dlg.max.x-16, dlg.max.y-22);

	backup = allocimage(display, dlg, screen->chan, 0, -1);
	if(backup)
		draw(backup, dlg, screen, nil, dlg.min);

	modalbegin();
	done = 0;

Redraw:
	/* first weekday of the month */
	tm = localtime(time(0));
	tm->year = year-1900;
	tm->mon = mon;
	tm->mday = 1;
	sec = tm2sec(tm);
	tm = localtime(sec);
	first = tm->wday;
	ndays = 31;
	if(mon == 1)
		ndays = (year%4 == 0 && year%100 != 0) || year%400 == 0 ? 29 : 28;
	else if(mon == 3 || mon == 5 || mon == 8 || mon == 10)
		ndays = 30;

	dlgframe(dlg, "Date / Time Properties");
	snprint(buf, sizeof buf, "%s %d", monthname[mon], year);
	string(screen, Pt((dlg.min.x+dlg.max.x)/2 - stringwidth(font, buf)/2,
		dlg.min.y+38), ddark, ZP, font, buf);
	panelbutton(prevb, "<", -1, 0);
	panelbutton(nextb, ">", -1, 0);
	panelbutton(closeb, "OK", -1, 0);

	cellw = Dx(grid)/7;
	cellh = Dy(grid)/7;
	for(i = 0; i < 7; i++){
		string(screen, Pt(grid.min.x+i*cellw+(cellw-stringwidth(font, wdays[i]))/2,
			grid.min.y), dactive, ZP, font, wdays[i]);
	}
	for(day = 1; day <= ndays; day++){
		i = first + day-1;
		x = grid.min.x + (i%7)*cellw;
		y = grid.min.y + (1 + i/7)*cellh;
		snprint(buf, sizeof buf, "%d", day);
		if(day == today && mon == tomon && year == toyear){
			draw(screen, Rect(x+1, y+1, x+cellw-1, y+cellh-1), dactive, nil, ZP);
			string(screen, Pt(x+(cellw-stringwidth(font, buf))/2,
				y+(cellh-font->height)/2), dhilite, ZP, font, buf);
		}else if(day == sel){
			draw(screen, insetrect(Rect(x+1, y+1, x+cellw-1, y+cellh-1), 1), dshadow, nil, ZP);
			string(screen, Pt(x+(cellw-stringwidth(font, buf))/2,
				y+(cellh-font->height)/2), ddark, ZP, font, buf);
		}else{
			string(screen, Pt(x+(cellw-stringwidth(font, buf))/2,
				y+(cellh-font->height)/2), ddark, ZP, font, buf);
		}
	}
	flushimage(display, 1);

	while(!done){
		readmouse(mctl);
		if(!(mctl->buttons & 1))
			continue;
		drainmouse(mctl, nil);
		if(ptinrect(mctl->xy, closeb)){
			done = 1;
			break;
		}
		if(ptinrect(mctl->xy, prevb)){
			if(--mon < 0){
				mon = 11;
				year--;
			}
			sel = 0;
			goto Redraw;
		}
		if(ptinrect(mctl->xy, nextb)){
			if(++mon > 11){
				mon = 0;
				year++;
			}
			sel = 0;
			goto Redraw;
		}
		if(ptinrect(mctl->xy, grid)){
			x = (mctl->xy.x - grid.min.x)/cellw;
			y = (mctl->xy.y - grid.min.y)/cellh - 1;
			i = y*7 + x - first + 1;
			if(i >= 1 && i <= ndays){
				sel = i;
				goto Redraw;
			}
		}
	}

	if(backup){
		draw(screen, dlg, backup, nil, dlg.min);
		freeimage(backup);
		flushimage(display, 1);
	}
	modalend();
}

/* ---- Ctrl+Alt+Del security dialog ---- */

void
secudlg(void)
{
	Rectangle dlg, b[5];
	Image *backup;
	char *bl[5];
	int i, done, sel;

	dlgcolors();
	bl[0] = "Lock Computer";
	bl[1] = "Log Off...";
	bl[2] = "Shut Down...";
	bl[3] = "Task Manager...";
	bl[4] = "Cancel";

	dlg = Rect((screen->r.min.x+screen->r.max.x)/2-190,
		(screen->r.min.y+screen->r.max.y)/2-105,
		(screen->r.min.x+screen->r.max.x)/2+190,
		(screen->r.min.y+screen->r.max.y)/2+105);

	backup = allocimage(display, dlg, screen->chan, 0, -1);
	if(backup)
		draw(backup, dlg, screen, nil, dlg.min);

	modalbegin();
	done = 0;
	sel = -1;

	dlgframe(dlg, "TaijiOS Security");
	string(screen, Pt(dlg.min.x+20, dlg.min.y+34), ddark, ZP, font,
		"Logon Message");
	for(i = 0; i < 5; i++){
		if(i < 4){
			b[i] = Rect(dlg.min.x+24 + (i%2)*170, dlg.min.y+62 + (i/2)*62,
				dlg.min.x+24 + (i%2)*170 + 150, dlg.min.y+62 + (i/2)*62 + 48);
			panelbutton(b[i], bl[i], -1, 0);
		}else{
			b[i] = Rect(dlg.max.x-110, dlg.max.y-52, dlg.max.x-20, dlg.max.y-28);
			panelbutton(b[i], bl[i], -1, 0);
		}
	}
	flushimage(display, 1);

	while(!done){
		readmouse(mctl);
		if(!(mctl->buttons & 1))
			continue;
		drainmouse(mctl, nil);
		for(i = 0; i < 5; i++){
			if(ptinrect(mctl->xy, b[i])){
				sel = i;
				done = 1;
				break;
			}
		}
	}

	if(backup){
		draw(screen, dlg, backup, nil, dlg.min);
		freeimage(backup);
		flushimage(display, 1);
	}
	modalend();

	switch(sel){
	case 0:
		lockscreen();
		break;
	case 1:
		splashlogout();
		break;
	case 2:
		panelshutdlg();
		break;
	case 3:
		panellaunch("q9taskmgr");
		break;
	}
}

/* ---- lock screen ---- */

static ulong
lglerp(ulong c0, ulong c1, int i, int n)
{
	int r, g, b;

	r = ((c0>>24 & 0xFF)*(n-i) + (c1>>24 & 0xFF)*i) / n;
	g = ((c0>>16 & 0xFF)*(n-i) + (c1>>16 & 0xFF)*i) / n;
	b = ((c0>>8 & 0xFF)*(n-i) + (c0>>8 & 0xFF)*i) / n;
	return r<<24 | g<<16 | b<<8 | 0xFF;
}

void
lockscreen(void)
{
	Rectangle dlg, pass, ok;
	char buf[32];
	char *ks;
	int blink, done, n, i;
	static Alt alts[3];
	Timer *timer;

	dlgcolors();
	buf[0] = 0;
	n = 0;

	dlg = Rect((screen->r.min.x+screen->r.max.x)/2-190,
		(screen->r.min.y+screen->r.max.y)/2-95,
		(screen->r.min.x+screen->r.max.x)/2+190,
		(screen->r.min.y+screen->r.max.y)/2+95);
	pass = Rect(dlg.min.x+160, dlg.min.y+78, dlg.max.x-20, dlg.min.y+78+font->height+8);
	ok = Rect(dlg.max.x-120, dlg.max.y-52, dlg.max.x-20, dlg.max.y-28);

	/* cover everything with the logon gradient */
	for(i = 0; i < 32; i++)
		draw(screen, Rect(screen->r.min.x, screen->r.min.y + Dy(screen->r)*i/32,
			screen->r.max.x, screen->r.min.y + Dy(screen->r)*(i+1)/32),
			getcolor(nil, lglerp(kthemecolor("background"), kthemecolor("circle"), i, 31)), nil, ZP);

	modalbegin();
	kdown[0] = 0;
	blink = 1;
	done = 0;
	n = 0;

Redraw:
	dlgframe(dlg, "Computer locked");
	string(screen, Pt(dlg.min.x+20, dlg.min.y+40), ddark, ZP, font,
		"Only glenda or an administrator can unlock");
	string(screen, Pt(dlg.min.x+20, dlg.min.y+56), ddark, ZP, font,
		"this computer.");
	string(screen, Pt(dlg.min.x+20, dlg.min.y+82), ddark, ZP, font, "Password:");
	dlgentrydraw(pass, buf, blink);
	panelbutton(ok, "OK", -1, 0);
	flushimage(display, 1);

	while(!done){
		while(nbrecv(kbctl->c, &ks) == 1){
			if(dlgkey(ks, buf, &n, sizeof buf) == 1){
				done = 1;
				break;
			}
			goto Redraw;
		}
		if(done)
			break;
		timer = timerstart(400);
		alts[0] = ALT(timer->c, nil, CHANRCV);
		alts[1] = ALT(mctl->c, &mctl->Mouse, CHANRCV);
		alts[2].op = CHANEND;
		switch(alt(alts)){
		case 0:
			blink = !blink;
			goto Redraw;
		case 1:
			timercancel(timer);
			if((mctl->buttons & 1) && ptinrect(mctl->xy, ok)){
				drainmouse(mctl, nil);
				done = 1;
			}
			break;
		}
	}
	modalend();

	/* put the desktop back */
	refresh();
}

/* ---- window system menu (title bar icon / right click / Alt+Space) ---- */

void
winsysmenu(Window *w, Rectangle anchor)
{
	char *items[8];
	int sel;

	if(w == nil)
		return;
	items[0] = w->maximized ? "Restore" : "!Restore";
	items[1] = "Move";
	items[2] = "Size";
	items[3] = w->hidden ? "!Minimize" : "Minimize";
	items[4] = w->maximized ? "!Maximize" : "Maximize";
	items[5] = "-";
	items[6] = "Close";
	items[7] = nil;

	sel = winmenuhit(mctl, anchor, items, 7, -1);
	switch(sel){
	case 0:
		if(w->maximized)
			wrestore(w);
		break;
	case 1:
		if(!w->maximized)
			grab(w, 1);
		break;
	case 2:
		sweep(w);
		break;
	case 3:
		animminimize(w);
		whide(w);
		paneldraw();
		break;
	case 4:
		wmaximize(w);
		break;
	case 6:
		wdelete(w);
		paneldraw();
		break;
	}
}

void
animminimize(Window *w)
{
	Rectangle r0, r1, rc;
	int i;

	r0 = w->rect;
	r1 = paneltaskrect(w);
	if(Dx(r1) <= 0 || Dy(r1) <= 0 || eqrect(r0, r1))
		return;
	for(i = 1; i <= 6; i++){
		rc.min.x = r0.min.x + (r1.min.x-r0.min.x)*i/6;
		rc.min.y = r0.min.y + (r1.min.y-r0.min.y)*i/6;
		rc.max.x = r0.max.x + (r1.max.x-r0.max.x)*i/6;
		rc.max.y = r0.max.y + (r1.max.y-r0.max.y)*i/6;
		drawgetrect(rc, 1);
		sleep(16);
		drawgetrect(rc, 0);
	}
	flushimage(display, 1);
}

/* ---- Alt+Tab task switcher ---- */

enum {
	SwMax = 64,
};

static struct {
	Window *w[SwMax];
	int n;
	int sel;
	int open;
	Rectangle r;
	Image *backup;
} sw;

int
switcheractive(void)
{
	return sw.open;
}

static void
switcherdraw(void)
{
	Rectangle ir;
	int i;

	if(!sw.open)
		return;
	for(i = 0; i < sw.n; i++){
		ir = Rect(sw.r.min.x+6, sw.r.min.y+6+i*22, sw.r.max.x-6, sw.r.min.y+6+(i+1)*22);
		draw(screen, ir, i == sw.sel ? dactive : dface, nil, ZP);
		paneldrawicon(Rect(ir.min.x+2, ir.min.y+2, ir.min.x+18, ir.min.y+18), Iprog);
		string(screen, Pt(ir.min.x+26, ir.min.y+(Dy(ir)-font->height)/2),
			i == sw.sel ? dhilite : ddark, ZP, font,
			sw.w[i]->cur ? sw.w[i]->cur->label : "window");
	}
	/* frame on top of items */
	winborder(screen, sw.r, ddark, dhilite);
	flushimage(display, 1);
}

void
switcheropen(void)
{
	Window *w;
	Rectangle wr;
	int h, n;

	if(sw.open)
		return;
	dlgcolors();
	n = 0;
	for(w = topwin; w && n < SwMax; w = w->lower){
		if(w->cur == nil)
			continue;
		sw.w[n++] = w;
	}
	if(n < 2){
		sw.n = 0;
		return;
	}
	sw.n = n;
	sw.sel = 1 % n;

	wr = panelworkrect();
	h = n*22 + 12;
	sw.r = Rect(wr.min.x + Dx(wr)/2 - 150, wr.min.y + Dy(wr)/2 - h/2,
		wr.min.x + Dx(wr)/2 + 150, wr.min.y + Dy(wr)/2 + h/2);
	sw.backup = allocimage(display, sw.r, screen->chan, 0, -1);
	if(sw.backup)
		draw(sw.backup, sw.r, screen, nil, sw.r.min);
	draw(screen, sw.r, dface, nil, ZP);
	sw.open = TRUE;
	switcherdraw();
}

void
switchercycle(int dir)
{
	if(!sw.open)
		return;
	sw.sel = (sw.sel + dir + sw.n) % sw.n;
	switcherdraw();
}

static void
switcherclose(void)
{
	if(sw.backup){
		draw(screen, sw.r, sw.backup, nil, sw.r.min);
		freeimage(sw.backup);
		sw.backup = nil;
		flushimage(display, 1);
	}
	sw.open = FALSE;
	sw.n = 0;
}

void
switchercommit(void)
{
	Window *w;

	if(!sw.open)
		return;
	w = sw.w[sw.sel];
	switcherclose();
	if(w != nil){
		if(w->hidden)
			wunhide(w);
		wraise(w);
		wfocus(w);
		paneldraw();
	}
}

void
switchercancel(void)
{
	if(!sw.open)
		return;
	switcherclose();
}
