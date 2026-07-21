#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

enum {
	Toolbarh = 30,
	Addrh = 24,
	Statush = 22,
	Treew = 170,
	Rowh = 18,
	Btnw = 70,
	Maxpath = 512,
};

typedef struct Entry Entry;
typedef struct TreeItem TreeItem;
struct Entry {
	char name[128];
	char type[16];
	vlong len;
	ulong mtime;
	int isdir;
};
struct TreeItem {
	char *label;
	char *path;
	int indent;
	int y;
	Rectangle r;
};

Image *face, *light, *shadow, *dark, *white, *text, *hilite;
Entry *ents;
int nents;
char cwd[Maxpath];
int scroll;
TreeItem tree[] = {
	{ "Desktop", "/usr/glenda", 10, 10 },
	{ "My Computer", "/", 22, 32 },
	{ "Namespace", "/", 34, 54 },
	{ "/", "/", 46, 76 },
	{ "/mnt", "/mnt", 46, 98 },
	{ "/usr/glenda", "/usr/glenda", 46, 120 },
	{ "Control Panel", "/lib/controlpanel", 34, 144 },
};

Rectangle listrect(void);

void
bevel(Rectangle r, int down)
{
	Image *tl, *br;

	tl = down ? dark : light;
	br = down ? light : dark;
	line(screen, r.min, Pt(r.max.x-1, r.min.y), 0, 0, 0, tl, ZP);
	line(screen, r.min, Pt(r.min.x, r.max.y-1), 0, 0, 0, tl, ZP);
	line(screen, Pt(r.min.x, r.max.y-1), subpt(r.max, Pt(1,1)), 0, 0, 0, br, ZP);
	line(screen, Pt(r.max.x-1, r.min.y), subpt(r.max, Pt(1,1)), 0, 0, 0, br, ZP);
}

void
btn(Rectangle r, char *s)
{
	draw(screen, r, face, nil, ZP);
	bevel(r, 0);
	string(screen, Pt(r.min.x+8, r.min.y+(Dy(r)-font->height)/2), text, ZP, font, s);
}

int
controlpanel(void)
{
	return strcmp(cwd, "/lib/controlpanel") == 0;
}

void
drawcpicon(Rectangle r, char *name)
{
	Point c;

	c = Pt((r.min.x+r.max.x)/2, r.min.y+22);
	if(strcmp(name, "Display") == 0){
		draw(screen, Rect(c.x-15, c.y-10, c.x+15, c.y+8), hilite, nil, ZP);
		border(screen, Rect(c.x-15, c.y-10, c.x+15, c.y+8), 2, dark, ZP);
		draw(screen, Rect(c.x-5, c.y+9, c.x+5, c.y+13), dark, nil, ZP);
	}else if(strcmp(name, "Keyboard") == 0){
		draw(screen, Rect(c.x-16, c.y-8, c.x+16, c.y+11), white, nil, ZP);
		border(screen, Rect(c.x-16, c.y-8, c.x+16, c.y+11), 1, dark, ZP);
		line(screen, Pt(c.x-12, c.y-1), Pt(c.x+12, c.y-1), 0, 0, 0, dark, ZP);
		line(screen, Pt(c.x-12, c.y+5), Pt(c.x+12, c.y+5), 0, 0, 0, dark, ZP);
	}else if(strcmp(name, "Mouse") == 0){
		ellipse(screen, c, 10, 15, 1, white, ZP);
		ellipse(screen, c, 10, 15, 0, dark, ZP);
		line(screen, Pt(c.x, c.y-12), Pt(c.x, c.y-2), 0, 0, 0, dark, ZP);
	}else if(strcmp(name, "Network") == 0){
		ellipse(screen, Pt(c.x-11, c.y-5), 5, 5, 1, hilite, ZP);
		ellipse(screen, Pt(c.x+11, c.y-5), 5, 5, 1, hilite, ZP);
		ellipse(screen, Pt(c.x, c.y+10), 5, 5, 1, hilite, ZP);
		line(screen, Pt(c.x-7, c.y-1), Pt(c.x-2, c.y+6), 0, 0, 0, dark, ZP);
		line(screen, Pt(c.x+7, c.y-1), Pt(c.x+2, c.y+6), 0, 0, 0, dark, ZP);
	}else if(strcmp(name, "Date-Time") == 0){
		ellipse(screen, c, 14, 14, 1, white, ZP);
		ellipse(screen, c, 14, 14, 0, dark, ZP);
		line(screen, c, Pt(c.x, c.y-9), 0, 0, 0, dark, ZP);
		line(screen, c, Pt(c.x+8, c.y+4), 0, 0, 0, dark, ZP);
	}else if(strcmp(name, "Users") == 0){
		ellipse(screen, Pt(c.x, c.y-7), 6, 6, 1, hilite, ZP);
		ellipse(screen, Pt(c.x, c.y-7), 6, 6, 0, dark, ZP);
		draw(screen, Rect(c.x-13, c.y+3, c.x+13, c.y+15), hilite, nil, ZP);
	}else if(strcmp(name, "Fonts") == 0){
		string(screen, Pt(c.x-12, c.y-12), dark, ZP, font, "A");
		string(screen, Pt(c.x, c.y-3), dark, ZP, font, "a");
	}else{
		draw(screen, Rect(c.x-14, c.y-13, c.x+14, c.y+13), white, nil, ZP);
		border(screen, Rect(c.x-14, c.y-13, c.x+14, c.y+13), 1, dark, ZP);
		line(screen, Pt(c.x-9, c.y-5), Pt(c.x+9, c.y-5), 0, 0, 0, dark, ZP);
		line(screen, Pt(c.x-9, c.y+2), Pt(c.x+9, c.y+2), 0, 0, 0, dark, ZP);
		line(screen, Pt(c.x-9, c.y+9), Pt(c.x+9, c.y+9), 0, 0, 0, dark, ZP);
	}
}

void
runentry(char *path)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "new -r 90 100 690 470 rc %s", path);
	close(fd);
}

int
cpitemat(Point p)
{
	Rectangle lr, r;
	int i, x, y;

	lr = listrect();
	x = lr.min.x + 20;
	y = lr.min.y + 20;
	for(i = 0; i < nents; i++){
		if(strcmp(ents[i].name, "..") == 0)
			continue;
		r = Rect(x, y, x+108, y+72);
		if(ptinrect(p, r))
			return i;
		x += 122;
		if(x+108 > lr.max.x-12){
			x = lr.min.x + 20;
			y += 84;
		}
		if(y+72 > lr.max.y-8)
			break;
	}
	return -1;
}

void
cleanpath(char *dst, int ndst, char *base, char *name)
{
	char buf[Maxpath];

	if(strcmp(name, "..") == 0){
		strecpy(buf, buf+sizeof buf, base);
		cleanname(buf);
		if(strcmp(buf, "/") != 0)
			*strrchr(buf, '/') = 0;
		if(buf[0] == 0)
			strecpy(buf, buf+sizeof buf, "/");
	}else if(strcmp(base, "/") == 0)
		snprint(buf, sizeof buf, "/%s", name);
	else
		snprint(buf, sizeof buf, "%s/%s", base, name);
	cleanname(buf);
	strecpy(dst, dst+ndst, buf);
}

int
loaddir(char *path)
{
	Dir *d;
	int fd, i, n, off;

	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	n = dirreadall(fd, &d);
	close(fd);
	if(n < 0)
		return -1;
	free(ents);
	ents = nil;
	nents = 0;
	ents = malloc((n+1)*sizeof(Entry));
	if(ents == nil){
		free(d);
		return -1;
	}
	off = 0;
	strecpy(ents[off].name, ents[off].name+sizeof ents[off].name, "..");
	strecpy(ents[off].type, ents[off].type+sizeof ents[off].type, "Folder");
	ents[off].isdir = 1;
	ents[off].len = 0;
	ents[off].mtime = 0;
	off++;
	for(i = 0; i < n; i++){
		strecpy(ents[off].name, ents[off].name+sizeof ents[off].name, d[i].name);
		strecpy(ents[off].type, ents[off].type+sizeof ents[off].type,
			(d[i].mode&DMDIR) ? "Folder" : "File");
		ents[off].isdir = (d[i].mode&DMDIR) != 0;
		ents[off].len = d[i].length;
		ents[off].mtime = d[i].mtime;
		off++;
	}
	nents = off;
	scroll = 0;
	strecpy(cwd, cwd+sizeof cwd, path);
	free(d);
	return 0;
}

Rectangle
listrect(void)
{
	Rectangle r;

	r = screen->r;
	r.min.y += Toolbarh + Addrh;
	r.max.y -= Statush;
	r.min.x += Treew;
	return r;
}

void
drawtree(Rectangle r)
{
	int i;
	Point p;

	draw(screen, r, white, nil, ZP);
	bevel(r, 1);
	for(i = 0; i < nelem(tree); i++){
		p = Pt(r.min.x+tree[i].indent, r.min.y+tree[i].y);
		tree[i].r = Rect(r.min.x+4, p.y-3, r.max.x-4, p.y+font->height+3);
		if(strcmp(cwd, tree[i].path) == 0){
			draw(screen, tree[i].r, hilite, nil, ZP);
			border(screen, tree[i].r, 1, shadow, ZP);
		}
		string(screen, p, text, ZP, font, tree[i].label);
	}
}

void
redraw(void)
{
	Rectangle r, lr, row;
	char buf[128];
	int i, y, x;

	draw(screen, screen->r, face, nil, ZP);
	r = Rect(screen->r.min.x, screen->r.min.y, screen->r.max.x, screen->r.min.y+Toolbarh);
	draw(screen, r, face, nil, ZP);
	btn(Rect(r.min.x+6, r.min.y+4, r.min.x+6+Btnw, r.max.y-4), "Back");
	btn(Rect(r.min.x+82, r.min.y+4, r.min.x+82+Btnw, r.max.y-4), "Up");
	btn(Rect(r.min.x+158, r.min.y+4, r.min.x+158+Btnw+12, r.max.y-4), "Refresh");
	btn(Rect(r.min.x+252, r.min.y+4, r.min.x+252+Btnw+18, r.max.y-4), "New Dir");

	r = Rect(screen->r.min.x, screen->r.min.y+Toolbarh, screen->r.max.x, screen->r.min.y+Toolbarh+Addrh);
	draw(screen, r, face, nil, ZP);
	string(screen, Pt(r.min.x+8, r.min.y+5), text, ZP, font, "Address");
	r.min.x += 70;
	r.max.x -= 8;
	draw(screen, insetrect(r, 3), white, nil, ZP);
	bevel(insetrect(r, 3), 1);
	string(screen, Pt(r.min.x+9, r.min.y+6), text, ZP, font, cwd);

	r = screen->r;
	r.min.y += Toolbarh + Addrh;
	r.max.y -= Statush;
	r.max.x = r.min.x + Treew;
	drawtree(r);

	lr = listrect();
	draw(screen, lr, white, nil, ZP);
	bevel(lr, 1);
	if(controlpanel()){
		x = lr.min.x + 20;
		y = lr.min.y + 20;
		for(i = 0; i < nents; i++){
			if(strcmp(ents[i].name, "..") == 0)
				continue;
			row = Rect(x, y, x+108, y+72);
			if(i == scroll){
				draw(screen, row, hilite, nil, ZP);
				border(screen, row, 1, dark, ZP);
			}
			drawcpicon(row, ents[i].name);
			string(screen, Pt(row.min.x+(Dx(row)-stringwidth(font, ents[i].name))/2,
				row.min.y+48), text, ZP, font, ents[i].name);
			x += 122;
			if(x+108 > lr.max.x-12){
				x = lr.min.x + 20;
				y += 84;
			}
			if(y+72 > lr.max.y-8)
				break;
		}
		goto Status;
	}
	row = Rect(lr.min.x+4, lr.min.y+4, lr.max.x-4, lr.min.y+24);
	draw(screen, row, face, nil, ZP);
	string(screen, Pt(row.min.x+8, row.min.y+4), text, ZP, font, "Name");
	string(screen, Pt(row.min.x+230, row.min.y+4), text, ZP, font, "Size");
	string(screen, Pt(row.min.x+320, row.min.y+4), text, ZP, font, "Type");
	y = row.max.y + 3;
	for(i = scroll; i < nents && y+Rowh < lr.max.y-4; i++){
		row = Rect(lr.min.x+4, y, lr.max.x-4, y+Rowh);
		if(i == scroll)
			draw(screen, row, hilite, nil, ZP);
		string(screen, Pt(row.min.x+8, row.min.y+3), text, ZP, font, ents[i].isdir ? "[ ]" : " - ");
		string(screen, Pt(row.min.x+34, row.min.y+3), text, ZP, font, ents[i].name);
		if(ents[i].isdir)
			snprint(buf, sizeof buf, "");
		else
			snprint(buf, sizeof buf, "%lld", ents[i].len);
		string(screen, Pt(row.min.x+230, row.min.y+3), text, ZP, font, buf);
		string(screen, Pt(row.min.x+320, row.min.y+3), text, ZP, font, ents[i].type);
		y += Rowh;
	}

Status:
	r = Rect(screen->r.min.x, screen->r.max.y-Statush, screen->r.max.x, screen->r.max.y);
	draw(screen, r, face, nil, ZP);
	if(controlpanel())
		snprint(buf, sizeof buf, "%d item%s", nents-1, nents==2 ? "" : "s");
	else
		snprint(buf, sizeof buf, "%d object%s", nents, nents==1 ? "" : "s");
	string(screen, Pt(r.min.x+8, r.min.y+4), text, ZP, font, buf);
	flushimage(display, 1);
}

void
openrow(Point p)
{
	Rectangle lr;
	int i;
	char path[Maxpath];

	lr = listrect();
	if(!ptinrect(p, lr))
		return;
	if(controlpanel()){
		i = cpitemat(p);
		if(i >= 0){
			cleanpath(path, sizeof path, cwd, ents[i].name);
			runentry(path);
		}
		return;
	}
	i = scroll + (p.y - (lr.min.y+27))/Rowh;
	if(i < 0 || i >= nents || !ents[i].isdir)
		return;
	cleanpath(path, sizeof path, cwd, ents[i].name);
	if(loaddir(path) == 0)
		redraw();
}

int
opentree(Point p)
{
	int i;

	for(i = 0; i < nelem(tree); i++)
		if(ptinrect(p, tree[i].r)){
			if(loaddir(tree[i].path) == 0)
				redraw();
			return 1;
		}
	return 0;
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		exits("resize");
	redraw();
}

void
main(int argc, char **argv)
{
	Event e;
	char path[Maxpath];

	ARGBEGIN{
	default:
		break;
	}ARGEND
	if(argc > 0)
		strecpy(path, path+sizeof path, argv[0]);
	else if(getwd(path, sizeof path) == nil)
		strecpy(path, path+sizeof path, "/");
	cleanname(path);
	if(initdraw(nil, nil, "explorer") < 0)
		sysfatal("initdraw: %r");
	face = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xC0C0C0FF);
	light = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFFFFFFF);
	shadow = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x808080FF);
	dark = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);
	white = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFFFFFFF);
	text = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x000000FF);
	hilite = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xE8E8E8FF);
	einit(Emouse|Ekeyboard);
	if(loaddir(path) < 0)
		sysfatal("open %s: %r", path);
	redraw();
	for(;;){
		switch(event(&e)){
		case Emouse:
			if(e.mouse.buttons & 1)
				if(!opentree(e.mouse.xy))
					openrow(e.mouse.xy);
			if(e.mouse.buttons & 8 && scroll > 0){
				scroll--;
				redraw();
			}
			if(e.mouse.buttons & 16 && scroll+1 < nents){
				scroll++;
				redraw();
			}
			break;
		case Ekeyboard:
			if(e.kbdc == 'q' || e.kbdc == Kdel)
				exits(nil);
			break;
		}
	}
}
