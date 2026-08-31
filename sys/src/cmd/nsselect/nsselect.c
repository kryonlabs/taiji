#include <u.h>
#include <libc.h>
#include "kryon.h"

/* Namespace selector for the TaijiOS boot.  Reads a session catalog
 * (id|name|description|command per line), draws the choices with the
 * Kryon runtime so the picker carries the system theme, and launches
 * the chosen command through a pre-forked rc launcher that owns the
 * namespace/session environment.  Choosing nothing (Esc) exits with a
 * non-empty status so the boot profile can drop to a shell. */

enum {
	Maxsessions = 16,
	Fieldlen = 128,
	Cmdlen = 256,
	Buflen = 8192,
	Cardw = 300,
	Cardh = 118,
	Cardgap = 22
};

typedef struct Session Session;
typedef struct Launch Launch;
struct Session {
	char id[Fieldlen];
	char name[Fieldlen];
	char desc[Fieldlen];
	char command[Cmdlen];
};
struct Launch {
	char id[Fieldlen];
	char command[Cmdlen];
};

Session sessions[Maxsessions];
int nsessions;
int selected;
int launchfd = -1;
int launchpid = -1;

char *catalog = "/lib/namespace.sessions";

void
trim(char *s)
{
	char *e;

	while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		memmove(s, s+1, strlen(s));
	e = s+strlen(s);
	while(e > s && (e[-1] == ' ' || e[-1] == '\t' ||
	    e[-1] == '\r' || e[-1] == '\n'))
		*--e = 0;
}

void
copystr(char *dst, int ndst, char *src)
{
	int n;

	if(ndst <= 0)
		return;
	dst[0] = 0;
	if(src == nil)
		return;
	n = strlen(src);
	if(n >= ndst)
		n = ndst-1;
	memmove(dst, src, n);
	dst[n] = 0;
}

int
loadsessions(char *path)
{
	char buf[Buflen+1], *lines[Maxsessions*2], *fields[4];
	int fd, n, nl, nf, i;

	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	n = read(fd, buf, Buflen);
	close(fd);
	if(n < 0)
		return -1;
	buf[n] = 0;
	nl = getfields(buf, lines, nelem(lines), 1, "\n");
	nsessions = 0;
	for(i = 0; i < nl && nsessions < Maxsessions; i++){
		trim(lines[i]);
		if(lines[i][0] == 0 || lines[i][0] == '#')
			continue;
		nf = getfields(lines[i], fields, nelem(fields), 0, "|");
		if(nf < 4)
			continue;
		trim(fields[0]);
		trim(fields[1]);
		trim(fields[2]);
		trim(fields[3]);
		if(fields[0][0] == 0 || fields[1][0] == 0 || fields[3][0] == 0)
			continue;
		copystr(sessions[nsessions].id, sizeof sessions[nsessions].id,
		    fields[0]);
		copystr(sessions[nsessions].name, sizeof sessions[nsessions].name,
		    fields[1]);
		copystr(sessions[nsessions].desc, sizeof sessions[nsessions].desc,
		    fields[2]);
		copystr(sessions[nsessions].command,
		    sizeof sessions[nsessions].command, fields[3]);
		nsessions++;
	}
	return nsessions;
}

void
waitmousefree(void)
{
	int fd, i;

	for(i = 0; i < 50; i++){
		fd = open("/dev/mouse", ORDWR);
		if(fd >= 0){
			close(fd);
			return;
		}
		sleep(100);
	}
}

int
readfull(int fd, void *buf, int n)
{
	char *p;
	int r, total;

	p = buf;
	total = 0;
	while(total < n){
		r = read(fd, p + total, n - total);
		if(r <= 0)
			return total;
		total += r;
	}
	return total;
}

int
writefull(int fd, void *buf, int n)
{
	char *p;
	int r, total;

	p = buf;
	total = 0;
	while(total < n){
		r = write(fd, p + total, n - total);
		if(r <= 0)
			return total;
		total += r;
	}
	return total;
}

void
launcher(int fd)
{
	Launch l;

	memset(&l, 0, sizeof l);
	if(readfull(fd, &l, sizeof l) != sizeof l)
		exits(nil);
	close(fd);
	if(l.id[0] == 0 || l.command[0] == 0)
		exits("empty launch");
	putenv("namespace", l.id);
	putenv("session", l.id);
	waitmousefree();
	execl("/bin/rc", "rc", "-c", l.command, nil);
	sysfatal("exec %s failed: %r", l.command);
}

void
startlauncher(void)
{
	int fd[2], pid;

	if(pipe(fd) < 0)
		sysfatal("launcher pipe failed: %r");
	switch(pid = rfork(RFPROC|RFFDG|RFENVG|RFNOTEG)){
	case -1:
		sysfatal("launcher rfork failed: %r");
	case 0:
		close(fd[1]);
		launcher(fd[0]);
		exits(nil);
	default:
		close(fd[0]);
		launchfd = fd[1];
		launchpid = pid;
		break;
	}
}

void
waitsession(void)
{
	Waitmsg *w;

	while((w = wait()) != nil){
		if(w->pid == launchpid){
			if(w->msg[0] != 0)
				exits(w->msg);
			free(w);
			return;
		}
		free(w);
	}
}

Color
opaque_color(Color color)
{
	color.a = 255;
	return color;
}

int
cardcols(void)
{
	return nsessions > 1 ? 2 : 1;
}

Rectangle
cardrect(int i)
{
	int cols, rows, totalw, totalh;
	Rectangle r;

	cols = cardcols();
	rows = (nsessions + cols - 1) / cols;
	totalw = cols*Cardw + (cols-1)*Cardgap;
	totalh = rows*Cardh + (rows-1)*Cardgap;
	r.x = (GetScreenWidth() - totalw)/2 + (i%cols)*(Cardw+Cardgap);
	r.y = (GetScreenHeight() - totalh)/2 + 24 + (i/cols)*(Cardh+Cardgap);
	r.width = Cardw;
	r.height = Cardh;
	return r;
}

void
drawcentered(char *s, int y, int size, Color color)
{
	int x;

	x = GetScreenWidth()/2 - MeasureText(s, size)/2;
	DrawText(s, x, y, size, color);
}

/* Returns the session index picked by a click this frame, or -1. */
int
drawsession(int i)
{
	Rectangle r, stripe;
	Vector2 mouse;
	Color fill;
	int hover;

	r = cardrect(i);
	mouse = GetMousePosition();
	hover = CheckCollisionPointRec(mouse, r);
	if(i == selected)
		fill = opaque_color(GetThemeButton());
	else
		fill = opaque_color(GetThemeSurface());
	DrawRectangleRounded(r, 0.07f, 8, fill);
	if(hover && i != selected){
		DrawRectangleRounded(r, 0.07f, 8, Fade(GetThemeButtonHover(), 0.38f));
	}
	stripe = r;
	stripe.x += 14;
	stripe.y += 14;
	stripe.width = 44;
	stripe.height = 6;
	DrawRectangleRec(stripe, i == selected ?
	    opaque_color(GetThemeLink()) : Fade(GetThemeLink(), 0.55f));
	DrawText(sessions[i].name, (int)r.x + 16, (int)r.y + 34, 21,
	    opaque_color(GetThemeText()));
	DrawText(sessions[i].desc, (int)r.x + 16, (int)r.y + 64, 14,
	    Fade(GetThemeText(), 0.62f));
	DrawText(sessions[i].id, (int)r.x + 16, (int)r.y + 86, 13,
	    Fade(GetThemeText(), 0.45f));
	if(hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		return i;
	return -1;
}

void
startsession(Session *s)
{
	Launch l;

	memset(&l, 0, sizeof l);
	copystr(l.id, sizeof l.id, s->id);
	copystr(l.command, sizeof l.command, s->command);
	CloseWindow();
	if(launchfd < 0 || writefull(launchfd, &l, sizeof l) != sizeof l)
		sysfatal("launch %s failed: %r", s->command);
	close(launchfd);
	launchfd = -1;
	waitsession();
	exits(nil);
}

void
usage(void)
{
	fprint(2, "usage: nsselect [-c catalog]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int i, pick, blocktop;

	ARGBEGIN{
	case 'c':
		catalog = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND
	if(argc != 0)
		usage();

	if(loadsessions(catalog) <= 0)
		sysfatal("no namespace sessions in %s", catalog);
	startlauncher();

	SetSingleInstance(0);
	InitWindow(640, 480, "namespace");
	if(!IsWindowReady())
		sysfatal("kryon window failed: %r");
	EnableEventWaiting();
	SetTargetFPS(30);
	SetUIDefaultFontAutoLoad(1);
	RefreshSystemTheme();
	SetThemeSource(THEME_SOURCE_SYSTEM);
	SetThemeMode(THEME_MODE_SYSTEM);
	SetThemeStyle(THEME_STYLE_SYSTEM);
	SetCurrentTheme(GetDefaultThemeForThemeStyle(THEME_STYLE_SYSTEM),
	    SystemThemePrefersDark());
	ApplyCurrentUITheme();

	while(!WindowShouldClose()){
		if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)){
			if(selected > 0)
				selected--;
		}
		if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN) ||
		    IsKeyPressed(KEY_TAB)){
			if(selected < nsessions-1)
				selected++;
		}
		if(IsKeyPressed(KEY_ESCAPE)){
			CloseWindow();
			exits("cancel");
		}
		if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
			startsession(&sessions[selected]);

		pick = -1;
		BeginDrawing();
		ClearBackground(opaque_color(GetThemeBackground()));
		BeginUIFrame(GetScreenWidth(), GetScreenHeight(), 1.0f);
		BeginUI(0x6e73656c);
		blocktop = GetScreenHeight()/2 - 24;
		drawcentered("Select namespace", blocktop - 96, 30,
		    opaque_color(GetThemeText()));
		drawcentered("Choose the session namespace to start",
		    blocktop - 56, 15, Fade(GetThemeText(), 0.62f));
		for(i = 0; i < nsessions; i++){
			if(drawsession(i) >= 0){
				selected = i;
				pick = i;
			}
		}
		drawcentered("Enter starts  |  arrows move  |  Esc exits",
		    GetScreenHeight() - 46, 14, Fade(GetThemeText(), 0.55f));
		EndUI();
		EndUIFrame();
		EndDrawing();
		if(pick >= 0)
			startsession(&sessions[pick]);
	}
	CloseWindow();
	exits("cancel");
}
