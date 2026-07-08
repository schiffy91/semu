// tube.frag — canonical mirror of the compositor fragment shader (FS in
// libsemutap.c). Regenerated from that string; edit the C source, then re-sync.
#version 330 core
out vec4 frag;
uniform sampler2D uGame; uniform sampler2D uBezel;
uniform vec2 uWin; uniform vec4 uRect; uniform vec4 uSrc; uniform vec4 uBezelRect;
uniform float uNative; uniform float uStyle; uniform float uHasArt; uniform vec3 uShell;
uniform vec4 uTV;  /* y=mask z=scanlineDepth w=bezelWidthFrac */
uniform vec4 uRef; /* z=halation w=cornerRadius */
uniform float uDebug; /* 0=off; else luma threshold for glass-edge detect */
uniform float uRetro; /* game-sample LOD: 0=sharp(native render), ~1.6=soft RETRO (downsample to ~240p) */
uniform float uShaderMode; /* 0=full(scan+mask+halation) 1=scanline-only 2=mask-only/sharp 3=OFF(raw) */
uniform vec4 uRect2; uniform vec4 uSrc2; uniform float uDual; /* nds/3ds: second screen rect+src; uDual>0.5 = dual */
uniform float uAlign; uniform vec4 uHole; /* alignment diag: uHole = declared screen hole (norm within bezel art) */
uniform sampler2D uGlass; uniform float uHasGlass; /* screen-glass layer: .a = screen MASK (real cutout shape), .rgb = glass */
uniform float uReflect; /* reflection/glare strength */ uniform float uCurve; /* CRT barrel curvature (0 = flat) */ uniform float uScreenCorner; /* CRT rounded-mask corner radius */
float sdRound(vec2 p, vec2 b, float r){ vec2 d=abs(p)-b+r; return length(max(d,vec2(0.0)))+min(max(d.x,d.y),0.0)-r; }
vec3 gameAt(vec2 uv, float lod){ return textureLod(uGame,(uSrc.xy+uv*uSrc.zw)/uWin,lod).rgb; }
vec3 artAt(vec2 sp){ vec2 auv=vec2((sp.x-uBezelRect.x)/uBezelRect.z, 1.0-(sp.y-uBezelRect.y)/uBezelRect.w); return texture(uBezel,auv).rgb; }
vec4 glassAt(vec2 sp){ vec2 auv=vec2((sp.x-uBezelRect.x)/uBezelRect.z, 1.0-(sp.y-uBezelRect.y)/uBezelRect.w); return texture(uGlass,auv); }
uniform sampler2D uMenu; uniform vec4 uMenuRect; uniform float uMenuOn; /* overlay menu (screen-px rect, GL y-up) */
vec3 menuComposite(vec3 c, vec2 px){ if(uMenuOn>0.5 && px.x>=uMenuRect.x&&px.x<=uMenuRect.x+uMenuRect.z&&px.y>=uMenuRect.y&&px.y<=uMenuRect.y+uMenuRect.w){ vec2 muv=vec2((px.x-uMenuRect.x)/uMenuRect.z, 1.0-(px.y-uMenuRect.y)/uMenuRect.w); vec4 m=texture(uMenu,muv); return mix(c,m.rgb,m.a);} return c; }
void main(){
  vec2 px=gl_FragCoord.xy;
  if(uAlign>0.5){
    vec3 bz = uHasArt>0.5 ? artAt(px) : vec3(0.05);
    vec3 outc = bz;
    if(px.x>=uRect.x&&px.x<=uRect.x+uRect.z&&px.y>=uRect.y&&px.y<=uRect.y+uRect.w){
      vec2 uv=(px-uRect.xy)/uRect.zw; vec3 g=textureLod(uGame,(uSrc.xy+uv*uSrc.zw)/uWin,0.0).rgb; outc=mix(bz,g,0.40);
    }
    float L=uBezelRect.x+uHole.x*uBezelRect.z, hw=uHole.z*uBezelRect.z;
    float B=uBezelRect.y+(1.0-uHole.y-uHole.w)*uBezelRect.w, hh=uHole.w*uBezelRect.w;
    float R=L+hw, T=B+hh, n=3.0;
    bool fv=(abs(px.x-L)<n||abs(px.x-R)<n)&&px.y>=B-n&&px.y<=T+n;
    bool fh=(abs(px.y-B)<n||abs(px.y-T)<n)&&px.x>=L-n&&px.x<=R+n;
    if(fv||fh) outc=vec3(0.78,0.0,1.0);
    frag=vec4(outc,1.0); return;
  }
  if(uDual>0.5){
    vec3 oc = uHasArt>0.5 ? artAt(px) : vec3(0.0);
    for(int s=0;s<2;s++){
      vec4 R=(s==0)?uRect:uRect2; vec4 S=(s==0)?uSrc:uSrc2;
      if(px.x>=R.x&&px.x<=R.x+R.z&&px.y>=R.y&&px.y<=R.y+R.w){
        vec2 uv=(px-R.xy)/R.zw; vec3 g=textureLod(uGame,(S.xy+uv*S.zw)/uWin,uRetro).rgb;
        if(uShaderMode<1.5){ float nx=uNative*R.z/R.w; g*=0.90+0.10*sqrt(abs(cos(uv.x*nx*3.14159265))*abs(cos(uv.y*uNative*3.14159265))); }
        oc=g;
      }
    }
    frag=vec4(menuComposite(oc,px),1.0); return;
  }
  vec2 cen=uRect.xy+uRect.zw*0.5; vec2 hf=uRect.zw*0.5;
  vec2 c=(px-cen)/hf;
  vec2 cc = c*(1.0 + uCurve*dot(c.yx,c.yx));
  vec2 uv = cc*0.5+0.5;
  bool inRect = uv.x>=0.0&&uv.x<=1.0&&uv.y>=0.0&&uv.y<=1.0;
  vec4 gl = uHasGlass>0.5 ? glassAt(px) : vec4(0.0);
  float mask = uHasArt<0.5 ? 1.0 : (uHasGlass>0.5 ? gl.a : (1.0 - smoothstep(-0.012,0.012, sdRound(cc, vec2(1.0), clamp(uScreenCorner,0.0,0.5)))));
  vec3 bez = uHasArt>0.5 ? artAt(px) : vec3(0.0);
  if(uTV.x>0.001 && uStyle<0.5 && uHasArt>0.5){
    vec2 nuv=clamp(uv,0.0,1.0); vec3 spill=gameAt(nuv,5.0);
    float sl=dot(spill,vec3(0.299,0.587,0.114)); spill*=1.0/(1.0+max(sl-0.72,0.0)*1.3);
    float fall=exp(-length(uv-nuv)*3.2);
    bez = 1.0-(1.0-bez)*(1.0-spill*fall*uTV.x*1.05);
  }
  vec3 outc = bez;
  if(inRect && mask>0.003){
    vec3 g=gameAt(uv,uRetro);
    if(uShaderMode<2.5){
      if(uStyle<0.5){
        if(uShaderMode<1.5){ float lum=dot(g,vec3(0.299,0.587,0.114)); float sl=uTV.z*(1.0-abs(cos(uv.y*uNative*3.14159265))); g*=1.0-sl*(1.0-0.3*lum); }
        if(uShaderMode>1.5){ float m=mod(px.x,3.0); vec3 cmask=(m<1.0)?vec3(1.0,0.72,0.72):(m<2.0)?vec3(0.72,1.0,0.72):vec3(0.72,0.72,1.0); g*=mix(vec3(1.0),cmask,uTV.y); }
        if(uShaderMode<0.5){
          vec3 bl=gameAt(uv,2.0); float bsat=max(bl.r,max(bl.g,bl.b))-min(bl.r,min(bl.g,bl.b));
          g += gameAt(uv,3.0)*uRef.z*0.6 + bl*bsat*uRef.x*2.8;
          float L=dot(g,vec3(0.299,0.587,0.114)); g *= 1.0/(1.0+max(L-0.72,0.0)*1.3);
          float m=mod(px.x,3.0); vec3 cmask=(m<1.0)?vec3(1.0,0.68,0.68):(m<2.0)?vec3(0.68,1.0,0.68):vec3(0.68,0.68,1.0); g*=mix(vec3(1.0),cmask,uTV.y);
          g *= exp(-uRef.y*dot(cc,cc)*1.4);
          g = clamp(mix(vec3(dot(g,vec3(0.299,0.587,0.114))), g, 1.75), 0.0, 1.0);
          g = mix(g, g*g*(3.0-2.0*g), 0.85);
          g = clamp((g-0.05)*1.20, 0.0, 1.0);
          if(uReflect>0.001){
            vec3 N=normalize(vec3(c*max(uCurve,0.02)*6.0,1.0)); float fres=pow(1.0-N.z,2.4);
            vec3 refl=artAt(cen+(px-cen)*1.13);
            g=mix(g, refl*0.42, clamp(fres*uReflect*3.2,0.0,0.6));
            vec3 H=normalize(vec3(-0.35,0.42,1.0)); float spec=pow(max(dot(N,H),0.0),5.0);
            g=1.0-(1.0-g)*(1.0-spec*uReflect*0.7);
          }
          float edge=min(min(uv.x,1.0-uv.x),min(uv.y,1.0-uv.y)); g*=mix(0.5,1.0,smoothstep(0.0,0.02,edge));
        }
      } else {
        if(uShaderMode<1.5){ float nx=uNative*uRect.z/uRect.w; g*=0.90+0.10*sqrt(abs(cos(uv.x*nx*3.14159265))*abs(cos(uv.y*uNative*3.14159265))); }
      }
    }
    if(uReflect>0.001 && uHasGlass>0.5) g = 1.0-(1.0-g)*(1.0-gl.rgb*gl.a*uReflect);
    outc = mix(bez, g, clamp(mask,0.0,1.0));
  }
  if(uDebug>0.0005){
    float bw=2.5;
    float L=uRect.x, R=uRect.x+uRect.z, B=uRect.y, T=uRect.y+uRect.w;
    bool inF = px.x>=L-bw&&px.x<=R+bw&&px.y>=B-bw&&px.y<=T+bw;
    bool inI = px.x>=L+bw&&px.x<=R-bw&&px.y>=B+bw&&px.y<=T-bw;
    bool green = inF&&!inI;
    bool purple=false;
    if(uHasArt>0.5){
      vec3 wv=vec3(0.299,0.587,0.114);
      float l0=dot(artAt(px),wv), lx=dot(artAt(px+vec2(2.0,0.0)),wv), ly=dot(artAt(px+vec2(0.0,2.0)),wv);
      bool g0=l0<uDebug, gx=lx<uDebug, gy=ly<uDebug;
      purple=(g0!=gx)||(g0!=gy);
    }
    if(purple) outc=vec3(0.80,0.0,1.0);
    if(green)  outc=vec3(0.0,1.0,0.0);
  }
  frag=vec4(menuComposite(outc,px),1.0);
}
