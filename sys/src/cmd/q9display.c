#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

/*
 * Kryon-native Display Properties: tabs for Background, Screen Saver
 * and Settings. Background picks a rio wallpaper, Screen Saver sets the
 * idle timeout; both talk to rio through wctl messages. Appearance
 * (theming) lives in the dedicated q9themes applet. Settings is
 * informational.
 */

enum {
	Tabh = 26,
	Btnw = 74,
	Btnh = 26,
	Rowh = 20,
	NTabs = 3,
};

char *tabnames[NTabs] = {
	"Background",
	"Screen Saver",
	"Settings",
};

char *wallpapers[] = {
	"teal",
	"gradient",
	"night",
	"slate",
	nil,
};

char *savers[] = {
	"None",
	"1 minute",
	"5 minutes",
	"10 minutes",
	"30 minutes",
	nil,
};

int savminutes[] = { 0, 1, 5, 10, 30 };

Image *face, *light, *shadow, *dark, *white, *text, *hilite, *navy, *teal;
int tab;
int wallsel, savsel;
int dirty;

char curwall[64];
int cursaver;

static void
readcurrents(void)
{
	char *home, *path, buf[128];
	int fd, n;

	strecpy(curwall, curwall+sizeof curwall, "teal");
	cursaver = 0;
	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/wallpaper", home);
	if(path != nil){
		fd = open(path, OREAD);
		if(fd >= 0){
			n = read(fd, buf, sizeof(buf)-1);
			if(n > 0){
				buf[n] = 0;
				while(n > 0 && (buf[n-1] == '\n' || buf[n-1] == ' '))
					buf[--n] = 0;
				if(buf[0])
					strecpy(curwall, curwall+sizeof curwall, buf);
			}
			close(fd);
		}
		free(path);
	}
	path = smprint("%s/lib/saver", home);
	if(path != nil){
		fd = open(path, OREAD);
		if(fd >= 0){
			n = read(fd, buf, sizeof(buf)-1);
			if(n > 0){
				buf[n] = 0;
				cursaver = atoi(buf);
			}
			close(fd);
		}
		free(path);
	}
	free(home);
}

/* look up a color rio published as a th_ named image, fall back to stock */
static Image*
thimg(char *name, ulong col)
{
	Image *ex;
	char *n;

	n = smprint("th_%s", name);
	ex = n != nil ? namedimage(display, n) : nil;
	free(n);
	if(ex != nil)
		return ex;
	return allocimage(display, Rect(0,0,1,1), RGBA32, 1, col);
}

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

static int
indexof(char **list, char *val)
{
	int i;

	for(i = 0; list[i]; i++)
		if(strcmp(list[i], val) == 0)
			return i;
	return 0;
}

static void
apply(void)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "wallpaper %s", wallpapers[wallsel]);
	fprint(fd, "saver %d", savminutes[savsel]);
	close(fd);
}

static void
drawmonitor(Rectangle r)
{
	/* little preview monitor */
	border(screen, r, 2, dark, ZP);
	draw(screen, insetrect(r, 2), face, nil, ZP);
	draw(screen, Rect(r.min.x+8, r.min.y+8, r.max.x-8, r.min.y+8+(Dy(r)-24)*2/3),
		wallsel == 0 ? teal : wallsel == 1 ? navy : wallsel == 2 ? dark : hilite, nil, ZP);
	if(wallsel == 1){
		/* fake gradient bands */
		int i;
		Rectangle sr;
		Rectangle scr = Rect(r.min.x+8, r.min.y+8, r.max.x-8, r.min.y+8+(Dy(r)-24)*2/3);
		for(i = 0; i < 6; i++){
			sr = Rect(scr.min.x, scr.min.y + Dy(scr)*i/6, scr.max.x, scr.min.y + Dy(scr)*(i+1)/6);
			draw(screen, sr, i%2 ? hilite : navy, nil, ZP);
		}
	}
	if(wallsel == 2){
		int i;
		Rectangle scr = Rect(r.min.x+8, r.min.y+8, r.max.x-8, r.min.y+8+(Dy(r)-24)*2/3);
		for(i = 0; i < 12; i++)
			draw(screen, Rect(scr.min.x+7+i*11, scr.min.y+5+(i*7)%(Dy(scr)-8),
				scr.min.x+8+i*11, scr.min.y+6+(i*7)%(Dy(scr)-8)), white, nil, ZP);
	}
	draw(screen, Rect(r.min.x+8, r.min.y+10+(Dy(r)-24)*2/3, r.max.x-8, r.min.y+12+(Dy(r)-24)*2/3),
		face, nil, ZP);
	/* mini taskbar */
	draw(screen, Rect(r.min.x+8, r.max.y-14, r.max.x-8, r.max.y-8), face, nil, ZP);
	draw(screen, Rect(r.min.x+9, r.max.y-13, r.min.x+30, r.max.y-9), navy, nil, ZP);
}

static void
listrects(Rectangle *listr, int n)
{
	int i;

	for(i = 0; i < n; i++)
		listr[i] = Rect(screen->r.min.x+30, screen->r.min.y+70+i*(Rowh+4),
			screen->r.min.x+30+220, screen->r.min.y+70+(i+1)*(Rowh+4)-4);
}

void
redraw(void)
{
	Rectangle r, lr[16];
	char buf[128];
	char *vg;
	int i, n;

	draw(screen, screen->r, face, nil, ZP);

	/* tab strip */
	for(i = 0; i < NTabs; i++){
		r = Rect(screen->r.min.x+10+i*130, screen->r.min.y+8,
			screen->r.min.x+10+i*130+126, screen->r.min.y+8+Tabh);
		if(i == tab){
			draw(screen, r, white, nil, ZP);
			border(screen, r, 1, dark, ZP);
			string(screen, Pt(r.min.x+(Dx(r)-stringwidth(font, tabnames[i]))/2,
				r.min.y+(Tabh-font->height)/2), text, ZP, font, tabnames[i]);
		}else{
			draw(screen, r, face, nil, ZP);
			bevel(r, 0);
			string(screen, Pt(r.min.x+(Dx(r)-stringwidth(font, tabnames[i]))/2,
				r.min.y+(Tabh-font->height)/2), shadow, ZP, font, tabnames[i]);
		}
	}
	r = Rect(screen->r.min.x+10, screen->r.min.y+8+Tabh-1, screen->r.max.x-10, screen->r.max.y-46);
	draw(screen, r, white, nil, ZP);
	border(screen, r, 1, dark, ZP);

	switch(tab){
	case 0:
		string(screen, Pt(screen->r.min.x+30, screen->r.min.y+48), text, ZP, font, "Wallpaper:");
		drawmonitor(Rect(screen->r.max.x-190, screen->r.min.y+60,
			screen->r.max.x-50, screen->r.min.y+220));
		n = nelem(wallpapers)-1;
		listrects(lr, n);
		for(i = 0; i < n; i++){
			if(i == wallsel){
				draw(screen, lr[i], navy, nil, ZP);
				string(screen, Pt(lr[i].min.x+6, lr[i].min.y+(Rowh-font->height)/2),
					white, ZP, font, wallpapers[i]);
			}else{
				bevel(lr[i], 1);
				draw(screen, insetrect(lr[i], 1), white, nil, ZP);
				string(screen, Pt(lr[i].min.x+6, lr[i].min.y+(Rowh-font->height)/2),
					text, ZP, font, wallpapers[i]);
			}
		}
		break;
	case 1:
		string(screen, Pt(screen->r.min.x+30, screen->r.min.y+48), text, ZP, font, "Screen saver:");
		n = nelem(savers)-1;
		listrects(lr, n);
		for(i = 0; i < n; i++){
			if(i == savsel){
				draw(screen, lr[i], navy, nil, ZP);
				string(screen, Pt(lr[i].min.x+6, lr[i].min.y+(Rowh-font->height)/2),
					white, ZP, font, savers[i]);
			}else{
				bevel(lr[i], 1);
				draw(screen, insetrect(lr[i], 1), white, nil, ZP);
				string(screen, Pt(lr[i].min.x+6, lr[i].min.y+(Rowh-font->height)/2),
					text, ZP, font, savers[i]);
			}
		}
		string(screen, Pt(screen->r.min.x+280, screen->r.min.y+60), text, ZP, font,
			"After the idle timeout rio blanks the");
		string(screen, Pt(screen->r.min.x+280, screen->r.min.y+78), text, ZP, font,
			"screen with a bouncing Plan 9 flag.");
		break;
	case 2:
		string(screen, Pt(screen->r.min.x+30, screen->r.min.y+56), text, ZP, font, "Resolution:");
		vg = getenv("vgasize");
		snprint(buf, sizeof buf, "%s (fixed by vgasize)", vg ? vg : "unknown");
		free(vg);
		string(screen, Pt(screen->r.min.x+30, screen->r.min.y+76), text, ZP, font, buf);
		string(screen, Pt(screen->r.min.x+30, screen->r.min.y+106), text, ZP, font, "Colors: True color (32 bit)");
		string(screen, Pt(screen->r.min.x+30, screen->r.min.y+126), text, ZP, font, "Display: Plan 9 devdraw");
		break;
	}

	btn(Rect(screen->r.max.x-260, screen->r.max.y-40, screen->r.max.x-260+Btnw, screen->r.max.y-40+Btnh), "OK");
	btn(Rect(screen->r.max.x-170, screen->r.max.y-40, screen->r.max.x-170+Btnw, screen->r.max.y-40+Btnh), "Cancel");
	btn(Rect(screen->r.max.x-80, screen->r.max.y-40, screen->r.max.x-80+Btnw, screen->r.max.y-40+Btnh), "Apply");
	flushimage(display, 1);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		exits("resize");
	redraw();
}

static int
tabat(Point p)
{
	int i;
	Rectangle r;

	for(i = 0; i < NTabs; i++){
		r = Rect(screen->r.min.x+10+i*130, screen->r.min.y+8,
			screen->r.min.x+10+i*130+126, screen->r.min.y+8+Tabh);
		if(ptinrect(p, r))
			return i;
	}
	return -1;
}

void
main(int argc, char **argv)
{
	Event e;
	Rectangle lr[16];
	int i, n, buttons;

	ARGBEGIN{
	default:
		break;
	}ARGEND
	if(initdraw(nil, nil, "q9display") < 0)
		sysfatal("initdraw: %r");
	/* follow the live Kryon theme rio publishes as th_ named images */
	face = thimg("3d_face", 0xC0C0C0FF);
	light = thimg("3d_hilight2", 0xFFFFFFFF);
	shadow = thimg("3d_shadow1", 0x808080FF);
	dark = thimg("3d_shadow2", 0x000000FF);
	white = thimg("back", 0xFFFFFFFF);
	text = thimg("text", 0x000000FF);
	hilite = thimg("high", 0xE8E8E8FF);
	navy = thimg("titlebar_active", 0x000080FF);
	teal = thimg("circle", 0x008080FF);
	einit(Emouse|Ekeyboard);

	readcurrents();
	wallsel = indexof(wallpapers, curwall);
	savsel = 0;
	for(i = 0; i < nelem(savminutes); i++)
		if(savminutes[i] == cursaver)
			savsel = i;

	redraw();
	buttons = 0;
	for(;;){
		switch(event(&e)){
		case Emouse:
			if((e.mouse.buttons & 1) && !(buttons & 1)){
				i = tabat(e.mouse.xy);
				if(i >= 0 && i != tab){
					tab = i;
					redraw();
					break;
				}
				if(ptinrect(e.mouse.xy, Rect(screen->r.max.x-260, screen->r.max.y-40,
					screen->r.max.x-260+Btnw, screen->r.max.y-40+Btnh))){
					apply();
					exits(nil);
				}
				if(ptinrect(e.mouse.xy, Rect(screen->r.max.x-170, screen->r.max.y-40,
					screen->r.max.x-170+Btnw, screen->r.max.y-40+Btnh)))
					exits(nil);
				if(ptinrect(e.mouse.xy, Rect(screen->r.max.x-80, screen->r.max.y-40,
					screen->r.max.x-80+Btnw, screen->r.max.y-40+Btnh))){
					apply();
					break;
				}
				/* list selection */
				n = tab == 0 ? nelem(wallpapers)-1 : tab == 1 ? nelem(savers)-1 : 0;
				listrects(lr, n);
				for(i = 0; i < n; i++)
					if(ptinrect(e.mouse.xy, lr[i])){
						if(tab == 0)
							wallsel = i;
						else if(tab == 1)
							savsel = i;
						redraw();
						break;
					}
			}
			buttons = e.mouse.buttons;
			break;
		case Ekeyboard:
			if(e.kbdc == Kdel || e.kbdc == 'q')
				exits(nil);
			break;
		}
	}
}
