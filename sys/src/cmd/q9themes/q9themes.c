/*
 * q9themes - Kryon theme manager for TaijiOS.
 *
 * A Taiji control panel applet for Kryon themes. Taiji owns the system
 * theme state and mirrors Kryon's palette catalog here without linking
 * the full Kryon runtime backend into this small libdraw window. It applies
 * the choice through rio's wctl "theme kryon:style:palette:mode"
 * message so the whole desktop re-skins at once.
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>

typedef int bool;
enum { false = 0, true = 1 };

typedef struct Color Color;
typedef struct ThemeMeta ThemeMeta;
typedef struct UIStyleTokens UIStyleTokens;

struct Color {
	uchar r;
	uchar g;
	uchar b;
	uchar a;
};

#define THEME_COUNT 12

typedef enum {
	THEME_SKY = 0,
	THEME_OCEAN,
	THEME_FOREST,
	THEME_SUNSET,
	THEME_LAVENDER,
	THEME_CHERRY,
	THEME_DAWN,
	THEME_SAGE,
	THEME_INK,
	THEME_MONO,
	THEME_MINT,
	THEME_COBALT
} ThemeId;

typedef enum {
	THEME_STYLE_SYSTEM = 0,
	THEME_STYLE_RETRO,
	THEME_STYLE_MATERIAL
} ThemeStyle;

struct ThemeMeta {
	const char *name;
	const char *light_scope;
	const char *dark_scope;
};

struct UIStyleTokens {
	float control_radius;
	float panel_radius;
	uchar control_alpha;
	uchar panel_alpha;
	uchar border_alpha;
	uchar shadow_alpha;
	uchar shine_alpha;
	int bevel_enabled;
	int touch_target_min;
	int shadow_offset_y;
};

enum {
	WinW = 480,
	WinH = 430,
	Titleh = 26,
	Rowh = 24,
	Btnw = 84,
	Btnh = 26,
	ListX = 12,
	ListW = 186,
	ListY = 44,
	RightX = 214,
	RightW = 254,
};

char *styles[] = { "Retro", "Material", nil };
char *modes[] = { "Light", "Dark", nil };

Image *face, *light, *shadow, *dark, *white, *text, *hilite, *accent,
      *titlebg, *titletext, *outline;

int sel;			/* palette index */
int style;			/* 0 retro, 1 material */
int mode;			/* 0 light, 1 dark */
int applied;

void redraw(void);

static int
themeatrow(int row)
{
	return row >= 0 && row < THEME_COUNT ? row : -1;
}

typedef struct ThemeCatalogColors ThemeCatalogColors;
struct ThemeCatalogColors {
	Color background;
	Color surface;
	Color text;
	Color circle;
	Color button;
	Color button_hover;
	Color icon;
	Color link;
};

const ThemeMeta themes[THEME_COUNT] = {
	[THEME_SKY] = {"Sky", "sky_light", "sky_dark"},
	[THEME_OCEAN] = {"Ocean", "ocean_light", "ocean_dark"},
	[THEME_FOREST] = {"Forest", "forest_light", "forest_dark"},
	[THEME_SUNSET] = {"Sunset", "sunset_light", "sunset_dark"},
	[THEME_LAVENDER] = {"Lavender", "lavender_light", "lavender_dark"},
	[THEME_CHERRY] = {"Cherry", "cherry_light", "cherry_dark"},
	[THEME_DAWN] = {"Dawn", "dawn_light", "dawn_dark"},
	[THEME_SAGE] = {"Sage", "sage_light", "sage_dark"},
	[THEME_INK] = {"Sepia", "ink_light", "ink_dark"},
	[THEME_MONO] = {"Default", "default_light", "default_dark"},
	[THEME_MINT] = {"Mint", "mint_light", "mint_dark"},
	[THEME_COBALT] = {"Cobalt", "cobalt_light", "cobalt_dark"}
};

static const ThemeCatalogColors catalog_light[THEME_COUNT] = {
	[THEME_SKY] = {{0xE2,0xEE,0xFC,0xFF}, {0xD4,0xE4,0xF5,0xFF}, {0x24,0x48,0x7C,0xFF}, {0x7E,0xB7,0xE6,0xFF}, {0xA6,0xCF,0xF2,0xFF}, {0x68,0x9E,0xD7,0xFF}, {0xE2,0xEE,0xFC,0xFF}, {0x4A,0x90,0xE2,0xFF}},
	[THEME_OCEAN] = {{0xD0,0xE8,0xF8,0xFF}, {0xC0,0xDD,0xEE,0xFF}, {0x1A,0x40,0x70,0xFF}, {0x5A,0xA0,0xD0,0xFF}, {0x80,0xC0,0xE0,0xFF}, {0x40,0x90,0xD0,0xFF}, {0xD0,0xE8,0xF8,0xFF}, {0x20,0x80,0xC0,0xFF}},
	[THEME_FOREST] = {{0xE0,0xF0,0xE0,0xFF}, {0xD1,0xE5,0xD1,0xFF}, {0x2A,0x50,0x30,0xFF}, {0x60,0xB0,0x70,0xFF}, {0xA0,0xD0,0xB0,0xFF}, {0x70,0xC0,0x90,0xFF}, {0xE0,0xF0,0xE0,0xFF}, {0x40,0x90,0x50,0xFF}},
	[THEME_SUNSET] = {{0xF8,0xE8,0xD8,0xFF}, {0xEC,0xD8,0xC6,0xFF}, {0x50,0x28,0x14,0xFF}, {0xD0,0x80,0x50,0xFF}, {0xF0,0xC0,0xA0,0xFF}, {0xE0,0x90,0x60,0xFF}, {0xF8,0xE8,0xD8,0xFF}, {0xC0,0x60,0x30,0xFF}},
	[THEME_LAVENDER] = {{0xF0,0xE8,0xF8,0xFF}, {0xE2,0xD7,0xEE,0xFF}, {0x40,0x28,0x60,0xFF}, {0x90,0x70,0xB0,0xFF}, {0xC0,0xA0,0xD0,0xFF}, {0xA0,0x80,0xC0,0xFF}, {0xF0,0xE8,0xF8,0xFF}, {0x70,0x50,0x90,0xFF}},
	[THEME_CHERRY] = {{0xF8,0xD8,0xE0,0xFF}, {0xEC,0xC9,0xD2,0xFF}, {0x60,0x20,0x30,0xFF}, {0xD0,0x60,0x80,0xFF}, {0xF0,0xA0,0xB0,0xFF}, {0xE0,0x70,0x90,0xFF}, {0xF8,0xD8,0xE0,0xFF}, {0xC0,0x40,0x60,0xFF}},
	[THEME_DAWN] = {{0xF5,0xE9,0xDF,0xFF}, {0xE7,0xD9,0xD0,0xFF}, {0x39,0x3B,0x4A,0xFF}, {0xE0,0x7A,0x6D,0xFF}, {0x8F,0xCF,0xC6,0xFF}, {0x62,0xB8,0xB0,0xFF}, {0xF5,0xE9,0xDF,0xFF}, {0xC8,0x62,0x5C,0xFF}},
	[THEME_SAGE] = {{0xE8,0xEC,0xDF,0xFF}, {0xD9,0xE0,0xD0,0xFF}, {0x35,0x44,0x38,0xFF}, {0x82,0xA0,0x7D,0xFF}, {0xB6,0xC9,0xA6,0xFF}, {0x94,0xAF,0x84,0xFF}, {0xE8,0xEC,0xDF,0xFF}, {0x5D,0x82,0x68,0xFF}},
	[THEME_INK] = {{0xF3,0xE4,0xC8,0xFF}, {0xE4,0xCF,0xAA,0xFF}, {0x3F,0x2B,0x1C,0xFF}, {0xB8,0x76,0x3F,0xFF}, {0xD8,0xB0,0x72,0xFF}, {0xC7,0x91,0x51,0xFF}, {0xF8,0xEE,0xD6,0xFF}, {0x8F,0x5B,0x2F,0xFF}},
	[THEME_MONO] = {{0xC0,0xC0,0xC0,0xFF}, {0xD4,0xD0,0xC8,0xFF}, {0x00,0x00,0x00,0xFF}, {0x80,0x80,0x80,0xFF}, {0xC0,0xC0,0xC0,0xFF}, {0xE8,0xE8,0xE8,0xFF}, {0x00,0x00,0x00,0xFF}, {0x00,0x00,0x80,0xFF}},
	[THEME_MINT] = {{0xE4,0xF4,0xEE,0xFF}, {0xD2,0xE7,0xDF,0xFF}, {0x1E,0x45,0x3A,0xFF}, {0x4B,0xB6,0x9A,0xFF}, {0x98,0xD8,0xC6,0xFF}, {0x70,0xC7,0xB2,0xFF}, {0xE4,0xF4,0xEE,0xFF}, {0x2D,0x8F,0x78,0xFF}},
	[THEME_COBALT] = {{0xE7,0xEA,0xF4,0xFF}, {0xD7,0xDD,0xEC,0xFF}, {0x19,0x27,0x4A,0xFF}, {0x4F,0x67,0xC8,0xFF}, {0xA9,0xB8,0xEF,0xFF}, {0x7D,0x92,0xE0,0xFF}, {0xE7,0xEA,0xF4,0xFF}, {0x35,0x4F,0xB5,0xFF}}
};

static const ThemeCatalogColors catalog_dark[THEME_COUNT] = {
	[THEME_SKY] = {{0x18,0x28,0x38,0xFF}, {0x22,0x36,0x48,0xFF}, {0xB0,0xD0,0xEA,0xFF}, {0x50,0x80,0xB0,0xFF}, {0x30,0x50,0x70,0xFF}, {0x40,0x70,0x90,0xFF}, {0xB0,0xD0,0xEA,0xFF}, {0x60,0xA0,0xD0,0xFF}},
	[THEME_OCEAN] = {{0x10,0x25,0x40,0xFF}, {0x1C,0x34,0x50,0xFF}, {0x98,0xC0,0xDE,0xFF}, {0x30,0x60,0x90,0xFF}, {0x20,0x40,0x60,0xFF}, {0x30,0x55,0x75,0xFF}, {0x98,0xC0,0xDE,0xFF}, {0x50,0x90,0xC0,0xFF}},
	[THEME_FOREST] = {{0x15,0x30,0x20,0xFF}, {0x20,0x3C,0x2A,0xFF}, {0xB4,0xD0,0xB4,0xFF}, {0x35,0x65,0x45,0xFF}, {0x25,0x45,0x30,0xFF}, {0x30,0x55,0x40,0xFF}, {0xB4,0xD0,0xB4,0xFF}, {0x50,0x80,0x60,0xFF}},
	[THEME_SUNSET] = {{0x30,0x18,0x10,0xFF}, {0x3E,0x24,0x18,0xFF}, {0xE0,0xB0,0x90,0xFF}, {0x80,0x40,0x28,0xFF}, {0x50,0x28,0x18,0xFF}, {0x60,0x35,0x25,0xFF}, {0xE0,0xB0,0x90,0xFF}, {0xC0,0x50,0x30,0xFF}},
	[THEME_LAVENDER] = {{0x20,0x15,0x30,0xFF}, {0x2E,0x20,0x42,0xFF}, {0xC4,0xA8,0xD0,0xFF}, {0x50,0x35,0x70,0xFF}, {0x35,0x20,0x50,0xFF}, {0x45,0x30,0x65,0xFF}, {0xC4,0xA8,0xD0,0xFF}, {0x90,0x60,0xB0,0xFF}},
	[THEME_CHERRY] = {{0x30,0x15,0x20,0xFF}, {0x40,0x20,0x2A,0xFF}, {0xD2,0x98,0xA8,0xFF}, {0x70,0x30,0x45,0xFF}, {0x45,0x20,0x28,0xFF}, {0x55,0x30,0x38,0xFF}, {0xD2,0x98,0xA8,0xFF}, {0xB0,0x40,0x60,0xFF}},
	[THEME_DAWN] = {{0x27,0x22,0x2C,0xFF}, {0x35,0x2D,0x38,0xFF}, {0xE9,0xC7,0xB7,0xFF}, {0xB8,0x5F,0x60,0xFF}, {0x3A,0x6B,0x70,0xFF}, {0x4D,0x83,0x85,0xFF}, {0xE9,0xC7,0xB7,0xFF}, {0xD0,0x72,0x67,0xFF}},
	[THEME_SAGE] = {{0x1F,0x2A,0x22,0xFF}, {0x2D,0x3A,0x30,0xFF}, {0xC7,0xD4,0xC0,0xFF}, {0x5F,0x81,0x65,0xFF}, {0x3C,0x54,0x40,0xFF}, {0x4E,0x66,0x50,0xFF}, {0xC7,0xD4,0xC0,0xFF}, {0x8B,0xA0,0x70,0xFF}},
	[THEME_INK] = {{0x25,0x1A,0x12,0xFF}, {0x33,0x24,0x18,0xFF}, {0xF1,0xD8,0xB0,0xFF}, {0xA3,0x65,0x32,0xFF}, {0x5A,0x3C,0x24,0xFF}, {0x72,0x4B,0x2C,0xFF}, {0xF1,0xD8,0xB0,0xFF}, {0xD5,0x91,0x52,0xFF}},
	[THEME_MONO] = {{0x20,0x20,0x20,0xFF}, {0x30,0x30,0x30,0xFF}, {0xF0,0xF0,0xF0,0xFF}, {0x80,0x80,0x80,0xFF}, {0x40,0x40,0x40,0xFF}, {0x58,0x58,0x58,0xFF}, {0xF0,0xF0,0xF0,0xFF}, {0x80,0xA0,0xFF,0xFF}},
	[THEME_MINT] = {{0x12,0x2D,0x28,0xFF}, {0x1B,0x3B,0x34,0xFF}, {0xB9,0xE4,0xD5,0xFF}, {0x2E,0x82,0x72,0xFF}, {0x25,0x55,0x4B,0xFF}, {0x32,0x69,0x5E,0xFF}, {0xB9,0xE4,0xD5,0xFF}, {0x6E,0xC8,0xB0,0xFF}},
	[THEME_COBALT] = {{0x12,0x18,0x33,0xFF}, {0x1B,0x24,0x45,0xFF}, {0xC2,0xCC,0xF5,0xFF}, {0x38,0x4C,0xA8,0xFF}, {0x27,0x35,0x73,0xFF}, {0x34,0x45,0x8B,0xFF}, {0xC2,0xCC,0xF5,0xFF}, {0x8E,0xA0,0xF0,0xFF}}
};

static uchar
clampbyte(int v)
{
	if(v < 0)
		return 0;
	if(v > 255)
		return 255;
	return v;
}

static Color
adjustcolor(Color c, int delta)
{
	c.r = clampbyte((int)c.r + delta);
	c.g = clampbyte((int)c.g + delta);
	c.b = clampbyte((int)c.b + delta);
	return c;
}

ThemeId
NormalizeTheme(int theme)
{
	if(theme < 0 || theme >= THEME_COUNT)
		return THEME_MONO;
	return (ThemeId)theme;
}

const ThemeMeta*
GetThemeMeta(ThemeId theme)
{
	return &themes[NormalizeTheme(theme)];
}

bool
GetThemeCatalogColor(ThemeId theme, bool darkmode, const char *key, Color *color)
{
	const ThemeCatalogColors *colors;

	if(key == nil || color == nil)
		return false;
	colors = darkmode ? &catalog_dark[NormalizeTheme(theme)] :
		&catalog_light[NormalizeTheme(theme)];
	if(strcmp(key, "background") == 0)
		*color = colors->background;
	else if(strcmp(key, "surface") == 0)
		*color = colors->surface;
	else if(strcmp(key, "text") == 0)
		*color = colors->text;
	else if(strcmp(key, "circle") == 0)
		*color = colors->circle;
	else if(strcmp(key, "button") == 0)
		*color = colors->button;
	else if(strcmp(key, "button_hover") == 0)
		*color = colors->button_hover;
	else if(strcmp(key, "icon") == 0)
		*color = colors->icon;
	else if(strcmp(key, "link") == 0)
		*color = colors->link;
	else
		return false;
	return true;
}

Color
LightenUIColor(Color c, int amount)
{
	return adjustcolor(c, amount < 0 ? 0 : amount);
}

Color
DarkenUIColor(Color c, int amount)
{
	return adjustcolor(c, amount < 0 ? 0 : -amount);
}

UIStyleTokens
GetUIStyleTokensForThemeStyle(ThemeStyle style)
{
	UIStyleTokens tokens;

	memset(&tokens, 0, sizeof tokens);
	tokens.control_radius = 2.0f;
	tokens.panel_radius = 0.0f;
	tokens.control_alpha = 255;
	tokens.panel_alpha = 255;
	tokens.border_alpha = 255;
	tokens.bevel_enabled = 1;
	tokens.touch_target_min = 36;
	if(style == THEME_STYLE_MATERIAL){
		tokens.control_radius = 4.0f;
		tokens.panel_radius = 6.0f;
		tokens.shadow_alpha = 36;
		tokens.bevel_enabled = 0;
		tokens.touch_target_min = 48;
		tokens.shadow_offset_y = 2;
	}
	return tokens;
}

static Rectangle
winrect(int x0, int y0, int x1, int y1)
{
	return Rect(screen->r.min.x+x0, screen->r.min.y+y0,
		screen->r.min.x+x1, screen->r.min.y+y1);
}

static int
vieww(void)
{
	int w;

	w = Dx(screen->r);
	return w > WinW ? w : WinW;
}

static int
viewh(void)
{
	int h;

	h = Dy(screen->r);
	return h > 380 ? h : 380;
}

static Point
winpt(int x, int y)
{
	return Pt(screen->r.min.x+x, screen->r.min.y+y);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("resize failed: %r");
	redraw();
}

/*
 * Chrome colors: rio names its live Kryon palette images th_<key> in
 * the shared draw device; pick those up, fall back to stock colors.
 */
static void
themeimages(void)
{
	Image *ex;
	char *n;
	int i;
	static char *names[] = {
		"3d_face", "3d_hilight2", "3d_shadow1", "3d_shadow2",
		"back", "text", "high", "circle", "titlebar_active",
		"titlebar_text_active", "window_frame_outline", nil
	};
	Image **slots[11];

	slots[0] = &face; slots[1] = &light; slots[2] = &shadow;
	slots[3] = &dark; slots[4] = &white; slots[5] = &text;
	slots[6] = &hilite; slots[7] = &accent; slots[8] = &titlebg;
	slots[9] = &titletext; slots[10] = &outline;
	for(i = 0; names[i]; i++){
		n = smprint("th_%s", names[i]);
		ex = n != nil ? namedimage(display, n) : nil;
		free(n);
		*slots[i] = ex;
	}
	if(face == nil)
		face = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0xC0C0C0FF);
	if(light == nil)
		light = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0xFFFFFFFF);
	if(shadow == nil)
		shadow = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0x808080FF);
	if(dark == nil)
		dark = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0x000000FF);
	if(white == nil)
		white = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0xFFFFFFFF);
	if(text == nil)
		text = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0x000000FF);
	if(hilite == nil)
		hilite = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0xE8E8E8FF);
	if(accent == nil)
		accent = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0x0000AAFF);
	if(titlebg == nil)
		titlebg = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0x000080FF);
	if(titletext == nil)
		titletext = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0xFFFFFFFF);
	if(outline == nil)
		outline = allocimage(display, Rect(0,0,1,1), RGBA32, 1, 0x000000FF);
}

static Image*
catimg(Color c)
{
	return allocimage(display, Rect(0,0,1,1), RGBA32, 1,
		(ulong)c.r<<24 | (ulong)c.g<<16 | (ulong)c.b<<8 | c.a);
}

static char*
kv(char *text, char *key)
{
	char *p, *eol;
	int klen, n;
	static char val[128];

	klen = strlen(key);
	eol = nil;
	for(p = text; p != nil && *p; ){
		eol = strchr(p, '\n');
		if(strncmp(p, key, klen) == 0 && p[klen] == '=')
			break;
		if(eol == nil)
			return nil;
		p = eol+1;
	}
	if(p == nil || *p == 0)
		return nil;
	p += klen+1;
	n = eol == nil ? strlen(p) : eol-p;
	if(n >= sizeof val)
		n = sizeof val-1;
	memmove(val, p, n);
	val[n] = 0;
	return val;
}

static void
readcurrent(void)
{
	char *home, *path, buf[512], *v;
	int fd, n, i;

	sel = THEME_MONO;
	style = 0;
	mode = 0;
	home = getenv("home");
	if(home == nil)
		return;
	path = smprint("%s/lib/kryon/theme", home);
	free(home);
	if(path == nil)
		return;
	fd = open(path, OREAD);
	free(path);
	if(fd < 0)
		return;
	n = read(fd, buf, sizeof buf-1);
	close(fd);
	if(n <= 0)
		return;
	buf[n] = 0;
	v = kv(buf, "name");
	if(v != nil)
		for(i = 0; i < THEME_COUNT; i++)
			if(strcmp(GetThemeMeta((ThemeId)i)->name, v) == 0)
				sel = i;
	v = kv(buf, "style");
	if(v != nil && strcmp(v, "material") == 0)
		style = 1;
	v = kv(buf, "mode");
	if(v != nil && strcmp(v, "dark") == 0)
		mode = 1;
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

static Rectangle
applyrect(void)
{
	return winrect(vieww()-2*Btnw-24, viewh()-Btnh-10,
		vieww()-Btnw-16, viewh()-10);
}

static Rectangle
closerect(void)
{
	return winrect(vieww()-Btnw-12, viewh()-Btnh-10, vieww()-12, viewh()-10);
}

static Rectangle
stylerect(int i)
{
	return winrect(RightX, 52+i*(Btnh+8), RightX+Btnw, 52+(i+1)*(Btnh+8)-8);
}

static Rectangle
moderect(int i)
{
	return winrect(RightX+Btnw+12, 52+i*(Btnh+8), RightX+2*Btnw+12, 52+(i+1)*(Btnh+8)-8);
}

static Rectangle
previewrect(void)
{
	return winrect(RightX, 128, RightX+RightW, 318);
}

static void
roundcorners(Rectangle r, int rad, Image *fill)
{
	Rectangle c;

	if(rad <= 0)
		return;
	c = Rect(r.min.x, r.min.y, r.min.x+rad, r.min.y+rad);
	draw(screen, c, face, nil, ZP);
	fillellipse(screen, Pt(c.max.x-1, c.max.y-1), rad, rad, fill, ZP);
	c = Rect(r.max.x-rad, r.min.y, r.max.x, r.min.y+rad);
	draw(screen, c, face, nil, ZP);
	fillellipse(screen, Pt(c.min.x, c.max.y-1), rad, rad, fill, ZP);
	c = Rect(r.min.x, r.max.y-rad, r.min.x+rad, r.max.y);
	draw(screen, c, face, nil, ZP);
	fillellipse(screen, Pt(c.max.x-1, c.min.y), rad, rad, fill, ZP);
	c = Rect(r.max.x-rad, r.max.y-rad, r.max.x, r.max.y);
	draw(screen, c, face, nil, ZP);
	fillellipse(screen, Pt(c.min.x, c.min.y), rad, rad, fill, ZP);
}

/* mini window preview drawn from the catalog + style tokens */
static void
drawpreview(Rectangle r)
{
	Color bg, surface, txt, link;
	Image *ibg, *isurf, *itxt, *ititle, *ititxt, *iout;
	UIStyleTokens t;
	Rectangle wr, tb;

	themeimages();
	if(!GetThemeCatalogColor((ThemeId)sel, mode != 0, "background", &bg) ||
	   !GetThemeCatalogColor((ThemeId)sel, mode != 0, "surface", &surface) ||
	   !GetThemeCatalogColor((ThemeId)sel, mode != 0, "text", &txt) ||
	   !GetThemeCatalogColor((ThemeId)sel, mode != 0, "link", &link))
		return;
	t = GetUIStyleTokensForThemeStyle(style == 1 ? THEME_STYLE_MATERIAL : THEME_STYLE_RETRO);

	ibg = catimg(bg);
	isurf = catimg(surface);
	itxt = catimg(txt);
	ititle = catimg(DarkenUIColor(link, 8));
	ititxt = catimg(txt);
	iout = catimg(DarkenUIColor(surface, 42));
	if(ibg == nil || isurf == nil || itxt == nil || ititle == nil)
		goto done;

	draw(screen, r, ibg, nil, ZP);
	wr = insetrect(r, 14);
	wr.max.y = wr.min.y + 108;
	if(t.bevel_enabled){
		border(screen, wr, 1, isurf, ZP);
		bevel(wr, 0);
	}else{
		border(screen, wr, 2, isurf, ZP);
		border(screen, wr, 1, iout, ZP);
		draw(screen, Rect(wr.min.x, wr.max.y-2, wr.max.x, wr.max.y), iout, nil, ZP);
		if((int)t.panel_radius > 0)
			roundcorners(wr, (int)t.panel_radius, isurf);
	}
	tb = Rect(wr.min.x+2, wr.min.y+2, wr.max.x-2, wr.min.y+20);
	draw(screen, tb, ititle, nil, ZP);
	string(screen, Pt(tb.min.x+8, tb.min.y+(Dy(tb)-font->height)/2),
		ititxt, ZP, font, GetThemeMeta((ThemeId)sel)->name);
	/* mock content rows on the surface */
	tb = insetrect(wr, 4);
	tb.min.y += 22;
	tb.max.y -= 6;
	draw(screen, tb, isurf, nil, ZP);
	string(screen, Pt(tb.min.x+8, tb.min.y+6), itxt, ZP, font,
		style == 1 ? "material window" : "retro window");
	string(screen, Pt(tb.min.x+8, tb.min.y+6+font->height+4), itxt, ZP, font,
		mode ? "dark mode" : "light mode");
done:
	freeimage(ibg); freeimage(isurf); freeimage(itxt);
	freeimage(ititle); freeimage(ititxt); freeimage(iout);
}

void
redraw(void)
{
	char buf[64];
	Rectangle r;
	Image *sw[3];
	Color c;
	int i, y, w;

	themeimages();
	draw(screen, screen->r, face, nil, ZP);

	/* title band */
	r = winrect(0, 0, vieww(), Titleh);
	draw(screen, r, titlebg, nil, ZP);
	draw(screen, winrect(0, Titleh-1, vieww(), Titleh), dark, nil, ZP);
	w = stringwidth(font, "Kryon Themes");
	string(screen, winpt((vieww()-w)/2, (Titleh-font->height)/2), titletext, ZP, font, "Kryon Themes");

	/* palette list with color swatches from the catalog */
	string(screen, winpt(ListX, ListY-font->height-6), text, ZP, font, "Palette");
	for(i = 0; i < THEME_COUNT; i++){
		y = screen->r.min.y + ListY + i*Rowh;
		r = Rect(screen->r.min.x+ListX, y, screen->r.min.x+ListX+ListW, y+Rowh-2);
		if(i == sel){
			draw(screen, r, hilite, nil, ZP);
			bevel(r, 1);
		}
		sw[0] = sw[1] = sw[2] = nil;
		if(GetThemeCatalogColor((ThemeId)i, mode != 0, "background", &c))
			sw[0] = catimg(c);
		if(GetThemeCatalogColor((ThemeId)i, mode != 0, "surface", &c))
			sw[1] = catimg(c);
		if(GetThemeCatalogColor((ThemeId)i, mode != 0, "link", &c))
			sw[2] = catimg(c);
		if(sw[0]){
			r = Rect(screen->r.min.x+ListX+4, y+4, screen->r.min.x+ListX+18, y+Rowh-6);
			draw(screen, r, sw[0], nil, ZP);
			border(screen, r, 1, dark, ZP);
		}
		if(sw[1]){
			r = Rect(screen->r.min.x+ListX+20, y+4, screen->r.min.x+ListX+34, y+Rowh-6);
			draw(screen, r, sw[1], nil, ZP);
			border(screen, r, 1, dark, ZP);
		}
		if(sw[2]){
			r = Rect(screen->r.min.x+ListX+36, y+4, screen->r.min.x+ListX+50, y+Rowh-6);
			draw(screen, r, sw[2], nil, ZP);
			border(screen, r, 1, dark, ZP);
		}
		string(screen, Pt(screen->r.min.x+ListX+56, y+(Rowh-2-font->height)/2),
			text, ZP, font, GetThemeMeta((ThemeId)i)->name);
		freeimage(sw[0]); freeimage(sw[1]); freeimage(sw[2]);
	}

	/* style + mode pickers */
	string(screen, winpt(RightX, 36), text, ZP, font, "Style");
	for(i = 0; i < 2; i++){
		r = stylerect(i);
		draw(screen, r, face, nil, ZP);
		bevel(r, i == style);
		if(i == style)
			draw(screen, insetrect(r, 3), hilite, nil, ZP);
		w = stringwidth(font, styles[i]);
		string(screen, Pt(r.min.x+(Dx(r)-w)/2, r.min.y+(Dy(r)-font->height)/2),
			text, ZP, font, styles[i]);
	}
	string(screen, winpt(RightX+Btnw+12, 36), text, ZP, font, "Mode");
	for(i = 0; i < 2; i++){
		r = moderect(i);
		draw(screen, r, face, nil, ZP);
		bevel(r, i == mode);
		if(i == mode)
			draw(screen, insetrect(r, 3), hilite, nil, ZP);
		w = stringwidth(font, modes[i]);
		string(screen, Pt(r.min.x+(Dx(r)-w)/2, r.min.y+(Dy(r)-font->height)/2),
			text, ZP, font, modes[i]);
	}

	/* live preview */
	string(screen, winpt(RightX, 118), text, ZP, font, "Preview");
	drawpreview(previewrect());

	/* buttons + status line */
	r = applyrect();
	draw(screen, r, face, nil, ZP);
	bevel(r, 0);
	w = stringwidth(font, "Apply");
	string(screen, Pt(r.min.x+(Dx(r)-w)/2, r.min.y+(Dy(r)-font->height)/2), text, ZP, font, "Apply");
	r = closerect();
	draw(screen, r, face, nil, ZP);
	bevel(r, 0);
	w = stringwidth(font, "Close");
	string(screen, Pt(r.min.x+(Dx(r)-w)/2, r.min.y+(Dy(r)-font->height)/2), text, ZP, font, "Close");

	snprint(buf, sizeof buf, "%s %s %s%s",
		styles[style], GetThemeMeta((ThemeId)sel)->name, modes[mode],
		applied ? "  (applied)" : "");
	string(screen, winpt(12, viewh()-Btnh-10+(Btnh-font->height)/2), text, ZP, font, buf);
	flushimage(display, 1);
}

static void
apply(void)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "theme kryon:%s:%s:%s",
		style ? "material" : "retro",
		GetThemeMeta((ThemeId)sel)->name,
		mode ? "dark" : "light");
	close(fd);
	applied = 1;
}

void
main(int argc, char **argv)
{
	Event e;
	int buttons, i;
	Rectangle r, lastr;

	ARGBEGIN{
	default:
		break;
	}ARGEND
	readcurrent();
	if(initdraw(nil, nil, "themes") < 0)
		sysfatal("initdraw: %r");
	einit(Emouse|Ekeyboard);
	redraw();
	lastr = screen->r;
	buttons = 0;
	for(;;){
		switch(event(&e)){
		case Emouse:
			if(!eqrect(screen->r, lastr)){
				lastr = screen->r;
				redraw();
			}
			if((e.mouse.buttons & 1) && !(buttons & 1)){
				i = (e.mouse.xy.y - (screen->r.min.y+ListY)) / Rowh;
				r = winrect(ListX, ListY+i*Rowh, ListX+ListW, ListY+(i+1)*Rowh);
				if(ptinrect(e.mouse.xy, r) && themeatrow(i) >= 0){
					sel = themeatrow(i);
					redraw();
					break;
				}
				for(i = 0; i < 2; i++)
					if(ptinrect(e.mouse.xy, stylerect(i))){
						style = i;
						redraw();
						break;
					}
				for(i = 0; i < 2; i++)
					if(ptinrect(e.mouse.xy, moderect(i))){
						mode = i;
						redraw();
						break;
					}
				if(ptinrect(e.mouse.xy, applyrect())){
					apply();
					themeimages();
					redraw();
				}else if(ptinrect(e.mouse.xy, closerect()))
					exits(nil);
			}
			buttons = e.mouse.buttons;
			break;
		case Ekeyboard:
			if(e.kbdc == Kdel || e.kbdc == 'q')
				exits(nil);
			if(e.kbdc == '\n'){
				apply();
				themeimages();
				redraw();
			}
			break;
		}
	}
}
