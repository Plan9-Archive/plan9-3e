enum
{
	Qdir,
	Qacme,
	Qcons,
	Qconsctl,
	Qdraw,
	Qeditout,
	Qindex,
	Qlabel,
	Qnew,

	QWaddr,
	QWbody,
	QWctl,
	QWdata,
	QWeditout,
	QWevent,
	QWrdsel,
	QWwrsel,
	QWtag,
	QMAX,
};

enum
{
	Blockincr =	256,
	Maxblock = 	8*1024,
	NRange =		10,
	Infinity = 		0x7FFFFFFF,	/* huge value for regexp address */
};

typedef	struct	Block Block;
typedef	struct	Buffer Buffer;
typedef	struct	Command Command;
typedef	struct	Column Column;
typedef	struct	Dirlist Dirlist;
typedef	struct	Dirtab Dirtab;
typedef	struct	Disk Disk;
typedef	struct	Expand Expand;
typedef	struct	Fid Fid;
typedef	struct	File File;
typedef	struct	Elog Elog;
typedef	struct	Mntdir Mntdir;
typedef	struct	Range Range;
typedef	struct	Rangeset Rangeset;
typedef	struct	Reffont Reffont;
typedef	struct	Row Row;
typedef	struct	Runestr Runestr;
typedef	struct	Text Text;
typedef	struct	Timer Timer;
typedef	struct	Window Window;
typedef	struct	Xfid Xfid;

struct Runestr
{
	Rune	*r;
	int	nr;
};

struct Range
{
	int	q0;
	int	q1;
};

struct Block
{
	uint		addr;	/* disk address in bytes */
	union
	{
		uint	n;		/* number of used runes in block */
		Block	*next;	/* pointer to next in free list */
	};
};

struct Disk
{
	int		fd;
	uint		addr;	/* length of temp file */
	Block	*free[Maxblock/Blockincr+1];
};

Disk*	diskinit(void);
Block*	disknewblock(Disk*, uint);
void		diskrelease(Disk*, Block*);
void		diskread(Disk*, Block*, Rune*, uint);
void		diskwrite(Disk*, Block**, Rune*, uint);

struct Buffer
{
	uint	nc;
	Rune	*c;			/* cache */
	uint	cnc;			/* bytes in cache */
	uint	cmax;		/* size of allocated cache */
	uint	cq;			/* position of cache */
	int		cdirty;	/* cache needs to be written */
	uint	cbi;			/* index of cache Block */
	Block	**bl;		/* array of blocks */
	uint	nbl;			/* number of blocks */
};
void		bufinsert(Buffer*, uint, Rune*, uint);
void		bufdelete(Buffer*, uint, uint);
uint		bufload(Buffer*, uint, int, int*);
void		bufread(Buffer*, uint, Rune*, uint);
void		bufclose(Buffer*);
void		bufreset(Buffer*);

struct Elog
{
	short	type;		/* Delete, Insert, Filename */
	uint		q0;		/* location of change (unused in f) */
	uint		nd;		/* number of deleted characters */
	uint		nr;		/* # runes in string or file name */
	Rune		*r;
};
void	elogterm(File*);
void	elogclose(File*);
void	eloginsert(File*, int, Rune*, int);
void	elogdelete(File*, int, int);
void	elogreplace(File*, int, int, Rune*, int);
void	elogapply(File*);

struct File
{
	Buffer;			/* the data */
	Buffer	delta;	/* transcript of changes */
	Buffer	epsilon;	/* inversion of delta for redo */
	Buffer	*elogbuf;	/* log of pending editor changes */
	Elog		elog;		/* current pending change */
	Rune		*name;	/* name of associated file */
	int		nname;	/* size of name */
	uint		qidpath;	/* of file when read */
	uint		mtime;	/* of file when read */
	int		dev;		/* of file when read */
	int		unread;	/* file has not been read from disk */
	int		editclean;	/* mark clean after edit command */

	int		seq;		/* if seq==0, File acts like Buffer */
	int		mod;
	Text		*curtext;	/* most recently used associated text */
	Text		**text;	/* list of associated texts */
	int		ntext;
	int		dumpid;	/* used in dumping zeroxed windows */
};
File*		fileaddtext(File*, Text*);
void		fileclose(File*);
void		filedelete(File*, uint, uint);
void		filedeltext(File*, Text*);
void		fileinsert(File*, uint, Rune*, uint);
uint		fileload(File*, uint, int, int*);
void		filemark(File*);
void		filereset(File*);
void		filesetname(File*, Rune*, int);
void		fileundelete(File*, Buffer*, uint, uint);
void		fileuninsert(File*, Buffer*, uint, uint);
void		fileunsetname(File*, Buffer*);
void		fileundo(File*, int, uint*, uint*);
uint		fileredoseq(File*);

enum	/* Text.what */
{
	Columntag,
	Rowtag,
	Tag,
	Body,
};

struct Text
{
	File		*file;
	Frame;
	Reffont	*reffont;
	uint	org;
	uint	q0;
	uint	q1;
	int	what;
	int	tabstop;
	Window	*w;
	Rectangle scrollr;
	Rectangle lastsr;
	Rectangle all;
	Row		*row;
	Column	*col;

	uint	eq0;	/* start of typing for ESC */
	uint	cq0;	/* cache position */
	int		ncache;	/* storage for insert */
	int		ncachealloc;
	Rune	*cache;
	int	nofill;
};

uint		textbacknl(Text*, uint, uint);
uint		textbsinsert(Text*, uint, Rune*, uint, int, int*);
int		textbswidth(Text*, Rune);
int		textclickmatch(Text*, int, int, int, uint*);
void		textclose(Text*);
void		textcolumnate(Text*, Dirlist**, int);
void		textcommit(Text*, int);
void		textdelete(Text*, uint, uint, int);
void		textdoubleclick(Text*, uint*, uint*);
void		textfill(Text*);
void		textframescroll(Text*, int);
void		textinit(Text*, File*, Rectangle, Reffont*, Image**);
void		textinsert(Text*, uint, Rune*, uint, int);
uint		textload(Text*, uint, char*, int);
Rune		textreadc(Text*, uint);
void		textredraw(Text*, Rectangle, Font*, Image*, int);
void		textreset(Text*);
int		textresize(Text*, Rectangle);
void		textscrdraw(Text*);
void		textscroll(Text*, int);
void		textselect(Text*);
int		textselect2(Text*, uint*, uint*, Text**);
int		textselect23(Text*, uint*, uint*, Image*, int);
int		textselect3(Text*, uint*, uint*);
void		textsetorigin(Text*, uint, int);
void		textsetselect(Text*, uint, uint);
void		textshow(Text*, uint, uint);
void		texttype(Text*, Rune);

struct Window
{
		QLock;
		Ref;
	Text		tag;
	Text		body;
	Rectangle	r;
	uchar	isdir;
	uchar	isscratch;
	uchar	filemenu;
	uchar	dirty;
	int		id;
	Range	addr;
	Range	limit;
	uchar	nopen[QMAX];
	uchar	nomark;
	uchar	noscroll;
	Range	wrselrange;
	int		rdselfd;
	Column	*col;
	Xfid		*eventx;
	char		*events;
	int		nevents;
	int		owner;
	int		maxlines;
	Dirlist	**dlp;
	int		ndl;
	int		putseq;
	int		nincl;
	Rune		**incl;
	Reffont	*reffont;
	QLock	ctllock;
	uint		ctlfid;
	char		*dumpstr;
	char		*dumpdir;
	int		dumpid;
	int		utflastqid;
	int		utflastboff;
	int		utflastq;
};

void	wininit(Window*, Window*, Rectangle);
void	winlock(Window*, int);
void	winlock1(Window*, int);
void	winunlock(Window*);
void	wintype(Window*, Text*, Rune);
void	winundo(Window*, int);
void	winsetname(Window*, Rune*, int);
void	winsettag(Window*);
void	winsettag1(Window*);
void	wincommit(Window*, Text*);
int	winresize(Window*, Rectangle, int);
void	winclose(Window*);
void	windelete(Window*);
int	winclean(Window*, int);
void	windirfree(Window*);
void	winevent(Window*, char*, ...);
void	winmousebut(Window*);
void	winaddincl(Window*, Rune*, int);
void	wincleartag(Window*);
void	winctlprint(Window*, char*);

struct Column
{
	Rectangle r;
	Text	tag;
	Row		*row;
	Window	**w;
	int		nw;
	int		safe;
};

void		colinit(Column*, Rectangle);
Window*	coladd(Column*, Window*, Window*, int);
void		colclose(Column*, Window*, int);
void		colcloseall(Column*);
void		colresize(Column*, Rectangle);
Text*	colwhich(Column*, Point);
void		coldragwin(Column*, Window*, int);
void		colgrow(Column*, Window*, int);
int		colclean(Column*);
void		colsort(Column*);
void		colmousebut(Column*);

struct Row
{
	QLock;
	Rectangle r;
	Text	tag;
	Column	**col;
	int		ncol;

};

void		rowinit(Row*, Rectangle);
Column*	rowadd(Row*, Column *c, int);
void		rowclose(Row*, Column*, int);
Text*	rowwhich(Row*, Point);
Column*	rowwhichcol(Row*, Point);
void		rowresize(Row*, Rectangle);
Text*	rowtype(Row*, Rune, Point);
void		rowdragcol(Row*, Column*, int but);
int		rowclean(Row*);
void		rowdump(Row*, char*);
void		rowload(Row*, char*, int);

struct Timer
{
	int		dt;
	int		cancel;
	Channel	*c;	/* chan(int) */
	Timer	*next;
};

struct Command
{
	int		pid;
	Rune		*name;
	int		nname;
	char		*text;
	char		**av;
	int		iseditcmd;
	Mntdir	*md;
	Command	*next;
};

struct Dirtab
{
	char	*name;
	uint	qid;
	uint	perm;
};

struct Mntdir
{
	int		id;
	int		ref;
	Rune		*dir;
	int		ndir;
	Mntdir	*next;
	int		nincl;
	Rune		**incl;
};

struct Fid
{
	int		fid;
	int		busy;
	int		open;
	Qid		qid;
	Window	*w;
	Dirtab	*dir;
	Fid		*next;
	Mntdir	*mntdir;
	int		nrpart;
	uchar	rpart[UTFmax];
};


struct Xfid
{
	void		*arg;	/* args to xfidinit */
	Fcall;
	Xfid	*next;
	Channel	*c;		/* chan(void(*)(Xfid*)) */
	Fid	*f;
	char	*buf;
	int	flushed;

};

void		xfidctl(void *);
void		xfidflush(Xfid*);
void		xfidwalk(Xfid*);
void		xfidopen(Xfid*);
void		xfidclose(Xfid*);
void		xfidread(Xfid*);
void		xfidwrite(Xfid*);
void		xfidctlwrite(Xfid*, Window*);
void		xfideventread(Xfid*, Window*);
void		xfideventwrite(Xfid*, Window*);
void		xfidindexread(Xfid*);
void		xfidutfread(Xfid*, Text*, uint, int);
int		xfidruneread(Xfid*, Text*, uint, uint);

struct Reffont
{
	Ref;
	Font		*f;

};
Reffont	*rfget(int, int, int, char*);
void		rfclose(Reffont*);

struct Rangeset
{
	Range	r[NRange];
};

struct Dirlist
{
	Rune	*r;
	int		nr;
	int		wid;
};

struct Expand
{
	uint	q0;
	uint	q1;
	Rune	*name;
	int	nname;
	char	*bname;
	int	jump;
	union{
		Text	*at;
		Rune	*ar;
	};
	int	(*agetc)(void*, uint);
	int	a0;
	int	a1;
};

enum
{
	/* fbufalloc() guarantees room off end of BUFSIZE */
	BUFSIZE = MAXRPC,	/* size from fbufalloc() */
	RBUFSIZE = BUFSIZE/sizeof(Rune),
	EVENTSIZE = 256,
	Scrollwid = 12,	/* width of scroll bar */
	Scrollgap = 4,	/* gap right of scroll bar */
	Margin = 4,	/* margin around text */
	Border = 2,	/* line between rows, cols, windows */
};

#define	QID(w,q)	((w<<8)|(q))
#define	WIN(q)	((((q).path&~CHDIR)>>8) & 0xFFFFFF)
#define	FILE(q)	((q).path & 0xFF)

enum
{
	FALSE,
	TRUE,
	XXX,
};

enum
{
	Empty	= 0,
	Null		= '-',
	Delete	= 'd',
	Insert	= 'i',
	Replace	= 'r',
	Filename	= 'f',
};

enum	/* editing */
{
	Inactive	= 0,
	Inserting,
	Collecting,
};

uint		seq;
uint		maxtab;	/* size of a tab, in units of the '0' character */

Display		*display;
Image		*screen;
Font			*font;
Mouse		*mouse;
Mousectl		*mousectl;
Keyboardctl	*keyboardctl;
Reffont		reffont;
Image		*modbutton;
Image		*colbutton;
Image		*button;
Image		*but2col;
Image		*but3col;
Cursor		boxcursor;
Row			row;
int			timerpid;
Disk			*disk;
Text			*seltext;
Text			*argtext;
Text			*mousetext;	/* global because Text.close needs to clear it */
Text			*typetext;		/* global because Text.close needs to clear it */
Text			*barttext;		/* shared between mousetask and keyboardthread */
int			bartflag;
Window		*activewin;
Column		*activecol;
Buffer		snarfbuf;
Rectangle		nullrect;
int			fsyspid;
char			*cputype;
char			*objtype;
char			*home;
char			*fontnames[2];
char			acmeerrorfile[128];
Image		*tagcols[NCOL];
Image		*textcols[NCOL];
int			plumbsendfd;
int			plumbeditfd;
char			wdir[];
int			editing;

Channel	*ckeyboard;	/* chan(Rune)[10] */
Channel	*cplumb;		/* chan(Plumbmsg*) */
Channel	*cwait;		/* chan(Waitmsg) */
Channel	*ccommand;	/* chan(Command*) */
Channel	*ckill;		/* chan(Rune*) */
Channel	*cxfidalloc;	/* chan(Xfid*) */
Channel	*cxfidfree;	/* chan(Xfid*) */
Channel	*mouseexit0;	/* chan(int) */
Channel	*mouseexit1;	/* chan(int) */
Channel	*cexit;		/* chan(int) */
Channel	*cerr;		/* chan(char*) */
Channel	*cedit;		/* chan(int) */

#define	STACK	8192