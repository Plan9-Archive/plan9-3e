/*
 * Memory and machine-specific definitions.  Used in C and assembler.
 */

/*
 * Sizes
 */
#define	BI2BY		8			/* bits per byte */
#define BI2WD		32			/* bits per word */
#define	BY2WD		4			/* bytes per word */
#define	BY2V		8			/* bytes per double word */
#define	BY2PG		4096			/* bytes per page */
#define	WD2PG		(BY2PG/BY2WD)		/* words per page */
#define	PGSHIFT		12			/* log(BY2PG) */
#define ROUND(s, sz)	(((s)+((sz)-1))&~((sz)-1))
#define PGROUND(s)	ROUND(s, BY2PG)
#define BLOCKALIGN	8

#define	MAXMACH		8			/* max # cpus system can run */
#define KSTACK		4096			/* Size of kernel stack */

/*
 * Time
 */
#define	HZ		(82)			/* clock frequency */
#define	MS2HZ		(1000/HZ)		/* millisec per clock tick */
#define	TK2SEC(t)	((t)/HZ)		/* ticks to seconds */
#define	MS2TK(t)	(((t)*HZ+500)/1000)	/* milliseconds to closest tick */

/*
 * Fundamental addresses
 */
#define IDTADDR		0x80000800		/* idt */
#define APBOOTSTRAP	0x80001000		/* AP bootstrap code */
#define CONFADDR	0x80001200		/* info passed from boot loader */
#define CPU0PDB		0x80002000		/* bootstrap processor PDB */
#define CPU0PTE		0x80003000		/* bootstrap processor PTE's for 0-4MB */
#define MACHADDR	0x80004000		/* as seen by current processor */
#define CPU0MACH	0x80005000		/* Mach for bootstrap processor */
#define	MACHSIZE	BY2PG

/*
 *  Address spaces
 *
 *  User is at 0-2GB
 *  Kernel is at 2GB-4GB
 */
#define	UZERO		0			/* base of user address space */
#define	UTZERO		(UZERO+BY2PG)		/* first address in user text */
#define	KZERO		0x80000000		/* base of kernel address space */
#define	KTZERO		0x80100000		/* first address in kernel text */
#define	USTKTOP		(KZERO-BY2PG)		/* byte just beyond user stack */
#define	USTKSIZE	(16*1024*1024)		/* size of user stack */
#define	TSTKTOP		(USTKTOP-USTKSIZE)	/* end of new stack in sysexec */
#define TSTKSIZ 	100

/*
 *  known x86 segments (in GDT) and their selectors
 */
#define	NULLSEG	0	/* null segment */
#define	KDSEG	1	/* kernel data/stack */
#define	KESEG	2	/* kernel executable */	
#define	UDSEG	3	/* user data/stack */
#define	UESEG	4	/* user executable */
#define TSSSEG	5	/* task segment */
#define	APMCSEG		6	/* APM code segment */
#define	APMCSEG16	7	/* APM 16-bit code segment */
#define	APMDSEG		8	/* APM data segment */
#define NGDT		10	/* number of GDT entries required */
/* #define	APM40SEG	8	/* APM segment 0x40 */

#define SELGDT	(0<<2)	/* selector is in gdt */
#define	SELLDT	(1<<2)	/* selector is in ldt */

#define SELECTOR(i, t, p)	(((i)<<3) | (t) | (p))

#define NULLSEL	SELECTOR(NULLSEG, SELGDT, 0)
#define KESEL	SELECTOR(KESEG, SELGDT, 0)
#define KDSEL	SELECTOR(KDSEG, SELGDT, 0)
#define UESEL	SELECTOR(UESEG, SELGDT, 3)
#define UDSEL	SELECTOR(UDSEG, SELGDT, 3)
#define TSSSEL	SELECTOR(TSSSEG, SELGDT, 0)
#define APMCSEL 	SELECTOR(APMCSEG, SELGDT, 0)
#define APMCSEL16	SELECTOR(APMCSEG16, SELGDT, 0)
#define APMDSEL		SELECTOR(APMDSEG, SELGDT, 0)
/* #define APM40SEL	SELECTOR(APM40SEG, SELGDT, 0) */

/*
 *  fields in segment descriptors
 */
#define SEGDATA	(0x10<<8)	/* data/stack segment */
#define SEGEXEC	(0x18<<8)	/* executable segment */
#define	SEGTSS	(0x9<<8)	/* TSS segment */
#define SEGCG	(0x0C<<8)	/* call gate */
#define	SEGIG	(0x0E<<8)	/* interrupt gate */
#define SEGTG	(0x0F<<8)	/* trap gate */
#define SEGTYPE	(0x1F<<8)

#define SEGP	(1<<15)		/* segment present */
#define SEGPL(x) ((x)<<13)	/* priority level */
#define SEGB	(1<<22)		/* granularity 1==4k (for expand-down) */
#define SEGG	(1<<23)		/* granularity 1==4k (for other) */
#define SEGE	(1<<10)		/* expand down */
#define SEGW	(1<<9)		/* writable (for data/stack) */
#define	SEGR	(1<<9)		/* readable (for code) */
#define SEGD	(1<<22)		/* default 1==32bit (for code) */

/*
 *  virtual MMU
 */
#define PTEMAPMEM	(1024*1024)	
#define	PTEPERTAB	(PTEMAPMEM/BY2PG)
#define SEGMAPSIZE	1984
#define SSEGMAPSIZE	16
#define PPN(x)		((x)&~(BY2PG-1))

/*
 *  physical MMU
 */
#define	PTEVALID	(1<<0)
#define	PTEWT		(1<<3)
#define	PTEUNCACHED	(1<<4)
#define 	PTEWRITE	(1<<1)
#define	PTERONLY	(0<<1)
#define	PTEKERNEL	(0<<2)
#define	PTEUSER		(1<<2)
#define	PTESIZE		(1<<7)

#define getpgcolor(a)	0
