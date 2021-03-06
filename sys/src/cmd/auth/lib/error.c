#include <u.h>
#include <libc.h>
#include <auth.h>
#include "authsrv.h"

void
error(char *fmt, ...)
{
	char buf[8192], *s;
	va_list arg;

	s = buf;
	s += sprint(s, "%s: ", argv0);
	va_start(arg, fmt);
	s = doprint(s, buf + sizeof(buf), fmt, arg);
	va_end(arg);
	*s++ = '\n';
	write(2, buf, s - buf);
	exits(buf);
}
