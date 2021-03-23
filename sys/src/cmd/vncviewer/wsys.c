#include "vnc.h"
#include "vncv.h"
#include <cursor.h>

typedef struct Cursor Cursor;

typedef struct Mouse Mouse;
struct Mouse {
	int buttons;
	Point xy;
};

static Rectangle
getscreenrect(void)
{
	char buf[5*12];
	int fd;
	Rectangle r;

	fd = open("/dev/screen", OREAD);
	if(fd < 0)
		sysfatal("open /dev/screen: %r");
	if(read(fd, buf, sizeof buf) != sizeof buf)
		sysfatal("read /dev/screen: %r");
	close(fd);

	r.min.x = atoi(buf+1*12);
	r.min.y = atoi(buf+2*12);
	r.max.x = atoi(buf+3*12);
	r.max.y = atoi(buf+4*12);
	return r;
}

static void
resize(Vnc *v, int first)
{
	int fd;
	Point d;

	d = addpt(v->dim, Pt(2*Borderwidth, 2*Borderwidth));
	lockdisplay(display);

	if(getwindow(display, Refnone) < 0)
		sysfatal("internal error: can't get the window image");

	/*
	 * limit the window to at most the vnc server's size
	 */
	if(first || d.x < Dx(screen->r) || d.y < Dy(screen->r)){
		fd = open("/dev/wctl", OWRITE);
		if(fd >= 0){
			fprint(fd, "resize -dx %d -dy %d", d.x, d.y);
			close(fd);
		}
	}
	unlockdisplay(display);
}

static void
eresized(void)
{
	resize(vnc, 0);

	requestupdate(vnc, 0);
}

static Cursor dotcursor = {
	{-7, -7},
	{0x00, 0x00,
	 0x00, 0x00,
	 0x00, 0x00,
	 0x00, 0x00, 
	 0x03, 0xc0,
	 0x07, 0xe0,
	 0x0f, 0xf0, 
	 0x0f, 0xf0,
	 0x0f, 0xf0,
	 0x07, 0xe0,
	 0x03, 0xc0,
	 0x00, 0x00, 
	 0x00, 0x00,
	 0x00, 0x00,
	 0x00, 0x00,
	 0x00, 0x00, },
	{0x00, 0x00,
	 0x00, 0x00,
	 0x00, 0x00,
	 0x00, 0x00, 
	 0x00, 0x00,
	 0x03, 0xc0,
	 0x07, 0xe0, 
	 0x07, 0xe0,
	 0x07, 0xe0,
	 0x03, 0xc0,
	 0x00, 0x00,
	 0x00, 0x00, 
	 0x00, 0x00,
	 0x00, 0x00,
	 0x00, 0x00,
	 0x00, 0x00, }
};

static void
mouseevent(Vnc *v, Mouse m)
{
	vnclock(v);
	vncwrchar(v, MMouse);
	vncwrchar(v, m.buttons);
	vncwrpoint(v, m.xy);
	vncflush(v);
	vncunlock(v);
}

enum {
	EventSize = 1+4*12
};
void
readmouse(Vnc *v)
{
	int cursorfd, fd, len, n;
	char buf[10*EventSize], *start, *end;
	uchar curs[2*4+2*2*16];
	Cursor *cs;
	Mouse m;

	cs = &dotcursor;

	snprint(buf, sizeof buf, "%s/mouse", display->devdir);
	if((fd = open(buf, OREAD)) < 0)
		sysfatal("open %s: %r", buf);

	snprint(buf, sizeof buf, "%s/cursor", display->devdir);
	if((cursorfd = open(buf, OWRITE)) < 0)
		sysfatal("open %s: %r", buf);

	BPLONG(curs+0*4, cs->offset.x);
	BPLONG(curs+1*4, cs->offset.y);
	memmove(curs+2*4, cs->clr, 2*2*16);
	write(cursorfd, curs, sizeof curs);

	resize(v, 1);
	requestupdate(vnc, 0);
	start = end = buf;
	len = 0;
	for(;;) {
		if((n = read(fd, end, sizeof(buf) - (end - buf))) < 0)
			sysfatal("read mouse failed");

		len += n;
		end += n;
		while(len >= EventSize) {
			if(*start == 'm') {
				m.xy.x = atoi(start+1);
				m.xy.y = atoi(start+1+12);
				m.buttons = atoi(start+1+2*12) & 7;
				m.xy = subpt(m.xy, screen->r.min);
				if(ptinrect(m.xy, Rpt(ZP, v->dim)))
					mouseevent(v, m);
			} else
				eresized();

			start += EventSize;
			len -= EventSize;
		}
		if(start - buf > sizeof(buf) - EventSize) {
			memmove(buf, start, len);
			start = buf;
			end = start+len;
		}
	}
}

static int snarffd = -1;
static ulong snarf_vers;

void 
writesnarf(Vnc *v, long n)
{
	uchar buf[8192];
	long m;
	Biobuf * fd;

	if( (fd = Bopen("/dev/snarf", OWRITE)) == nil ) {
		vncgobble(v, n);
		return;
	}

	while(n > 0) {
		m = n;
		if(m > sizeof(buf))
			m = sizeof(buf);
		vncrdbytes(v, buf, m);
		n -= m;

		Bwrite(fd, buf, m);
	}
	Bterm(fd);
	snarf_vers++;
}

char *
getsnarf(int * sz)
{
	char * snarf, * p;
	int n, c;

	*sz =0;
	n = 8192;
	p = snarf = malloc(n);

	seek(snarffd, 0, 0);
	while ((c = read(snarffd, p, n)) > 0) {
		p += c;
		n -= c;
		*sz += c;
		if ( n == 0 ) {
			snarf = realloc(snarf, *sz + 8192);
			n = 8192;
		}
	}
	return snarf;
}

void
checksnarf(Vnc *v)
{
	Dir dir;
	char *snarf;
	int len;

	if(snarffd < 0){
		snarffd = open("/dev/snarf", OREAD);
		if(snarffd < 0)
			sysfatal("can't open /dev/snarf: %r");
	}

	while(1) {
		sleep(1000);

		if(dirstat("/dev/snarf", &dir) >= 0 && dir.qid.vers != snarf_vers) {
			snarf = getsnarf(&len);

			vnclock(v);
			vncwrchar(v, MCCut);
			vncwrbytes(v, "pad", 3);
			vncwrlong(v, len);
			vncwrbytes(v, snarf, len);
			vncflush(v);
			vncunlock(v);

			free(snarf);

			snarf_vers = dir.qid.vers;
		}
	}
}
