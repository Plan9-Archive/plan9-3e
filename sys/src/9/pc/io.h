#define X86STEPPING(x)	((x) & 0x0F)
#define X86MODEL(x)	(((x)>>4) & 0x0F)
#define X86FAMILY(x)	(((x)>>8) & 0x0F)

enum {
	VectorNMI	= 2,		/* non-maskable interrupt */
	VectorBPT	= 3,		/* breakpoint */
	VectorUD	= 6,		/* invalid opcode exception */
	VectorCNA	= 7,		/* coprocessor not available */
	VectorCSO	= 9,		/* coprocessor segment overrun */
	VectorPF	= 14,		/* page fault */
	VectorCERR	= 16,		/* coprocessor error */

	VectorPIC	= 32,		/* external i8259 interrupts */
	IrqCLOCK	= 0,
	IrqKBD		= 1,
	IrqUART1	= 3,
	IrqUART0	= 4,
	IrqPCMCIA	= 5,
	IrqFLOPPY	= 6,
	IrqLPT		= 7,
	IrqIRQ7		= 7,
	IrqAUX		= 12,		/* PS/2 port */
	IrqIRQ13	= 13,		/* coprocessor on 386 */
	IrqATA0		= 14,
	IrqATA1		= 15,
	MaxIrqPIC	= 15,

	VectorLAPIC	= VectorPIC+16,	/* local APIC interrupts */
	IrqLINT0	= 16,		/* LINT[01] must be offsets 0 and 1 */
	IrqLINT1	= 17,
	IrqTIMER	= 18,
	IrqERROR	= 19,
	IrqPCINT	= 20,
	IrqSPURIOUS	= 31,		/* must have bits [3-0] == 0x0F */
	MaxIrqLAPIC	= 31,

	VectorSYSCALL	= 64,

	VectorAPIC	= 65,		/* external APIC interrupts */
	MaxVectorAPIC	= 255,
};

typedef struct Vctl {
	Vctl*	next;			/* handlers on this vector */

	char	name[NAMELEN];		/* of driver */
	int	isintr;			/* interrupt or fault/trap */
	int	irq;
	int	tbdf;
	int	(*isr)(int);		/* get isr bit for this irq */
	int	(*eoi)(int);		/* eoi */

	void	(*f)(Ureg*, void*);	/* handler to call */
	void*	a;			/* argument to call it with */
} Vctl;

enum {
	BusCBUS		= 0,		/* Corollary CBUS */
	BusCBUSII,			/* Corollary CBUS II */
	BusEISA,			/* Extended ISA */
	BusFUTURE,			/* IEEE Futurebus */
	BusINTERN,			/* Internal bus */
	BusISA,				/* Industry Standard Architecture */
	BusMBI,				/* Multibus I */
	BusMBII,			/* Multibus II */
	BusMCA,				/* Micro Channel Architecture */
	BusMPI,				/* MPI */
	BusMPSA,			/* MPSA */
	BusNUBUS,			/* Apple Macintosh NuBus */
	BusPCI,				/* Peripheral Component Interconnect */
	BusPCMCIA,			/* PC Memory Card International Association */
	BusTC,				/* DEC TurboChannel */
	BusVL,				/* VESA Local bus */
	BusVME,				/* VMEbus */
	BusXPRESS,			/* Express System Bus */
};

#define MKBUS(t,b,d,f)	(((t)<<24)|(((b)&0xFF)<<16)|(((d)&0x1F)<<11)|(((f)&0x07)<<8))
#define BUSFNO(tbdf)	(((tbdf)>>8)&0x07)
#define BUSDNO(tbdf)	(((tbdf)>>11)&0x1F)
#define BUSBNO(tbdf)	(((tbdf)>>16)&0xFF)
#define BUSTYPE(tbdf)	((tbdf)>>24)
#define BUSBDF(tbdf)	((tbdf)&0x00FFFF00)
#define BUSUNKNOWN	(-1)

enum {
	MaxEISA		= 16,
	CfgEISA		= 0xC80,
};

/*
 * PCI support code.
 */
enum {					/* type 0 and type 1 pre-defined header */
	PciVID		= 0x00,		/* vendor ID */
	PciDID		= 0x02,		/* device ID */
	PciPCR		= 0x04,		/* command */
	PciPSR		= 0x06,		/* status */
	PciRID		= 0x08,		/* revision ID */
	PciCCRp		= 0x09,		/* programming interface class code */
	PciCCRu		= 0x0A,		/* sub-class code */
	PciCCRb		= 0x0B,		/* base class code */
	PciCLS		= 0x0C,		/* cache line size */
	PciLTR		= 0x0D,		/* latency timer */
	PciHDT		= 0x0E,		/* header type */
	PciBST		= 0x0F,		/* BIST */

	PciBAR0		= 0x10,		/* base address */
	PciBAR1		= 0x14,

	PciINTL		= 0x3C,		/* interrupt line */
	PciINTP		= 0x3D,		/* interrupt pin */
};

enum {					/* type 0 pre-defined header */
	PciBAR2		= 0x18,
	PciBAR3		= 0x1C,
	PciBAR4		= 0x20,
	PciBAR5		= 0x24,
	PciCIS		= 0x28,		/* cardbus CIS pointer */
	PciSVID		= 0x2C,		/* subsystem vendor ID */
	PciSID		= 0x2E,		/* cardbus CIS pointer */
	PciEBAR0	= 0x30,		/* expansion ROM base address */
	PciMGNT		= 0x3E,		/* burst period length */
	PciMLT		= 0x3F,		/* maximum latency between bursts */
};

enum {					/* type 1 pre-defined header */
	PciPBN		= 0x18,		/* primary bus number */
	PciSBN		= 0x19,		/* secondary bus number */
	PciUBN		= 0x1A,		/* subordinate bus number */
	PciSLTR		= 0x1B,		/* secondary latency timer */
	PciIBR		= 0x1C,		/* I/O base */
	PciILR		= 0x1D,		/* I/O limit */
	PciSPSR		= 0x1E,		/* secondary status */
	PciMBR		= 0x20,		/* memory base */
	PciMLR		= 0x22,		/* memory limit */
	PciPMBR		= 0x24,		/* prefetchable memory base */
	PciPMLR		= 0x26,		/* prefetchable memory limit */
	PciPUBR		= 0x28,		/* prefetchable base upper 32 bits */
	PciPULR		= 0x2C,		/* prefetchable limit upper 32 bits */
	PciIUBR		= 0x30,		/* I/O base upper 16 bits */
	PciIULR		= 0x32,		/* I/O limit upper 16 bits */
	PciEBAR1	= 0x28,		/* expansion ROM base address */
	PciBCR		= 0x3E,		/* bridge control register */
};

enum {					/* type 2 pre-defined header */
	PciCBExCA	= 0x10,
	PciCBSPSR	= 0x16,
	PciCBPBN	= 0x18,		/* primary bus number */
	PciCBSBN	= 0x19,		/* secondary bus number */
	PciCBUBN	= 0x1A,		/* subordinate bus number */
	PciCBSLTR	= 0x1B,		/* secondary latency timer */
	PciCBMBR0	= 0x1C,
	PciCBMLR0	= 0x20,
	PciCBMBR1	= 0x24,
	PciCBMLR1	= 0x28,
	PciCBIBR0	= 0x2C,		/* I/O base */
	PciCBILR0	= 0x30,		/* I/O limit */
	PciCBIBR1	= 0x34,		/* I/O base */
	PciCBILR1	= 0x38,		/* I/O limit */
	PciCBSVID	= 0x40,		/* subsystem vendor ID */
	PciCBSID	= 0x42,		/* subsystem ID */
	PciCBLMBAR	= 0x44,		/* legacy mode base address */
};

typedef struct Pcisiz Pcisiz;
struct Pcisiz
{
	Pcidev*	dev;
	int	siz;
	int	bar;
};

typedef struct Pcidev Pcidev;
struct Pcidev
{
	int	tbdf;			/* type+bus+device+function */
	ushort	vid;			/* vendor ID */
	ushort	did;			/* device ID */

	uchar	rid;
	uchar	ccrp;
	uchar	ccru;
	uchar	ccrb;

	struct {
		ulong	bar;		/* base address */
		int	size;
	} mem[6];

	uchar	intl;			/* interrupt line */

	Pcidev*	list;
	Pcidev*	link;			/* next device on this bno */

	Pcidev*	bridge;			/* down a bus */
	struct {
		ulong	bar;
		int	size;
	} ioa, mema;
	ulong	pcr;
};

#define PCIWINDOW	0
#define PCIWADDR(va)	(PADDR(va)+PCIWINDOW)
#define ISAWINDOW	0
#define ISAWADDR(va)	(PADDR(va)+ISAWINDOW)

/* SMBus transactions */
enum
{
	SMBquick,		/* sends address only */

	/* write */
	SMBsend,		/* sends address and cmd */
	SMBbytewrite,		/* sends address and cmd and 1 byte */
	SMBwordwrite,		/* sends address and cmd and 2 bytes */

	/* read */
	SMBrecv,		/* sends address, recvs 1 byte */
	SMBbyteread,		/* sends address and cmd, recv's byte */
	SMBwordread,		/* sends address and cmd, recv's 2 bytes */
};

typedef struct SMBus SMBus;
struct SMBus {
	QLock;		/* mutex */
	Rendez	r;	/* rendezvous point for completion interrupts */
	void	*arg;	/* implementation dependent */
	ulong	base;	/* port or memory base of smbus */
	int	busy;
	void	(*transact)(SMBus*, int, int, int, uchar*);
};

/*
 * PCMCIA support code.
 */

typedef struct PCMslot		PCMslot;
typedef struct PCMconftab	PCMconftab;
typedef struct Cisdat 		Cisdat;

/*
 * Map between ISA memory space and PCMCIA card memory space.
 */
struct PCMmap {
	ulong	ca;			/* card address */
	ulong	cea;			/* card end address */
	ulong	isa;			/* ISA address */
	int	len;			/* length of the ISA area */
	int	attr;			/* attribute memory */
	int	ref;
};

/* configuration table entry */
struct PCMconftab
{
	int	index;
	ushort	irqs;		/* legal irqs */
	uchar	irqtype;
	uchar	bit16;		/* true for 16 bit access */
	struct {
		ulong	start;
		ulong	len;
	} io[16];
	int	nio;
	uchar	vpp1;
	uchar	vpp2;
	uchar	memwait;
	ulong	maxwait;
	ulong	readywait;
	ulong	otherwait;
};

/* cis memory walking */
struct Cisdat
{
	uchar	*cisbase;
	int	cispos;
	int	cisskip;
	int	cislen;
};

/* a card slot */
struct PCMslot
{
	Lock;
	int	ref;

	void	*cp;		/* controller for this slot */
	long	memlen;		/* memory length */
	uchar	base;		/* index register base */
	uchar	slotno;		/* slot number */

	/* status */
	uchar	special;	/* in use for a special device */
	uchar	already;	/* already inited */
	uchar	occupied;
	uchar	battery;
	uchar	wrprot;
	uchar	powered;
	uchar	configed;
	uchar	enabled;
	uchar	busy;

	/* cis info */
	ulong	msec;		/* time of last slotinfo call */
	char	verstr[512];	/* version string */
	uchar	cpresent;	/* config registers present */
	ulong	caddr;		/* relative address of config registers */
	int	nctab;		/* number of config table entries */
	PCMconftab	ctab[8];
	PCMconftab	*def;		/* default conftab */

	/* for walking through cis */
	Cisdat;

	/* memory maps */
	Lock	mlock;		/* lock down the maps */
	int	time;
	PCMmap	mmap[4];	/* maps, last is always for the kernel */
};
