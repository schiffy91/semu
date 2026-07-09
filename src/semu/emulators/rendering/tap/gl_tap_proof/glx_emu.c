// glx_emu.c — headless "fake GL emulator" over GLX (the exact present path the Deck
// standalone emulators use). Renders a game frame into a GLX window's back buffer,
// reports a SemuTapState, and calls glXSwapBuffers — intercepted by LD_PRELOAD=
// libsemutap.so, which composites the bezel+shader around the reported content rect.
// We read the composited framebuffer back and PROVE the bezel got drawn.
//
// Build: cc -o glx_emu glx_emu.c -lGL -lX11 -ldl -lm
// Run:   xvfb-run -s "-screen 0 1280x800x24" env LD_PRELOAD=./libsemutap.so ... ./glx_emu out.ppm
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semu_tap.h"

#define FBW 1280
#define FBH 800
#define CX 240
#define CY 100
#define CW 800
#define CH 600

int main(int argc, char** argv){
    const char* outpath = argc>1 ? argv[1] : "out.ppm";
    Display* dpy = XOpenDisplay(NULL);
    if(!dpy){ fprintf(stderr,"glx_emu: XOpenDisplay failed (no X)\n"); return 2; }

    int fbattr[]={ GLX_X_RENDERABLE,True, GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
                   GLX_RENDER_TYPE,GLX_RGBA_BIT, GLX_DOUBLEBUFFER,True,
                   GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,GLX_ALPHA_SIZE,8, None };
    int nfb=0; GLXFBConfig* fbc=glXChooseFBConfig(dpy,DefaultScreen(dpy),fbattr,&nfb);
    if(!fbc||nfb<1){ fprintf(stderr,"glx_emu: no fbconfig\n"); return 2; }
    XVisualInfo* vi=glXGetVisualFromFBConfig(dpy,fbc[0]);
    Window root=RootWindow(dpy,vi->screen);
    XSetWindowAttributes swa; memset(&swa,0,sizeof swa);
    swa.colormap=XCreateColormap(dpy,root,vi->visual,AllocNone);
    swa.event_mask=StructureNotifyMask;
    Window win=XCreateWindow(dpy,root,0,0,FBW,FBH,0,vi->depth,InputOutput,vi->visual,
                             CWColormap|CWEventMask,&swa);
    XMapWindow(dpy,win); XSync(dpy,False);
    GLXContext ctx=glXCreateNewContext(dpy,fbc[0],GLX_RGBA_TYPE,NULL,True);
    if(!ctx){ fprintf(stderr,"glx_emu: no context\n"); return 2; }
    if(!glXMakeCurrent(dpy,win,ctx)){ fprintf(stderr,"glx_emu: makeCurrent failed\n"); return 2; }
    fprintf(stderr,"glx_emu: GL_RENDERER=%s\n  GL_VERSION=%s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    // --- render the "game": black whole fb, bright game fill in content rect (GL bottom-left) ---
    glDrawBuffer(GL_BACK);
    glViewport(0,0,FBW,FBH);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST); glScissor(CX,CY,CW,CH);
    glClearColor(0.90f,0.10f,0.80f,1.0f); glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST); glFinish();

    unsigned char pre[4]={9,9,9,9};
    glReadBuffer(GL_BACK); glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pre);
    fprintf(stderr,"glx_emu: border(20,20) BEFORE = %d,%d,%d\n", pre[0],pre[1],pre[2]);

    SemuTapState s; memset(&s,0,sizeof s);
    s.abi=SEMU_TAP_ABI; s.active=1; s.fb_width=FBW; s.fb_height=FBH;
    s.content_x=CX; s.content_y=CY; s.content_w=CW; s.content_h=CH;
    s.native_w=640; s.native_h=448; s.origin=SEMU_TAP_ORIGIN_BOTTOM_LEFT;

    // Real emulators present continuously; the tap lazy-inits its GL + loads the bezel
    // texture on the first frame and may fade art in. Drive several frames like an
    // emulator would, re-rendering the game each time, before we read the result back.
    int NFRAMES = 12;
    { const char* nf=getenv("EMU_FRAMES"); if(nf&&atoi(nf)>0) NFRAMES=atoi(nf); }
    for(int fr=0; fr<NFRAMES; fr++){
        glDrawBuffer(GL_BACK);
        glViewport(0,0,FBW,FBH);
        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST); glScissor(CX,CY,CW,CH);
        glClearColor(0.90f,0.10f,0.80f,1.0f); glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST); glFinish();
        semu_tap_report_safe(&s);
        glXSwapBuffers(dpy,win);   // tap-out: intercepted -> composite bezel+shader
        glFinish();
    }

    // Capture what was actually PRESENTED to the window (robust: avoids the GL
    // front/back ambiguity under software GLX). XGetImage reads the composited pixmap.
    XSync(dpy,False);
    XImage* img = XGetImage(dpy,win,0,0,FBW,FBH,AllPlanes,ZPixmap);
    if(!img){ fprintf(stderr,"glx_emu: XGetImage failed\n"); return 2; }
    unsigned long rm=vi->red_mask, gm=vi->green_mask, bm=vi->blue_mask;
    int rs=0,gs=0,bs=0; { unsigned long m; for(m=rm;m&&!(m&1);m>>=1)rs++; for(m=gm;m&&!(m&1);m>>=1)gs++; for(m=bm;m&&!(m&1);m>>=1)bs++; }
    unsigned char* px=malloc((size_t)FBW*FBH*3);
    for(int y=0;y<FBH;y++) for(int x=0;x<FBW;x++){
        unsigned long p=XGetPixel(img,x,y);
        px[(y*FBW+x)*3+0]=(unsigned char)((p&rm)>>rs);
        px[(y*FBW+x)*3+1]=(unsigned char)((p&gm)>>gs);
        px[(y*FBW+x)*3+2]=(unsigned char)((p&bm)>>bs);
    }
    int bi=(20*FBW+20)*3;
    fprintf(stderr,"glx_emu: border(20,20) AFTER  = %d,%d,%d\n", px[bi],px[bi+1],px[bi+2]);

    // Metric: the OUTER EDGE BAND (within M px of the framebuffer edge) is always
    // outside the game hole, so it isolates the bezel frame from the game. With art
    // it is the TV-frame (non-black); with art off the tap paints it pure black.
    const int M=40;
    long band=0,lit=0;
    for(int y=0;y<FBH;y++) for(int x=0;x<FBW;x++){
        if(!(x<M||x>=FBW-M||y<M||y>=FBH-M)) continue;
        band++; int i=(y*FBW+x)*3;
        if(px[i]>8||px[i+1]>8||px[i+2]>8) lit++;
    }
    double frac=band?(double)lit/band:0.0;
    fprintf(stderr,"glx_emu: OUTER-BAND non-black fraction = %.3f (%ld/%ld)\n", frac,lit,band);

    FILE* f=fopen(outpath,"wb");
    if(f){ fprintf(f,"P6\n%d %d\n255\n",FBW,FBH);
        fwrite(px,1,(size_t)FBW*FBH*3,f); fclose(f);
        fprintf(stderr,"glx_emu: wrote %s\n",outpath); }

    int pass=(frac>0.90);   // bezel present => outer frame ~fully painted with art
    fprintf(stderr,"glx_emu: %s\n", pass?"PASS (bezel composited over GL frame)":"FAIL (outer frame not bezel — no art composited)");
    return pass?0:1;
}
