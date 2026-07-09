/* semutap_glue.h — raw-C glue for libsemutap.btrc.
 *
 * The btrc port keeps ALL the logic in libsemutap.btrc, but a handful of C
 * constructs cannot be expressed in btrc today (see the header comment in the
 * .btrc for the full list): GL enum #defines, function-pointer typedefs, C
 * structs with multi-dimensional fixed arrays, and the state struct. Those live
 * here, verbatim C, #included by the btrc file. The btrc analyzer does not parse
 * this header, so anything the btrc code needs to *call* is exposed as a plain
 * `gl_*` wrapper that takes a void* function pointer + primitive args — every
 * awkward GL/X11 type stays inside this file.
 *
 * This file is a faithful lift of the type/constant declarations and the GL call
 * shapes from libsemutap.c; keep the two in lockstep.
 */
#ifndef SEMUTAP_GLUE_H
#define SEMUTAP_GLUE_H

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/stat.h>

/* mtime as a double, platform-aware (macOS uses st_mtimespec). Returns 0 if the
 * file is absent. Keeps the #ifdef out of the btrc side (the C had it inline). */
static inline double stat_mtime_d(const char* path){
    struct stat st;
    if (stat(path, &st) != 0) { return 0.0; }
#if defined(__APPLE__)
    return (double)st.st_mtimespec.tv_sec + (double)st.st_mtimespec.tv_nsec * 1e-9;
#else
    return (double)st.st_mtim.tv_sec + (double)st.st_mtim.tv_nsec * 1e-9;
#endif
}

/* ---- GL type aliases ---- */
typedef unsigned int GLenum; typedef int GLint; typedef int GLsizei; typedef unsigned int GLuint;
typedef unsigned int GLbitfield; typedef float GLfloat; typedef float GLclampf; typedef unsigned char GLboolean;
typedef char GLchar; typedef long GLsizeiptr; typedef long GLintptr;

/* ---- GL enum constants ---- */
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
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
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
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_PACK_ALIGNMENT 0x0D05

#define SEMU_TAP_ABI 1
#define SEMU_TAP_ORIGIN_TOP_LEFT 1

/* Sizes referenced from btrc (kept as macros for the CPU menu blitter). */
#define SEMU_MENU_W 560
#define SEMU_MENU_H 720
#define SEMU_GLY_COLS 16
#define SEMU_GLY_ROWS 6

/* ---- contract structs ---- */
typedef struct SemuTapState {
    int abi; int active;
    int fb_width; int fb_height;
    int content_x; int content_y; int content_w; int content_h;
    int native_w; int native_h;
    int rotation; int origin; int reserved[4];
} SemuTapState;

typedef struct SemuTapGeometryInput {
    int win_w; int win_h; int native_w; int native_h;
    float display_aspect; int priority_bezel; int fill_hole; int has_art;
    int art_w; int art_h; float hole_x; float hole_y; float hole_w; float hole_h;
} SemuTapGeometryInput;

typedef struct SemuTapGeometry {
    int game_x; int game_y; int game_w; int game_h;
    float bezel_x; float bezel_y; float bezel_w; float bezel_h; int bezel_priority;
} SemuTapGeometry;

typedef struct SemuTapMenuState {
    int level; int selected; int system_kind; int priority_mode;
    int bezel_index; int bezel_count; int shader_index; int save_slot;
    int nds_layout; int nds_primary_scale; int nds_secondary_scale;
    int wii_controller; int action; int action_slot;
} SemuTapMenuState;

/* ---- GL/X11 function-pointer typedefs ---- */
typedef void (*PFN_swap)(void*, unsigned long);
typedef unsigned int (*PFN_eglswap)(void*, void*);
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
typedef void (*PFN_pixstore)(GLenum, GLint);
typedef void (*PFN_bindbuf)(GLenum, GLuint);
typedef int (*PFN_qkm)(void*, char*);
typedef unsigned char (*PFN_k2kc)(void*, unsigned long);
typedef unsigned long (*PFN_s2ks)(const char*);
typedef unsigned char (*PFN_isenab)(GLenum);
typedef void (*PFN_enable)(GLenum);
typedef void (*PFN_readpix)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
typedef void* (*PFN_dlsym)(void*, const char*);

/* nested-struct wrappers so btrc can hold the C's [4][1024] / [4][4] arrays */
typedef struct PathStr { char v[1024]; } PathStr;
typedef struct Rect4 { float v[4]; } Rect4;

/* One global state struct = all the C file-scope statics. */
typedef struct TapG {
    SemuTapState g_state; int g_have; int win_w; int win_h;

    void* real_swap; void* real_egl_swap; void* p_query; void* p_getiv;
    void* p_gentex; void* p_bindtex; void* p_teximg;
    void* p_copytex; void* p_texpar; void* p_active;
    void* p_genva; void* p_bindva; void* p_createsh;
    void* p_shsrc; void* p_compsh; void* p_shiv; void* p_shlog;
    void* p_createpr; void* p_attach; void* p_link; void* p_priv;
    void* p_use; void* p_uloc; void* p_u1i; void* p_u2f;
    void* p_u4f; void* p_u3f; void* p_u1f; void* p_draw; void* p_viewport;
    void* p_disable; void* p_genmip; void* p_pixstore; void* p_bindbuf;
    void* p_qkm; void* p_k2kc; void* p_s2ks;
    void* p_isenab; void* p_enable; void* p_readpix; void* real_dlsym;

    int inited; int disabled; int gl_ready; int tap_style; int env_nw; int env_nh; int debug_on;
    int standalone;
    int ov_l; int ov_r; int ov_t; int ov_b;
    int retro_on; int priority_mode;
    int bezel_idx; int shader_idx; int bezel_count;
    int nds_layout; int nds_pri; int nds_sec; int dual_mode;
    float pip_expand_budget; float pip_seconds; float pip_w_current;
    double pip_expand_until; double pip_touch_seen;
    int fill_hole; int align_on;
    PathStr glass_paths[4];
    float reflect_amt; float curve_amt; float screen_corner;
    float glow_amt; float bloom_amt; float vignette_amt;
    float lcd_grid_amt; float lcd_subpixel_amt;
    float lcd_mono_amt; float lcd_wash_amt;
    int menu_on; int menu_lvl; int menu_sel;
    int save_slot; int wii_ctrl; int sys_kind;
    GLuint menu_tex; unsigned char* menu_buf; int menu_dirty;
    unsigned char* atlas_buf; int atlas_w; int atlas_h;
    int GLY_W; int GLY_H;
    PathStr menu_font_path_s;
    float retro_lod; char retro_key[32];
    int kcode[7]; int kprev[7]; int kready;
    float debug_thresh;
    float shell_r; float shell_g; float shell_b;
    float tv_mask; float tv_scan; float tv_corner; float tv_bzlw;
    float has_art; PathStr art_paths[4]; int art_w; int art_h;
    int art_ws[4]; int art_hs[4];
    float scr_x; float scr_y; float scr_w; float scr_h;
    Rect4 scr_rects[4];
    Rect4 pri_rects[4]; Rect4 sec_rects[4];
    int dual_hole_set[4];
    float disp_aspect;
    GLuint prog; GLuint vao; GLuint game_tex;
    GLuint bezel_texs[4]; GLuint glass_texs[4];
    GLint uWin; GLint uRect; GLint uSrc; GLint uBezelRect; GLint uNative; GLint uStyle; GLint uShell;
    GLint uTV; GLint uRef; GLint uHasArt; GLint uDebug; GLint uRetro; GLint uShaderMode;
    GLint uRect2; GLint uSrc2; GLint uDual; GLint uAlign; GLint uHole; GLint uGlass; GLint uHasGlass;
    GLint uReflect; GLint uCurve; GLint uScreenCorner; GLint uLcdGrid; GLint uLcdSub; GLint uLcdMono; GLint uLcdWash;
    GLint uMenuRect; GLint uMenuOn;
    long frames;
    char tap_state_dir_buf[1024]; int tap_state_dir_ready;

    GLint saved_unpack_pbo;
    GLint sv_prog; GLint sv_vao; GLint sv_active; GLint sv_tex[4]; GLint sv_view[4]; GLint sv_unpack[4];
    unsigned char sv_depth; unsigned char sv_blend; unsigned char sv_scissor;

    int swaptype_logged; int null_swap_logged_glx; int null_swap_logged_egl; int loaded_logged;
} TapG;

/* The one global. Declared here so both this header's inlines and the btrc code
 * reference the same object. */
static TapG G;

/* ===================== GL call wrappers (void* fp -> typed call) =====================
 * Each takes the resolved entry point as a void*, matching the C's `p_*` pointers.
 * A NULL fp is a hard no-op so the btrc side can call unconditionally where the C
 * did (the C guarded most, but a few are guarded by the caller). */
static inline void        gl_active(void* fp, GLenum t){ if(fp) ((PFN_active)fp)(t); }
static inline void        gl_gentex(void* fp, GLsizei n, GLuint* out){ if(fp) ((PFN_gentex)fp)(n,out); }
static inline void        gl_bindtex(void* fp, GLenum tgt, GLuint tex){ if(fp) ((PFN_bindtex)fp)(tgt,tex); }
static inline void        gl_texpar(void* fp, GLenum tgt, GLenum p, GLint v){ if(fp) ((PFN_texpar)fp)(tgt,p,v); }
static inline void        gl_teximg(void* fp, GLenum tgt, GLint lvl, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum fmt, GLenum ty, const void* px){ if(fp) ((PFN_teximg)fp)(tgt,lvl,ifmt,w,h,b,fmt,ty,px); }
static inline void        gl_copytex(void* fp, GLenum tgt, GLint lvl, GLenum ifmt, GLint x, GLint y, GLsizei w, GLsizei h, GLint b){ if(fp) ((PFN_copytex)fp)(tgt,lvl,ifmt,x,y,w,h,b); }
static inline void        gl_genva(void* fp, GLsizei n, GLuint* out){ if(fp) ((PFN_genva)fp)(n,out); }
static inline void        gl_bindva(void* fp, GLuint v){ if(fp) ((PFN_bindva)fp)(v); }
static inline GLuint      gl_createsh(void* fp, GLenum t){ return fp ? ((PFN_createsh)fp)(t) : 0u; }
static inline void        gl_shsrc(void* fp, GLuint s, const char* src){ if(fp){ const GLchar* p=(const GLchar*)src; ((PFN_shsrc)fp)(s,1,&p,NULL);} }
static inline void        gl_compsh(void* fp, GLuint s){ if(fp) ((PFN_compsh)fp)(s); }
static inline void        gl_shiv(void* fp, GLuint s, GLenum pname, GLint* out){ if(fp) ((PFN_shiv)fp)(s,pname,out); }
static inline void        gl_shlog(void* fp, GLuint s, GLsizei cap, char* out){ if(fp) ((PFN_shlog)fp)(s,cap,NULL,(GLchar*)out); }
static inline GLuint      gl_createpr(void* fp){ return fp ? ((PFN_createpr)fp)() : 0u; }
static inline void        gl_attach(void* fp, GLuint pr, GLuint sh){ if(fp) ((PFN_attach)fp)(pr,sh); }
static inline void        gl_link(void* fp, GLuint pr){ if(fp) ((PFN_link)fp)(pr); }
static inline void        gl_priv(void* fp, GLuint pr, GLenum pname, GLint* out){ if(fp) ((PFN_priv)fp)(pr,pname,out); }
static inline void        gl_use(void* fp, GLuint pr){ if(fp) ((PFN_use)fp)(pr); }
static inline GLint       gl_uloc(void* fp, GLuint pr, const char* n){ return fp ? ((PFN_uloc)fp)(pr,(const GLchar*)n) : -1; }
static inline void        gl_u1i(void* fp, GLint l, GLint v){ if(fp) ((PFN_u1i)fp)(l,v); }
static inline void        gl_u1f(void* fp, GLint l, GLfloat v){ if(fp) ((PFN_u1f)fp)(l,v); }
static inline void        gl_u2f(void* fp, GLint l, GLfloat a, GLfloat b){ if(fp) ((PFN_u2f)fp)(l,a,b); }
static inline void        gl_u3f(void* fp, GLint l, GLfloat a, GLfloat b, GLfloat c){ if(fp) ((PFN_u3f)fp)(l,a,b,c); }
static inline void        gl_u4f(void* fp, GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d){ if(fp) ((PFN_u4f)fp)(l,a,b,c,d); }
static inline void        gl_draw(void* fp, GLenum mode, GLint first, GLsizei count){ if(fp) ((PFN_draw)fp)(mode,first,count); }
static inline void        gl_viewport(void* fp, GLint x, GLint y, GLsizei w, GLsizei h){ if(fp) ((PFN_viewport)fp)(x,y,w,h); }
static inline void        gl_disable(void* fp, GLenum c){ if(fp) ((PFN_disable)fp)(c); }
static inline void        gl_enable(void* fp, GLenum c){ if(fp) ((PFN_enable)fp)(c); }
static inline unsigned char gl_isenab(void* fp, GLenum c){ return fp ? ((PFN_isenab)fp)(c) : 0; }
static inline void        gl_genmip(void* fp, GLenum t){ if(fp) ((PFN_genmip)fp)(t); }
static inline void        gl_pixstore(void* fp, GLenum p, GLint v){ if(fp) ((PFN_pixstore)fp)(p,v); }
static inline void        gl_bindbuf(void* fp, GLenum t, GLuint b){ if(fp) ((PFN_bindbuf)fp)(t,b); }
static inline void        gl_getiv(void* fp, GLenum p, GLint* out){ if(fp) ((PFN_getiv)fp)(p,out); }
static inline void        gl_readpix(void* fp, GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum ty, void* px){ if(fp) ((PFN_readpix)fp)(x,y,w,h,fmt,ty,px); }
/* glGetIntegerv into a scalar (viewport / bindings) */
static inline GLint       gl_getiv1(void* fp, GLenum p){ GLint v=0; if(fp) ((PFN_getiv)fp)(p,&v); return v; }
/* glXQueryDrawable -> unsigned int out */
static inline unsigned int gl_query(void* fp, void* dpy, unsigned long dr, int attr){ unsigned int v=0; if(fp) ((PFN_query)fp)(dpy,dr,attr,&v); return v; }
/* viewport read: fill 4 ints */
static inline void        gl_getviewport(void* fp, GLint* out4){ if(fp) ((PFN_getiv)fp)(GL_VIEWPORT,out4); }

/* X11 (only present under GLX) */
static inline int         x_querykeymap(void* fp, void* dpy, char* keys32){ return fp ? ((PFN_qkm)fp)(dpy,keys32) : 0; }
static inline unsigned char x_keysym2keycode(void* fp, void* dpy, unsigned long ks){ return fp ? ((PFN_k2kc)fp)(dpy,ks) : 0; }
static inline unsigned long x_string2keysym(void* fp, const char* nm){ return fp ? ((PFN_s2ks)fp)(nm) : 0; }

/* present-hook trampolines */
static inline void        call_real_swap(void* fp, void* dpy, unsigned long dr){ ((PFN_swap)fp)(dpy,dr); }
static inline unsigned int call_real_eglswap(void* fp, void* dpy, void* s){ return ((PFN_eglswap)fp)(dpy,s); }
static inline void*       call_real_dlsym(void* fp, void* handle, const char* name){ return ((PFN_dlsym)fp)(handle,name); }

/* The two present hooks are defined in the generated C (from btrc). Forward
 * declare them so this header can hand back their addresses — btrc cannot cast a
 * bare function name to void* in an expression, so the self-pointer guard in
 * dlsym() reads the addresses through these accessors. */
void glXSwapBuffers(void* dpy, unsigned long drawable);
unsigned int eglSwapBuffers(void* dpy, void* surface);
static inline void*       addr_glx_hook(void){ return (void*)glXSwapBuffers; }
static inline void*       addr_egl_hook(void){ return (void*)eglSwapBuffers; }
static inline void*       rtld_next(void){ return RTLD_NEXT; }
static inline void*       rtld_default(void){ return RTLD_DEFAULT; }

/* Address of a scalar field (the C passed &var to sscanf/stat/etc.). btrc's &
 * works on G fields directly, so most of these are unneeded, but scanf into a
 * struct's float needs a stable lvalue address — provided directly in btrc. */

#endif /* SEMUTAP_GLUE_H */
