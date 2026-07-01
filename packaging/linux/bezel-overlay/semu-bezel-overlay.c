// semu-bezel-overlay: draw a bezel PNG (transparent center) as a gamescope external overlay.
// Uses a 32-bit ARGB X11 visual (so the transparent center actually shows the game), Cairo to
// paint the PNG with alpha, GAMESCOPE_EXTERNAL_OVERLAY so gamescope composites it on top, and an
// empty XShape input region so all input passes through to the emulator behind it.
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <stdio.h>
static void draw(cairo_surface_t* surf, cairo_surface_t* img, int W, int H, int iw, int ih){
    cairo_t* cr=cairo_create(surf);
    cairo_set_operator(cr,CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr,0,0,0,0); cairo_paint(cr);     // clear fully transparent
    cairo_scale(cr,(double)W/iw,(double)H/ih);
    cairo_set_source_surface(cr,img,0,0); cairo_paint(cr);  // bezel (with its own alpha)
    cairo_destroy(cr);
}
int main(int argc,char**argv){
    const char* png=argc>1?argv[1]:"bezel.png";
    Display* d=XOpenDisplay(NULL);
    if(!d){ fprintf(stderr,"semu-bezel: cannot open X display\n"); return 1; }
    int scr=DefaultScreen(d);
    XVisualInfo vi;
    if(!XMatchVisualInfo(d,scr,32,TrueColor,&vi)){ fprintf(stderr,"semu-bezel: no 32-bit ARGB visual\n"); return 2; }
    Window root=RootWindow(d,scr);
    int W=DisplayWidth(d,scr), H=DisplayHeight(d,scr);
    XSetWindowAttributes a;
    a.colormap=XCreateColormap(d,root,vi.visual,AllocNone);
    a.background_pixel=0; a.border_pixel=0; a.override_redirect=False;
    Window win=XCreateWindow(d,root,0,0,W,H,0,vi.depth,InputOutput,vi.visual,
        CWColormap|CWBackPixel|CWBorderPixel,&a);
    long one=1;
    XChangeProperty(d,win,XInternAtom(d,"GAMESCOPE_EXTERNAL_OVERLAY",False),XA_CARDINAL,32,
        PropModeReplace,(unsigned char*)&one,1);
    XShapeCombineRectangles(d,win,ShapeInput,0,0,NULL,0,ShapeSet,Unsorted); // empty input -> passthrough
    XStoreName(d,win,"semu-bezel-overlay"); XMapRaised(d,win); XFlush(d);
    cairo_surface_t* surf=cairo_xlib_surface_create(d,win,vi.visual,W,H);
    cairo_surface_t* img=cairo_image_surface_create_from_png(png);
    if(cairo_surface_status(img)!=CAIRO_STATUS_SUCCESS){ fprintf(stderr,"semu-bezel: png load failed: %s\n",png); return 3; }
    int iw=cairo_image_surface_get_width(img), ih=cairo_image_surface_get_height(img);
    draw(surf,img,W,H,iw,ih);
    fprintf(stderr,"semu-bezel: overlay up (%dx%d) atom set, drawing %s (%dx%d)\n",W,H,png,iw,ih);
    XSelectInput(d,win,ExposureMask);
    for(;;){ XEvent e; XNextEvent(d,&e); if(e.type==Expose) draw(surf,img,W,H,iw,ih), XFlush(d); }
    return 0;
}
