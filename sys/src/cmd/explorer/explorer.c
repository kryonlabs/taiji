#include <u.h>
#include <libc.h>
#include "kryon.h"

/*
 * Kryon-native TaijiOS Explorer: tree sidebar, history navigation,
 * list/icon views of directories, My Computer and Control Panel views,
 * a recycle bin with restore bookkeeping, and rio window launches via
 * wctl. Modal helpers (menus, prompts, properties) are states of the
 * single Kryon frame loop.
 */

enum {
	Menuh = 22,
	Toolbarh = 30,
	Addrh = 24,
	Statush = 22,
	Treew = 170,
	Rowh = 18,
	Btnw = 70,
	Iconw = 108,
	Iconh = 72,
	Maxpath = 512,
	Maxhist = 32,
	Mitemh = 20,
	Fontsz = 13,
};

enum {
	ViewList,
	ViewIcon,
};

enum {
	UINone,
	UIMenu,
	UIMsg,
	UIPrompt,
	UIProps,
};

enum {
	PActNone,
	PActNewDir,
	PActRename,
};

typedef struct Entry Entry;
typedef struct TreeItem TreeItem;
typedef struct McEnt McEnt;
struct Entry {
	char name[128];
	char type[16];
	vlong len;
	ulong mtime;
	int isdir;
};
struct TreeItem {
	char *label;
	char path[Maxpath];
	int indent;
	int y;
	Rectangle r;
};
struct McEnt {
	char *label;
	char *path;	/* nil: not accessible */
	int drive;	/* 0 folder-ish, 1 disk, 2 floppy, 3 cd */
};

Entry *ents;
int nents;
char cwd[Maxpath];
int scroll;
int selectedtree = -1;
int viewmode = ViewList;
int sel = -1;			/* selected entry */
int mycomp;			/* My Computer special view */
char clipboard[Maxpath];	/* copied/cut path */
int clipcut;
static char hist[Maxhist][Maxpath];
static int nhist;
static int hpos;
static double lastclick;
static int lastsel;

TreeItem tree[] = {
	{ "My Computer", "/mycomputer", 10, 10 },
	{ "Namespace", "/", 22, 32 },
	{ "/", "/", 34, 54 },
	{ "/mnt", "/mnt", 34, 76 },
	{ "/usr/glenda", "/usr/glenda", 34, 98 },
	{ "Recycle Bin", "", 22, 120 },
	{ "Control Panel", "/lib/controlpanel", 22, 142 },
};

static McEnt mcents[] = {
	{ "Local Disk (C:)", "/", 1 },
	{ "Floppy (A:)", nil, 2 },
	{ "CD-ROM (D:)", nil, 3 },
	{ "Control Panel", "/lib/controlpanel", 0 },
	{ "Recycle Bin", "", 0 },
	{ "My Documents", "/usr/glenda", 0 },
	{ nil, nil, 0 },
};

/* modal state */
int uimode = UINone;
char **menuitems;
int nmenuitems;
Rectangle menurect;
int menuhover;
int menubarnum;			/* -1: context menu */
int ctxitem;			/* item index the context menu acts on */
char msgtitle[64];
char msgline[Maxpath+64];
char ptitle[64];
char plabel[Maxpath+64];
char pbuf[128];
int plen, pcap;
int paction;
int pitem;
int propsidx;
int uirun = 1;

static char *menutitles[] = { "File", "Edit", "View", "Help", nil };
static char *fitems[] = { "New Window", "New Folder", "-", "Close", nil };
static char *eitems[] = { "Copy", "Cut", "Paste", "-", "Select All", nil };
static char *vitems[] = { "", "-", "Refresh", nil };
static char *hitems[] = { "About Explorer", nil };
static char *bgitems[] = { "New Folder", "Paste", "-", "Refresh", "-", "Properties", nil };
static char *ititems[] = { "Open", "Copy", "Cut", "Rename", "Delete", "-", "Properties", nil };
static char *tritems[] = { "Restore", "-", "Empty Recycle Bin", nil };
static char *cpitems[] = { "Open", nil };

static void openmsg(char *title, char *line);
static void openprompt(char *title, char *label, char *buf, int cap,
	int action, int item);
static void openprops(int i);
static void openmenu(char **items, Rectangle at, int barnum, int item);
static int mcopen(int i);

static Color
oc(Color c)
{
	c.a = 255;
	return c;
}

static Color
muted(float a)
{
	Color c;

	c = GetThemeText();
	c.a = (unsigned char)(255.0f * a);
	return c;
}

static Color
mkcol(int r, int g, int b)
{
	Color c;

	c.r = r;
	c.g = g;
	c.b = b;
	c.a = 255;
	return c;
}

/* ---- layout ---- */

static Rectangle
listrect(void)
{
	Rectangle r;

	r.x = Treew;
	r.y = Menuh + Toolbarh + Addrh;
	r.width = GetScreenWidth() - Treew;
	r.height = GetScreenHeight() - (Menuh + Toolbarh + Addrh) - Statush;
	return r;
}

static Rectangle
toolbtnrect(int i)
{
	Rectangle r;
	int x, w;

	switch(i){
	case 0: x = 6; w = Btnw; break;
	case 1: x = 80; w = Btnw; break;
	case 2: x = 154; w = Btnw; break;
	case 3: x = 228; w = Btnw+12; break;
	case 4: x = 322; w = Btnw+18; break;
	default: x = 412; w = Btnw; break;
	}
	r.x = x;
	r.y = Menuh+4;
	r.width = w;
	r.height = Toolbarh-8;
	return r;
}

static Rectangle
menubaritemrect(int i)
{
	Rectangle r;
	int k, x;

	x = 8;
	for(k = 0; menutitles[k] && k < i; k++)
		x += MeasureText(menutitles[k], Fontsz) + 18;
	r.x = x-4;
	r.y = 0;
	r.width = MeasureText(menutitles[i], Fontsz) + 10;
	r.height = Menuh;
	return r;
}

/* grid slot for the icon views (mycomp / control panel / icon mode) */
static Rectangle
gridrect(Rectangle lr, int i)
{
	Rectangle r;
	int cols, x0, y0;

	x0 = (int)lr.x + 20;
	y0 = (int)lr.y + 20;
	cols = ((int)lr.width - 32) / 122;
	if(cols < 1)
		cols = 1;
	r.x = x0 + (i%cols)*122;
	r.y = y0 + (i/cols)*84;
	r.width = Iconw;
	r.height = Iconh;
	return r;
}

/* ---- data ---- */

static char *
trashdir(void)
{
	static char t[Maxpath];
	char *home;

	home = getenv("home");
	if(home == nil)
		strecpy(t, t+sizeof t, "/tmp");
	else{
		snprint(t, sizeof t, "%s/.trash", home);
		free(home);
	}
	return t;
}

static int
trashview(void)
{
	return strcmp(cwd, trashdir()) == 0;
}

static int
controlpanel(void)
{
	return strcmp(cwd, "/lib/controlpanel") == 0;
}

static void
settreeforpath(char *path)
{
	int i;

	selectedtree = -1;
	for(i = 0; i < nelem(tree); i++)
		if(strcmp(path, tree[i].path) == 0)
			selectedtree = i;
}

static void
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

static int
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
	sel = -1;
	strecpy(cwd, cwd+sizeof cwd, path);
	settreeforpath(cwd);
	free(d);
	return 0;
}

static void
navigate(char *path);

static void
goback(void)
{
	if(mycomp && hpos < 0){
		mycomp = 0;
		return;
	}
	if(hpos > 0){
		hpos--;
		mycomp = 0;
		loaddir(hist[hpos]);
	}
}

static void
goforward(void)
{
	if(hpos+1 < nhist){
		hpos++;
		loaddir(hist[hpos]);
	}
}

static void
navigate(char *path)
{
	char clean[Maxpath];

	if(strcmp(path, "/mycomputer") == 0){
		mycomp = 1;
		selectedtree = 0;
		strecpy(cwd, cwd+sizeof cwd, "My Computer");
		nhist = hpos+1;
		return;
	}
	mycomp = 0;
	strecpy(clean, clean+sizeof clean, path);
	cleanname(clean);
	if(loaddir(clean) != 0)
		return;
	if(hpos >= 0 && hpos < nhist && strcmp(hist[hpos], cwd) == 0)
		return;
	if(hpos+1 >= Maxhist){
		memmove(hist, hist+1, sizeof hist - Maxpath);
		nhist--;
	}
	hpos++;
	strecpy(hist[hpos], hist[hpos]+Maxpath, cwd);
	nhist = hpos+1;
}

/* ---- wctl launches ---- */

static void
runentry(char *path)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "new -r 90 100 690 470 rc %s", path);
	close(fd);
}

static void
openfile(char *path)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd < 0)
		return;
	fprint(fd, "new -r 90 100 790 570 jot %s", path);
	close(fd);
}

static void
expnewwin(void)
{
	int fd;

	fd = open("/dev/wctl", OWRITE);
	if(fd >= 0){
		if(mycomp)
			fprint(fd, "new -r 60 80 760 560 explorer /mycomputer");
		else
			fprint(fd, "new -r 60 80 760 560 explorer %s", cwd);
		close(fd);
	}
}

static void
openpath(char *path, int isdir)
{
	if(isdir)
		navigate(path);
	else
		openfile(path);
}

static int
mcopen(int i)
{
	char path[Maxpath];

	if(i < 0 || i >= nelem(mcents)-1)
		return 0;
	if(mcents[i].path == nil){
		openmsg("Error", "The device is not accessible.");
		return 0;
	}
	if(mcents[i].path[0] == 0)
		strecpy(path, path+sizeof path, trashdir());
	else
		strecpy(path, path+sizeof path, mcents[i].path);
	navigate(path);
	return 1;
}

/* ---- file operations ---- */

static int
dircopy(char *from, char *to)
{
	Dir *d;
	char *sub, *subto;
	uchar *buf;
	int fd, n, i, ok, in, out;

	d = dirstat(from);
	if(d == nil)
		return -1;
	if(!(d->mode & DMDIR)){
		in = open(from, OREAD);
		if(in < 0){
			free(d);
			return -1;
		}
		remove(to);
		out = create(to, OWRITE, d->mode & 0777);
		if(out < 0){
			close(in);
			free(d);
			return -1;
		}
		buf = malloc(65536);
		if(buf != nil){
			while((n = read(in, buf, 65536)) > 0)
				if(write(out, buf, n) < 0)
					break;
			free(buf);
		}
		close(in);
		close(out);
		free(d);
		return 0;
	}
	free(d);
	remove(to);
	if(create(to, OREAD, DMDIR|0777) < 0)
		return -1;
	fd = open(from, OREAD);
	if(fd < 0)
		return -1;
	n = dirreadall(fd, &d);
	close(fd);
	ok = 0;
	for(i = 0; i < n; i++){
		sub = smprint("%s/%s", from, d[i].name);
		subto = smprint("%s/%s", to, d[i].name);
		if(sub && subto && dircopy(sub, subto) < 0)
			ok = -1;
		free(sub);
		free(subto);
	}
	free(d);
	return ok;
}

static int
removeall(char *path)
{
	Dir *d;
	char *sub;
	int fd, n, i, rc;

	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	n = dirreadall(fd, &d);
	close(fd);
	if(n < 0)
		return -1;
	rc = 0;
	for(i = 0; i < n; i++){
		sub = smprint("%s/%s", path, d[i].name);
		if(sub == nil)
			continue;
		if(d[i].mode & DMDIR)
			removeall(sub);
		if(remove(sub) < 0)
			rc = -1;
		free(sub);
	}
	free(d);
	if(remove(path) < 0 && rc == 0)
		rc = -1;
	return rc;
}

static void
originpath(char *dst, int nd)
{
	char *t;

	t = trashdir();
	snprint(dst, nd, "%s/.origin", t);
}

static void
originrecord(char *base, char *origdir)
{
	char op[Maxpath];
	int fd;

	originpath(op, sizeof op);
	fd = open(op, OWRITE);
	if(fd < 0)
		fd = create(op, OWRITE, 0666);
	if(fd < 0)
		return;
	seek(fd, 0, 2);
	fprint(fd, "%s\t%s\n", base, origdir);
	close(fd);
}

static int
originlookup(char *base, char *origdir, int nd)
{
	char op[Maxpath], buf[8192];
	char *f[3];
	int nf, found, fd, n, i, start;

	originpath(op, sizeof op);
	found = 0;
	fd = open(op, OREAD);
	if(fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return 0;
	buf[n] = 0;
	start = 0;
	for(i = 0; i <= n; i++){
		if(i == n || buf[i] == '\n'){
			if(i > start){
				buf[i] = 0;
				nf = getfields(buf+start, f, nelem(f), 0, "\t");
				if(nf >= 2 && strcmp(f[0], base) == 0){
					strecpy(origdir, origdir+nd, f[1]);
					found = 1;
					break;
				}
			}
			start = i+1;
		}
	}
	return found;
}

static void
originremove(char *base)
{
	char op[Maxpath], buf[8192], out[8192];
	char *f[3];
	int nf, fd, n, i, start, o, len, save;

	originpath(op, sizeof op);
	fd = open(op, OREAD);
	if(fd < 0)
		return;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0)
		return;
	buf[n] = 0;
	o = 0;
	start = 0;
	for(i = 0; i <= n; i++){
		if(i != n && buf[i] != '\n')
			continue;
		if(i > start){
			save = buf[i];
			buf[i] = 0;
			nf = getfields(buf+start, f, nelem(f), 0, "\t");
			if(!(nf >= 1 && strcmp(f[0], base) == 0)){
				len = i - start;
				if(o + len + 1 < sizeof out){
					memmove(out+o, buf+start, len);
					o += len;
					out[o++] = '\n';
				}
			}
			buf[i] = save;
		}
		start = i+1;
	}
	fd = create(op, OWRITE, 0666);
	if(fd >= 0){
		write(fd, out, o);
		close(fd);
	}
}

static void
deletetotrash(int i)
{
	char path[Maxpath], dest[Maxpath], base[136], *t;
	int n, fd;

	if(i < 0 || i >= nents || strcmp(ents[i].name, "..") == 0)
		return;
	cleanpath(path, sizeof path, cwd, ents[i].name);
	t = trashdir();
	fd = create(t, OREAD, DMDIR|0777);
	if(fd >= 0)
		close(fd);
	strecpy(base, base+sizeof base, ents[i].name);
	for(n = 0; n < 100; n++){
		if(n == 0)
			snprint(dest, sizeof dest, "%s/%s", t, base);
		else
			snprint(dest, sizeof dest, "%s/%s %d", t, base, n+1);
		if(access(dest, AEXIST) < 0)
			break;
	}
	if(dircopy(path, dest) == 0){
		removeall(path);
		originrecord(strrchr(dest, '/')+1, cwd);
	}
	loaddir(cwd);
}

static void
restorefromtrash(int i)
{
	char path[Maxpath], origdir[Maxpath], dest[Maxpath];

	if(i < 0 || i >= nents || strcmp(ents[i].name, "..") == 0)
		return;
	cleanpath(path, sizeof path, cwd, ents[i].name);
	if(!originlookup(ents[i].name, origdir, sizeof origdir))
		strecpy(origdir, origdir+sizeof origdir, "/usr/glenda");
	if(strcmp(origdir, "/") == 0)
		snprint(dest, sizeof dest, "/%s", ents[i].name);
	else
		snprint(dest, sizeof dest, "%s/%s", origdir, ents[i].name);
	if(dircopy(path, dest) == 0){
		removeall(path);
		originremove(ents[i].name);
	}
	loaddir(cwd);
}

static void
emptybin(void)
{
	Dir *d;
	char *t, *sub;
	int fd, n, i;

	t = trashdir();
	fd = open(t, OREAD);
	if(fd < 0)
		return;
	n = dirreadall(fd, &d);
	close(fd);
	for(i = 0; i < n; i++){
		sub = smprint("%s/%s", t, d[i].name);
		if(sub == nil)
			continue;
		if(d[i].mode & DMDIR)
			removeall(sub);
		remove(sub);
		free(sub);
	}
	free(d);
	loaddir(cwd);
}

static void
pasteclip(void)
{
	char dest[Maxpath], *slash;

	if(clipboard[0] == 0)
		return;
	slash = strrchr(clipboard, '/');
	cleanpath(dest, sizeof dest, cwd, slash ? slash+1 : clipboard);
	if(dircopy(clipboard, dest) == 0 && clipcut){
		removeall(clipboard);
		clipboard[0] = 0;
	}
	loaddir(cwd);
}

static void
renameentry(int i, char *name)
{
	char buf[Maxpath];
	Dir *d;

	if(i < 0 || i >= nents || strcmp(ents[i].name, "..") == 0)
		return;
	if(name[0] == 0 || strcmp(name, ents[i].name) == 0)
		return;
	cleanpath(buf, sizeof buf, cwd, ents[i].name);
	d = dirstat(buf);
	if(d == nil)
		return;
	strecpy(d->name, d->name+sizeof d->name, name);
	if(dirwstat(buf, d) < 0){
		free(d);
		return;
	}
	free(d);
	loaddir(cwd);
}

/* ---- modal helpers ---- */

static void
openmsg(char *title, char *line)
{
	strecpy(msgtitle, msgtitle+sizeof msgtitle, title);
	strecpy(msgline, msgline+sizeof msgline, line);
	uimode = UIMsg;
}

static void
openprompt(char *title, char *label, char *buf, int cap, int action, int item)
{
	strecpy(ptitle, ptitle+sizeof ptitle, title);
	strecpy(plabel, plabel+sizeof plabel, label);
	if(buf != nil){
		strecpy(pbuf, pbuf+sizeof pbuf, buf);
		plen = strlen(pbuf);
	}else
		plen = 0;
	pcap = cap;
	paction = action;
	pitem = item;
	uimode = UIPrompt;
}

static void
openprops(int i)
{
	propsidx = i;
	uimode = UIProps;
}

static void
openmenu(char **items, Rectangle at, int barnum, int item)
{
	int i, w;

	menuitems = items;
	nmenuitems = 0;
	for(i = 0; items[i]; i++)
		nmenuitems++;
	w = 0;
	for(i = 0; i < nmenuitems; i++)
		if(MeasureText(items[i], Fontsz) > w)
			w = MeasureText(items[i], Fontsz);
	menurect.x = at.x;
	menurect.y = at.y;
	menurect.width = w + 30;
	menurect.height = nmenuitems*Mitemh + 6;
	if(menurect.x + menurect.width > GetScreenWidth())
		menurect.x = GetScreenWidth() - menurect.width;
	if(menurect.y + menurect.height > GetScreenHeight())
		menurect.y = GetScreenHeight() - menurect.height;
	if(menurect.x < 0)
		menurect.x = 0;
	if(menurect.y < 0)
		menurect.y = 0;
	menubarnum = barnum;
	ctxitem = item;
	menuhover = -1;
	uimode = UIMenu;
}

/* ---- menu actions ---- */

static void
domenu(int m, int item, int r)
{
	char path[Maxpath];

	switch(m){
	case 0:		/* File */
		switch(r){
		case 0: expnewwin(); break;
		case 1:
			openprompt("New Folder", "Name:", nil, sizeof pbuf,
			    PActNewDir, -1);
			break;
		case 3:
			uirun = 0;
			break;
		}
		break;
	case 1:		/* Edit */
		switch(r){
		case 0:
		case 1:
			if(item >= 0 && item < nents){
				cleanpath(clipboard, sizeof clipboard, cwd,
				    ents[item].name);
				clipcut = r == 1;
			}
			break;
		case 2:
			pasteclip();
			break;
		case 4:
			sel = nents > 1 ? 1 : -1;
			break;
		}
		break;
	case 2:		/* View */
		switch(r){
		case 0:
			viewmode = viewmode == ViewList ? ViewIcon : ViewList;
			scroll = 0;
			break;
		case 2:
			if(mycomp)
				navigate("/mycomputer");
			else
				loaddir(cwd);
			break;
		}
		break;
	case 3:		/* Help */
		if(r == 0)
			openmsg("About Explorer",
			    "TaijiOS Explorer - Kryon desktop shell");
		break;
	default:	/* context menus */
		if(menuitems == cpitems){
			if(r == 0 && item >= 0 && item < nents){
				cleanpath(path, sizeof path, cwd, ents[item].name);
				runentry(path);
			}
			break;
		}
		if(menuitems == tritems){
			if(r == 0)
				restorefromtrash(item);
			else if(r == 2)
				emptybin();
			break;
		}
		if(menuitems == bgitems){
			switch(r){
			case 0:
				openprompt("New Folder", "Name:", nil, sizeof pbuf,
				    PActNewDir, -1);
				break;
			case 1:
				pasteclip();
				break;
			case 3:
				loaddir(cwd);
				break;
			case 5:
				openprops(item >= 0 ? item : -1);
				break;
			}
			break;
		}
		/* item menu */
		switch(r){
		case 0:
			if(item >= 0 && item < nents){
				cleanpath(path, sizeof path, cwd, ents[item].name);
				openpath(path, ents[item].isdir);
			}
			break;
		case 1:
			if(item >= 0 && item < nents){
				cleanpath(clipboard, sizeof clipboard, cwd,
				    ents[item].name);
				clipcut = 0;
			}
			break;
		case 2:
			if(item >= 0 && item < nents){
				cleanpath(clipboard, sizeof clipboard, cwd,
				    ents[item].name);
				clipcut = 1;
			}
			break;
		case 3:
			if(item >= 0 && item < nents)
				openprompt("Rename", "New name:", ents[item].name,
				    sizeof pbuf, PActRename, item);
			break;
		case 4:
			deletetotrash(item);
			break;
		case 6:
			openprops(item);
			break;
		}
		break;
	}
}

/* ---- icon drawing ---- */

static void
drawfileicon(Rectangle r, int isdir, int big)
{
	Rectangle b;
	int s, x, y;

	s = big ? 2 : 1;
	if(isdir){
		b.x = r.x + 3*s;
		b.y = r.y + 6*s;
		b.width = 16*s;
		b.height = 10*s;
		DrawRectangleRec(b, mkcol(0xE8, 0xC0, 0x50));
		DrawRectangleLines((int)b.x, (int)b.y, (int)b.width, (int)b.height,
		    muted(0.5f));
		b.x = r.x + 5*s;
		b.y = r.y + 3*s;
		b.width = 8*s;
		b.height = 5*s;
		DrawRectangleRec(b, mkcol(0xE8, 0xC0, 0x50));
		return;
	}
	b.x = r.x + 5*s;
	b.y = r.y + 3*s;
	b.width = 12*s;
	b.height = 14*s;
	DrawRectangleRec(b, oc(GetThemeSurface()));
	DrawRectangleLines((int)b.x, (int)b.y, (int)b.width, (int)b.height,
	    muted(0.5f));
	x = (int)b.x + 3*s;
	y = (int)b.y + 5*s;
	DrawLine(x, y, x + 6*s, y, muted(0.5f));
	DrawLine(x, y + 4*s, x + 6*s, y + 4*s, muted(0.5f));
}

static void
drawcpicon(Rectangle r, char *name)
{
	int cx, cy;

	cx = (int)r.x + Iconw/2;
	cy = (int)r.y + 22;
	if(strcmp(name, "Display") == 0){
		DrawRectangle(cx-15, cy-10, 30, 18, oc(GetThemeSurface()));
		DrawRectangleLines(cx-15, cy-10, 30, 18, muted(0.6f));
		DrawRectangle(cx-5, cy+9, 10, 4, muted(0.6f));
	}else if(strcmp(name, "Keyboard") == 0){
		DrawRectangle(cx-16, cy-8, 32, 19, oc(GetThemeSurface()));
		DrawRectangleLines(cx-16, cy-8, 32, 19, muted(0.6f));
		DrawLine(cx-12, cy, cx+12, cy, muted(0.6f));
		DrawLine(cx-12, cy+6, cx+12, cy+6, muted(0.6f));
	}else if(strcmp(name, "Mouse") == 0){
		DrawCircle(cx, cy, 11, oc(GetThemeSurface()));
		DrawCircleLines(cx, cy, 11, muted(0.6f));
		DrawLine(cx, cy-8, cx, cy-2, muted(0.6f));
	}else if(strcmp(name, "Network") == 0){
		DrawCircle(cx-11, cy-5, 5, oc(GetThemeSurface()));
		DrawCircle(cx+11, cy-5, 5, oc(GetThemeSurface()));
		DrawCircle(cx, cy+10, 5, oc(GetThemeSurface()));
		DrawLine(cx-7, cy-1, cx-2, cy+6, muted(0.6f));
		DrawLine(cx+7, cy-1, cx+2, cy+6, muted(0.6f));
	}else if(strcmp(name, "Date-Time") == 0){
		DrawCircle(cx, cy, 14, oc(GetThemeSurface()));
		DrawCircleLines(cx, cy, 14, muted(0.6f));
		DrawLine(cx, cy, cx, cy-9, muted(0.6f));
		DrawLine(cx, cy, cx+8, cy+4, muted(0.6f));
	}else if(strcmp(name, "Users") == 0){
		DrawCircle(cx, cy-7, 6, oc(GetThemeSurface()));
		DrawCircleLines(cx, cy-7, 6, muted(0.6f));
		DrawRectangle(cx-13, cy+3, 26, 12, oc(GetThemeSurface()));
	}else if(strcmp(name, "Fonts") == 0){
		DrawText("A", cx-12, cy-11, Fontsz, oc(GetThemeText()));
		DrawText("a", cx, cy-2, Fontsz, oc(GetThemeText()));
	}else{
		DrawRectangle(cx-14, cy-13, 28, 26, oc(GetThemeSurface()));
		DrawRectangleLines(cx-14, cy-13, 28, 26, muted(0.6f));
		DrawLine(cx-9, cy-5, cx+9, cy-5, muted(0.6f));
		DrawLine(cx-9, cy+2, cx+9, cy+2, muted(0.6f));
		DrawLine(cx-9, cy+9, cx+9, cy+9, muted(0.6f));
	}
}

static void
drawmcicon(Rectangle r, McEnt *e)
{
	int cx, cy;

	cx = (int)r.x + Iconw/2;
	cy = (int)r.y + 22;
	switch(e->drive){
	case 1:
	case 2:
		DrawRectangle(cx-17, cy-9, 34, 18, oc(GetThemeSurface()));
		DrawRectangleLines(cx-17, cy-9, 34, 18, muted(0.6f));
		DrawRectangle(cx-13, cy-5, 26, 6, oc(GetThemeBackground()));
		DrawRectangle(cx+6, cy+4, 7, 3, mkcol(0x30, 0xC0, 0x50));
		if(e->drive == 2)
			DrawRectangle(cx-13, cy+3, 26, 2, muted(0.6f));
		break;
	case 3:
		DrawCircle(cx, cy, 14, oc(GetThemeSurface()));
		DrawCircleLines(cx, cy, 14, muted(0.6f));
		DrawCircle(cx, cy, 4, oc(GetThemeBackground()));
		break;
	default:
		if(strcmp(e->label, "Recycle Bin") == 0){
			DrawLine(cx-7, cy-10, cx+7, cy-10, muted(0.6f));
			DrawLine(cx-9, cy-9, cx-5, cy+11, muted(0.6f));
			DrawLine(cx+9, cy-9, cx+5, cy+11, muted(0.6f));
			DrawLine(cx-5, cy+11, cx+5, cy+11, muted(0.6f));
			DrawRectangle(cx-4, cy-8, 2, 17, muted(0.4f));
			DrawRectangle(cx+3, cy-8, 2, 17, muted(0.4f));
			DrawRectangle(cx-3, cy-13, 6, 3, muted(0.6f));
		}else if(strcmp(e->label, "Control Panel") == 0){
			DrawRectangle(cx-14, cy-10, 28, 20, oc(GetThemeSurface()));
			DrawRectangleLines(cx-14, cy-10, 28, 20, muted(0.6f));
			DrawRectangle(cx-11, cy-7, 22, 3, oc(GetThemeLink()));
			DrawLine(cx-7, cy-1, cx+7, cy-1, muted(0.6f));
			DrawLine(cx-7, cy+3, cx+7, cy+3, muted(0.6f));
			DrawRectangle(cx-4, cy-2, 3, 3, mkcol(0x30, 0xC0, 0x50));
			DrawRectangle(cx+1, cy+2, 3, 3, mkcol(0xE0, 0x40, 0x40));
		}else{
			DrawRectangle(cx-16, cy-6, 32, 17, mkcol(0xE8, 0xC0, 0x50));
			DrawRectangle(cx-12, cy-11, 14, 5, mkcol(0xE8, 0xC0, 0x50));
			DrawRectangleLines(cx-16, cy-6, 32, 17, muted(0.5f));
		}
		break;
	}
}

/* ---- chrome drawing ---- */

static int
drawtoolbtn(int i, char *s)
{
	Rectangle r;
	Vector2 m;
	int hover;

	r = toolbtnrect(i);
	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	DrawRectangleRounded(r, 0.15f, 4,
	    hover ? oc(GetThemeButtonHover()) : oc(GetThemeButton()));
	DrawText(s, (int)r.x + 8, (int)r.y + ((int)r.height-Fontsz)/2, Fontsz,
	    oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int
drawmenubaritem(int i)
{
	Rectangle r;
	Vector2 m;
	int hover;

	r = menubaritemrect(i);
	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	if(hover)
		DrawRectangleRec(r, Fade(GetThemeButtonHover(), 0.4f));
	DrawText(menutitles[i], (int)r.x + 4,
	    (Menuh-Fontsz)/2, Fontsz, oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int
drawtreeitem(int i)
{
	Rectangle r;
	Vector2 m;
	int hover;

	if(tree[i].path[0] == 0)
		strecpy(tree[i].path, tree[i].path+Maxpath, trashdir());
	r.x = 4;
	r.y = tree[i].y - 3;
	r.width = Treew - 8;
	r.height = Fontsz + 6;
	tree[i].r = r;
	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	if(i == selectedtree)
		DrawRectangleRounded(r, 0.1f, 4, Fade(GetThemeLink(), 0.35f));
	else if(hover)
		DrawRectangleRounded(r, 0.1f, 4, Fade(GetThemeButtonHover(), 0.4f));
	DrawText(tree[i].label, tree[i].indent, tree[i].y, Fontsz,
	    oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* one entry row in list view; returns 1 when clicked */
static int
drawlistrow(Rectangle lr, int i, int y)
{
	Rectangle r;
	Vector2 m;
	char buf[64];
	int hover, on;

	r.x = lr.x + 4;
	r.y = y;
	r.width = lr.width - 8;
	r.height = Rowh;
	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	on = i == sel;
	if(on)
		DrawRectangleRec(r, oc(GetThemeButtonHover()));
	else if(hover)
		DrawRectangleRec(r, Fade(GetThemeButtonHover(), 0.35f));
	drawfileicon(r, ents[i].isdir, 0);
	DrawText(ents[i].name, (int)r.x + 24, (int)r.y + 2, Fontsz,
	    oc(GetThemeText()));
	if(!ents[i].isdir){
		snprint(buf, sizeof buf, "%lld", ents[i].len);
		DrawText(buf, (int)lr.x + 230, (int)r.y + 2, Fontsz,
		    muted(0.85f));
	}
	DrawText(ents[i].type, (int)lr.x + 320, (int)r.y + 2, Fontsz,
	    muted(0.85f));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* one entry tile in icon view; returns 1 when clicked */
static int
drawicontile(Rectangle r, int i)
{
	Vector2 m;
	char *name;
	int hover, on, w;

	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	on = i == sel;
	if(on)
		DrawRectangleRounded(r, 0.08f, 4, Fade(GetThemeLink(), 0.35f));
	else if(hover)
		DrawRectangleRounded(r, 0.08f, 4, Fade(GetThemeButtonHover(), 0.4f));
	drawfileicon(r, ents[i].isdir, 1);
	name = ents[i].name;
	w = MeasureText(name, Fontsz);
	if((int)r.width < w){
		DrawText(name, (int)r.x, (int)r.y + 48, Fontsz, oc(GetThemeText()));
	}else
		DrawText(name, (int)r.x + ((int)r.width - w)/2, (int)r.y + 48,
		    Fontsz, oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int
drawmctile(Rectangle r, int i)
{
	Vector2 m;
	char *name;
	int hover, w;

	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	if(hover)
		DrawRectangleRounded(r, 0.08f, 4, Fade(GetThemeButtonHover(), 0.4f));
	drawmcicon(r, &mcents[i]);
	name = mcents[i].label;
	w = MeasureText(name, Fontsz);
	if((int)r.width < w)
		DrawText(name, (int)r.x, (int)r.y + 48, Fontsz, oc(GetThemeText()));
	else
		DrawText(name, (int)r.x + ((int)r.width - w)/2, (int)r.y + 48,
		    Fontsz, oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int
drawcptile(Rectangle r, int i)
{
	Vector2 m;
	char *name;
	int hover, w;

	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	if(hover)
		DrawRectangleRounded(r, 0.08f, 4, Fade(GetThemeButtonHover(), 0.4f));
	drawcpicon(r, ents[i].name);
	name = ents[i].name;
	w = MeasureText(name, Fontsz);
	if((int)r.width < w)
		DrawText(name, (int)r.x, (int)r.y + 48, Fontsz, oc(GetThemeText()));
	else
		DrawText(name, (int)r.x + ((int)r.width - w)/2, (int)r.y + 48,
		    Fontsz, oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* ---- modal drawing ---- */

static void
drawmodalmenu(void)
{
	Rectangle ir;
	Vector2 m;
	int i, hover;

	DrawRectangleRec(menurect, oc(GetThemeSurface()));
	DrawRectangleLines((int)menurect.x, (int)menurect.y,
	    (int)menurect.width, (int)menurect.height, muted(0.5f));
	m = GetMousePosition();
	menuhover = -1;
	for(i = 0; i < nmenuitems; i++){
		ir.x = menurect.x + 3;
		ir.y = menurect.y + 3 + i*Mitemh;
		ir.width = menurect.width - 6;
		ir.height = Mitemh;
		if(menuitems[i][0] == '-' && menuitems[i][1] == 0){
			DrawLine((int)ir.x + 4, (int)ir.y + Mitemh/2,
			    (int)(ir.x + ir.width) - 4, (int)ir.y + Mitemh/2,
			    muted(0.4f));
			continue;
		}
		hover = CheckCollisionPointRec(m, ir) ? i : -1;
		if(hover == i)
			DrawRectangleRec(ir, oc(GetThemeButtonHover()));
		DrawText(menuitems[i], (int)ir.x + 8,
		    (int)ir.y + (Mitemh-Fontsz)/2, Fontsz, oc(GetThemeText()));
		if(hover == i)
			menuhover = i;
	}
}

static int
drawokbtn(Rectangle r)
{
	Vector2 m;
	int hover;

	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, r);
	DrawRectangleRounded(r, 0.15f, 4,
	    hover ? oc(GetThemeButtonHover()) : oc(GetThemeButton()));
	DrawText("OK", (int)r.x + ((int)r.width - MeasureText("OK", Fontsz))/2,
	    (int)r.y + ((int)r.height-Fontsz)/2, Fontsz, oc(GetThemeText()));
	return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void
drawmodalmsg(void)
{
	Rectangle dlg, ok;

	dlg.x = GetScreenWidth()/2 - 170;
	dlg.y = GetScreenHeight()/2 - 60;
	dlg.width = 340;
	dlg.height = 120;
	DrawRectangleRec(dlg, oc(GetThemeSurface()));
	DrawRectangleLines((int)dlg.x, (int)dlg.y, (int)dlg.width,
	    (int)dlg.height, muted(0.5f));
	DrawRectangle((int)dlg.x + 3, (int)dlg.y + 3,
	    (int)dlg.width - 6, 24, oc(GetThemeLink()));
	DrawText(msgtitle, (int)dlg.x + 10, (int)dlg.y + 8, Fontsz,
	    oc(GetThemeBackground()));
	DrawText(msgline, (int)dlg.x + 20, (int)dlg.y + 44, Fontsz,
	    oc(GetThemeText()));
	ok.x = dlg.x + dlg.width - 100;
	ok.y = dlg.y + dlg.height - 34;
	ok.width = 84;
	ok.height = 26;
	if(drawokbtn(ok))
		uimode = UINone;
}

static void
drawmodalprompt(void)
{
	Rectangle dlg, entry, ok, cancel;
	Vector2 m;
	char newpath[Maxpath], ebuf[Maxpath+64];
	int hover;

	dlg.x = GetScreenWidth()/2 - 190;
	dlg.y = GetScreenHeight()/2 - 80;
	dlg.width = 380;
	dlg.height = 160;
	DrawRectangleRec(dlg, oc(GetThemeSurface()));
	DrawRectangleLines((int)dlg.x, (int)dlg.y, (int)dlg.width,
	    (int)dlg.height, muted(0.5f));
	DrawRectangle((int)dlg.x + 3, (int)dlg.y + 3,
	    (int)dlg.width - 6, 24, oc(GetThemeLink()));
	DrawText(ptitle, (int)dlg.x + 10, (int)dlg.y + 8, Fontsz,
	    oc(GetThemeBackground()));
	DrawText(plabel, (int)dlg.x + 20, (int)dlg.y + 38, Fontsz,
	    oc(GetThemeText()));
	entry.x = dlg.x + 20;
	entry.y = dlg.y + 64;
	entry.width = dlg.width - 40;
	entry.height = 28;
	DrawRectangleRec(entry, oc(GetThemeBackground()));
	DrawRectangleLines((int)entry.x, (int)entry.y, (int)entry.width,
	    (int)entry.height, muted(0.5f));
	pbuf[plen] = 0;
	DrawText(pbuf, (int)entry.x + 6,
	    (int)entry.y + ((int)entry.height-Fontsz)/2, Fontsz,
	    oc(GetThemeText()));
	ok.x = dlg.x + dlg.width - 180;
	ok.y = dlg.y + dlg.height - 34;
	ok.width = 84;
	ok.height = 26;
	cancel = ok;
	cancel.x = dlg.x + dlg.width - 90;
	m = GetMousePosition();
	hover = CheckCollisionPointRec(m, ok);
	DrawRectangleRounded(ok, 0.15f, 4,
	    hover ? oc(GetThemeButtonHover()) : oc(GetThemeButton()));
	DrawText("OK", (int)ok.x + ((int)ok.width - MeasureText("OK", Fontsz))/2,
	    (int)ok.y + ((int)ok.height-Fontsz)/2, Fontsz, oc(GetThemeText()));
	if(hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
		if(paction == PActNewDir){
			if(pbuf[0] == 0)
				strecpy(pbuf, pbuf+sizeof pbuf, "New Folder");
			cleanpath(newpath, sizeof newpath, cwd, pbuf);
			if(create(newpath, OREAD, DMDIR|0777) < 0){
				snprint(ebuf, sizeof ebuf,
				    "Cannot create %s", newpath);
				openmsg("Error", ebuf);
			}
			loaddir(cwd);
		}else if(paction == PActRename)
			renameentry(pitem, pbuf);
		if(uimode != UIMsg)
			uimode = UINone;
		return;
	}
	hover = CheckCollisionPointRec(m, cancel);
	DrawRectangleRounded(cancel, 0.15f, 4,
	    hover ? oc(GetThemeButtonHover()) : oc(GetThemeButton()));
	DrawText("Cancel",
	    (int)cancel.x + ((int)cancel.width - MeasureText("Cancel", Fontsz))/2,
	    (int)cancel.y + ((int)cancel.height-Fontsz)/2, Fontsz,
	    oc(GetThemeText()));
	if(hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		uimode = UINone;
}

static void
drawmodalprops(void)
{
	Rectangle dlg, ok;
	char buf[128];
	char *cts;

	if(propsidx < 0 || propsidx >= nents){
		uimode = UINone;
		return;
	}
	cts = ctime(ents[propsidx].mtime);
	dlg.x = GetScreenWidth()/2 - 170;
	dlg.y = GetScreenHeight()/2 - 90;
	dlg.width = 340;
	dlg.height = 180;
	DrawRectangleRec(dlg, oc(GetThemeSurface()));
	DrawRectangleLines((int)dlg.x, (int)dlg.y, (int)dlg.width,
	    (int)dlg.height, muted(0.5f));
	DrawRectangle((int)dlg.x + 3, (int)dlg.y + 3,
	    (int)dlg.width - 6, 24, oc(GetThemeLink()));
	DrawText("Properties", (int)dlg.x + 10, (int)dlg.y + 8, Fontsz,
	    oc(GetThemeBackground()));
	DrawText(ents[propsidx].name, (int)dlg.x + 20, (int)dlg.y + 40, Fontsz,
	    oc(GetThemeText()));
	DrawText(ents[propsidx].isdir ? "Type: File Folder" : "Type: File",
	    (int)dlg.x + 20, (int)dlg.y + 58, Fontsz, oc(GetThemeText()));
	if(!ents[propsidx].isdir){
		snprint(buf, sizeof buf, "Size: %lld bytes", ents[propsidx].len);
		DrawText(buf, (int)dlg.x + 20, (int)dlg.y + 76, Fontsz,
		    oc(GetThemeText()));
	}
	if(ents[propsidx].mtime > 0){
		snprint(buf, sizeof buf, "Modified: %.20s", cts);
		DrawText(buf, (int)dlg.x + 20, (int)dlg.y + 94, Fontsz,
		    oc(GetThemeText()));
	}
	DrawText("Location:", (int)dlg.x + 20, (int)dlg.y + 112, Fontsz,
	    oc(GetThemeText()));
	DrawText(cwd, (int)dlg.x + 20, (int)dlg.y + 130, Fontsz,
	    muted(0.8f));
	ok.x = dlg.x + dlg.width - 100;
	ok.y = dlg.y + dlg.height - 34;
	ok.width = 84;
	ok.height = 26;
	if(drawokbtn(ok))
		uimode = UINone;
}

/* ---- item hit testing (pure) ---- */

static int
gridcount(void)
{
	int i, n;

	if(mycomp){
		for(i = 0; mcents[i].label != nil; i++)
			;
		return i;
	}
	n = 0;
	for(i = 0; i < nents; i++){
		if(controlpanel() && strcmp(ents[i].name, "..") == 0)
			continue;
		n++;
	}
	return n;
}

static int
itemat(Vector2 p)
{
	Rectangle lr, r;
	int i, n;

	lr = listrect();
	if(!CheckCollisionPointRec(p, lr))
		return -1;
	if(mycomp){
		n = gridcount();
		for(i = 0; i < n; i++)
			if(CheckCollisionPointRec(p, gridrect(lr, i)))
				return i;
		return -1;
	}
	if(strcmp(cwd, "/lib/controlpanel") == 0){
		n = 0;
		for(i = 0; i < nents; i++){
			if(strcmp(ents[i].name, "..") == 0)
				continue;
			if(CheckCollisionPointRec(p, gridrect(lr, n)))
				return i;
			n++;
		}
		return -1;
	}
	if(viewmode == ViewIcon){
		for(i = scroll; i < nents; i++){
			r = gridrect(lr, i - scroll);
			if(r.y + Iconh > lr.y + lr.height)
				break;
			if(CheckCollisionPointRec(p, r))
				return i;
		}
		return -1;
	}
	i = scroll + ((int)p.y - ((int)lr.y + 24)) / Rowh;
	if(i < 0 || i >= nents)
		return -1;
	return i;
}

/* ---- main ---- */

void
main(int argc, char *argv[])
{
	Rectangle r, lr;
	char path[Maxpath], buf[128], viewlabel[8];
	Vector2 m;
	double now;
	int i, n, done, hit, c;

	ARGBEGIN{
	default:
		break;
	}ARGEND
	if(argc > 0)
		strecpy(path, path+sizeof path, argv[0]);
	else if(getwd(path, sizeof path) == nil)
		strecpy(path, path+sizeof path, "/");
	if(strcmp(path, "/mycomputer") == 0){
		mycomp = 1;
		strecpy(path, path+sizeof path, "/");
	}
	cleanname(path);

	SetSingleInstance(0);
	InitWindow(700, 480, "explorer");
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

	if(!mycomp && loaddir(path) < 0)
		sysfatal("open %s: %r", path);
	if(mycomp){
		strecpy(cwd, cwd+sizeof cwd, "My Computer");
		selectedtree = 0;
		nhist = 0;
		hpos = -1;
	}else{
		strecpy(hist[0], hist[0]+Maxpath, cwd);
		nhist = 1;
		hpos = 0;
	}

	done = 0;
	while(uirun && !done && !WindowShouldClose()){
		m = GetMousePosition();
		now = GetTime()*1000.0;

		/* modal input first */
		if(uimode == UIPrompt){
			while((c = GetCharPressed()) != 0){
				if(c >= ' ' && c < 0x7f && plen < pcap-2)
					pbuf[plen++] = c;
			}
			if(IsKeyPressed(KEY_BACKSPACE) && plen > 0){
				while(plen > 0 && (pbuf[plen-1] & 0xC0) == 0x80)
					plen--;
				if(plen > 0)
					plen--;
			}
			if(IsKeyPressed(KEY_ESCAPE))
				uimode = UINone;
		}else{
			while(GetCharPressed() != 0)
				;	/* drain */
			if(IsKeyPressed(KEY_ESCAPE)){
				if(uimode != UINone)
					uimode = UINone;
				else
					done = 1;
			}
			if(uimode == UINone){
				if(IsKeyPressed(KEY_Q))
					done = 1;
				if(IsKeyPressed(KEY_V)){
					viewmode = viewmode == ViewList ?
					    ViewIcon : ViewList;
					scroll = 0;
				}
				if((IsKeyPressed(KEY_ENTER) ||
				    IsKeyPressed(KEY_KP_ENTER)) &&
				    sel >= 0 && sel < nents && !mycomp){
					cleanpath(path, sizeof path, cwd,
					    ents[sel].name);
					openpath(path, ents[sel].isdir);
				}
				if(IsKeyPressed(KEY_DELETE) && sel >= 0 &&
				    sel < nents && !mycomp && !controlpanel() &&
				    !trashview())
					deletetotrash(sel);
			}else if(uimode == UIMsg || uimode == UIProps){
				if(IsKeyPressed(KEY_ENTER) ||
				    IsKeyPressed(KEY_KP_ENTER))
					uimode = UINone;
			}else if(uimode == UIMenu){
				if(IsKeyPressed(KEY_ESCAPE))
					uimode = UINone;
			}
		}

		BeginDrawing();
		ClearBackground(oc(GetThemeBackground()));
		BeginUIFrame(GetScreenWidth(), GetScreenHeight(), 1.0f);
		BeginUI(0x6578706c);

		/* menu bar */
		r.x = 0;
		r.y = 0;
		r.width = GetScreenWidth();
		r.height = Menuh;
		DrawRectangleRec(r, oc(GetThemeSurface()));
		hit = -1;
		for(i = 0; menutitles[i]; i++)
			if(uimode == UINone && drawmenubaritem(i))
				hit = i;

		/* toolbar */
		r.y = Menuh;
		r.height = Toolbarh;
		DrawRectangleRec(r, oc(GetThemeSurface()));
		strecpy(viewlabel, viewlabel+sizeof viewlabel,
		    viewmode == ViewList ? "Icons" : "List");
		{
			char *tblabels[6];
			tblabels[0] = "Back";
			tblabels[1] = "Forward";
			tblabels[2] = "Up";
			tblabels[3] = "Refresh";
			tblabels[4] = "New Dir";
			tblabels[5] = viewlabel;
			if(uimode == UINone){
				if(drawtoolbtn(0, tblabels[0]))
					goback();
				if(drawtoolbtn(1, tblabels[1]))
					goforward();
				if(drawtoolbtn(2, tblabels[2])){
					if(mycomp)
						navigate("/");
					else{
						cleanpath(path, sizeof path,
						    cwd, "..");
						navigate(path);
					}
				}
				if(drawtoolbtn(3, tblabels[3])){
					if(!mycomp)
						loaddir(cwd);
				}
				if(drawtoolbtn(4, tblabels[4])){
					if(!mycomp)
						openprompt("New Folder", "Name:",
						    nil, sizeof pbuf, PActNewDir,
						    -1);
				}
				if(drawtoolbtn(5, tblabels[5])){
					viewmode = viewmode == ViewList ?
					    ViewIcon : ViewList;
					scroll = 0;
				}
			}else{
				for(i = 0; i < 6; i++)
					drawtoolbtn(i, tblabels[i]);
			}
		}

		/* address bar */
		r.y = Menuh + Toolbarh;
		r.height = Addrh;
		DrawRectangleRec(r, oc(GetThemeSurface()));
		DrawText("Address", 8, Menuh + Toolbarh + 5, Fontsz,
		    muted(0.8f));
		r.x = 70;
		r.width = GetScreenWidth() - 78;
		r.y += 3;
		r.height -= 6;
		DrawRectangleRec(r, oc(GetThemeBackground()));
		DrawRectangleLines((int)r.x, (int)r.y, (int)r.width,
		    (int)r.height, muted(0.4f));
		DrawText(mycomp ? "My Computer" : cwd, (int)r.x + 9,
		    (int)r.y + 5, Fontsz, oc(GetThemeText()));

		/* tree sidebar */
		r.x = 0;
		r.y = Menuh + Toolbarh + Addrh;
		r.width = Treew;
		r.height = GetScreenHeight() - r.y - Statush;
		DrawRectangleRec(r, oc(GetThemeSurface()));
		DrawRectangleLines((int)r.x, (int)r.y, (int)r.width,
		    (int)r.height, muted(0.3f));
		for(i = 0; i < nelem(tree); i++)
			if(uimode == UINone && drawtreeitem(i))
				navigate(tree[i].path);

		/* content area */
		lr = listrect();
		DrawRectangleRec(lr, oc(GetThemeBackground()));
		DrawRectangleLines((int)lr.x, (int)lr.y, (int)lr.width,
		    (int)lr.height, muted(0.3f));
		if(uimode == UINone){
			i = (int)GetMouseWheelMove();
			if(i > 0 && scroll > 0)
				scroll -= i;
			if(i < 0 && scroll+1 < nents)
				scroll += -i;
		}

		if(mycomp){
			n = gridcount();
			for(i = 0; i < n; i++){
				r = gridrect(lr, i);
				if(r.y + Iconh > lr.y + lr.height)
					break;
				if(drawmctile(r, i) && uimode == UINone){
					sel = i;
					if(i == lastsel &&
					    now - lastclick < 400.0){
						mcopen(i);
						lastsel = -1;
						lastclick = 0;
					}else{
						lastsel = i;
						lastclick = now;
					}
				}
			}
		}else if(controlpanel()){
			n = 0;
			for(i = 0; i < nents; i++){
				if(strcmp(ents[i].name, "..") == 0)
					continue;
				r = gridrect(lr, n);
				if(r.y + Iconh > lr.y + lr.height)
					break;
				if(drawcptile(r, i) && uimode == UINone){
					sel = i;
					if(i == lastsel &&
					    now - lastclick < 400.0){
						cleanpath(path, sizeof path,
						    cwd, ents[i].name);
						runentry(path);
						lastsel = -1;
						lastclick = 0;
					}else{
						lastsel = i;
						lastclick = now;
					}
				}
				n++;
			}
		}else if(viewmode == ViewIcon){
			for(i = scroll; i < nents; i++){
				r = gridrect(lr, i - scroll);
				if(r.y + Iconh > lr.y + lr.height)
					break;
				if(drawicontile(r, i) && uimode == UINone){
					sel = i;
					if(i == lastsel && now - lastclick < 400.0){
						cleanpath(path, sizeof path,
						    cwd, ents[i].name);
						openpath(path, ents[i].isdir);
						lastsel = -1;
						lastclick = 0;
					}else{
						lastsel = i;
						lastclick = now;
					}
				}
			}
		}else{
			DrawText("Name", (int)lr.x + 28, (int)lr.y + 4, Fontsz,
			    muted(0.8f));
			DrawText("Size", (int)lr.x + 230, (int)lr.y + 4, Fontsz,
			    muted(0.8f));
			DrawText("Type", (int)lr.x + 320, (int)lr.y + 4, Fontsz,
			    muted(0.8f));
			for(i = scroll; i < nents; i++){
				r.y = lr.y + 24 + (i - scroll)*Rowh;
				if(r.y + Rowh > lr.y + lr.height)
					break;
				if(drawlistrow(lr, i, r.y) && uimode == UINone){
					sel = i;
					if(i == lastsel && now - lastclick < 400.0){
						cleanpath(path, sizeof path,
						    cwd, ents[i].name);
						openpath(path, ents[i].isdir);
						lastsel = -1;
						lastclick = 0;
					}else{
						lastsel = i;
						lastclick = now;
					}
				}
			}
		}

		/* status bar */
		r.x = 0;
		r.y = GetScreenHeight() - Statush;
		r.width = GetScreenWidth();
		r.height = Statush;
		DrawRectangleRec(r, oc(GetThemeSurface()));
		if(mycomp)
			snprint(buf, sizeof buf, "%d object%s", gridcount(),
			    gridcount() == 2 ? "" : "s");
		else if(trashview())
			snprint(buf, sizeof buf, "Recycle Bin: %d object%s",
			    nents-1, nents == 2 ? "" : "s");
		else
			snprint(buf, sizeof buf, "%d object%s", nents,
			    nents == 1 ? "" : "s");
		DrawText(buf, 8, (int)r.y + 4, Fontsz, muted(0.85f));

		/* modals on top */
		if(uimode == UIMenu){
			drawmodalmenu();
			if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
				if(menuhover >= 0){
					uimode = UINone;
					domenu(menubarnum, ctxitem, menuhover);
				}else if(!CheckCollisionPointRec(m, menurect))
					uimode = UINone;
			}
		}else if(uimode == UIMsg)
			drawmodalmsg();
		else if(uimode == UIPrompt)
			drawmodalprompt();
		else if(uimode == UIProps)
			drawmodalprops();

		/* menubar clicks open menus (after draw so the rect is known) */
		if(hit >= 0 && uimode == UINone){
			vitems[0] = viewmode == ViewList ? "Large Icons" : "List";
			switch(hit){
			case 0: menuitems = fitems; break;
			case 1: menuitems = eitems; break;
			case 2: menuitems = vitems; break;
			default: menuitems = hitems; break;
			}
			r = menubaritemrect(hit);
			openmenu(menuitems, r, hit, sel);
		}

		/* right button opens the context menu */
		if(uimode == UINone && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){
			i = itemat(m);
			if(mycomp || controlpanel()){
				if(i >= 0){
					r.x = m.x;
					r.y = m.y;
					openmenu(cpitems, r, -2, i);
				}
			}else if(trashview()){
				r.x = m.x;
				r.y = m.y;
				if(i >= 0)
					openmenu(tritems, r, -2, i);
				else
					openmenu(bgitems, r, -2, -1);
			}else{
				r.x = m.x;
				r.y = m.y;
				if(i >= 0){
					sel = i;
					openmenu(ititems, r, -2, i);
				}else
					openmenu(bgitems, r, -2, -1);
			}
		}

		EndUI();
		EndUIFrame();
		EndDrawing();
	}
	CloseWindow();
	exits(nil);
}
