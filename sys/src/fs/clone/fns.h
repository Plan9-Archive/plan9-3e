ulong	strtoul(char*, char**, int);

#include "../port/portfns.h"

void	aamloop(int);
void	cgaputc(int);
void	cgaputs(char*, int);
int	cistrcmp(char*, char*);
int	cistrncmp(char*, char*, int);
void	etherinit(void);
void	etherstart(void);
int	floppyinit(void);
void	floppyproc(void);
long	floppyread(int, void*, long);
long	floppyseek(int, long);
long	floppywrite(int, void*, long);
char*	getconf(char*);
ulong	getcr0(void);
ulong	getcr2(void);
ulong	getcr4(void);
int	getfields(char*, char**, int, char);
ulong	getstatus(void);
int	atainit(void);
long	ataread(int, void*, long);
long	ataseek(int, long);
long	atawrite(int, void*, long);
void	i8042a20(void);
void	i8042reset(void);
int	inb(int);
void	insb(int, void*, int);
ushort	ins(int);
void	inss(int, void*, int);
ulong	inl(int);
void	insl(int, void*, int);
int	isaconfig(char*, int, ISAConf*);
void	kbdinit(void);
int	kbdintr0(void);
int	kbdgetc(void);
long*	mapaddr(ulong);
void	microdelay(int);
void	mmuinit(void);
uchar	nvramread(int);
void	outb(int, int);
void	outsb(int, void*, int);
void	outs(int, ushort);
void	outss(int, void*, int);
void	outl(int, ulong);
void	outsl(int, void*, int);
int	pcicfgr8(Pcidev*, int);
int	pcicfgr16(Pcidev*, int);
int	pcicfgr32(Pcidev*, int);
void	pcicfgw8(Pcidev*, int, int);
void	pcicfgw16(Pcidev*, int, int);
void	pcicfgw32(Pcidev*, int, int);
void	pcihinv(Pcidev*);
Pcidev* pcimatch(Pcidev*, int, int);
Pcidev* pcimatchtbdf(int);
void	pcireset(void);
void	printcpufreq(void);
void	putgdt(Segdesc*, int);
void	putidt(Segdesc*, int);
void	putcr3(ulong);
void	putcr4(ulong);
void	puttr(ulong);
void	scsiinit(void);
long	scsiread(int, void*, long);
long	scsiseek(int, long);
long	scsiwrite(int, void*, long);
int	setatapart(int, char*);
int	setscsipart(int, char*);
void	setvec(int, void (*)(Ureg*, void*), void*);
int	tas(Lock*);
void	trapinit(void);
void	uartspecial(int, void (*)(int), int (*)(void), int);
int	uartgetc(void);
void	uartputc(int);
int	cpuid(char*, int*, int*);
#define PADDR(a)	((ulong)(a)&~KZERO)