/*
 * kryon.c - Kryon-native theming for the TaijiOS window manager.
 *
 * Rio links the real Kryon theme engine (libkryon, libdraw backend) and lets
 * it own the whole desktop look: the window decorations, taskbar, menus and
 * dialogs all read their colors from Kryon's theme catalog, and the geometry
 * (bevels vs rounded corners, shadows) follows Kryon's style tokens, so rio
 * windows look like Kryon applications.
 *
 * Everything here goes through the public Kryon API - SetThemeStyle for the
 * retro/material styles, SetCurrentTheme for the 13 palette themes with
 * light/dark modes, GetUIStyleTokens for the style metrics, and the
 * Lighten/DarkenUIColor helpers for derived shades. The choice is persisted
 * to $home/lib/kryon/theme and published as /lib/kryon/system-theme, which
 * Kryon apps read as the system theme so the whole desktop re-skins
 * together.
 */
#include "inc.h"

/*
 * Kryon's raylib-style names (Rectangle, Image, Font, ...) collide with the
 * Plan 9 ones rio is built against. Rename the Plan 9 side for the duration
 * of the Kryon header block, the same way Kryon's libdraw backend does.
 * Rect needs no rename: Kryon publishes it as a macro for UIRect, which is
 * undefined afterwards so rio keeps the libc Rect constructor.
 */
#define Point KryonP9Point
#define Rectangle KryonP9Rectangle
#define Image KryonP9Image
#define Font KryonP9Font
#define Screen KryonP9Screen
#define Display KryonP9Display
#define Mouse KryonP9Mouse
#define Event KryonP9Event
#define Cursor KryonP9Cursor
#define Cursor2 KryonP9Cursor2
#define Menu KryonP9Menu
#define Text KryonP9Text
#include "kryon.h"
#undef Text
#undef Menu
#undef Cursor2
#undef Cursor
#undef Event
#undef Mouse
#undef Display
#undef Screen
#undef Font
#undef Image
#undef Rectangle
#undef Point
#undef Rect

/* Desktop theme state, driven through the Kryon API. */
static ThemeStyle kstyle = THEME_STYLE_RETRO;
static ThemeId ktheme = THEME_PLAN9;
static ThemeMode kmode = THEME_MODE_LIGHT;
static int kloaded;

/* Cached decoration images, rebuilt by kryoninittheme. */
static Image *kface;
static Image *khilite1;
static Image *khilite2;
static Image *kshadow1;
static Image *kshadow2;
static Image *koutline;
static Image *kedgeline;
static Image *ktitleact;
static Image *ktitleinact;
static Image *ktitletext;
static Image *ktitletextinact;
static Image *kappdot;
static Image *kappdotcore;
static Image *kbtnmask[4];	/* min, max, restore, close */

static char kminbtn[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,
	0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static char kmaxbtn[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
	0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,
	0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,
	0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,
	0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,
	0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,
	0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,
	0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,
	0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static char krstbtn[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,
	0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,
	0,0,0,1,0,0,0,0,1,1,1,1,1,1,0,0,
	0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,0,1,0,0,
	0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
	0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
	0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static char kclosebtn[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,1,0,0,0,0,0,1,1,0,0,0,0,
	0,0,0,0,1,1,0,0,0,1,1,0,0,0,0,0,
	0,0,0,0,0,1,1,0,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,1,1,0,1,1,0,0,0,0,0,0,
	0,0,0,0,1,1,0,0,0,1,1,0,0,0,0,0,
	0,0,0,1,1,0,0,0,0,0,1,1,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static ulong
krgba(Color c)
{
	return (ulong)c.r<<24 | (ulong)c.g<<16 | (ulong)c.b<<8 | c.a;
}

static int
klum(Color c)
{
	return (c.r*299 + c.g*587 + c.b*114) / 1000;
}

/* Pick whichever of two candidates reads better on the given surface. */
static Color
kreadable(Color on, Color a, Color b)
{
	int la = klum(on) - klum(a);
	int lb = klum(on) - klum(b);

	if(la < 0)
		la = -la;
	if(lb < 0)
		lb = -lb;
	return lb > la ? b : a;
}

/* Current Kryon palette color as a Plan 9 RGBA value, for panel.c,
 * dialog.c and splash.c. */
ulong
kthemecolor(char *key)
{
	return krgba(GetCurrentThemeColor(key));
}

/* Register (or replace) a named theme color image; getcolor() callers pick
 * these up, and nameimage(replace) keeps live theme switching honest. */
static Image *
kset(char *name, Color c)
{
	Image *img;
	char *n;

	img = allocimage(display, Rect(0,0,1,1), RGBA32, 1, krgba(c));
	if(img == nil)
		return nil;
	if(name != nil){
		n = smprint("th_%s", name);
		if(n != nil){
			nameimage(img, n, 1);
			free(n);
		}
	}
	return img;
}

/* ---- persisted state ------------------------------------------------- */

static ThemeStyle
kstylebyname(char *name)
{
	if(strcmp(name, "material") == 0)
		return THEME_STYLE_MATERIAL;
	if(strcmp(name, "system") == 0)
		return THEME_STYLE_SYSTEM;
	return THEME_STYLE_RETRO;
}

static ThemeId
kthemebyname(char *name)
{
	int i;

	for(i = 0; i < THEME_COUNT; i++)
		if(strcmp(themes[i].name, name) == 0)
			return (ThemeId)i;
	return (ThemeId)-1;
}

static ThemeMode
kmodebyname(char *name)
{
	if(strcmp(name, "dark") == 0)
		return THEME_MODE_DARK;
	if(strcmp(name, "system") == 0)
		return THEME_MODE_SYSTEM;
	return THEME_MODE_LIGHT;
}

char*
kthemename(void)
{
	return (char*)GetThemeMeta(ktheme)->name;
}

char*
kstylename(void)
{
	if(kstyle == THEME_STYLE_MATERIAL)
		return "material";
	if(kstyle == THEME_STYLE_SYSTEM)
		return "system";
	return "retro";
}

char*
kmodename(void)
{
	if(kmode == THEME_MODE_DARK)
		return "dark";
	if(kmode == THEME_MODE_SYSTEM)
		return "system";
	return "light";
}

static char*
khomedir(char *buf, int nbuf, char *leaf)
{
	char *home;

	home = getenv("home");
	if(home == nil)
		return nil;
	if(leaf != nil)
		snprint(buf, nbuf, "%s/%s", home, leaf);
	else
		snprint(buf, nbuf, "%s", home);
	free(home);
	return buf;
}

static void
kwritefile(char *path, char *text)
{
	char dir[256];
	char *slash;
	int fd;

	slash = strrchr(path, '/');
	if(slash != nil){
		strecpy(dir, dir+sizeof(dir), path);
		dir[slash-path] = '\0';
		fd = create(dir, OREAD, DMDIR|0777);
		if(fd >= 0)
			close(fd);
	}
	fd = create(path, OWRITE|OTRUNC, 0666);
	if(fd < 0)
		return;
	fprint(fd, "%s", text);
	close(fd);
}

/* Publish the desktop theme so Kryon apps follow it as the system theme:
 * a per-user copy plus the system-wide file when /lib is writable. */
static void
kthemepublish(void)
{
	char path[256];
	char text[128];

	snprint(text, sizeof(text), "name=%s\nmode=%s\nstyle=%s\n",
		kthemename(), kmodename(), kstylename());
	if(khomedir(path, sizeof(path), "lib/kryon/theme") != nil)
		kwritefile(path, text);
	kwritefile("/lib/kryon/system-theme", text);
}

static char*
kvalue(char *text, char *key)
{
	char *p, *eol;
	int klen;

	klen = strlen(key);
	for(p = text; p != nil && *p; p = eol){
		eol = strchr(p, '\n');
		if(eol != nil)
			*eol++ = '\0';
		if(strncmp(p, key, klen) == 0 && p[klen] == '=')
			return p+klen+1;
	}
	return nil;
}

static void
kthemeload(void)
{
	char path[256];
	char buf[512];
	char *v;
	int fd, n;

	kloaded = 1;
	if(khomedir(path, sizeof(path), "lib/kryon/theme") == nil)
		return;
	fd = open(path, OREAD);
	if(fd < 0)
		return;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return;
	buf[n] = '\0';
	v = kvalue(buf, "name");
	if(v != nil){
		if(kpaletteknown(v))
			ktheme = kthemebyname(v);
	}
	v = kvalue(buf, "mode");
	if(v != nil && kmodeknown(v))
		kmode = kmodebyname(v);
	v = kvalue(buf, "style");
	if(v != nil && kstyleknown(v))
		kstyle = kstylebyname(v);
}

/* Apply a new style/palette/mode (nil keeps the current value), publish it,
 * and let the caller retheme the decorations. */
int
kthemeapply(char *style, char *palette, char *mode)
{
	if(style != nil){
		if(!kstyleknown(style))
			return 0;
		kstyle = kstylebyname(style);
	}
	if(palette != nil){
		if(!kpaletteknown(palette))
			return 0;
		ktheme = kthemebyname(palette);
	}
	if(mode != nil){
		if(!kmodeknown(mode))
			return 0;
		kmode = kmodebyname(mode);
	}
	SetThemeStyle(kstyle);
	SetCurrentTheme(ktheme, kmode == THEME_MODE_DARK);
	kthemepublish();
	return 1;
}

/* Parse "kryon[:style[:palette[:mode]]]". */
int
kthemekryon(char *spec)
{
	char buf[128];
	char *f[6];
	int nf;

	strecpy(buf, buf+sizeof(buf), spec);
	nf = getfields(buf, f, 6, 0, ":");
	if(nf < 1 || strcmp(f[0], "kryon") != 0)
		return 0;
	return kthemeapply(
		nf > 1 && f[1][0] ? f[1] : nil,
		nf > 2 && f[2][0] ? f[2] : nil,
		nf > 3 && f[3][0] ? f[3] : nil);
}

int
kstyleknown(char *name)
{
	return strcmp(name, "retro") == 0 || strcmp(name, "material") == 0 ||
		strcmp(name, "system") == 0;
}

int
kpaletteknown(char *name)
{
	return kthemebyname(name) >= 0;
}

int
kmodeknown(char *name)
{
	return strcmp(name, "light") == 0 || strcmp(name, "dark") == 0 ||
		strcmp(name, "system") == 0;
}

/* ---- decoration ------------------------------------------------------ */

/* Rounded corners in the Kryon material manner: the corner squares are
 * painted back to the desktop and quarter-ellipses restore the frame fill,
 * matching Kryon's panel_radius token. */
static void
kround(Image *img, Rectangle r, int rad, Image *fill, Image *outside)
{
	Rectangle c;

	if(rad <= 0)
		return;
	c = Rect(r.min.x, r.min.y, r.min.x+rad, r.min.y+rad);
	draw(img, c, outside, nil, ZP);
	fillellipse(img, Pt(c.max.x-1, c.max.y-1), rad, rad, fill, ZP);
	c = Rect(r.max.x-rad, r.min.y, r.max.x, r.min.y+rad);
	draw(img, c, outside, nil, ZP);
	fillellipse(img, Pt(c.min.x, c.max.y-1), rad, rad, fill, ZP);
	c = Rect(r.min.x, r.max.y-rad, r.min.x+rad, r.max.y);
	draw(img, c, outside, nil, ZP);
	fillellipse(img, Pt(c.max.x-1, c.min.y), rad, rad, fill, ZP);
	c = Rect(r.max.x-rad, r.max.y-rad, r.max.x, r.max.y);
	draw(img, c, outside, nil, ZP);
	fillellipse(img, Pt(c.min.x, c.min.y), rad, rad, fill, ZP);
}

/* Retro bevel button chip (Kryon retro controls draw a classic bevel). */
static void
kbtnbevel(Image *img, Rectangle r, Image *mask)
{
	winborder(img, r, khilite2, kshadow2);
	r = insetrect(r, 1);
	winborder(img, r, khilite1, kshadow1);
	r = insetrect(r, 1);
	draw(img, r, kface, nil, ZP);
	r = insetrect(r, -2);
	draw(img, r, ktitletext, mask, ZP);
}

/* Material flat button: just the glyph on the title band. */
static void
kbtnflat(Image *img, Rectangle r, Image *mask)
{
	draw(img, r, ktitletext, mask, ZP);
}

static void
kdrawglyphs(Window *w, Rectangle r)
{
	Rectangle br;
	int flat = GetEffectiveThemeStyle() == THEME_STYLE_MATERIAL;

	br = insetrect(r, 2);
	br.min.x = br.max.x - Dy(br) - 2;
	if(flat)
		kbtnflat(w->frame, br, kbtnmask[3]);
	else
		kbtnbevel(w->frame, br, kbtnmask[3]);
	br = rectaddpt(br, Pt(-Dx(br)-2, 0));
	if(flat)
		kbtnflat(w->frame, br, kbtnmask[1+w->maximized]);
	else
		kbtnbevel(w->frame, br, kbtnmask[1+w->maximized]);
	br = rectaddpt(br, Pt(-Dx(br)-2, 0));
	if(flat)
		kbtnflat(w->frame, br, kbtnmask[0]);
	else
		kbtnbevel(w->frame, br, kbtnmask[0]);

	fillellipse(w->frame, Pt(r.min.x+10, r.min.y+Dy(r)/2), 5, 5,
		kappdot, ZP);
	fillellipse(w->frame, Pt(r.min.x+10, r.min.y+Dy(r)/2), 2, 2,
		kappdotcore, ZP);
}

static void
kdecorretro(Window *w)
{
	int inact = w != focused;
	Rectangle r;
	Point pt;

	if(!w->noborder){
		r = w->rect;
		border(w->frame, r, bordersz, kface, ZP);
		winborder(w->frame, r, khilite1, kshadow2);
		r = insetrect(r, 1);
		winborder(w->frame, r, khilite2, kshadow1);
	}

	if(!w->notitle){
		r = w->titlerect;
		r.max.y -= 1;
		draw(w->frame, r, inact ? ktitleinact : ktitleact, nil, ZP);
		draw(w->frame, Rect(r.min.x, r.max.y, r.max.x, r.max.y+1), kface, nil, ZP);
		kdrawglyphs(w, r);
		pt = Pt(r.min.x + 22, r.min.y + (Dy(r)-font->height)/2);
		if(w->cur)
			string(w->frame, pt, inact ? ktitletextinact : ktitletext,
				pt, font, w->cur->label);
	}

	r = w->tabrect;
	draw(w->frame, r, kface, nil, ZP);
	draw(w->frame, Rect(r.min.x, r.max.y-1, r.max.x, r.max.y), kshadow2, nil, ZP);
}

static void
kdecormaterial(Window *w)
{
	UIStyleTokens t = GetUIStyleTokens();
	int inact = w != focused;
	Rectangle r;
	Point pt;
	int rad;

	rad = (int)t.panel_radius;

	if(!w->noborder){
		r = w->rect;
		border(w->frame, r, bordersz, kface, ZP);
		border(w->frame, r, 1, koutline, ZP);
		/* Kryon material elevation: a soft dark edge under the panel. */
		draw(w->frame, Rect(r.min.x, r.max.y-2, r.max.x, r.max.y), kedgeline, nil, ZP);
		if(rad > 0)
			kround(w->frame, r, rad, kface, background);
	}

	if(!w->notitle){
		r = w->titlerect;
		r.max.y -= 1;
		draw(w->frame, r, inact ? ktitleinact : ktitleact, nil, ZP);
		/* Title band separator, like Kryon's app title bars. */
		draw(w->frame, Rect(r.min.x, r.max.y-1, r.max.x, r.max.y), kedgeline, nil, ZP);
		kdrawglyphs(w, r);
		pt = Pt(r.min.x + 22, r.min.y + (Dy(r)-font->height)/2);
		if(w->cur)
			string(w->frame, pt, inact ? ktitletextinact : ktitletext,
				pt, font, w->cur->label);
	}

	r = w->tabrect;
	draw(w->frame, r, kface, nil, ZP);
	draw(w->frame, Rect(r.min.x, r.max.y-1, r.max.x, r.max.y), kedgeline, nil, ZP);
}

void
kryonwdecor(Window *w)
{
	if(w->frame == nil)
		return;
	if(GetEffectiveThemeStyle() == THEME_STYLE_MATERIAL)
		kdecormaterial(w);
	else
		kdecorretro(w);
}

void
kryoninittheme(void)
{
	Color bg, surface, text, link, button, hover;
	Color tbact, tbinact;

	if(!kloaded)
		kthemeload();
	SetThemeStyle(kstyle);
	SetCurrentTheme(ktheme, kmode == THEME_MODE_DARK);

	bg = GetCurrentThemeColor("background");
	surface = GetCurrentThemeColor("surface");
	text = GetCurrentThemeColor("text");
	link = GetCurrentThemeColor("link");
	button = GetCurrentThemeColor("button");
	hover = GetCurrentThemeColor("button_hover");

	titlegradient = 0;

	background = kset("background", bg);

	kface = kset("3d_face", surface);
	khilite1 = kset("3d_hilight1", LightenUIColor(surface, 18));
	khilite2 = kset("3d_hilight2", LightenUIColor(surface, 36));
	kshadow1 = kset("3d_shadow1", DarkenUIColor(surface, 24));
	kshadow2 = kset("3d_shadow2", DarkenUIColor(surface, 48));
	kset("button_face", button);
	kset("button_hilight", LightenUIColor(button, 24));
	kset("button_shadow", DarkenUIColor(button, 28));
	koutline = kset("window_frame_outline", DarkenUIColor(surface, 42));
	kedgeline = kset("window_frame_edge", DarkenUIColor(surface, 60));
	kset("window_frame", surface);
	kset("border_active", DarkenUIColor(surface, 42));
	kset("border_inactive", DarkenUIColor(surface, 24));

	/* Title bands follow Kryon's title bar recipe: a slightly darkened
	 * background, with the palette accent marking the active window. */
	tbact = DarkenUIColor(link, 8);
	tbinact = DarkenUIColor(bg, 14);
	ktitleact = kset("titlebar_active", tbact);
	ktitleinact = kset("titlebar_inactive", tbinact);
	ktitletext = kset("titlebar_text_active", kreadable(tbact, text, bg));
	ktitletextinact = kset("titlebar_text_inactive",
		kreadable(tbinact, DarkenUIColor(text, 50), LightenUIColor(text, 50)));
	kset("title", tbact);
	kset("ltitle", tbinact);
	kset("titletext", kreadable(tbact, text, bg));
	kset("ltitletext", kreadable(tbinact, DarkenUIColor(text, 50), LightenUIColor(text, 50)));
	kset("titlehold", hover);
	kset("titleholdtext", kreadable(hover, text, bg));
	kset("ltitlehold", hover);
	kset("ltitleholdtext", kreadable(hover, text, bg));
	kset("frame", surface);
	kset("lframe", DarkenUIColor(surface, 12));

	/* Menus and the taskbar. */
	kset("menuback", bg);
	kset("menubord", DarkenUIColor(bg, 42));
	kset("menuhigh", hover);
	kset("menuhtext", kreadable(hover, text, bg));
	kset("menutext", text);
	kset("tipback", LightenUIColor(bg, 18));

	/* Window content palette (rio text windows) from the same theme. */
	colors[BACK] = kset("back", bg);
	colors[BORD] = kset("bord", DarkenUIColor(bg, 42));
	colors[HIGH] = kset("high", hover);
	colors[HTEXT] = kset("htext", kreadable(hover, text, bg));
	colors[TEXT] = kset("text", text);
	colors[PALETEXT] = kset("paletext",
		klum(bg) > 128 ? DarkenUIColor(text, 40) : LightenUIColor(text, 40));
	colors[HOLDTEXT] = kset("holdtext", link);
	colors[PALEHOLDTEXT] = kset("paleholdtext", DarkenUIColor(link, 30));

	/* Application mark: the theme accent as a Kryon-style circle. */
	kappdot = kset(nil, hover);
	kappdotcore = kset(nil, kreadable(hover, text, bg));

	kbtnmask[0] = mkiconmask(kminbtn, 16, 14);
	kbtnmask[1] = mkiconmask(kmaxbtn, 16, 14);
	kbtnmask[2] = mkiconmask(krstbtn, 16, 14);
	kbtnmask[3] = mkiconmask(kclosebtn, 16, 14);
}
