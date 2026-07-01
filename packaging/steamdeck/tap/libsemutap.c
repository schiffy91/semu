// libsemutap.c — Semu compositor, OpenGL back end (the GL "tap").
//
// The emulator (built from source) reports its exact content rect + active state via
// semu_tap_report (semu_tap.h). At glXSwapBuffers we composite from that report ONLY —
// no pixel inspection.
//
// GEOMETRY (v8 — the model the user has insisted on):
//   1. GAME = the LARGEST INTEGER scale of the core's native resolution that fits the
//      screen (both axes), CENTERED. Never shrunk to fit the bezel.
//   2. BEZEL ART is mapped so its screen-HOLE (SEMU_TAP_SCREEN, normalized rect in the
//      art) lands exactly on the integer game rect. The art scales with the game and its
//      outer edges simply clip off-screen if the game is large. No cutout PNG, no z-layer.
//
// Env: SEMU_TAP_NATIVE_W/H (integer-scale base, per system), SEMU_TAP_ART (bezel PNG),
//      SEMU_TAP_SCREEN (x,y,w,h norm, TOP-LEFT: the art's screen hole), SEMU_TAP_STYLE/SHELL,
//      SEMU_TAP_MASK/SCAN/CORNER, SEMU_TAP_DISABLE.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "vendor/stb_image.h"
#define SEMU_TAP_NO_HELPER
#include "semu_tap.h"
#include "tap_geometry.h"

static SemuTapState g_state; static int g_have; static int win_w, win_h;
__attribute__((visibility("default"), used))
void semu_tap_report(const SemuTapState* s) { if (s && s->abi == SEMU_TAP_ABI) { g_state = *s; g_have = 1; } }

typedef unsigned int GLenum; typedef int GLint; typedef int GLsizei; typedef unsigned int GLuint;
typedef unsigned int GLbitfield; typedef float GLfloat; typedef float GLclampf; typedef unsigned char GLboolean;
typedef char GLchar; typedef long GLsizeiptr; typedef long GLintptr;

#define GL_VIEWPORT 0x0BA2
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GLX_WIDTH 0x801D
#define GLX_HEIGHT 0x801E

typedef void (*PFN_swap)(void*, unsigned long);
typedef unsigned int (*PFN_eglswap)(void*, void*);   // EGLBoolean eglSwapBuffers(EGLDisplay, EGLSurface)
typedef void (*PFN_query)(void*, unsigned long, int, unsigned int*);
typedef void (*PFN_getiv)(GLenum, GLint*);
typedef void (*PFN_gentex)(GLsizei, GLuint*);
typedef void (*PFN_bindtex)(GLenum, GLuint);
typedef void (*PFN_teximg)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
typedef void (*PFN_copytex)(GLenum,GLint,GLenum,GLint,GLint,GLsizei,GLsizei,GLint);
typedef void (*PFN_texpar)(GLenum,GLenum,GLint);
typedef void (*PFN_active)(GLenum);
typedef void (*PFN_genva)(GLsizei,GLuint*);
typedef void (*PFN_bindva)(GLuint);
typedef GLuint (*PFN_createsh)(GLenum);
typedef void (*PFN_shsrc)(GLuint,GLsizei,const GLchar* const*,const GLint*);
typedef void (*PFN_compsh)(GLuint);
typedef void (*PFN_shiv)(GLuint,GLenum,GLint*);
typedef void (*PFN_shlog)(GLuint,GLsizei,GLsizei*,GLchar*);
typedef GLuint (*PFN_createpr)(void);
typedef void (*PFN_attach)(GLuint,GLuint);
typedef void (*PFN_link)(GLuint);
typedef void (*PFN_priv)(GLuint,GLenum,GLint*);
typedef void (*PFN_use)(GLuint);
typedef GLint (*PFN_uloc)(GLuint,const GLchar*);
typedef void (*PFN_u1i)(GLint,GLint);
typedef void (*PFN_u2f)(GLint,GLfloat,GLfloat);
typedef void (*PFN_u3f)(GLint,GLfloat,GLfloat,GLfloat);
typedef void (*PFN_u4f)(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
typedef void (*PFN_u1f)(GLint,GLfloat);
typedef void (*PFN_draw)(GLenum,GLint,GLsizei);
typedef void (*PFN_viewport)(GLint,GLint,GLsizei,GLsizei);
typedef void (*PFN_disable)(GLenum);
typedef void (*PFN_genmip)(GLenum);
typedef void (*PFN_pixstore)(GLenum, GLint);          // glPixelStorei
typedef int (*PFN_qkm)(void*, char*);                 // XQueryKeymap(Display*, char[32])
typedef unsigned char (*PFN_k2kc)(void*, unsigned long); // XKeysymToKeycode(Display*, KeySym)
typedef unsigned long (*PFN_s2ks)(const char*);       // XStringToKeysym(const char*)

static PFN_swap real_swap; static PFN_eglswap real_egl_swap; static PFN_query p_query; static PFN_getiv p_getiv;
static PFN_gentex p_gentex; static PFN_bindtex p_bindtex; static PFN_teximg p_teximg;
static PFN_copytex p_copytex; static PFN_texpar p_texpar; static PFN_active p_active;
static PFN_genva p_genva; static PFN_bindva p_bindva; static PFN_createsh p_createsh;
static PFN_shsrc p_shsrc; static PFN_compsh p_compsh; static PFN_shiv p_shiv; static PFN_shlog p_shlog;
static PFN_createpr p_createpr; static PFN_attach p_attach; static PFN_link p_link; static PFN_priv p_priv;
static PFN_use p_use; static PFN_uloc p_uloc; static PFN_u1i p_u1i; static PFN_u2f p_u2f;
static PFN_u4f p_u4f; static PFN_u3f p_u3f; static PFN_u1f p_u1f; static PFN_draw p_draw; static PFN_viewport p_viewport;
static PFN_disable p_disable; static PFN_genmip p_genmip; static PFN_pixstore p_pixstore;
static PFN_qkm p_qkm; static PFN_k2kc p_k2kc; static PFN_s2ks p_s2ks;

static int inited, disabled, gl_ready, tap_style = 0, env_nw = 0, env_nh = 0, debug_on = 0;
static int standalone = 0;   // standalone emulators (PCSX2/Cemu/Azahar/Ryujinx): no tap report -> synth state from the live frame
static int ov_l = 0, ov_r = 0, ov_t = 0, ov_b = 0;   // declared overscan crop, native px (L,R,T,B)
// Radial-driven live state (XQueryKeymap edge-detect). retro_on: soft(1)/sharp(0); priority_bezel: bezel(1)/game(0).
static int retro_on = 1, priority_bezel = 0;
static int bezel_idx = 0, shader_idx = 0, bezel_count = 1; // Phase 2: bezel cycle (idx==count -> OFF), shader 0..3
static int nds_layout = 0, nds_pri = 3, nds_sec = 3, dual_mode = 0; // Phase 3: layout(0=vert,1=horiz) + scale idx (def 2x)
static int fill_hole = 0;   // handhelds: fill the device's screen hole at display aspect (non-integer, Duimon HSM_NON_INTEGER_SCALE) instead of largest-integer
static int align_on = 0;    // alignment diagnostic: purple frame at the declared hole + translucent game, to check the game-vs-bezel fit
static char glass_paths[4][1024] = {{0}};           // per-variant screen-glass layer: .a = exact screen cutout mask, .rgb = glass/reflections
static float reflect_amt = 0.0f, curve_amt = 0.0f, screen_corner = 0.06f;  // reflection/curvature/CRT-corner params
// ---- Radial/overlay MENU (compositor-rendered; mirrors Steam Input). Fully isolated: only drawn when
// menu_on; the normal compositing path is untouched when closed. Text via a CPU-blitted font atlas. ----
static int menu_on = 0, menu_lvl = 0, menu_sel = 0;     // lvl: 0=root 1=rendering 2=save/load 3=per-system
static int save_slot = 0, wii_ctrl = 0;                 // save/load slot index, wii controller-layout index
static int sys_kind = 0;                                // 0=generic,1=dualscreen(nds/3ds),2=wii  (SEMU_TAP_SYSKIND)
static GLuint menu_tex = 0; static unsigned char *menu_buf = 0; static int menu_dirty = 1;
static const int MENU_W = 560, MENU_H = 720;
static unsigned char *atlas_buf = 0; static int atlas_w = 0, atlas_h = 0;   // font atlas (CPU RGBA)
static int GLY_W = 24, GLY_H = 48; static const int GLY_COLS = 16, GLY_ROWS = 6;   // atlas cell geometry (computed at load)
static char menu_font_path[1024] = "/home/deck/.local/share/semu-bezel/menu-font.png";
static float retro_lod = 1.6f; static char retro_key[32] = "Insert"; // kc 118; F13 is NOT in the Xwayland keymap
// multi-key radial. index->action: 0=retro 1=priority 2=bezel 3=shader 4=ndsLayout 5=ndsPri 6=ndsSec
static const char *knames[7] = { "Insert", "Home", "Prior", "Next", "End", "Pause", "Menu" };
static int kcode[7] = {0}, kprev[7] = {0}, kready = 0;
static float debug_thresh = 0.10f;
static float shell_r = 0.55f, shell_g = 0.56f, shell_b = 0.60f;
static float tv_mask = 0.30f, tv_scan = 0.22f, tv_corner = 0.05f, tv_bzlw = 0.55f;
static float has_art = 0.0f; static char art_paths[4][1024]; static int art_w, art_h;  // up to 4 bezel variants (a,b,c,d)
static int art_ws[4] = {0}, art_hs[4] = {0};  // per-variant device-art dims (variants can be different models -> diff aspect)
static float scr_x, scr_y, scr_w, scr_h;     // the art's screen HOLE (norm, top-left); variant 0
static float scr_rects[4][4];                // per-variant screen HOLE (variant b/c/d can be a different device model)
static float disp_aspect = 0.0f;             // display aspect (0 = use native square nw/nh)
static GLuint prog, vao, game_tex, bezel_texs[4], glass_texs[4];
static GLint uWin, uRect, uSrc, uBezelRect, uNative, uStyle, uShell, uTV, uRef, uHasArt, uDebug, uRetro, uShaderMode;
static GLint uRect2, uSrc2, uDual, uAlign, uHole, uGlass, uHasGlass, uReflect, uCurve, uScreenCorner, uMenuRect, uMenuOn;
static long frames;

static void logf_(const char *fmt, GLuint a, GLuint b, GLuint c) {
    FILE *f = fopen("/home/deck/semutap.log", "a"); if (f) { fprintf(f, fmt, a, b, c); fclose(f); }
}

static const char *VS =
 "#version 330 core\n"
 "void main(){ vec2 p=vec2((gl_VertexID==1)?3.0:-1.0,(gl_VertexID==2)?3.0:-1.0); gl_Position=vec4(p,0.0,1.0); }\n";

static const char *FS =
 "#version 330 core\n"
 "out vec4 frag;\n"
 "uniform sampler2D uGame; uniform sampler2D uBezel;\n"
 "uniform vec2 uWin; uniform vec4 uRect; uniform vec4 uSrc; uniform vec4 uBezelRect;\n"
 "uniform float uNative; uniform float uStyle; uniform float uHasArt; uniform vec3 uShell;\n"
 "uniform vec4 uTV;  /* y=mask z=scanlineDepth w=bezelWidthFrac */\n"
 "uniform vec4 uRef; /* z=halation w=cornerRadius */\n"
 "uniform float uDebug; /* 0=off; else luma threshold for glass-edge detect */\n"
 "uniform float uRetro; /* game-sample LOD: 0=sharp(native render), ~1.6=soft RETRO (downsample to ~240p) */\n"
 "uniform float uShaderMode; /* 0=full(scan+mask+halation) 1=scanline-only 2=mask-only/sharp 3=OFF(raw) */\n"
 "uniform vec4 uRect2; uniform vec4 uSrc2; uniform float uDual; /* nds/3ds: second screen rect+src; uDual>0.5 = dual */\n"
 "uniform float uAlign; uniform vec4 uHole; /* alignment diag: uHole = declared screen hole (norm within bezel art) */\n"
 "uniform sampler2D uGlass; uniform float uHasGlass; /* screen-glass layer: .a = screen MASK (real cutout shape), .rgb = glass */\n"
 "uniform float uReflect; /* reflection/glare strength */ uniform float uCurve; /* CRT barrel curvature (0 = flat) */ uniform float uScreenCorner; /* CRT rounded-mask corner radius */\n"
 "float sdRound(vec2 p, vec2 b, float r){ vec2 d=abs(p)-b+r; return length(max(d,vec2(0.0)))+min(max(d.x,d.y),0.0)-r; }\n"
 "vec3 gameAt(vec2 uv, float lod){ return textureLod(uGame,(uSrc.xy+uv*uSrc.zw)/uWin,lod).rgb; }\n"
 "vec3 artAt(vec2 sp){ vec2 auv=vec2((sp.x-uBezelRect.x)/uBezelRect.z, 1.0-(sp.y-uBezelRect.y)/uBezelRect.w); return texture(uBezel,auv).rgb; }\n"
 "vec4 glassAt(vec2 sp){ vec2 auv=vec2((sp.x-uBezelRect.x)/uBezelRect.z, 1.0-(sp.y-uBezelRect.y)/uBezelRect.w); return texture(uGlass,auv); }\n"
 "uniform sampler2D uMenu; uniform vec4 uMenuRect; uniform float uMenuOn; /* overlay menu (screen-px rect, GL y-up) */\n"
 "vec3 menuComposite(vec3 c, vec2 px){ if(uMenuOn>0.5 && px.x>=uMenuRect.x&&px.x<=uMenuRect.x+uMenuRect.z&&px.y>=uMenuRect.y&&px.y<=uMenuRect.y+uMenuRect.w){ vec2 muv=vec2((px.x-uMenuRect.x)/uMenuRect.z, 1.0-(px.y-uMenuRect.y)/uMenuRect.w); vec4 m=texture(uMenu,muv); return mix(c,m.rgb,m.a);} return c; }\n"
 "void main(){\n"
 "  vec2 px=gl_FragCoord.xy;\n"
 "  if(uAlign>0.5){\n"                                            // ALIGNMENT DIAG: bezel + translucent game + purple hole frame
 "    vec3 bz = uHasArt>0.5 ? artAt(px) : vec3(0.05);\n"
 "    vec3 outc = bz;\n"
 "    if(px.x>=uRect.x&&px.x<=uRect.x+uRect.z&&px.y>=uRect.y&&px.y<=uRect.y+uRect.w){\n"  // translucent game @40%
 "      vec2 uv=(px-uRect.xy)/uRect.zw; vec3 g=textureLod(uGame,(uSrc.xy+uv*uSrc.zw)/uWin,0.0).rgb; outc=mix(bz,g,0.40);\n"
 "    }\n"
 "    float L=uBezelRect.x+uHole.x*uBezelRect.z, hw=uHole.z*uBezelRect.z;\n"             // declared hole -> screen rect (GL y up)
 "    float B=uBezelRect.y+(1.0-uHole.y-uHole.w)*uBezelRect.w, hh=uHole.w*uBezelRect.w;\n"
 "    float R=L+hw, T=B+hh, n=3.0;\n"
 "    bool fv=(abs(px.x-L)<n||abs(px.x-R)<n)&&px.y>=B-n&&px.y<=T+n;\n"
 "    bool fh=(abs(px.y-B)<n||abs(px.y-T)<n)&&px.x>=L-n&&px.x<=R+n;\n"
 "    if(fv||fh) outc=vec3(0.78,0.0,1.0);\n"                       // purple N-px hole frame, forefront
 "    frag=vec4(outc,1.0); return;\n"
 "  }\n"
 "  if(uDual>0.5){\n"                                              // nds/3ds: two independent screens
 "    vec3 oc = uHasArt>0.5 ? artAt(px) : vec3(0.02);\n"
 "    for(int s=0;s<2;s++){\n"
 "      vec4 R=(s==0)?uRect:uRect2; vec4 S=(s==0)?uSrc:uSrc2;\n"
 "      if(px.x>=R.x&&px.x<=R.x+R.z&&px.y>=R.y&&px.y<=R.y+R.w){\n"
 "        vec2 uv=(px-R.xy)/R.zw; vec3 g=textureLod(uGame,(S.xy+uv*S.zw)/uWin,uRetro).rgb;\n"
 "        if(uShaderMode<1.5){ float nx=uNative*R.z/R.w; g*=0.90+0.10*sqrt(abs(cos(uv.x*nx*3.14159265))*abs(cos(uv.y*uNative*3.14159265))); }\n"
 "        oc=g;\n"
 "      }\n"
 "    }\n"
 "    frag=vec4(menuComposite(oc,px),1.0); return;\n"
 "  }\n"
 "  vec2 cen=uRect.xy+uRect.zw*0.5; vec2 hf=uRect.zw*0.5;\n"
 "  vec2 c=(px-cen)/hf;\n"                                          // -1..1 across the game rect
 "  vec2 cc = c*(1.0 + uCurve*dot(c.yx,c.yx));\n"                   // CRT barrel curvature (uCurve=0 -> flat)
 "  vec2 uv = cc*0.5+0.5;\n"
 "  bool inRect = uv.x>=0.0&&uv.x<=1.0&&uv.y>=0.0&&uv.y<=1.0;\n"    // off the (curved) screen -> bezel
 // SCREEN CUTOUT MASK: the real screen shape, NOT a hardcoded round. Handhelds use the Glass layer's alpha
 // (exact rounded-corner device screen); CRTs use a rounded-rect SDF matching the tube. Anti-aliased.
 "  vec4 gl = uHasGlass>0.5 ? glassAt(px) : vec4(0.0);\n"
 "  float mask = uHasGlass>0.5 ? gl.a : (1.0 - smoothstep(-0.012,0.012, sdRound(cc, vec2(1.0), clamp(uScreenCorner,0.0,0.5))));\n"
 "  vec3 bez = uHasArt>0.5 ? artAt(px) : vec3(0.0);\n"
 "  if(uHasArt<0.5){ vec2 outd=max(abs(c)-1.0,vec2(0.0)); float d=length(outd); float bevel=1.0-clamp(d/uTV.w,0.0,1.0); bez=(uStyle<0.5)?mix(vec3(0.04),vec3(0.14),bevel*bevel):uShell*mix(0.78,1.06,bevel); }\n"
 "  vec3 outc = bez;\n"
 "  if(inRect && mask>0.003){\n"
 "    vec3 g=gameAt(uv,uRetro);\n"
 "    if(uShaderMode<2.5){\n"                                       // 3 = OFF (raw, no shader)
 "      if(uStyle<0.5){\n"                                          // CRT
 "        if(uShaderMode<1.5) g*=1.0-uTV.z*(1.0-abs(cos(uv.y*uNative*3.14159265)));\n"   // scanlines: modes 0,1
 "        if(uShaderMode<0.5||uShaderMode>1.5){ float m=mod(px.x,3.0); vec3 cmask=(m<1.0)?vec3(1.0,0.7,0.7):(m<2.0)?vec3(0.7,1.0,0.7):vec3(0.7,0.7,1.0); g*=mix(vec3(1.0),cmask,uTV.y); }\n" // mask: 0,2
 "        if(uShaderMode<0.5) g+=gameAt(uv,3.0)*uRef.z;\n"          // halation: mode 0
 "      } else {\n"                                                 // LCD
 "        if(uShaderMode<1.5){ float nx=uNative*uRect.z/uRect.w; g*=0.90+0.10*sqrt(abs(cos(uv.x*nx*3.14159265))*abs(cos(uv.y*uNative*3.14159265))); }\n" // grid: 0,1
 "      }\n"
 "    }\n"
 // REFLECTIONS: the glass layer's own glass tint/sheen (screen-blend = highlight, never darkens) + a soft
 // diagonal glare across the screen. Strength = uReflect (0 disables).
 "    if(uReflect>0.001){\n"
 "      if(uHasGlass>0.5) g = 1.0-(1.0-g)*(1.0-gl.rgb*gl.a*uReflect);\n"
 "      float glare = smoothstep(0.45,1.0, (1.0-uv.y)*0.7 + uv.x*0.45) * smoothstep(0.0,0.35,uv.y);\n"
 "      g += glare*uReflect*0.12;\n"
 "    }\n"
 "    outc = mix(bez, g, clamp(mask,0.0,1.0));\n"                   // anti-aliased screen cutout (mask edges)
 "  }\n"
 "  if(uDebug>0.0005){\n"                                           // DEBUG overlay
 "    float bw=2.5;\n"
 "    float L=uRect.x, R=uRect.x+uRect.z, B=uRect.y, T=uRect.y+uRect.w;\n"
 "    bool inF = px.x>=L-bw&&px.x<=R+bw&&px.y>=B-bw&&px.y<=T+bw;\n"
 "    bool inI = px.x>=L+bw&&px.x<=R-bw&&px.y>=B+bw&&px.y<=T-bw;\n"
 "    bool green = inF&&!inI;\n"                                     // GREEN = game rect (uRect)
 "    bool purple=false;\n"
 "    if(uHasArt>0.5){\n"                                           // PURPLE = the art's REAL glass edge (luma transition)
 "      vec3 wv=vec3(0.299,0.587,0.114);\n"
 "      float l0=dot(artAt(px),wv), lx=dot(artAt(px+vec2(2.0,0.0)),wv), ly=dot(artAt(px+vec2(0.0,2.0)),wv);\n"
 "      bool g0=l0<uDebug, gx=lx<uDebug, gy=ly<uDebug;\n"
 "      purple=(g0!=gx)||(g0!=gy);\n"
 "    }\n"
 "    if(purple) outc=vec3(0.80,0.0,1.0);\n"
 "    if(green)  outc=vec3(0.0,1.0,0.0);\n"
 "  }\n"
 "  frag=vec4(menuComposite(outc,px),1.0);\n"
 "}\n";

static GLuint mkshader(GLenum type, const char *src) {
    GLuint s = p_createsh(type); p_shsrc(s, 1, &src, 0); p_compsh(s);
    GLint ok = 0; p_shiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char buf[1024]={0}; if(p_shlog) p_shlog(s,1024,0,buf); FILE *f=fopen("/home/deck/semutap.log","a"); if(f){fprintf(f,"SHADER %u FAIL: %s\n",(unsigned)type,buf); fclose(f);} }
    return s;
}

static void gl_init(void) {
    if (gl_ready) return;
    p_active(GL_TEXTURE0);
    p_gentex(1,&game_tex); p_bindtex(GL_TEXTURE_2D,game_tex);
    p_texpar(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    p_texpar(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    p_texpar(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); p_texpar(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if (p_genva) { p_genva(1,&vao); }
    GLuint vs=mkshader(GL_VERTEX_SHADER,VS), fs=mkshader(GL_FRAGMENT_SHADER,FS);
    prog=p_createpr(); p_attach(prog,vs); p_attach(prog,fs); p_link(prog);
    GLint ok=0; p_priv(prog,GL_LINK_STATUS,&ok);
    uWin=p_uloc(prog,"uWin"); uRect=p_uloc(prog,"uRect"); uSrc=p_uloc(prog,"uSrc"); uBezelRect=p_uloc(prog,"uBezelRect");
    uNative=p_uloc(prog,"uNative"); uStyle=p_uloc(prog,"uStyle"); uShell=p_uloc(prog,"uShell");
    uTV=p_uloc(prog,"uTV"); uRef=p_uloc(prog,"uRef"); uHasArt=p_uloc(prog,"uHasArt"); uDebug=p_uloc(prog,"uDebug"); uRetro=p_uloc(prog,"uRetro"); uShaderMode=p_uloc(prog,"uShaderMode");
    uRect2=p_uloc(prog,"uRect2"); uSrc2=p_uloc(prog,"uSrc2"); uDual=p_uloc(prog,"uDual");
    uAlign=p_uloc(prog,"uAlign"); uHole=p_uloc(prog,"uHole");
    uGlass=p_uloc(prog,"uGlass"); uHasGlass=p_uloc(prog,"uHasGlass"); uReflect=p_uloc(prog,"uReflect"); uCurve=p_uloc(prog,"uCurve"); uScreenCorner=p_uloc(prog,"uScreenCorner");
    uMenuRect=p_uloc(prog,"uMenuRect"); uMenuOn=p_uloc(prog,"uMenuOn");
    GLint muni=p_uloc(prog,"uMenu");   // uMenu sampler -> unit 3 (set AFTER p_use below; glUniform needs the program active)
    p_gentex(1,&menu_tex); p_active(GL_TEXTURE3); p_bindtex(GL_TEXTURE_2D,menu_tex);   // menu texture (uploaded per state change)
    p_texpar(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); p_texpar(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    p_texpar(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); p_texpar(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE); p_active(GL_TEXTURE0);
    GLint guni=p_uloc(prog,"uGame"); GLint buni=p_uloc(prog,"uBezel");
    p_use(prog); if(guni>=0)p_u1i(guni,0); if(muni>=0)p_u1i(muni,3);   // uMenu MUST be bound after p_use, else it defaults to unit 0 (the game tex)
    { int loaded=0;
      for(int bi=0; bi<4; bi++){
        if(!art_paths[bi][0]) continue;
        int aw=0,ah=0,n=0; unsigned char *img=stbi_load(art_paths[bi],&aw,&ah,&n,4);
        if(img){
            p_gentex(1,&bezel_texs[bi]); p_active(GL_TEXTURE1); p_bindtex(GL_TEXTURE_2D,bezel_texs[bi]);
            // RetroArch leaves GL_UNPACK_* state set; without resetting it, our CPU->GPU upload reads the
            // image buffer with the wrong stride -> sheared/striped texture. Reset to tightly-packed rows.
            if(p_pixstore){ p_pixstore(GL_UNPACK_ALIGNMENT,1); p_pixstore(GL_UNPACK_ROW_LENGTH,0); p_pixstore(GL_UNPACK_SKIP_PIXELS,0); p_pixstore(GL_UNPACK_SKIP_ROWS,0); }
            p_teximg(GL_TEXTURE_2D,0,GL_RGBA,aw,ah,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
            if(p_genmip) p_genmip(GL_TEXTURE_2D);   // clean minification (portrait shells downscale ~2.5x)
            p_texpar(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,p_genmip?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR); p_texpar(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            p_texpar(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); p_texpar(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            stbi_image_free(img); art_ws[bi]=aw; art_hs[bi]=ah; if(bi==0){ art_w=aw; art_h=ah; } loaded++;
            logf_("art%u w=%u tex=%u\n",(GLuint)bi,(GLuint)aw,(GLuint)bezel_texs[bi]);
        } else { logf_("art%u load FAILED\n",(GLuint)bi,0,0); }
      }
      bezel_count = loaded; if(loaded>0) has_art=1.0f; p_active(GL_TEXTURE0);
    }
    for(int gi=0; gi<4; gi++){   // per-variant screen-glass layer (unit 2): .a = exact screen cutout mask, .rgb = glass/reflections
        if(!glass_paths[gi][0]) continue;
        int gw=0,gh=0,gn=0; unsigned char *gimg=stbi_load(glass_paths[gi],&gw,&gh,&gn,4);
        if(gimg){
            p_gentex(1,&glass_texs[gi]); p_active(GL_TEXTURE2); p_bindtex(GL_TEXTURE_2D,glass_texs[gi]);
            if(p_pixstore){ p_pixstore(GL_UNPACK_ALIGNMENT,1); p_pixstore(GL_UNPACK_ROW_LENGTH,0); p_pixstore(GL_UNPACK_SKIP_PIXELS,0); p_pixstore(GL_UNPACK_SKIP_ROWS,0); }
            p_teximg(GL_TEXTURE_2D,0,GL_RGBA,gw,gh,0,GL_RGBA,GL_UNSIGNED_BYTE,gimg);
            if(p_genmip) p_genmip(GL_TEXTURE_2D);
            p_texpar(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,p_genmip?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR); p_texpar(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            p_texpar(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); p_texpar(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            stbi_image_free(gimg); p_active(GL_TEXTURE0);
            logf_("glass%u %ux%u tex=%u\n",(GLuint)gi,(GLuint)gw,(GLuint)glass_texs[gi]);
        } else { logf_("glass%u load FAILED\n",(GLuint)gi,0,0); }
    }
    { GLint gluni=p_uloc(prog,"uGlass"); if(gluni>=0)p_u1i(gluni,2); }   // uGlass sampler -> unit 2
    if(buni>=0)p_u1i(buni, has_art>0.5f ? 1 : 0);
    logf_("gl_init prog=%u link=%u hasArt=%u\n", prog, (GLuint)ok, (GLuint)(has_art>0.5f?1:0));
    gl_ready = 1;
}

// ================= Radial/overlay MENU: CPU renderer into menu_buf (no GL here) + nav/actions =================
static void mb_fill(int x,int y,int w,int h,int r,int g,int b,int a){
    if(!menu_buf)return;
    for(int j=0;j<h;j++){int yy=y+j;if(yy<0||yy>=MENU_H)continue;
      for(int i=0;i<w;i++){int xx=x+i;if(xx<0||xx>=MENU_W)continue;
        unsigned char*p=menu_buf+((size_t)yy*MENU_W+xx)*4;float af=a/255.0f,ia=1.0f-af;
        p[0]=(unsigned char)(r*af+p[0]*ia);p[1]=(unsigned char)(g*af+p[1]*ia);p[2]=(unsigned char)(b*af+p[2]*ia);p[3]=(unsigned char)(a+p[3]*ia);}}
}
static void mb_glyph(int x,int y,char c,int r,int g,int b){
    if(!menu_buf||!atlas_buf)return;if(c<32||c>127)c='?';
    int idx=c-32,col=idx%GLY_COLS,row=idx/GLY_COLS,sx=col*GLY_W,sy=row*GLY_H;
    for(int j=0;j<GLY_H;j++){int yy=y+j;if(yy<0||yy>=MENU_H)continue;int ay=sy+j;if(ay>=atlas_h)continue;
      for(int i=0;i<GLY_W;i++){int xx=x+i;if(xx<0||xx>=MENU_W)continue;int ax=sx+i;if(ax>=atlas_w)continue;
        int lum=atlas_buf[((size_t)ay*atlas_w+ax)*4+3];if(lum<16)continue;float af=lum/255.0f,ia=1.0f-af;
        unsigned char*p=menu_buf+((size_t)yy*MENU_W+xx)*4;
        p[0]=(unsigned char)(r*af+p[0]*ia);p[1]=(unsigned char)(g*af+p[1]*ia);p[2]=(unsigned char)(b*af+p[2]*ia);p[3]=(unsigned char)(255*af+p[3]*ia);}}
}
static void mb_text(int x,int y,const char*s,int r,int g,int b){int cx=x;for(;*s;s++){mb_glyph(cx,y,*s,r,g,b);cx+=GLY_W-6;}}
static void mb_disc(int cx,int cy,int rad,int r,int g,int b,int a){ if(!menu_buf)return;
    for(int j=-rad;j<=rad;j++){int yy=cy+j;if(yy<0||yy>=MENU_H)continue; for(int i=-rad;i<=rad;i++){ if(i*i+j*j>rad*rad)continue; int xx=cx+i;if(xx<0||xx>=MENU_W)continue;
        unsigned char*p=menu_buf+((size_t)yy*MENU_W+xx)*4;float af=a/255.0f,ia=1.0f-af;
        p[0]=(unsigned char)(r*af+p[0]*ia);p[1]=(unsigned char)(g*af+p[1]*ia);p[2]=(unsigned char)(b*af+p[2]*ia);p[3]=(unsigned char)(a+p[3]*ia);}}}
static int menu_count(void){   // number of wedges at the current level
    if(menu_lvl==0) return 2 + ((sys_kind==1||sys_kind==2)?1:0);
    if(menu_lvl==1) return 4; if(menu_lvl==2) return 4;
    if(menu_lvl==3) return (sys_kind==1?4:sys_kind==2?2:1);
    return 1;
}
static void menu_short(int i,char*o){ o[0]=0;
    if(menu_lvl==0){ const char*a[3]={"Rendering","Save/Load",(sys_kind==2?"Controls":"Screens")}; if(i<3)strcpy(o,a[i]); }
    else if(menu_lvl==1){ const char*a[4]={"Aspect","Bezel","Shader","Back"}; if(i<4)strcpy(o,a[i]); }
    else if(menu_lvl==2){ const char*a[4]={"Slot","Save","Load","Back"}; if(i<4)strcpy(o,a[i]); }
    else if(menu_lvl==3){ if(sys_kind==1){const char*a[4]={"Layout","Top","Bottom","Back"};if(i<4)strcpy(o,a[i]);}
        else if(sys_kind==2){const char*a[2]={"Pad","Back"};if(i<2)strcpy(o,a[i]);} else strcpy(o,"Back"); }
}
static void menu_val(int i,char*o){ o[0]=0;
    if(menu_lvl==1){ if(i==0)strcpy(o,priority_bezel?"Bezel-priority":"Game-priority");
        else if(i==1)strcpy(o,bezel_idx>=bezel_count?"Off":bezel_idx==0?"A":bezel_idx==1?"B":"C");
        else if(i==2)strcpy(o,shader_idx==3?"Off":shader_idx==0?"A":shader_idx==1?"B":"C"); }
    else if(menu_lvl==2){ if(i==0)snprintf(o,16,"%d",save_slot+1); }
    else if(menu_lvl==3){ const char*sc[5]={"0.25x","0.5x","1x","2x","3x"};
        if(sys_kind==1){ if(i==0)strcpy(o,nds_layout?"Horizontal":"Vertical"); else if(i==1)strcpy(o,sc[nds_pri>4?4:nds_pri]); else if(i==2)strcpy(o,sc[nds_sec>4?4:nds_sec]); }
        else if(sys_kind==2){ const char*wc[4]={"Wiimote","Wiimote+Nunchuk","Pro Controller","Classic"}; if(i==0)strcpy(o,wc[wii_ctrl>3?3:wii_ctrl]); } }
}
static void menu_persist(void){   // menu state IS the override files (single source of truth; compositor re-reads them each frame)
    FILE*f; if((f=fopen("/home/deck/semu-priority","w"))){fputc(priority_bezel?'b':'g',f);fclose(f);}
    if((f=fopen("/home/deck/semu-bezel","w"))){fprintf(f,"%d",bezel_idx);fclose(f);}
    if((f=fopen("/home/deck/semu-shader","w"))){fprintf(f,"%d",shader_idx);fclose(f);}
    if((f=fopen("/home/deck/semu-ndslayout","w"))){fputc(nds_layout?'1':'0',f);fclose(f);}
    if((f=fopen("/home/deck/semu-ndspri","w"))){fprintf(f,"%d",nds_pri);fclose(f);}
    if((f=fopen("/home/deck/semu-ndssec","w"))){fprintf(f,"%d",nds_sec);fclose(f);}
    if((f=fopen("/home/deck/semu-wiictrl","w"))){fprintf(f,"%d",wii_ctrl);fclose(f);}
}
static void menu_build(void){   // RADIAL: wedges (short labels) around a hub; hub shows level + selected value
    if(!menu_buf) menu_buf=(unsigned char*)calloc((size_t)MENU_W*MENU_H,4);
    if(!menu_buf) return; memset(menu_buf,0,(size_t)MENU_W*MENU_H*4);
    int cx=MENU_W/2, cy=MENU_H/2; int R=(int)(((MENU_W<MENU_H)?MENU_W:MENU_H)*0.34);
    mb_disc(cx,cy,R+72, 10,12,18, 232);                 // wheel backdrop
    int n=menu_count(); int cw=GLY_W-6;
    for(int i=0;i<n;i++){ double ang=-1.5707963+i*6.2831853/(double)n; int ix=cx+(int)(R*cos(ang)), iy=cy+(int)(R*sin(ang));
        char s[48]; menu_short(i,s); int tw=(int)strlen(s)*cw;
        if(i==menu_sel) mb_disc(ix,iy,48, 60,140,220,255);
        mb_text(ix-tw/2, iy-GLY_H/2, s, i==menu_sel?255:205, i==menu_sel?255:205, i==menu_sel?255:205); }
    mb_disc(cx,cy,86, 26,72,132,255);                   // center hub
    const char*titles[4]={"SEMU","RENDER","SAVE","SYSTEM"}; char tt[48]; strcpy(tt,titles[menu_lvl<4?menu_lvl:0]);
    mb_text(cx-(int)strlen(tt)*cw/2, cy-GLY_H, tt, 255,255,255);
    char sn[48],sv[48]; menu_short(menu_sel,sn); menu_val(menu_sel,sv);
    char line[64]; snprintf(line,64,"%s", sv[0]?sv:sn);
    mb_text(cx-(int)strlen(line)*cw/2, cy+4, line, 255,228,120);
    menu_dirty=0;
}
static void menu_activate(void){
    if(menu_lvl==0){ menu_lvl=(menu_sel==0)?1:(menu_sel==1)?2:3; menu_sel=0; }
    else if(menu_lvl==1){ if(menu_sel==0)priority_bezel=!priority_bezel; else if(menu_sel==1)bezel_idx=(bezel_idx+1)%(bezel_count+1);
        else if(menu_sel==2)shader_idx=(shader_idx+1)%4; else {menu_lvl=0;menu_sel=0;} }
    else if(menu_lvl==2){ if(menu_sel==0)save_slot=(save_slot+1)%3;
        else if(menu_sel==1){FILE*f=fopen("/home/deck/semu-savestate","w");if(f){fprintf(f,"save:%d",save_slot+1);fclose(f);}}
        else if(menu_sel==2){FILE*f=fopen("/home/deck/semu-savestate","w");if(f){fprintf(f,"load:%d",save_slot+1);fclose(f);}}
        else {menu_lvl=0;menu_sel=0;} }
    else if(menu_lvl==3){ if(sys_kind==1){ if(menu_sel==0)nds_layout=!nds_layout; else if(menu_sel==1)nds_pri=(nds_pri+1)%5;
            else if(menu_sel==2)nds_sec=(nds_sec+1)%5; else {menu_lvl=0;menu_sel=0;} }
        else if(sys_kind==2){ if(menu_sel==0)wii_ctrl=(wii_ctrl+1)%4; else {menu_lvl=0;menu_sel=0;} }
        else {menu_lvl=0;menu_sel=0;} }
    menu_persist(); menu_dirty=1;
}
// dlsym interposition: SDL/emulators resolve glXSwapBuffers/eglSwapBuffers via dlopen+dlsym, which bypasses
// plain LD_PRELOAD symbol interposition. So we ALSO export dlsym and hand back OUR hooks for those two names.
// Internally we must use the REAL dlsym (via dlvsym) to avoid infinite recursion.
// forward decls of the present hooks (defined after tap_init)
void glXSwapBuffers(void *dpy, unsigned long drawable);
unsigned int eglSwapBuffers(void *dpy, void *surface);
typedef void* (*PFN_dlsym)(void*, const char*);
static PFN_dlsym real_dlsym = 0;
static void ensure_real_dlsym(void){
    if(real_dlsym) return;
    real_dlsym = (PFN_dlsym)dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.34");
    if(!real_dlsym) real_dlsym = (PFN_dlsym)dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
}
void *dlsym(void *handle, const char *name){
    ensure_real_dlsym();
    if(name){
        if(!strcmp(name,"glXSwapBuffers")){ if(!real_swap)     real_swap=(PFN_swap)real_dlsym(handle,name);     return (void*)glXSwapBuffers; }
        if(!strcmp(name,"eglSwapBuffers")){ if(!real_egl_swap) real_egl_swap=(PFN_eglswap)real_dlsym(handle,name); return (void*)eglSwapBuffers; }
    }
    return real_dlsym ? real_dlsym(handle,name) : 0;
}
#define LD(v,n) do{ ensure_real_dlsym(); v=(void*)(real_dlsym?real_dlsym(RTLD_NEXT,n):0); }while(0)
static void tap_init(void) {
    if (inited) return; inited = 1;
    LD(real_swap,"glXSwapBuffers"); LD(real_egl_swap,"eglSwapBuffers"); LD(p_query,"glXQueryDrawable"); LD(p_getiv,"glGetIntegerv");
    LD(p_gentex,"glGenTextures"); LD(p_bindtex,"glBindTexture"); LD(p_teximg,"glTexImage2D");
    LD(p_copytex,"glCopyTexImage2D"); LD(p_texpar,"glTexParameteri"); LD(p_active,"glActiveTexture");
    LD(p_genva,"glGenVertexArrays"); LD(p_bindva,"glBindVertexArray"); LD(p_createsh,"glCreateShader");
    LD(p_shsrc,"glShaderSource"); LD(p_compsh,"glCompileShader"); LD(p_shiv,"glGetShaderiv"); LD(p_shlog,"glGetShaderInfoLog");
    LD(p_createpr,"glCreateProgram"); LD(p_attach,"glAttachShader"); LD(p_link,"glLinkProgram"); LD(p_priv,"glGetProgramiv");
    LD(p_use,"glUseProgram"); LD(p_uloc,"glGetUniformLocation"); LD(p_u1i,"glUniform1i"); LD(p_u2f,"glUniform2f");
    LD(p_u4f,"glUniform4f"); LD(p_u3f,"glUniform3f"); LD(p_u1f,"glUniform1f"); LD(p_draw,"glDrawArrays"); LD(p_viewport,"glViewport");
    LD(p_disable,"glDisable"); LD(p_genmip,"glGenerateMipmap"); LD(p_pixstore,"glPixelStorei");
    p_qkm=(PFN_qkm)dlsym(RTLD_DEFAULT,"XQueryKeymap");          // RA has libX11 loaded
    p_k2kc=(PFN_k2kc)dlsym(RTLD_DEFAULT,"XKeysymToKeycode");
    p_s2ks=(PFN_s2ks)dlsym(RTLD_DEFAULT,"XStringToKeysym");
    const char *rl=getenv("SEMU_RETRO_LOD"); if(rl) retro_lod=(float)atof(rl);
    const char *rk=getenv("SEMU_RETRO_KEY"); if(rk&&rk[0]){ strncpy(retro_key,rk,31); retro_key[31]=0; }
    const char *rstart=getenv("SEMU_RETRO_START"); if(rstart&&rstart[0]=='0') retro_on=0;   // default 1 (retro/soft)
    const char *pri=getenv("SEMU_TAP_PRIORITY"); if(pri&&(pri[0]=='b'||pri[0]=='B')) priority_bezel=1; // bezel|game
    const char *dl=getenv("SEMU_TAP_DUAL"); if(dl&&dl[0]=='1') dual_mode=1;   // nds/3ds: split into two screens
    const char *fh=getenv("SEMU_TAP_FILL"); if(fh&&fh[0]=='1') fill_hole=1;   // handhelds: fill screen hole at aspect
    const char *aln=getenv("SEMU_TAP_ALIGN"); if(aln&&aln[0]=='1') align_on=1; // alignment diagnostic overlay
    const char *gp=getenv("SEMU_TAP_GLASS");   if(gp&&gp[0]){  strncpy(glass_paths[0],gp,1023);  glass_paths[0][1023]=0; }   // per-variant screen mask + reflections
    const char *gpb=getenv("SEMU_TAP_GLASS_B"); if(gpb&&gpb[0]){ strncpy(glass_paths[1],gpb,1023); glass_paths[1][1023]=0; }
    const char *gpc=getenv("SEMU_TAP_GLASS_C"); if(gpc&&gpc[0]){ strncpy(glass_paths[2],gpc,1023); glass_paths[2][1023]=0; }
    const char *gpd=getenv("SEMU_TAP_GLASS_D"); if(gpd&&gpd[0]){ strncpy(glass_paths[3],gpd,1023); glass_paths[3][1023]=0; }
    const char *rf=getenv("SEMU_TAP_REFLECT"); if(rf) reflect_amt=(float)atof(rf);                                // glass reflection/glare strength
    const char *cv=getenv("SEMU_TAP_CURVE");   if(cv) curve_amt=(float)atof(cv);                                  // CRT barrel curvature (0 = flat)
    const char *cn=getenv("SEMU_TAP_CORNER");  if(cn) screen_corner=(float)atof(cn);                              // CRT procedural-mask corner radius (non-integer)
    const char *enh=getenv("SEMU_TAP_NATIVE_H"); if(enh&&atoi(enh)>0) env_nh=atoi(enh);
    const char *enw=getenv("SEMU_TAP_NATIVE_W"); if(enw&&atoi(enw)>0) env_nw=atoi(enw);
    const char *es=getenv("SEMU_TAP_STYLE"); if(es&&(es[0]=='h'||es[0]=='H'||es[0]=='1')) tap_style=1;
    const char *esh=getenv("SEMU_TAP_SHELL"); if(esh){ float r=0,g=0,b=0; if(sscanf(esh,"%f,%f,%f",&r,&g,&b)==3){ shell_r=r; shell_g=g; shell_b=b; } }
    const char *gv;
    if((gv=getenv("SEMU_TAP_MASK")))   tv_mask=(float)atof(gv);
    if((gv=getenv("SEMU_TAP_SCAN")))   tv_scan=(float)atof(gv);
    if((gv=getenv("SEMU_TAP_CORNER"))) tv_corner=(float)atof(gv);
    const char *ar=getenv("SEMU_TAP_ART");   if(ar&&ar[0]){   strncpy(art_paths[0],ar,1023);  art_paths[0][1023]=0; }
    const char *arb=getenv("SEMU_TAP_ART_B"); if(arb&&arb[0]){ strncpy(art_paths[1],arb,1023); art_paths[1][1023]=0; }
    const char *arc=getenv("SEMU_TAP_ART_C"); if(arc&&arc[0]){ strncpy(art_paths[2],arc,1023); art_paths[2][1023]=0; }
    const char *ard=getenv("SEMU_TAP_ART_D"); if(ard&&ard[0]){ strncpy(art_paths[3],ard,1023); art_paths[3][1023]=0; }
    const char *sc=getenv("SEMU_TAP_SCREEN"); if(sc){ sscanf(sc,"%f,%f,%f,%f",&scr_x,&scr_y,&scr_w,&scr_h); }
    // Per-variant screen holes: variant b/c/d may be a DIFFERENT device model (e.g. GBA SP clamshell) with
    // its own screen position. Default each variant to the base hole; override from SEMU_TAP_SCREEN_B/_C/_D.
    for(int v=0;v<4;v++){ scr_rects[v][0]=scr_x; scr_rects[v][1]=scr_y; scr_rects[v][2]=scr_w; scr_rects[v][3]=scr_h; }
    const char *scb=getenv("SEMU_TAP_SCREEN_B"); if(scb) sscanf(scb,"%f,%f,%f,%f",&scr_rects[1][0],&scr_rects[1][1],&scr_rects[1][2],&scr_rects[1][3]);
    const char *scc=getenv("SEMU_TAP_SCREEN_C"); if(scc) sscanf(scc,"%f,%f,%f,%f",&scr_rects[2][0],&scr_rects[2][1],&scr_rects[2][2],&scr_rects[2][3]);
    const char *scd=getenv("SEMU_TAP_SCREEN_D"); if(scd) sscanf(scd,"%f,%f,%f,%f",&scr_rects[3][0],&scr_rects[3][1],&scr_rects[3][2],&scr_rects[3][3]);
    const char *as=getenv("SEMU_TAP_ASPECT"); if(as){ float aw=0,ah=0; if(sscanf(as,"%f:%f",&aw,&ah)==2&&ah>0.0f) disp_aspect=aw/ah; else { float v=(float)atof(as); if(v>0.01f) disp_aspect=v; } }
    const char *dbg=getenv("SEMU_TAP_DEBUG"); if(dbg){ float v=(float)atof(dbg); if(v>0.0f){ debug_on=1; debug_thresh=(v>=1.0f)?0.10f:v; } }
    const char *ov=getenv("SEMU_TAP_OVERSCAN"); if(ov){ sscanf(ov,"%d,%d,%d,%d",&ov_l,&ov_r,&ov_t,&ov_b); }  // native px L,R,T,B
    if (getenv("SEMU_TAP_DISABLE")) disabled=1;
    if (getenv("SEMU_TAP_STANDALONE")) standalone=1;   // full-frame mode for non-RA emulators
    const char *skd=getenv("SEMU_TAP_SYSKIND"); if(skd) sys_kind=atoi(skd);   // 0 generic, 1 dual-screen, 2 wii
    const char *mfp=getenv("SEMU_TAP_MENUFONT"); if(mfp&&mfp[0]){ strncpy(menu_font_path,mfp,1023); menu_font_path[1023]=0; }
    { int aw=0,ah=0,an=0; unsigned char*ab=stbi_load(menu_font_path,&aw,&ah,&an,4); if(ab){ atlas_buf=ab; atlas_w=aw; atlas_h=ah; if(GLY_COLS>0)GLY_W=aw/GLY_COLS; if(GLY_ROWS>0)GLY_H=ah/GLY_ROWS; } }
    FILE *f=fopen("/home/deck/semutap.log","w");
    if(f){ fprintf(f,"semutap v8 envNative=%dx%d art=%s hole=%.3f,%.3f,%.3f,%.3f\n",env_nw,env_nh,art_paths[0][0]?art_paths[0]:"(none)",scr_x,scr_y,scr_w,scr_h); fclose(f); }
}

static void tap_frame(void *dpy, unsigned long drawable, int is_egl) {
    tap_init();
    static int swaptype_logged=0;
    if(!swaptype_logged && !disabled){ swaptype_logged=1; FILE *st=fopen("/home/deck/semu-swaptype.log","a"); if(st){ fprintf(st,"%s swap first-call standalone=%d\n", is_egl?"EGL":"GLX", standalone); fclose(st);} }
    if (standalone && !disabled) {   // synth a tap report from the live framebuffer (the standalone emulator renders the game full-screen)
        unsigned int W=0,H=0;
        if(!is_egl && p_query){ p_query(dpy,drawable,GLX_WIDTH,&W); p_query(dpy,drawable,GLX_HEIGHT,&H); }
        if((W==0||H==0)&&p_getiv){ GLint vp[4]={0,0,0,0}; p_getiv(GL_VIEWPORT,vp); W=(unsigned)vp[2]; H=(unsigned)vp[3]; }
        if(W>0 && H>0){
            float asp = disp_aspect>0.01f ? disp_aspect : (float)W/(float)H;   // game display aspect (per system)
            int cw=(int)W, ch=(int)H;                                          // content = aspect-fit CENTER (crops the emu letterbox)
            if((float)W/(float)H > asp){ ch=(int)H; cw=(int)((float)ch*asp+0.5f); } else { cw=(int)W; ch=(int)((float)cw/asp+0.5f); }
            g_state.abi=SEMU_TAP_ABI; g_state.active=1; g_state.rotation=0;
            g_state.fb_width=(int)W; g_state.fb_height=(int)H;
            g_state.content_x=((int)W-cw)/2; g_state.content_y=((int)H-ch)/2; g_state.content_w=cw; g_state.content_h=ch;
            g_state.native_w = env_nw>0?env_nw:cw; g_state.native_h = env_nh>0?env_nh:ch;
            g_state.origin=SEMU_TAP_ORIGIN_TOP_LEFT; g_have=1;
        }
    }
    if (!disabled && p_copytex && p_createsh && p_draw && p_use && g_have && g_state.active && g_state.abi == SEMU_TAP_ABI) {
        int w = g_state.fb_width, h = g_state.fb_height;
        if (w <= 0 || h <= 0) {
            unsigned int W=0,H=0;
            if(!is_egl && p_query){ p_query(dpy,drawable,GLX_WIDTH,&W); p_query(dpy,drawable,GLX_HEIGHT,&H); }
            if((W==0||H==0)&&p_getiv){ GLint vp[4]={0,0,0,0}; p_getiv(GL_VIEWPORT,vp); W=vp[2]; H=vp[3]; }
            w=(int)W; h=(int)H;
        }
        if (w>0 && h>0) {
            win_w=w; win_h=h;
            // RADIAL keys (XQueryKeymap, edge-detected). knames[] index -> action (see static comment).
            // Only under GLX (dpy is the X Display). Under EGL dpy is an EGLDisplay -> skip (overrides still work).
            if(!is_egl && p_qkm && p_s2ks && p_k2kc){
                if(!kready){ for(int i=0;i<7;i++){ const char* nm=(i==0)?retro_key:knames[i]; unsigned long ks=p_s2ks(nm); kcode[i]=ks?p_k2kc(dpy,ks):0; } kready=1; logf_("radial keys kc0=%u kc1=%u kc2=%u\n",(GLuint)kcode[0],(GLuint)kcode[1],(GLuint)kcode[2]); }
                char keys[32]; p_qkm(dpy,keys);
                for(int i=0;i<7;i++){ int down=kcode[i]>0?((keys[kcode[i]>>3]>>(kcode[i]&7))&1):0; int edge=(down&&!kprev[i]); kprev[i]=down;
                    if(edge){
                        if(menu_on){   // MENU nav: Menu=close, Prior=up, Next=down, Insert/End=select, Home=back
                            int N=menu_count(); if(N<1)N=1;
                            if(i==6) menu_on=0;
                            else if(i==2) { menu_sel=(menu_sel-1+N)%N; menu_dirty=1; }
                            else if(i==3) { menu_sel=(menu_sel+1)%N; menu_dirty=1; }
                            else if(i==0||i==4) menu_activate();
                            else if(i==1) { if(menu_lvl!=0){menu_lvl=0;menu_sel=0;menu_dirty=1;} else menu_on=0; }
                        } else {       // MENU closed: Menu opens it; 0-5 keep the direct toggles (backward compat)
                            if(i==6) { menu_on=1; menu_lvl=0; menu_sel=0; menu_dirty=1; }
                            else if(i==0) retro_on=!retro_on;
                            else if(i==1) priority_bezel=!priority_bezel;
                            else if(i==2) bezel_idx=(bezel_idx+1)%(bezel_count+1);
                            else if(i==3) shader_idx=(shader_idx+1)%4;
                            else if(i==4) nds_layout=!nds_layout;
                            else if(i==5) nds_pri=(nds_pri+1)%5;
                        }
                        { FILE *kf=fopen("/home/deck/semutap.log","a"); if(kf){ fprintf(kf,"KEY EDGE %d menu=%d lvl=%d sel=%d\n",i,menu_on,menu_lvl,menu_sel); fclose(kf);} }
                    }
                }
            }
            { FILE *rf=fopen("/home/deck/semu-retro","r"); if(rf){ int ch=fgetc(rf); fclose(rf); retro_on=(ch=='0')?0:1; } }
            { FILE *pf=fopen("/home/deck/semu-priority","r"); if(pf){ int ch=fgetc(pf); fclose(pf); priority_bezel=(ch=='b'||ch=='1')?1:0; } }
            { FILE *sf=fopen("/home/deck/semu-shader","r"); if(sf){ int ch=fgetc(sf); fclose(sf); if(ch>='0'&&ch<='3') shader_idx=ch-'0'; } }
            { FILE *bf=fopen("/home/deck/semu-bezel","r"); if(bf){ int ch=fgetc(bf); fclose(bf); if(ch>='0'&&ch<='9') bezel_idx=ch-'0'; } }
            { FILE *nl=fopen("/home/deck/semu-ndslayout","r"); if(nl){ int ch=fgetc(nl); fclose(nl); nds_layout=(ch=='1')?1:0; } }
            { FILE *np=fopen("/home/deck/semu-ndspri","r"); if(np){ int ch=fgetc(np); fclose(np); if(ch>='0'&&ch<='4') nds_pri=ch-'0'; } }
            { FILE *ns=fopen("/home/deck/semu-ndssec","r"); if(ns){ int ch=fgetc(ns); fclose(ns); if(ch>='0'&&ch<='4') nds_sec=ch-'0'; } }
            { FILE *af=fopen("/home/deck/semu-align","r"); if(af){ int ch=fgetc(af); fclose(af); align_on=(ch=='1'); } }   // alignment diag
            // menu test overrides (no Steam-Input keys needed): semu-menu = 0/1 (closed/open at level);
            // semu-menu-nav = u/d/s/b (up/down/select/back) then auto-cleared.
            { FILE *mf=fopen("/home/deck/semu-menu","r"); if(mf){ int ch=fgetc(mf); fclose(mf); int nm=(ch=='1'); if(nm&&!menu_on){menu_lvl=0;menu_sel=0;} if(nm!=menu_on){menu_on=nm; menu_dirty=1;} } }
            { FILE *nf=fopen("/home/deck/semu-menu-nav","r"); if(nf){ int ch=fgetc(nf); fclose(nf); remove("/home/deck/semu-menu-nav"); if(menu_on){ int N=menu_count(); if(N<1)N=1;
                if(ch=='u'){menu_sel=(menu_sel-1+N)%N;menu_dirty=1;} else if(ch=='d'){menu_sel=(menu_sel+1)%N;menu_dirty=1;}
                else if(ch=='s') menu_activate(); else if(ch=='b'){ if(menu_lvl!=0){menu_lvl=0;menu_sel=0;menu_dirty=1;} else {menu_on=0;} } } } }
            float eff_has_art = (bezel_idx < bezel_count) ? has_art : 0.0f;   // bezel OFF when idx past the last art
            // active variant -> its hole + art dims (variant b/c/d may be a different device model)
            int avar = bezel_idx; if(avar>=bezel_count) avar=(bezel_count>0?bezel_count-1:0); if(avar<0)avar=0;
            float sxn=scr_rects[avar][0], syn=scr_rects[avar][1], swn=scr_rects[avar][2], shn=scr_rects[avar][3];
            int awv=art_ws[avar]>0?art_ws[avar]:art_w, ahv=art_hs[avar]>0?art_hs[avar]:art_h;
            // uSrc = the emulator's real content rect (the game pixels), GL bottom-left
            int sx=g_state.content_x, sy=g_state.content_y, sw=g_state.content_w, sh=g_state.content_h;
            if (g_state.origin == SEMU_TAP_ORIGIN_TOP_LEFT) sy = h - sy - sh;
            int nh = env_nh>0 ? env_nh : (g_state.native_h>0 ? g_state.native_h : (sh>0?sh:240));
            int nw = env_nw>0 ? env_nw : (g_state.native_w>0 ? g_state.native_w : (sw>0?sw:320));
            // DECLARED overscan crop: inset uSrc by native-px*contentScale (GL bottom-left: B from sy).
            if((ov_l|ov_r|ov_t|ov_b) && nw>0 && nh>0){
                float scx=(float)sw/(float)nw, scy=(float)sh/(float)nh;
                int dl=(int)(ov_l*scx), dr=(int)(ov_r*scx), dt=(int)(ov_t*scy), db=(int)(ov_b*scy);
                sx+=dl; sw-=(dl+dr); sy+=db; sh-=(dt+db);
                if(sw<1)sw=1; if(sh<1)sh=1;
            }
            SemuTapGeometryInput geom_in; memset(&geom_in,0,sizeof(geom_in));
            geom_in.win_w=w; geom_in.win_h=h; geom_in.native_w=nw; geom_in.native_h=nh;
            geom_in.display_aspect=disp_aspect>0.01f ? disp_aspect : (float)nw/(float)nh;
            geom_in.priority_bezel=priority_bezel; geom_in.fill_hole=fill_hole;
            geom_in.has_art=eff_has_art>0.5f; geom_in.art_w=awv; geom_in.art_h=ahv;
            geom_in.hole_x=sxn; geom_in.hole_y=syn; geom_in.hole_w=swn; geom_in.hole_h=shn;
            SemuTapGeometry geom;
            if(!semu_tap_compute_geometry(&geom_in,&geom)) return;
            int gx=geom.game_x, gy=geom.game_y, gw=geom.game_w, gh=geom.game_h;
            float bdx_gl=geom.bezel_x, bdy_gl=geom.bezel_y, bdw=geom.bezel_w, bdh=geom.bezel_h;
            // DUAL (nds/3ds): split the combined content into two screens; layout + per-screen scale.
            int r1x=gx,r1y=gy,r1w=gw,r1h=gh, r2x=0,r2y=0,r2w=0,r2h=0;
            int s1x=sx,s1y=sy,s1w=sw,s1h=sh, s2x=sx,s2y=sy,s2w=sw,s2h=sh;
            if(dual_mode){
                float scl[5]={0.25f,0.5f,1.0f,2.0f,3.0f};
                int pi=nds_pri<0?0:(nds_pri>4?4:nds_pri), si=nds_sec<0?0:(nds_sec>4?4:nds_sec);
                int nhp=(nh>=288)?nh/2:nh;   // per-screen native height (split the combined nds/3ds frame)
                int pw=(int)(scl[pi]*nw), ph=(int)(scl[pi]*nhp), qw=(int)(scl[si]*nw), qh=(int)(scl[si]*nhp);
                int half=sh/2;
                s1x=sx; s1y=sy+half; s1w=sw; s1h=half;   // primary = top screen (upper half, GL high y)
                s2x=sx; s2y=sy;      s2w=sw; s2h=half;   // secondary = bottom screen
                if(nds_layout==0){ int gap=8,total=ph+qh+gap,top=(h-total)/2;   // VERTICAL: primary above secondary
                    r1x=(w-pw)/2; r1y=h-top-ph; r1w=pw; r1h=ph;
                    r2x=(w-qw)/2; r2y=h-(top+ph+gap)-qh; r2w=qw; r2h=qh;
                } else { int gap=8,total=pw+qw+gap,left=(w-total)/2;            // HORIZONTAL: primary left of secondary
                    r1x=left; r1y=(h-ph)/2; r1w=pw; r1h=ph;
                    r2x=left+pw+gap; r2y=(h-qh)/2; r2w=qw; r2h=qh;
                }
            }
            gl_init();
            p_active(GL_TEXTURE0); p_bindtex(GL_TEXTURE_2D,game_tex);
            p_copytex(GL_TEXTURE_2D,0,GL_RGB,0,0,w,h,0);
            if(p_genmip) p_genmip(GL_TEXTURE_2D);
            int active = bezel_idx; if(active>=bezel_count) active=(bezel_count>0?bezel_count-1:0); if(active<0)active=0;
            if(eff_has_art>0.5f){ p_active(GL_TEXTURE1); p_bindtex(GL_TEXTURE_2D,bezel_texs[active]); p_active(GL_TEXTURE0); }
            GLuint gtex = glass_texs[active]; float hg = gtex ? 1.0f : 0.0f;   // per-variant screen glass
            if(hg>0.5f){ p_active(GL_TEXTURE2); p_bindtex(GL_TEXTURE_2D,gtex); p_active(GL_TEXTURE0); }
            if(menu_on){ int need_upload = menu_dirty; if(menu_dirty) menu_build();   // rebuild menu_buf on state change (clears menu_dirty)
                if(menu_buf){ p_active(GL_TEXTURE3); p_bindtex(GL_TEXTURE_2D,menu_tex);   // BIND EVERY FRAME: RA clobbers texture units between our swaps,
                    if(need_upload){                                                      // so unit 3 must be re-pointed at menu_tex or uMenu samples an
                        if(p_pixstore){ p_pixstore(GL_UNPACK_ALIGNMENT,1); p_pixstore(GL_UNPACK_ROW_LENGTH,0); p_pixstore(GL_UNPACK_SKIP_PIXELS,0); p_pixstore(GL_UNPACK_SKIP_ROWS,0); }
                        p_teximg(GL_TEXTURE_2D,0,GL_RGBA,MENU_W,MENU_H,0,GL_RGBA,GL_UNSIGNED_BYTE,menu_buf);   // incomplete texture -> whole draw corrupts on RADV
                    }
                    p_active(GL_TEXTURE0); } }
            if(p_disable){ p_disable(GL_DEPTH_TEST); p_disable(GL_BLEND); p_disable(GL_SCISSOR_TEST); }
            p_viewport(0,0,w,h);
            p_use(prog); if(p_bindva&&vao) p_bindva(vao);
            if(uWin>=0)  p_u2f(uWin,(float)w,(float)h);
            if(uRect>=0) p_u4f(uRect,(float)r1x,(float)r1y,(float)r1w,(float)r1h);  // game (or primary screen)
            if(uSrc>=0)  p_u4f(uSrc,(float)s1x,(float)s1y,(float)s1w,(float)s1h);
            if(uRect2>=0)p_u4f(uRect2,(float)r2x,(float)r2y,(float)r2w,(float)r2h); // secondary screen (dual)
            if(uSrc2>=0) p_u4f(uSrc2,(float)s2x,(float)s2y,(float)s2w,(float)s2h);
            if(uDual>=0) p_u1f(uDual,(float)dual_mode);
            if(uAlign>=0) p_u1f(uAlign,(float)align_on);
            if(uHole>=0) p_u4f(uHole,sxn,syn,swn,shn);   // active variant's declared hole (norm within bezel art)
            if(uBezelRect>=0) p_u4f(uBezelRect,bdx_gl,bdy_gl,bdw,bdh);           // whole bezel art -> this rect
            if(uNative>=0)p_u1f(uNative,(float)nh);
            if(uStyle>=0)p_u1f(uStyle,(float)tap_style);
            if(uShell>=0&&p_u3f)p_u3f(uShell,shell_r,shell_g,shell_b);
            if(uTV>=0)   p_u4f(uTV,0.0f,tv_mask,tv_scan,tv_bzlw);
            if(uRef>=0)  p_u4f(uRef,0.0f,5.0f,0.12f,tv_corner);
            if(uHasArt>=0)p_u1f(uHasArt,eff_has_art);
            if(uDebug>=0)p_u1f(uDebug, debug_on?debug_thresh:0.0f);
            if(uRetro>=0)p_u1f(uRetro, retro_on?retro_lod:0.0f);   // 0=sharp, retro_lod=soft
            if(uShaderMode>=0)p_u1f(uShaderMode,(float)shader_idx);
            if(uHasGlass>=0)p_u1f(uHasGlass,hg);
            if(uReflect>=0)p_u1f(uReflect,reflect_amt);
            if(uCurve>=0)p_u1f(uCurve,curve_amt);
            if(uScreenCorner>=0)p_u1f(uScreenCorner,screen_corner);
            if(uMenuOn>=0)p_u1f(uMenuOn, menu_on?1.0f:0.0f);
            if(uMenuRect>=0){ float mw=(float)MENU_W,mh=(float)MENU_H,sc=1.0f; if(mh>(float)h*0.92f)sc=(float)h*0.92f/mh; mw*=sc; mh*=sc; p_u4f(uMenuRect,((float)w-mw)*0.5f,((float)h-mh)*0.5f,mw,mh); }
            p_draw(GL_TRIANGLES,0,3);
            if(++frames<=2 || (menu_on && (frames%30)==0)){ FILE *lf=fopen("/home/deck/semutap.log","a"); if(lf){ fprintf(lf,"frame%ld: fb=%dx%d native=%dx%d content=(%d,%d,%d,%d) pri=%d game=(%d,%d,%d,%d) bd=(%d,%d,%d,%d) menu_on=%d effArt=%.1f bidx=%d bcnt=%d uMenuOn=%d uHasArt=%d\n",frames,w,h,nw,nh,g_state.content_x,g_state.content_y,g_state.content_w,g_state.content_h,priority_bezel,gx,gy,gw,gh,(int)bdx_gl,(int)bdy_gl,(int)bdw,(int)bdh,menu_on,eff_has_art,bezel_idx,bezel_count,(int)(uMenuOn>=0),(int)(uHasArt>=0)); fclose(lf);} }
        }
    }
}

// Public present hooks. Standalone/modern emulators often present via EGL (eglSwapBuffers) even on X11,
// so we hook BOTH — the compositing is identical (all GL calls); only the framebuffer-size query + the X
// keymap differ (handled via is_egl).
__attribute__((constructor)) static void semu_loaded(void) {   // proves LD_PRELOAD applied (fires even if no swap is hooked)
    FILE *f=fopen("/home/deck/semu-loaded.log","a"); if(f){ fprintf(f,"libsemutap loaded pid=%d ppid=%d\n",(int)getpid(),(int)getppid()); fclose(f);} }
void glXSwapBuffers(void *dpy, unsigned long drawable) {
    tap_frame(dpy, drawable, 0);
    if (real_swap) real_swap(dpy, drawable);
}
unsigned int eglSwapBuffers(void *dpy, void *surface) {
    tap_frame(dpy, (unsigned long)surface, 1);
    return real_egl_swap ? real_egl_swap(dpy, surface) : 1;
}
