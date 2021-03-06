#include <u.h>
#include <libc.h>
#include <ip.h>

enum
{
	Isprefix= 16,
};

uchar prefixvals[256] =
{
[0x00] 0 | Isprefix,
[0x80] 1 | Isprefix,
[0xC0] 2 | Isprefix,
[0xE0] 3 | Isprefix,
[0xF0] 4 | Isprefix,
[0xF8] 5 | Isprefix,
[0xFC] 6 | Isprefix,
[0xFE] 7 | Isprefix,
[0xFF] 8 | Isprefix,
};

int
eipconv(va_list *arg, Fconv *f)
{
	char buf[8*5];
	static char *efmt = "%.2lux%.2lux%.2lux%.2lux%.2lux%.2lux";
	static char *ifmt = "%d.%d.%d.%d";
	uchar *p, ip[16];
	ulong *lp;
	ushort s;
	int i, j, n, eln, eli;

	switch(f->chr) {
	case 'E':		/* Ethernet address */
		p = va_arg(*arg, uchar*);
		sprint(buf, efmt, p[0], p[1], p[2], p[3], p[4], p[5]);
		break;
	case 'I':		/* Ip address */
		p = va_arg(*arg, uchar*);
common:
		if(memcmp(p, v4prefix, 12) == 0)
			sprint(buf, ifmt, p[12], p[13], p[14], p[15]);
		else {
			/* find longest elision */
			eln = eli = -1;
			for(i = 0; i < 16; i += 2){
				for(j = i; j < 16; j += 2)
					if(p[j] != 0 || p[j+1] != 0)
						break;
				if(j > i && j - i > eln){
					eli = i;
					eln = j - i;
				}
			}

			/* print with possible elision */
			n = 0;
			for(i = 0; i < 16; i += 2){
				if(i == eli){
					n += sprint(buf+n, "::");
					i += eln;
					if(i >= 16)
						break;
				} else if(i != 0)
					n += sprint(buf+n, ":");
				s = (p[i]<<8) + p[i+1];
				n += sprint(buf+n, "%ux", s);
			}
		}
		break;
	case 'i':		/* v6 address as 4 longs */
		lp = va_arg(*arg, ulong*);
		for(i = 0; i < 4; i++)
			hnputl(ip+4*i, *lp++);
		p = ip;
		goto common;
	case 'V':		/* v4 ip address */
		p = va_arg(*arg, uchar*);
		sprint(buf, ifmt, p[0], p[1], p[2], p[3]);
		break;
	case 'M':		/* ip mask */
		p = va_arg(*arg, uchar*);

		/* look for a prefix mask */
		for(i = 0; i < 16; i++)
			if(p[i] != 0xff)
				break;
		if(i < 16){
			if((prefixvals[p[i]] & Isprefix) == 0)
				goto common;
			for(j = i+1; j < 16; j++)
				if(p[j] != 0)
					goto common;
			n = 8*i + (prefixvals[p[i]] & ~Isprefix);
		} else
			n = 8*16;

		/* got one, use /xx format */
		sprint(buf, "/%d", n);
		break;
	default:
		strcpy(buf, "(eipconv)");
	}
	strconv(buf, f);
	return sizeof(uchar*);
}
