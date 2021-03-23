#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "io.h"

#define	Image	IMAGE
#include <draw.h>
#include <memdraw.h>
#include <cursor.h>
#include "screen.h"

/*
 *  mouse types
 */
enum
{
	Mouseother=	0,
	Mouseserial=	1,
	MousePS2=	2,
};
static int mousetype;

/*
 *  setup a serial mouse
 */
static void
serialmouse(int port, char *type, int setspeed)
{
	if(mousetype == Mouseserial)
		error(Emouseset);

	if(port >= 3 || port < 0)
		error(Ebadarg);

	/* set up /dev/eia? as the mouse */
	if(setspeed)
		setspeed = 1200;
	if(type && *type == 'M')
		ns16552special(port, setspeed, 0, 0, m3mouseputc);
	else
		ns16552special(port, setspeed, 0, 0, mouseputc);
	mousetype = Mouseserial;
}

/*
 *  ps/2 mouse message is three bytes
 *
 *	byte 0 -	0 0 SDY SDX 1 M R L
 *	byte 1 -	DX
 *	byte 2 -	DY
 *
 *  shift & left button is the same as middle button
 */
static void
ps2mouseputc(int c, int shift)
{
	static short msg[3];
	static int nb;
	static uchar b[] = {0, 1, 4, 5, 2, 3, 6, 7, 0, 1, 2, 5, 2, 3, 6, 7 };
	int buttons, dx, dy;

	/* 
	 *  check byte 0 for consistency
	 */
	if(nb==0 && (c&0xc8)!=0x08)
		return;

	msg[nb] = c;
	if(++nb == 3){
		nb = 0;
		if(msg[0] & 0x10)
			msg[1] |= 0xFF00;
		if(msg[0] & 0x20)
			msg[2] |= 0xFF00;

		buttons = b[(msg[0]&7) | (shift ? 8 : 0)];
		dx = msg[1];
		dy = -msg[2];
		mousetrack(buttons, dx, dy);
	}
	return;
}

/*
 *  set up a ps2 mouse
 */
static void
ps2mouse(void)
{
	if(mousetype == MousePS2)
		return;

	i8042auxenable(ps2mouseputc);
	/* make mouse streaming, enabled */
	i8042auxcmd(0xEA);
	i8042auxcmd(0xF4);

	mousetype = MousePS2;
}

static int intellimouse;
static int resolution;
static int accelerated;

static void
setaccelerated(int x)
{
	accelerated = x;
	switch(mousetype){
	case MousePS2:
		i8042auxcmd(0xE7);
		break;
	default:
		mouseaccelerate(x);
		break;
	}
}

static void
setlinear(void)
{
	accelerated = 0;
	switch(mousetype){
	case MousePS2:
		i8042auxcmd(0xE6);
		break;
	default:
		mouseaccelerate(0);
		break;
	}
}

static void
setres(int n)
{
	resolution = n;
	switch(mousetype){
	case MousePS2:
		i8042auxcmd(0xE8);
		i8042auxcmd(n);
		break;
	}
}

static void
setintellimouse(void)
{
	intellimouse = 1;
	switch(mousetype){
	case MousePS2:
		i8042auxcmd(0xF3);	/* set sample */
		i8042auxcmd(0xC8);
		i8042auxcmd(0xF3);	/* set sample */
		i8042auxcmd(0x64);
		i8042auxcmd(0xF3);	/* set sample */
		i8042auxcmd(0x50);
		break;
	}
}

static void
resetmouse(void)
{
	switch(mousetype){
	case MousePS2:
		i8042auxcmd(0xF6);
		i8042auxcmd(0xEA);	/* streaming */
		i8042auxcmd(0xE8);	/* set resolution */
		i8042auxcmd(3);
		i8042auxcmd(0xF4);	/* enabled */
		break;
	}
}

void
mousectl(char* field[], int n)
{
	if(strncmp(field[0], "serial", 6) == 0){
		switch(n){
		case 1:
			serialmouse(atoi(field[0]+6), 0, 1);
			break;
		case 2:
			serialmouse(atoi(field[1]), 0, 0);
			break;
		case 3:
		default:
			serialmouse(atoi(field[1]), field[2], 0);
			break;
		}
	} else if(strcmp(field[0], "ps2") == 0){
		ps2mouse();
	} else if(strcmp(field[0], "ps2intellimouse") == 0){
		ps2mouse();
		setintellimouse();
	} else if(strcmp(field[0], "accelerated") == 0){
		setaccelerated(n == 1 ? 1 : atoi(field[1]));
	} else if(strcmp(field[0], "linear") == 0){
		setlinear();
	} else if(strcmp(field[0], "res") == 0){
		if(n >= 2)
			n = atoi(field[1]);
		setres(n);
	} else if(strcmp(field[0], "reset") == 0){
		resetmouse();
		if(accelerated)
			setaccelerated(accelerated);
		if(resolution)
			setres(resolution);
		if(intellimouse)
			setintellimouse();
	} else if(strcmp(field[0], "intellimouse") == 0){
		setintellimouse();
	}
	else
		error(Ebadctl);
}
