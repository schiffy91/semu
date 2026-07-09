// xcap.c — grab the X root window to PPM and assert the OUTER EDGE BAND is
// painted (bezel present). Used by run_standalone.sh to prove the GL tap
// composited its bezel over a real, uninstrumented GL program (glxgears) in
// SEMU_TAP_STANDALONE mode — the exact path Cemu/Dolphin/PCSX2 hit (no report;
// the tap synthesizes the content rect from the live framebuffer).
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char** argv){
    const char* out = argc>1 ? argv[1] : "cap.ppm";
    Display* display = XOpenDisplay(NULL);
    if(!display){ fprintf(stderr,"xcap: no display\n"); return 2; }
    Window root = DefaultRootWindow(display);
    XWindowAttributes attributes; XGetWindowAttributes(display, root, &attributes);
    int width = attributes.width, height = attributes.height;
    XImage* image = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    if(!image){ fprintf(stderr,"xcap: XGetImage failed\n"); return 2; }
    unsigned long redMask=image->red_mask, greenMask=image->green_mask, blueMask=image->blue_mask;
    int redShift=0, greenShift=0, blueShift=0; unsigned long bit;
    for(bit=redMask;   bit && !(bit&1); bit>>=1) redShift++;
    for(bit=greenMask; bit && !(bit&1); bit>>=1) greenShift++;
    for(bit=blueMask;  bit && !(bit&1); bit>>=1) blueShift++;

    FILE* file = fopen(out,"wb");
    if(file) fprintf(file,"P6\n%d %d\n255\n", width, height);
    const int margin = 40; long band=0, lit=0;
    for(int y=0;y<height;y++) for(int x=0;x<width;x++){
        unsigned long pixel = XGetPixel(image, x, y);
        unsigned char rgb[3] = {
            (unsigned char)((pixel&redMask)>>redShift),
            (unsigned char)((pixel&greenMask)>>greenShift),
            (unsigned char)((pixel&blueMask)>>blueShift) };
        if(file) fwrite(rgb,1,3,file);
        if(x<margin || x>=width-margin || y<margin || y>=height-margin){
            band++;
            if(rgb[0]>8 || rgb[1]>8 || rgb[2]>8) lit++;
        }
    }
    if(file) fclose(file);
    double fraction = band ? (double)lit/band : 0.0;
    fprintf(stderr,"xcap: wrote %s %dx%d; outer-band non-black = %.3f (%ld/%ld)\n",
        out, width, height, fraction, lit, band);
    // The tap paints the whole bezel frame; a bare desktop leaves the band black.
    int pass = fraction > 0.90;
    fprintf(stderr,"xcap: %s\n", pass ? "PASS (bezel frame present)" : "FAIL (outer band not bezel)");
    return pass ? 0 : 1;
}
