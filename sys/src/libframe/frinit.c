#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <frame.h>

void
frinit(Frame *f, Rectangle r, Font *ft, Image *b, Image *cols[NCOL])
{
	f->font = ft;
	f->display = b->display;
	f->maxtab = 8*stringwidth(ft, "0");
	f->nbox = 0;
	f->nalloc = 0;
	f->nchars = 0;
	f->nlines = 0;
	f->p0 = 0;
	f->p1 = 0;
	f->box = 0;
	f->lastlinefull = 0;
	if(cols != 0)
		memmove(f->cols, cols, sizeof f->cols);
	frsetrects(f, r, b);
	if(f->tick==nil && f->cols[BACK]!=0)
		frinittick(f);
}

void
frinittick(Frame *f)
{
	Image *b;
	Font *ft;

	b = f->display->image;
	ft = f->font;
	if(f->tick)
		free(f->tick);
	f->tick = allocimage(f->display, Rect(0, 0, FRTICKW, ft->height), b->chan, 0, DWhite);
	if(f->tick == nil)
		return;
	f->tickback = allocimage(f->display, f->tick->r, b->chan, 0, DWhite);
	if(f->tickback == 0){
		freeimage(f->tick);
		f->tick = 0;
		return;
	}
	/* background color */
	draw(f->tick, f->tick->r, f->cols[BACK], nil, ZP);
	/* vertical line */
	draw(f->tick, Rect(FRTICKW/2, 0, FRTICKW/2+1, ft->height), f->display->black, nil, ZP);
	/* box on each end */
	draw(f->tick, Rect(0, 0, FRTICKW, FRTICKW), f->cols[TEXT], nil, ZP);
	draw(f->tick, Rect(0, ft->height-FRTICKW, FRTICKW, ft->height), f->cols[TEXT], nil, ZP);
}

void
frsetrects(Frame *f, Rectangle r, Image *b)
{
	f->b = b;
	f->entire = r;
	f->r = r;
	f->r.max.y -= (r.max.y-r.min.y)%f->font->height;
	f->maxlines = (r.max.y-r.min.y)/f->font->height;
}

void
frclear(Frame *f, int freeall)
{
	if(f->nbox)
		_frdelbox(f, 0, f->nbox-1);
	if(f->box)
		free(f->box);
	if(freeall){
		freeimage(f->tick);
		freeimage(f->tickback);
		f->tick = 0;
		f->tickback = 0;
	}
	f->box = 0;
	f->ticked = 0;
}
