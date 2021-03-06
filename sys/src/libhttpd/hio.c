#include <u.h>
#include <libc.h>
#include <httpd.h>

static	void	hexit(void);

static	Hio	*files[16];
static	int	maxfiles;
static	int	isinited;
static	char	hstates[] = "nrewE";
static	char	hxfers[] = " x";


int
hinit(Hio *h, int fd, int mode)
{
	if(!isinited){
		atexit(hexit);
		isinited = 1;
	}

	if(fd == -1 || mode != Hread && mode != Hwrite)
		return -1;

	files[maxfiles++] = h;
	h->hh = nil;
	h->fd = fd;
	h->seek = 0;
	h->state = mode;
	h->start = h->buf + 16;		/* leave space for chunk length */
	h->stop = h->pos = h->start;
	if(mode == Hread){
		h->bodylen = ~0UL;
		*h->pos = '\0';
	}else
		h->stop = h->start + Hsize;
	return 0;
}

int
hiserror(Hio *h)
{
	return h->state == Herr;
}

int
hgetc(Hio *h)
{
	uchar *p;

	p = h->pos;
	if(p < h->stop){
		h->pos = p + 1;
		return *p;
	}
	p -= UTFmax;
	if(p < h->start)
		p = h->start;
	if(!hreadbuf(h, p) || h->pos == h->stop)
		return -1;
	return *h->pos++;
}

int
hungetc(Hio *h)
{
	if(h->state == Hend)
		h->state = Hread;
	else if(h->state == Hread)
		h->pos--;
	if(h->pos < h->start || h->state != Hread){
		h->state = Herr;
		h->pos = h->stop;
		return -1;
	}
	return 0;
}

/*
 * fill the buffer, saving contents from vsave onwards.
 * nothing is saved if vsave is nil.
 * returns the beginning of the buffer.
 *
 * understands message body sizes and chunked transfer encoding
 */
void *
hreadbuf(Hio *h, void *vsave)
{
	Hio *hh;
	uchar *save;
	int c, in, cpy, dpos;

	save = vsave;
	if(save && (save < h->start || save > h->stop)
	|| h->state != Hread && h->state != Hend){
		h->state = Herr;
		h->pos = h->stop;
		return nil;
	}

	dpos = 0;
	if(save && h->pos > save)
		dpos = h->pos - save;
	cpy = 0;
	if(save){
		cpy = h->stop - save;
		memmove(h->start, save, cpy);
	}
	h->seek += h->stop - h->start - cpy;
	h->pos = h->start + dpos;

	in = Hsize - cpy;
	if(h->state == Hend)
		in = 0;
	else if(in > h->bodylen)
		in = h->bodylen;

	/*
	 * for chunked encoding, fill buffer,
	 * then read in new chunk length and wipe out that line
	 */
	hh = h->hh;
	if(hh != nil){
		if(!in && h->xferenc && h->state != Hend){
			if(h->xferenc == 2){
				c = hgetc(hh);
				if(c == '\r')
					c = hgetc(hh);
				if(c != '\n'){
					h->pos = h->stop;
					h->state = Herr;
					return nil;
				}
			}
			h->xferenc = 2;
			in = 0;
			while((c = hgetc(hh)) != '\n'){
				if(c >= '0' && c <= '9')
					c -= '0';
				else if(c >= 'a' && c <= 'f')
					c -= 'a' - 10;
				else if(c >= 'A' && c <= 'F')
					c -= 'A' - 10;
				else
					break;
				in = in * 16 + c;
			}
			while(c != '\n'){
				if(c < 0){
					h->pos = h->stop;
					h->state = Herr;
					return nil;
				}
				c = hgetc(hh);
			}
			h->bodylen = in;

			in = Hsize - cpy;
			if(in > h->bodylen)
				in = h->bodylen;
		}
		if(in){
			while(hh->pos + in > hh->stop){
				if(hreadbuf(hh, hh->pos) == nil){
					h->pos = h->stop;
					h->state = Herr;
					return nil;
				}
			}
			memmove(h->start + cpy, hh->pos, in);
			hh->pos += in;
		}
	}else if(in && (in = read(h->fd, h->start + cpy, in)) < 0){
		h->state = Herr;
		h->pos = h->stop;
		return nil;
	}
	if(in == 0)
		h->state = Hend;

	h->bodylen -= in;

	h->stop = h->start + cpy + in;
	*h->stop = '\0';
	if(h->pos == h->stop)
		return nil;
	return h->start;
}

int
hbuflen(Hio *h, void *p)
{
	return h->stop - (uchar*)p;
}

/*
 * prepare to receive a message body
 * len is the content length (~0 => unspecified)
 * te is the transfer encoding
 * returns < 0 if setup failed
 */
Hio*
hbodypush(Hio *hh, ulong len, HFields *te)
{
	Hio *h;
	int xe;

	if(hh->state != Hread)
		return nil;
	xe = 0;
	if(te != nil){
		if(te->params != nil || te->next != nil)
			return nil;
		if(cistrcmp(te->s, "chunked") == 0){
			xe = 1;
			len = 0;
		}else if(cistrcmp(te->s, "identity") == 0)
			;
		else
			return nil;
	}

	h = malloc(sizeof *h);
	if(h == nil)
		return nil;

	h->hh = hh;
	h->fd = -1;
	h->seek = 0;
	h->state = Hread;
	h->xferenc = xe;
	h->start = h->buf + 16;		/* leave space for chunk length */
	h->stop = h->pos = h->start;
	*h->pos = '\0';
	h->bodylen = len;
	return h;
}

/*
 * dump the state of the io buffer into a string
 */
char *
hunload(Hio *h)
{
	uchar *p, *t, *stop, *buf;
	int ne, n, c;

	stop = h->stop;
	ne = 0;
	for(p = h->pos; p < stop; p++){
		c = *p;
		if(c == 0x80)
			ne++;
	}
	p = h->pos;

	n = (stop - p) + ne + 3;
	buf = mallocz(n, 1);
	if(buf == nil)
		return nil;
	buf[0] = hstates[h->state];
	buf[1] = hxfers[h->xferenc];

	t = &buf[2];
	for(; p < stop; p++){
		c = *p;
		if(c == 0 || c == 0x80){
			*t++ = 0x80;
			if(c == 0x80)
				*t++ = 0x80;
		}else
			*t++ = c;
	}
	*t++ = '\0';
	if(t != buf + n)
		return nil;
	return (char*)buf;
}

/*
 * read the io buffer state from a string
 */
int
hload(Hio *h, char *buf)
{
	uchar *p, *t, *stop;
	char *s;
	int c;

	s = strchr(hstates, buf[0]);
	if(s == nil)
		return 0;
	h->state = s - hstates;

	s = strchr(hxfers, buf[1]);
	if(s == nil)
		return 0;
	h->xferenc = s - hxfers;

	t = h->start;
	stop = t + Hsize;
	for(p = (uchar*)&buf[2]; c = *p; p++){
		if(c == 0x80){
			if(p[1] != 0x80)
				c = 0;
			else
				s++;
		}
		*t++ = c;
		if(t >= stop)
			return 0;
	}
	*t = '\0';
	h->pos = h->start;
	h->stop = t;
	h->seek = 0;
	return 1;
}

static void
hexit(void)
{
	int i;

	for(i = 0; i < maxfiles; i++)
		if(files[i] != nil && files[i]->state == Hwrite)
			hxferenc(files[i], 0);
}

void
hclose(Hio *h)
{
	int i;

	if(h->fd >= 0){
		hxferenc(h, 0);
		close(h->fd);
	}
	h->stop = h->pos = nil;
	h->fd = -1;

	for(i = 0; i < maxfiles; i++)
		if(files[i] == h)
			files[i] = nil;
}

/*
 * flush the buffer and possibly change encoding modes
 */
int
hxferenc(Hio *h, int on)
{
	if(h->xferenc && !on && h->pos != h->start)
		hflush(h);
	if(hflush(h) < 0)
		return -1;
	h->xferenc = !!on;
	return 0;
}

int
hputc(Hio *h, int c)
{
	uchar *p;

	p = h->pos;
	if(p < h->stop){
		h->pos = p + 1;
		return *p = c;
	}
	if(hflush(h) < 0)
		return -1;
	return *h->pos++ = c;
}

int
hprint(Hio *h, char *fmt, ...)
{
	uchar *ep, *out;
	int n;
	va_list arg;

	ep = h->stop;
	va_start(arg, fmt);
	out = (uchar*)doprint((char*)h->pos, (char*)ep, fmt, arg);
	if(out >= ep-5) {
		if(hflush(h) < 0)
			return -1;
		out = (uchar*)doprint((char*)h->pos, (char*)ep, fmt, arg);
		if(out >= ep-5)
			return -1;
	}
	va_end(arg);
	n = out - h->pos;
	h->pos = out;
	return n;
}

int
hflush(Hio *h)
{
	uchar *s;
	int w;

	if(h->state != Hwrite){
		h->state = Herr;
		h->stop = h->pos;
		return -1;
	}
	s = h->start;
	w = h->pos - s;
	if(h->xferenc){
		*--s = '\n';
		*--s = '\r';
		do{
			*--s = "0123456789abcdef"[w & 0xf];
			w >>= 4;
		}while(w);
		h->pos[0] = '\r';
		h->pos[1] = '\n';
		w = &h->pos[2] - s;
	}
	if(write(h->fd, s, w) != w){
		h->state = Herr;
		h->stop = h->pos;
		return -1;
	}
	h->seek += w;
	h->pos = h->start;
	return 0;
}

int
hwrite(Hio *h, void *vbuf, int len)
{
	uchar *pos, *buf;
	int n, m;

	buf = vbuf;
	n = len;
	if(n < 0 || h->state != Hwrite){
		h->state = Herr;
		h->stop = h->pos;
		return -1;
	}
	pos = h->pos;
	if(pos + n >= h->stop){
		m = pos - h->start;
		if(m){
			m = Hsize - m;
			if(m){
				memmove(pos, buf, m);
				buf += m;
				n -= m;
			}
			if(write(h->fd, h->start, Hsize) != Hsize){
				h->state = Herr;
				h->stop = h->pos;
				return -1;
			}
			h->seek += Hsize;
		}
		m = n % Hsize;
		n -= m;
		if(write(h->fd, buf, n) != n){
			h->state = Herr;
			h->stop = h->pos;
			return -1;
		}
		h->seek += n;
		buf += n;
		pos = h->pos = h->start;
		n = m;
	}
	memmove(pos, buf, n);
	h->pos = pos + n;
	return len;
}
