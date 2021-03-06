#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"ureg.h"
#include	"../port/error.h"

typedef struct OSTimer
{
	ulong	osmr[4];	/* match registers */
	ulong	oscr;		/* counter register */
	ulong	ossr;		/* status register */
	ulong	ower;		/* watchdog enable register */
	ulong	oier;		/* timer interrupt enable register */
} OSTimer;

static OSTimer *timerregs = (OSTimer*)OSTIMERREGS;
static int clockinited;

static void	clockintr(Ureg*, void*);

void
clockinit(void)
{
	ulong x;
	ulong id;

	/* map the clock registers */
	timerregs = mapspecial(OSTIMERREGS, 32);

	/* enable interrupts on match register 0, turn off all others */
	timerregs->ossr |= 1<<0;
	intrenable(IRQ, IRQtimer0, clockintr, nil, "clock");
	timerregs->oier = 1<<0;

	/* figure out processor frequency */
	x = powerregs->ppcr & 0x1f;
	conf.hz = ClockFreq*(x*4+16);
	conf.mhz = (conf.hz+499999)/1000000;

	/* get processor type */
	id = getcpuid();

	print("%lud MHZ ARM, ver %lux/part %lux/step %lud\n", conf.mhz,
		(id>>16)&0xff, (id>>4)&0xfff, id&0xf);

	/* post interrupt 1/HZ secs from now */
	timerregs->osmr[0] = timerregs->oscr + ClockFreq/HZ;

	clockinited = 1;
}

/*  turn 32 bit counter into a 64 bit one.  since todfix calls
 *  us at least once a second and we overflow once every 1165
 *  seconds, we won't miss an overflow.
 */
vlong
fastticks(uvlong *hz)
{
	static vlong high;
	static ulong last;
	ulong x;

	if(hz != nil)
		*hz = ClockFreq;
	x = timerregs->oscr;
	if(x < last)
		high += 1LL<<32;
	last = x;
	return high+x;
}

static void
clockintr(Ureg *ureg, void*)
{
	/* reset previous interrupt */
	timerregs->ossr |= 1<<0;

	/* post interrupt 1/HZ secs from now */
	timerregs->osmr[0] = timerregs->oscr + ClockFreq/HZ;

	drawactive(0);	/* screen saver */
	portclock(ureg);
}

void
delay(int ms)
{
	ulong start;
	int i;

	if(clockinited){
		while(ms-- > 0){
			start = timerregs->oscr;
			while(timerregs->oscr-start < ClockFreq/1000)
				;
		}
	} else {
		while(ms-- > 0){
			for(i = 0; i < 1000; i++)
				;
		}
	}
}

void
??delay(int ??s)
{
	ulong start;
	int i;

	if(clockinited){
		start = timerregs->oscr;
		while(timerregs->oscr - start < (??s*ClockFreq)/1000000)
			;
	} else {
		while(??s-- > 0){
			for(i = 0; i < 10; i++)
				;
		}
	}
}
