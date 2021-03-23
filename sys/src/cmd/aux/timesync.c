#include <u.h>
#include <libc.h>
#include <ip.h>
#include <mp.h>

/* nanosecond times */
#define SEC 1000000000LL
#define MIN (60LL*SEC)
#define HOUR (60LL*MIN)
#define DAY (24LL*HOUR)

enum {
	Fs,
	Rtc,
	Ntp,

	HZAvgSecs=	3*60,	/* target averaging period for the frequency in seconds */
	MinSampleSecs=	60,	/* minimum sampling time in seconds */
};


char *dir = "/tmp";	// directory sample files live in
char *logfile = "timesync";
char *timeserver;
int debug;
int impotent;
int logging;
int type;
int gmtdelta;	// rtc+gmtdelta = gmt

// ntp server info
int stratum = 14;
vlong mydisp, rootdisp;
vlong mydelay, rootdelay;
vlong avgdelay;
uchar rootid[4];
char *sysid;

// list of time samples
typedef struct Sample Sample;
struct Sample
{
	Sample	*next;
	uvlong	ticks;
	vlong	ltime;
	vlong	stime;
};

// ntp packet
typedef struct NTPpkt NTPpkt;
struct NTPpkt
{
	uchar	mode;
	uchar	stratum;
	uchar	poll;
	uchar	precision;
	uchar	rootdelay[4];
	uchar	rootdisp[4];
	uchar	rootid[4];
	uchar	refts[8];
	uchar	origts[8];		// departed client
	uchar	recvts[8];		// arrived at server
	uchar	xmitts[8];		// departed server
	uchar	keyid[4];
	uchar	digest[16];
};

// ntp server
typedef struct NTPserver NTPserver;
struct NTPserver
{
	NTPserver *next;
	char	*name;
	uchar	stratum;
	uchar	precision;
	vlong	rootdelay;
	vlong	rootdisp;
	vlong	disp;
	vlong	rtt;
	vlong	dt;
};

NTPserver *ntpservers;

enum
{
	NTPSIZE= 	48,		// basic ntp packet
	NTPDIGESTSIZE=	20,		// key and digest
};

static void	addntpserver(char *name);
static int	adjustperiod(vlong diff, vlong accuracy, int secs);
static int	caperror(vlong dhz, int tsecs, vlong taccuracy);
static long	fstime(void);
static int	gettime(vlong *nsec, uvlong *ticks, uvlong *hz); // returns time, ticks, hz
static void	hnputts(void *p, vlong nsec);
static void	hnputts(void *p, vlong nsec);
static void	inittime(void);
static vlong	nhgetts(void *p);
static vlong	nhgetts(void *p);
static void	ntpserver(void);
static vlong	ntpsample(void);
static int	ntptimediff(NTPserver *ns);
static int	openfreqfile(void);
static vlong	readfreqfile(int fd, vlong ohz, vlong minhz, vlong maxhz);
static long	rtctime(void);
static vlong	sample(long (*get)(void));
static void	setpriority(void);
static void	setrootid(char *d);
static void	settime(vlong now, uvlong hz, vlong delta, int n); // set time, hz, delta, period
static uvlong	vabs(vlong);
static uvlong	whatisthefrequencykenneth(uvlong hz, uvlong minhz, uvlong maxhz, vlong dt, vlong ticks, vlong period);
static void	writefreqfile(int fd, vlong hz, int secs, vlong diff);

// ((1970-1900)*365 + 17/*leap days*/)*24*60*60
#define EPOCHDIFF 2208988800UL

void
main(int argc, char **argv)
{
	int i;
	int secs;	// sampling period
	int tsecs;	// temporary sampling period
	int t, fd;
	Sample *s, *x, *first, **l;
	vlong diff, accuracy, taccuracy;
	uvlong hz, minhz, maxhz, avgerr, period, nhz;
	int server;
	char *a;
	Tm tl, tg;
	int already = 0;

	type = Fs;		// by default, sync with the file system
	debug = 0;
	accuracy = 1000000LL;	// default accuracy is 1 millisecond
	server = 0;
	tsecs = secs = MinSampleSecs;

	ARGBEGIN{
	case 'a':
		a = ARGF();
		if(a == nil)
			sysfatal("bad accuracy specified");
		accuracy = strtoll(a, 0, 0);	// accuracy specified in ns
		if(accuracy <= 1LL)
			sysfatal("bad accuracy specified");
		break;
	case 'f':
		type = Fs;
		stratum = 2;
		break;
	case 'r':
		type = Rtc;
		stratum = 0;
		break;
	case 'n':
		type = Ntp;
		break;
	case 'D':
		debug = 1;
		break;
	case 'd':
		dir = ARGF();
		break;
	case 'L':
		//
		// Assume time source in local time rather than GMT.
		// Calculate difference so that rtctime can return GMT.
		// This is useful with the rtc on PC's that run Windows
		// since Windows keeps the local time in the rtc.
		//
		t = time(0);
		tl = *localtime(t);
		tg = *gmtime(t);

		// if the years are different, we're at most a day off, so just rewrite
		if(tl.year < tg.year){
			tg.year--;
			tg.yday = tl.yday + 1;
		}else if(tl.year > tg.year){
			tl.year--;
			tl.yday = tg.yday+1;
		}
		assert(tl.year == tg.year);

		tg.sec -= tl.sec;
		tg.min -= tl.min;
		tg.hour -= tl.hour;
		tg.yday -= tl.yday;
		gmtdelta = tg.sec+60*(tg.min+60*(tg.hour+tg.yday*24));

		assert(abs(gmtdelta) <= 24*60*60);
		break;
	case 'i':
		impotent = 1;
		break;
	case 's':
		server = 1;
		break;
	case 'l':
		logging = 1;
		break;
	}ARGEND;

	fmtinstall('E', eipconv);
	fmtinstall('I', eipconv);
	fmtinstall('V', eipconv);
	sysid = getenv("sysname");

	switch(type){
	case Fs:
		if(argc > 0)
			timeserver = argv[0];
		else
			timeserver = "/srv/boot";
		break;
	case Ntp:
		if(argc > 0){
			for(i = 0; i <argc; i++)
				addntpserver(argv[i]);
		} else
			addntpserver("$ntp");
		break;
	}

	setpriority();

	//
	//  detach from the current namespace
	//
	if(debug)
		rfork(RFNAMEG);
	else {
		switch(rfork(RFPROC|RFFDG|RFNAMEG|RFNOTEG|RFNOWAIT)){
		case -1:
			sysfatal("forking: %r");
			break;
		case 0:
			break;
		default:
			exits(0);
		}
	}

	// figure out our time interface and initial frequency
	inittime();
	gettime(0, 0, &hz);
	minhz = hz - (hz>>2);
	maxhz = hz + (hz>>2);

	// convert the accuracy from nanoseconds to ticks
	taccuracy = hz*accuracy/SEC;

	//
	//  bind in clocks
	//
	switch(type){
	case Fs:
		fd = open(timeserver, ORDWR);
		if(fd < 0)
			sysfatal("opening %s: %r\n", timeserver);
		if(mount(fd, "/n/boot", MREPL, "") < 0)
			sysfatal("mounting %s: %r\n", timeserver);
		close(fd);
		break;
	case Rtc:
		bind("#r", "/dev", MAFTER);
		if(access("/dev/rtc", AREAD) < 0)
			sysfatal("accessing /dev/rtc: %r\n");
		break;
	}

	//
	//  start a local ntp server
	//
	if(server){
		switch(rfork(RFPROC|RFFDG|RFMEM|RFNOWAIT)){
		case -1:
			sysfatal("forking: %r");
			break;
		case 0:
			ntpserver();
			_exits(0);
			break;
		default:
			break;
		}
	}

	// get the last known frequency from the file
	fd = openfreqfile();
	hz = readfreqfile(fd, hz, minhz, maxhz);

	// this is the main loop.  it gets a sample, adjusts the
	// clock and computes a sleep period until the next loop.
	// we balance frequency drift against the length of the
	// period to avoid blowing the accuracy limit.
	first = nil;
	l = &first;
	avgerr = accuracy>>1;
	for(;; sleep(tsecs*(1000))){
		s = mallocz(sizeof(*s), 1);
		diff = 0;

		// get times for this sample
		switch(type){
		case Fs:
			s->stime = sample(fstime);
			break;
		case Rtc:
			s->stime = sample(rtctime);
			break;
		case Ntp:
			diff = ntpsample();
			if(diff == 0LL){
				if(logging)
					syslog(0, logfile, "no sample");
				free(s);
				if(secs > 60*15)
					tsecs = 60*15;
				continue;
			}
			break;
		}

		// use fastest method to read local clock and ticks
		gettime(&s->ltime, &s->ticks, 0);
		if(type == Ntp)
			s->stime = s->ltime + diff;

		// if the sample was bad or if this is the first sample, ignore it
		if(s->stime < 0 || !already){
			already = 1;
			free(s);
			continue;
		}

		// reset local time
		diff = s->stime - s->ltime;
		if(diff > 10*SEC || diff < -10*SEC){
			// we're way off, just set the time
			secs = MinSampleSecs;
			settime(s->stime, 0, 0, 0);
		} else {
			// keep a running average of the error.
			avgerr = (avgerr>>1) + (vabs(diff)>>1);

			// the time to next sample depends on how good or
			// bad we're doing.
			tsecs = secs = adjustperiod(diff, accuracy, secs);

			// work off the fixed difference.  This is done
			// by adding a ramp to the clock.  Each 100th of a
			// second (or so) the kernel will add diff/(4*secs*100)
			// to the clock.  we only do 1/4 of the difference per
			// period to dampen any measurement noise.
			settime(-1, 0, diff, 4*secs);

		}
		if(debug)
			fprint(2, "δ %lld avgδ %lld f %lld\n", diff, avgerr, hz);

		// dump old samples (keep at least one)
		while(first != nil){
			if(first->next == nil)
				break;
			if(s->stime - first->next->stime < DAY)
				break;
			x = first;
			first = first->next;
			free(x);
		}

		// The sampling error is limited by the total error.  If
		// we make sure the sampling period is at least 16 million
		// times the average error, we should calculate a frequency
		// with on average a 1e-7 error.
		//
		// So that big hz changes don't blow our accuracy requirement,
		// we shorten the period to make sure that δhz*secs will be
		// greater than the accuracy limit.
		period = avgerr<<24;
		for(x = first; x != nil; x = x->next){
			if(s->stime - x->stime < period)
				break;
			if(x->next == nil || s->stime - x->next->stime < period)
				break;
		}
		if(x != nil){
			nhz = whatisthefrequencykenneth(
				hz, minhz, maxhz,
				s->stime - x->stime,
				s->ticks - x->ticks,
				period);
			tsecs = caperror(vabs(nhz-hz), tsecs, taccuracy);
			hz = nhz;
			writefreqfile(fd, hz, (s->stime - x->stime)/SEC, diff);
		}

		// add current sample to list.
		*l = s;
		l = &s->next;

		if(logging)
			syslog(0, logfile, "δ %lld avgδ %lld hz %lld",
				diff, avgerr, hz);
	}
}

//
// adjust the sampling period with some histeresis
//
static int
adjustperiod(vlong diff, vlong accuracy, int secs)
{
	uvlong absdiff;

	absdiff = vabs(diff);

	if(absdiff < (accuracy>>1))
		secs += 60;
	else if(absdiff > accuracy)
		secs >>= 1;
	else
		secs -= 60;
	if(secs < MinSampleSecs)
		secs = MinSampleSecs;
	return secs;
}

//
// adjust the frequency
//
static uvlong
whatisthefrequencykenneth(uvlong hz, uvlong minhz, uvlong maxhz, vlong dt, vlong ticks, vlong period)
{
	static mpint *mpdt;
	static mpint *mpticks;
	static mpint *mphz;
	static mpint *mpbillion;
	uvlong ohz = hz;

	// sanity check
	if(dt <= 0 || ticks <= 0)
		return hz;

	if(mphz == nil){
		mphz = mpnew(0);
		mpbillion = uvtomp(SEC, nil);
	}

	// hz = (ticks*SEC)/dt
	mpdt = vtomp(dt, mpdt);
	mpticks = vtomp(ticks, mpticks);
	mpmul(mpticks, mpbillion, mpticks);
	mpdiv(mpticks, mpdt, mphz, nil);
	hz = mptoui(mphz);

	// sanity
	if(hz < minhz || hz > maxhz)
		return ohz;

	// damp the change if we're shorter than the target period
	if(period > dt)
		hz = (12ULL*ohz + 4ULL*hz)/16ULL;

	settime(-1, hz, 0, 0);
	return hz;
}

// We may be changing the frequency to match a bad measurement
// or to match a condition no longer in effet.  To make sure
// that this doesn't blow our error budget over the next measurement
// period, shorten the period to make sure that δhz*secs will be
// less than the accuracy limit.  Here taccuracy is accuracy converted
// from nanoseconds to ticks.
static int
caperror(vlong dhz, int tsecs, vlong taccuracy)
{
	if(dhz*tsecs < taccuracy)
		return tsecs;

	if(debug)
		fprint(2, "δhz %lld tsecs %d tacc %lld\n", dhz, tsecs, taccuracy);

	tsecs = taccuracy/dhz;
	if(tsecs < MinSampleSecs)
		tsecs = MinSampleSecs;
	return tsecs;
}

//
//  kernel interface
//
enum
{
	Ibintime,
	Insec,
	Itiming,
};
int ifc;
int bintimefd = -1;
int timingfd = -1;
int nsecfd = -1;
int fastclockfd = -1;

static void
inittime(void)
{
	int mode;

	if(impotent)
		mode = OREAD;
	else
		mode = ORDWR;

	// bind in clocks
	if(access("/dev/time", 0) < 0)
		bind("#c", "/dev", MAFTER);
	if(access("/dev/rtc", 0) < 0)
		bind("#r", "/dev", MAFTER);

	// figure out what interface we have
	ifc = Ibintime;
	bintimefd = open("/dev/bintime", mode);
	if(bintimefd >= 0)
		return;
	ifc = Insec;
	nsecfd = open("/dev/nsec", mode);
	if(nsecfd < 0)
		sysfatal("opening /dev/nsec");
	fastclockfd = open("/dev/fastclock", mode);
	if(fastclockfd < 0)
		sysfatal("opening /dev/fastclock");
	timingfd = open("/dev/timing", OREAD);
	if(timingfd < 0)
		return;
	ifc = Itiming;
}

//
//  convert binary numbers from/to kernel
//
static uvlong uvorder = 0x0001020304050607ULL;

static uchar*
be2vlong(vlong *to, uchar *f)
{
	uchar *t, *o;
	int i;

	t = (uchar*)to;
	o = (uchar*)&uvorder;
	for(i = 0; i < sizeof(vlong); i++)
		t[o[i]] = f[i];
	return f+sizeof(vlong);
}

static uchar*
vlong2be(uchar *t, vlong from)
{
	uchar *f, *o;
	int i;

	f = (uchar*)&from;
	o = (uchar*)&uvorder;
	for(i = 0; i < sizeof(vlong); i++)
		t[i] = f[o[i]];
	return t+sizeof(vlong);
}

static long order = 0x00010203;

static uchar*
be2long(long *to, uchar *f)
{
	uchar *t, *o;
	int i;

	t = (uchar*)to;
	o = (uchar*)&order;
	for(i = 0; i < sizeof(long); i++)
		t[o[i]] = f[i];
	return f+sizeof(long);
}

static uchar*
long2be(uchar *t, long from)
{
	uchar *f, *o;
	int i;

	f = (uchar*)&from;
	o = (uchar*)&order;
	for(i = 0; i < sizeof(long); i++)
		t[i] = f[o[i]];
	return t+sizeof(long);
}

//
// read ticks and local time in nanoseconds
//
static int
gettime(vlong *nsec, uvlong *ticks, uvlong *hz)
{
	int i, n;
	uchar ub[3*8], *p;
	char b[2*24+1];

	switch(ifc){
	case Ibintime:
		n = sizeof(vlong);
		if(hz != nil)
			n = 3*sizeof(vlong);
		if(ticks != nil)
			n = 2*sizeof(vlong);
		i = read(bintimefd, ub, n);
		if(i != n)
			break;
		p = ub;
		if(nsec != nil)
			be2vlong(nsec, ub);
		p += sizeof(vlong);
		if(ticks != nil)
			be2vlong((vlong*)ticks, p);
		p += sizeof(vlong);
		if(hz != nil)
			be2vlong((vlong*)hz, p);
		return 0;
	case Itiming:
		n = sizeof(vlong);
		if(ticks != nil)
			n = 2*sizeof(vlong);
		i = read(timingfd, ub, n);
		if(i != n)
			break;
		p = ub;
		if(nsec != nil)
			be2vlong(nsec, ub);
		p += sizeof(vlong);
		if(ticks != nil)
			be2vlong((vlong*)ticks, p);
		if(hz != nil){
			seek(fastclockfd, 0, 0);
			n = read(fastclockfd, b, sizeof(b)-1);
			if(n <= 0)
				break;
			b[n] = 0;
			*hz = strtoll(b+24, 0, 0);
		}
		return 0;
	case Insec:
		if(nsec != nil){
			seek(nsecfd, 0, 0);
			n = read(nsecfd, b, sizeof(b)-1);
			if(n <= 0)
				break;
			b[n] = 0;
			*nsec = strtoll(b, 0, 0);
		}
		if(ticks != nil){
			seek(fastclockfd, 0, 0);
			n = read(fastclockfd, b, sizeof(b)-1);
			if(n <= 0)
				break;
			b[n] = 0;
			*ticks = strtoll(b, 0, 0);
		}
		if(hz != nil){
			seek(fastclockfd, 0, 0);
			n = read(fastclockfd, b, sizeof(b)-1);
			if(n <= 24)
				break;
			b[n] = 0;
			*hz = strtoll(b+24, 0, 0);
		}
		return 0;
	}
	return -1;
}

static void
settime(vlong now, uvlong hz, vlong delta, int n)
{
	uchar b[1+sizeof(vlong)+sizeof(long)], *p;

	if(debug)
		fprint(2, "settime(now=%lld, hz=%llud, delta=%lld, period=%d)\n", now, hz, delta, n);
	if(impotent)
		return;
	switch(ifc){
	case Ibintime:
		if(now >= 0){
			p = b;
			*p++ = 'n';
			p = vlong2be(p, now);
			if(write(bintimefd, b, p-b) < 0)
				sysfatal("writing /dev/bintime: %r");
		}
		if(delta != 0){
			p = b;
			*p++ = 'd';
			p = vlong2be(p, delta);
			p = long2be(p, n);
			if(write(bintimefd, b, p-b) < 0)
				sysfatal("writing /dev/bintime: %r");
		}
		if(hz != 0){
			p = b;
			*p++ = 'f';
			p = vlong2be(p, hz);
			if(write(bintimefd, b, p-b) < 0)
				sysfatal("writing /dev/bintime: %r");
		}
		break;
	case Itiming:
	case Insec:
		seek(nsecfd, 0, 0);
		if(now >= 0 || delta != 0){
			if(fprint(nsecfd, "%lld %lld %d", now, delta, n) < 0)
				sysfatal("writing /dev/nsec: %r");
		}
		if(hz > 0){
			seek(fastclockfd, 0, 0);
			if(fprint(fastclockfd, "%lld", hz) < 0)
				sysfatal("writing /dev/fastclock: %r");
		}
	}
}

//
//  set priority high and wire process to a processor
//
static void
setpriority(void)
{
	int fd;
	char buf[32];

	sprint(buf, "/proc/%d/ctl", getpid());
	fd = open(buf, ORDWR);
	if(fd < 0){
		fprint(2, "can't set priority\n");
		return;
	}
	if(fprint(fd, "pri 100") < 0)
		fprint(2, "can't set priority\n");
	if(fprint(fd, "wired 2") < 0)
		fprint(2, "can't wire process\n");
	close(fd);
}

// convert to ntp timestamps
static void
hnputts(void *p, vlong nsec)
{
	uchar *a;
	ulong tsh;
	ulong tsl;

	a = p;

	// zero is a special case
	if(nsec == 0)
		return;

	tsh = (nsec/SEC);
	nsec -= tsh*SEC;
	tsl = (nsec<<32)/SEC;
	hnputl(a, tsh+EPOCHDIFF);
	hnputl(a+4, tsl);
}

// convert from ntp timestamps
static vlong
nhgetts(void *p)
{
	uchar *a;
	ulong tsh, tsl;
	vlong nsec;

	a = p;
	tsh = nhgetl(a);
	tsl = nhgetl(a+4);
	nsec = tsl*SEC;
	nsec >>= 32;
	nsec += (tsh - EPOCHDIFF)*SEC;
	return nsec;
}

// convert to ntp 32 bit fixed point
static void
hnputfp(void *p, vlong nsec)
{
	uchar *a;
	ulong fp;

	a = p;

	fp = nsec/(SEC/((vlong)(1<<16)));
	hnputl(a, fp);
}

// convert from ntp fixed point to nanosecs
static vlong
nhgetfp(void *p)
{
	uchar *a;
	ulong fp;
	vlong nsec;

	a = p;
	fp = nhgetl(a);
	nsec = ((vlong)fp)*(SEC/((vlong)(1<<16)));
	return nsec;
}

// get network address of the server
static void
setrootid(char *d)
{
	char buf[128];
	int fd, n;
	char *p;

	snprint(buf, sizeof(buf), "%s/remote", d);
	fd = open(buf, OREAD);
	if(fd < 0)
		return;
	n = read(fd, buf, sizeof buf);
	close(fd);
	if(n <= 0)
		return;
	p = strchr(buf, '!');
	if(p != nil)
		*p = 0;
	v4parseip(rootid, buf);
}

static void
ding(void*, char *s)
{
	if(strstr(s, "alarm") != nil)
		noted(NCONT);
	noted(NDFLT);
}

static void
addntpserver(char *name)
{
	NTPserver *ns, **l;

	ns = mallocz(sizeof(NTPserver), 1);
	if(ns == nil)
		sysfatal("addntpserver: %r");
	ns->name = name;
	for(l = &ntpservers; *l != nil; l = &(*l)->next)
		;
	*l = ns;
}

//
//  sntp client, we keep calling if the delay seems
//  unusually high, i.e., 30% longer than avg.
//
static int
ntptimediff(NTPserver *ns)
{
	int fd, tries, n;
	NTPpkt ntpin, ntpout;
	vlong dt, recvts, origts, xmitts, destts, x;
	char dir[64];

	fd = dial(netmkaddr(ns->name, "udp", "ntp"), 0, 0, 0);
	if(fd < 0){
		syslog(0, logfile, "can't reach %s: %r", ns->name);
		return -1;
	}
	setrootid(dir);
	notify(ding);

	memset(&ntpout, 0, sizeof(ntpout));
	ntpout.mode = 3 | (3 << 3);

	for(tries = 0; tries < 3; tries++){
		alarm(2*1000);

		gettime(&x, 0, 0);
		hnputts(ntpout.xmitts, x);
		if(write(fd, &ntpout, NTPSIZE) < 0){
			alarm(0);
			continue;
		}

		n = read(fd, &ntpin, sizeof(ntpin));
		alarm(0);
		gettime(&destts, 0, 0);
		if(n >= NTPSIZE){
			close(fd);

			// we got one, use it
			recvts = nhgetts(ntpin.recvts);
			origts = nhgetts(ntpin.origts);
			xmitts = nhgetts(ntpin.xmitts);
			dt = ((recvts - origts) + (xmitts - destts))/2;

			// save results
			ns->rtt = ((destts - origts) - (xmitts - recvts))/2;
			ns->dt = dt;
			ns->stratum = ntpin.stratum;
			ns->precision = ntpin.precision;
			ns->rootdelay = nhgetfp(ntpin.rootdelay);
			ns->rootdisp = nhgetfp(ntpin.rootdisp);

			if(debug)
				fprint(2, "ntp %s stratum %d ntpdelay(%lld)\n", 
					ns->name, ntpin.stratum, ns->rtt);
			return 0;
		}

		// try again
		sleep(250);
	}
	close(fd);
	return -1;
}

static vlong
ntpsample(void)
{
	NTPserver *tns, *ns;
	vlong metric, x;

	metric = 1000LL*SEC;
	ns = nil;
	for(tns = ntpservers; tns != nil; tns = tns->next){
		if(ntptimediff(tns) < 0)
			continue;
		if(tns->stratum == 0)
			x = 0xff;
		else
			x = tns->stratum;
		x *= SEC;
		x += (vabs(tns->rootdelay+tns->rtt)>>1) + tns->rootdisp + tns->rtt;
if(debug) fprint(2, "ntp %s rootdelay %lld rootdisp %lld metric %lld\n", tns->name, tns->rootdelay, tns->rootdisp, x);
		if(x < metric){
			metric = x;
			ns = tns;
		}
	}

	if(ns == nil)
		return 0LL;

	// save data for our server
	rootdisp = ns->rootdisp;
	rootdelay = ns->rootdelay;
	mydelay = ns->rtt;
	x = vabs(ns->dt);
	if(ns->disp == 0LL)
		ns->disp = x;
	else
		ns->disp = (x+3LL*ns->disp)>>2;
	mydisp = ns->disp;
	if(ns->stratum == 0)
		stratum = 0;
	else
		stratum = ns->stratum + 1;

	return ns->dt;
}

//
//  sntp server
//
static int
openlisten(char *net)
{
	int fd, cfd;
	char data[128];
	char devdir[40];

	sprint(data, "%s/udp!*!ntp", net);
	cfd = announce(data, devdir);
	if(cfd < 0)
		sysfatal("can't announce");
	if(fprint(cfd, "headers") < 0)
		sysfatal("can't set header mode");

	sprint(data, "%s/data", devdir);

	fd = open(data, ORDWR);
	if(fd < 0)
		sysfatal("open udp data");
	return fd;
}
static void
ntpserver(void)
{
	int fd, n;
	NTPpkt *ntp;
	char buf[512];
	int vers, mode;
	vlong recvts, x;

	fd = openlisten("/net");

	switch(type){
	case Fs:
		memmove(rootid, "WWV", 3);
		break;
	case Rtc:
		memmove(rootid, "LOCL", 3);
		break;
	case Ntp:
		/* set by the ntp client */
		break;
	}

	for(;;){
		n = read(fd, buf, sizeof(buf));
		gettime(&recvts, 0, 0);
		if(n < 0)
			return;
		if(n < Udphdrsize + NTPSIZE)
			continue;

		ntp = (NTPpkt*)(buf+Udphdrsize);
		mode = ntp->mode & 7;
		vers = (ntp->mode>>3) & 7;
		if(mode != 3)
			continue;

		ntp->mode = (vers<<3)|4;
		ntp->stratum = stratum;
		hnputfp(ntp->rootdelay, rootdelay + mydelay);
		hnputfp(ntp->rootdisp, rootdisp + mydisp);
		memmove(ntp->origts, ntp->xmitts, sizeof(ntp->origts));
		hnputts(ntp->recvts, recvts);
		memmove(ntp->rootid, rootid, sizeof(ntp->rootid));
		gettime(&x, 0, 0);
		hnputts(ntp->xmitts, x);
		write(fd, buf, NTPSIZE+Udphdrsize);
	}
}

//
//  get the current time from the file system
//
static long
fstime(void)
{
	Dir d;

	if(dirstat("/n/boot", &d) < 0)
		sysfatal("stating /n/boot: %r");
	return d.atime;
}

//
//  get the current time from the real time clock
//
static long
rtctime(void)
{
	char b[20];
	static int f = -1;
	int i, retries;

	memset(b, 0, sizeof(b));
	for(retries = 0; retries < 100; retries++){
		if(f < 0)
			f = open("/dev/rtc", OREAD|OCEXEC);
		if(f < 0)
			break;
		if(seek(f, 0, 0) < 0 || (i = read(f, b, sizeof(b))) < 0){
			close(f);
			f = -1;
		} else {
			if(i != 0)
				break;
		}
	}
	return strtoul(b, 0, 10)+gmtdelta;
}


//
//  Sample a clock.  We wait for the clock to always
//  be at the leading edge of a clock period.
//
static vlong
sample(long (*get)(void))
{
	long this, last;
	vlong start, end;

	/*
	 *  wait for the second to change
	 */
	last = (*get)();
	for(;;){
		gettime(&start, 0, 0);
		this = (*get)();
		gettime(&end, 0, 0);
		if(this != last)
			break;
		last = this;
	}
	return SEC*this - (end-start)/2;
}

//
// the name of the frequency file has the method and possibly the
// server name encoded in it.
//
static int
openfreqfile(void)
{
	char buf[64];
	int fd;

	if(sysid == nil)
		return -1;

	switch(type){
	case Ntp:
		snprint(buf, sizeof buf, "%s/ts.%s.%d.%s", dir, sysid, type, timeserver);
		break;
	default:
		snprint(buf, sizeof buf, "%s/ts.%s.%d", dir, sysid, type);
		break;
	}
	fd = open(buf, ORDWR);
	if(fd < 0)
		fd = create(buf, ORDWR, 0666);
	if(fd < 0)
		return -1;
	return fd;
}

//
//  the file contains the last known frequency and the
//  number of seconds it was sampled over
//
static vlong
readfreqfile(int fd, vlong ohz, vlong minhz, vlong maxhz)
{
	int n;
	char buf[128];
	vlong hz;

	n = read(fd, buf, sizeof(buf)-1);
	if(n <= 0)
		return ohz;
	buf[n] = 0;
	hz = strtoll(buf, nil, 0);

	if(hz > maxhz || hz < minhz)
		return ohz;

	settime(-1, hz, 0, 0);
	return hz;
}

//
//  remember hz and averaging period
//
static void
writefreqfile(int fd, vlong hz, int secs, vlong diff)
{
	long now;
	static long last;

	if(fd < 0)
		return;
	now = time(0);
	if(now - last < 60*60)
		return;
	last = now;
	if(seek(fd, 0, 0) < 0)
		return;
	fprint(fd, "%lld %d %d %lld\n", hz, secs, type, diff);
}

static uvlong
vabs(vlong x)
{
	if(x < 0LL)
		return (uvlong)-x;
	else
		return (uvlong)x;
}