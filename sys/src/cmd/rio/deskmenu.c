#include "inc.h"

static	Image	*menutxt;
static	Image	*back;
static	Image	*high;
static	Image	*bord;
static	Image	*text;
static	Image	*htext;

enum
{
	Border = 2,
	ItemBorder = 1,
	Itemwidth = 40,
	Itemheight = 30
};

static
void
menucolors(void)
{
	back = getcolor("menuback", 0xC0C0C0FF);
	high = getcolor("menuhigh", 0x000080FF);
	bord = getcolor("menubord", 0x000000FF);
	text = getcolor("menutext", 0x000000FF);
	htext = getcolor("menuhtext", 0xFFFFFFFF);
	if(back==nil || high==nil || bord==nil || text==nil || htext==nil)
		goto Error;
	return;

    Error:
	freeimage(back);
	freeimage(high);
	freeimage(bord);
	freeimage(text);
	freeimage(htext);
	back = display->white;
	high = display->black;
	bord = display->black;
	text = display->black;
	htext = display->white;
}

static Rectangle
menurect(Rectangle r, int i, int j)
{
	if(i < 0 || j < 0)
		return Rect(0, 0, 0, 0);
	return rectaddpt(Rect(0, 0, Itemwidth, Itemheight),
		Pt(r.min.x+i*Itemwidth, r.min.y+j*Itemheight));
}

static void
paintitem(Image *m, Rectangle contr, int i, int j, int highlight)
{
	Rectangle r;

	if(i < 0 || j < 0)
		return;
	r = menurect(contr, i, j);
	draw(m, r, highlight? high : back, nil, ZP);
	border(m, r, ItemBorder, bord, ZP);
}

static Point
menusel(Rectangle r, Point p)
{
	if(!ptinrect(p, r))
		return Pt(-1,-1);
	return Pt((p.x-r.min.x)/Itemwidth, (p.y-r.min.y)/Itemheight);
}


static void
menupaint(Image *m, Rectangle contr, int nx, int ny)
{
	int i, j;

	draw(m, contr, back, nil, ZP);
	for(i = 0; i < nx; i++)
	for(j = 0; j < ny; j++)
		paintitem(m, contr, i, j, 0);
}

static Point
clampscreen(Rectangle r)
{
	Point pt;

	pt = ZP;
	if(r.max.x>screen->r.max.x)
		pt.x = screen->r.max.x-r.max.x;
	if(r.max.y>screen->r.max.y)
		pt.y = screen->r.max.y-r.max.y;
	if(r.min.x<screen->r.min.x)
		pt.x = screen->r.min.x-r.min.x;
	if(r.min.y<screen->r.min.y)
		pt.y = screen->r.min.y-r.min.y;
	return pt;
}

Point
dmenuhit(int but, Mousectl *mc, int nx, int ny, Point last)
{
	Rectangle r, menur, contr;
	Point delta;
	Point hover, sel;
	int butmask;

	if(back == nil)
		menucolors();

	if(last.x < 0) last.x = 0;
	if(last.x >= nx) last.x = nx-1;
	if(last.y < 0) last.y = 0;
	if(last.y >= ny) last.y = ny-1;

	r = insetrect(Rect(0, 0, nx*Itemwidth, ny*Itemheight), -Border);
	r = rectsubpt(r, Pt(last.x*Itemwidth+Itemwidth/2, last.y*Itemheight+Itemheight/2));
	menur = rectaddpt(r, mc->xy);
	delta = clampscreen(menur);
	menur = rectaddpt(menur, delta);
	contr = insetrect(menur, Border);

	Image *b, *backup;
	{
		b = screen;
		backup = allocimage(display, menur, screen->chan, 0, -1);
		draw(backup, menur, screen, nil, menur.min);
	}
	draw(b, menur, back, nil, ZP);
	border(b, menur, Border, bord, ZP);
	menupaint(b, contr, nx, ny);

	butmask = 1<<(but-1);
	sel = Pt(-1, -1);
	hover = Pt(-1, -1);
	while(mc->buttons & butmask)
		readmouse(mc);
	for(;;){
		Point ij;

		ij = menusel(contr, mc->xy);
		if(!eqpt(ij, hover)){
			paintitem(b, contr, hover.x, hover.y, 0);
			hover = ij;
			paintitem(b, contr, hover.x, hover.y, 1);
			flushimage(display, 1);
		}
		readmouse(mc);
		if(mc->buttons & 7){
			if(hover.x >= 0 && hover.y >= 0)
				sel = hover;
			while(mc->buttons & 7)
				readmouse(mc);
			break;
		}
	}

	if(backup){
		draw(screen, menur, backup, nil, menur.min);
		freeimage(backup);
	}
	flushimage(display, 1);

	return sel;
}
