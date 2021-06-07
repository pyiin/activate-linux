// fuck this -g -O2 -lX11 -lX11-xcb -lxcb -lxcb-res -lXrender -lfontconfig -lXft -I/usr/include/freetype2
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>

#define LINES_MAX 10
#define MESSAGE "Go to Settings to activate Linux"
#define ACTIVATE "Activate Linux"

const int w_width = 600;
const int w_height = 62;
const unsigned int w_x = 1590;
const unsigned int w_y = 970;
const unsigned long int background = 0x00000000;
int running = 1;
char* message;
//const unsigned long int font_color = 0xffffffff;
XftColor* font_color;
XftColor* back_color;
XEvent event;

typedef struct _Fnt {
	Display *dpy;
	unsigned int h;
	XftFont *xfont;
	FcPattern *pattern;
	struct Fnt *next;
} Fnt;

typedef struct _Cmap{
	XftColor* front;
	XftColor* back;
} Cmap;

typedef struct _Drw{
	int depth;
	int screen;
	Colormap cmap;
	Drawable drawable;
	Visual* visual;
	unsigned int w;
	unsigned int h;
	Display *dpy;
	Window root;
	Window window;
	GC gc;
	XftColor *scheme;
	Fnt* font;
	Fnt* font_two;
	Fnt* font_three;
	Cmap* colormap;
} Drw;

Cmap colormap;

//{{{
static Fnt *
xfont_create(Drw *drw, const char *fontname, FcPattern *fontpattern)
{
	Fnt *font;
	XftFont *xfont = NULL;
	FcPattern *pattern = NULL;

	if (fontname) {
		/* Using the pattern found at font->xfont->pattern does not yield the
		 * same substitution results as using the pattern returned by
		 * FcNameParse; using the latter results in the desired fallback
		 * behaviour whereas the former just results in missing-character
		 * rectangles being drawn, at least with some fonts. */
		if (!(xfont = XftFontOpenName(drw->dpy, drw->screen, fontname))) {
			fprintf(stderr, "error, cannot load font from name: '%s'\n", fontname);
			return NULL;
		}
		if (!(pattern = FcNameParse((FcChar8 *) fontname))) {
			fprintf(stderr, "error, cannot parse font name to pattern: '%s'\n", fontname);
			XftFontClose(drw->dpy, xfont);
			return NULL;
		}
	} else if (fontpattern) {
		if (!(xfont = XftFontOpenPattern(drw->dpy, fontpattern))) {
			fprintf(stderr, "error, cannot load font from pattern.\n");
			return NULL;
		}
	} else {
		fprintf(stderr,"no font specified.");
	}

	font = calloc(1, sizeof(Fnt));
	font->xfont = xfont;
	font->pattern = pattern;
	font->h = xfont->ascent + xfont->descent;
	font->dpy = drw->dpy;

	return font;
}
//}}}

//good
XftColor* createColor(unsigned int col){
	XftColor* ret = calloc(1, sizeof(XftColor));
	ret->pixel = col;
	ret->color = (XRenderColor){(col&0x00ff0000) >> 8, col&0x0000ff00, (col&0x000000ff) << 8, (col&0xff000000) >> 16};

	return ret;
}

//+/-
Drw doXorgShit(int n_lines){
	Drw drw;
	// open connection to the server
	drw.dpy = XOpenDisplay(NULL);
	if (drw.dpy == NULL)
	{
		fprintf(stderr, "Cannot open display\n");
		exit(1);
	}
	drw.screen = DefaultScreen(drw.dpy);
	drw.depth = 32;
	drw.w = w_width;
	drw.h = w_height;
	drw.root = DefaultRootWindow(drw.dpy);


	XVisualInfo vinfo;
	XMatchVisualInfo(drw.dpy, drw.screen, drw.depth, TrueColor, &vinfo);
	XSetWindowAttributes attr;
	drw.visual = vinfo.visual;
	attr.colormap = XCreateColormap(drw.dpy, drw.root, drw.visual, AllocNone);
	attr.border_pixel = 0;
	attr.background_pixel = background;
	attr.override_redirect = 1;

	//drw.font = xfont_create(&drw, "ubuntu mono nerdfont:pixelsize=27", NULL);
	drw.font = xfont_create(&drw, "Segoe ui:pixelsize=32", NULL);
	drw.font_two = xfont_create(&drw, "Segoe ui:pixelsize=22", NULL);
	drw.font_three = drw.font;
	drw.h = LINES_MAX*drw.font->h+10;
	drw.drawable = XCreatePixmap(drw.dpy, drw.root, drw.w, drw.h, drw.depth);
	// create window
	drw.window = XCreateWindow(drw.dpy, drw.root, 0, 0, drw.w, drw.h, 0, drw.depth, InputOutput,
					drw.visual, CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect, &attr);

	// select kind of events we are interested in
	XSelectInput(drw.dpy, drw.window, ExposureMask);

	// map (show) the window
	XMapWindow(drw.dpy, drw.window);

	drw.gc = XCreateGC (drw.dpy, drw.drawable, 0, NULL);

	XSetLineAttributes(drw.dpy, drw.gc, 1, LineSolid, CapButt, JoinMiter);

	XMoveResizeWindow(drw.dpy, drw.window, w_x, w_y, drw.w, drw.h);

	colormap = (Cmap){createColor(0xffababab), createColor(0x00000000)};
	drw.colormap = &colormap;
	return drw;
}

int k = 0;

void draw_text(Drw* drw, unsigned int width, int x, int y, char* text){
	/*
		x specified in pixels, same as width, y specified in lines
	*/
	XftDraw *d = XftDrawCreate(drw->dpy, drw->drawable, drw->visual, drw->cmap);

	int print_height = y;
	XftDrawRect (d, drw->colormap->back, 0,0, width, drw->font->h);
	//get string length
	unsigned int l;
	XGlyphInfo ext;
	XftTextExtentsUtf8(drw->dpy, drw->font->xfont, (XftChar8 *)text, strlen(text), &ext);
	l = ext.xOff;

	//draw
	if(l <= width)
		XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, 0, drw->font->xfont->ascent, text, strlen(text));
	else{
		l += drw->font->h;
		int offset = l-(k)%l;
		XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, offset, drw->font->xfont->ascent, text, strlen(text));
		XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, offset-l, drw->font->xfont->ascent, text, strlen(text));
	}
	//apply changes
	XCopyArea(drw->dpy, drw->drawable, drw->window, drw->gc,0, 0, width, drw->font->h, x, print_height);

}
//draw_percent: {{{
void draw_text_percent(Drw* drw, unsigned int width, int x, int y, char* text, unsigned char percent){
	/*
		x specified in pixels, same as width, y specified in lines
	*/
	XftDraw *d = XftDrawCreate(drw->dpy, drw->drawable, drw->visual, drw->cmap);

	int print_height = y*(drw->font->h);
	//get string length
	unsigned int l;
	XGlyphInfo ext;
	XftTextExtentsUtf8(drw->dpy, drw->font->xfont, (XftChar8 *)text, strlen(text), &ext);
	l = ext.xOff;
	XftDrawRect (d, drw->colormap->back, 0,0, width, drw->font->h);

	//draw
	if(l <= width)
		XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, 0, drw->font->xfont->ascent, text, strlen(text));
	else{
		l += drw->font->h;
		int offset = l-(k)%l;
		XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, offset, drw->font->xfont->ascent, text, strlen(text));
		XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, offset-l, drw->font->xfont->ascent, text, strlen(text));
	}
	//apply changes
	XCopyArea(drw->dpy, drw->drawable, drw->window, drw->gc,0, 0, width, drw->font->h*percent/0xff, x, print_height);

	XftDrawRect (d, drw->colormap->front, 0,0, width, drw->font->h);
	if(l <= width)
		XftDrawStringUtf8(d, drw->colormap->back, drw->font->xfont, 0, drw->font->xfont->ascent, text, strlen(text));
	else{
		l += drw->font->h;
		int offset = l-(k)%l;
		XftDrawStringUtf8(d, drw->colormap->back, drw->font->xfont, offset, drw->font->xfont->ascent, text, strlen(text));
		XftDrawStringUtf8(d, drw->colormap->back, drw->font->xfont, offset-l, drw->font->xfont->ascent, text, strlen(text));
	}
	XCopyArea(drw->dpy, drw->drawable, drw->window, drw->gc,0, drw->font->h*percent/0xff, width, drw->font->h - drw->font->h*percent/0xff, x, print_height+drw->font->h*percent/0xff);
}
//}}}

//to change
void redraw(Drw* drw, char** to_print, int limit){
	drw->font = drw->font_three;
	draw_text(drw, w_width/2, 0, 0, ACTIVATE);
	drw->font = drw->font_two;
	draw_text(drw, w_width/2, 0, 36, MESSAGE);
	XFlush(drw->dpy); //!!!important
}


//good
static void (*handler[LASTEvent]) (XEvent *) = {
//	[ButtonPress] = pass,
//	[ClientMessage] = pass,
//	[ConfigureRequest] = pass,
//	[ConfigureNotify] = pass,
//	[DestroyNotify] = pass,
//	[EnterNotify] = pass,
//	[Expose] = pass,
//	[FocusIn] = pass,
	//[KeyPress] = keypress,
//	[MappingNotify] = pass,
//	[MapRequest] = pass,
//	[MotionNotify] = pass,
//	[PropertyNotify] = pass,
//	[UnmapNotify] = pass,
};


int main(int argc, char ** argv)
{
	Drw drw = doXorgShit(argc);

	int number;

	// event loop
	//DONE -> array of event functions
	//TODO -> text writing line by line
	//DONE -> text rotation + fps
	//TODO -> modes + editing
	while(running){
		//XSync(drw.dpy,0);
		redraw(&drw, argv+number, argc-number); //TODO: make loop logic
		//loop[mode](&drw);
		k++;
		usleep(33333); //TODO: optimise for exact frame rate
		while(QLength(drw.dpy)>0){ //fucking finally works
			XNextEvent(drw.dpy, &event);
			if(handler[event.type])
				handler[event.type](&event);
		}
		XRaiseWindow(drw.dpy, drw.window);
	}

	// close connection to the server
	printf("%d", running);
	XCloseDisplay(drw.dpy);

	return 0;
 }
