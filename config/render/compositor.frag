#version 330 core
out vec4 frag;
uniform sampler2D uGame;uniform sampler2D uGame2;uniform sampler2D uRaw;uniform sampler2D uRaw2;uniform sampler2D uBezel;uniform sampler2D uGlass;uniform sampler2D uGlass2;uniform sampler2D uBackground;uniform sampler2D uMenu;
uniform vec4 uRect;uniform vec4 uRect2;uniform vec4 uTube;uniform vec4 uTube2;uniform vec4 uShape;uniform vec4 uShape2;uniform vec4 uFx;uniform vec4 uFx2;
uniform vec4 uSurround;uniform vec4 uSurround2;uniform vec4 uReflect;uniform vec4 uBezelRect;uniform vec4 uBackgroundRect;uniform vec4 uMenuRect;
uniform vec4 uFlags;uniform vec4 uFrame;uniform vec4 uFrameColor;uniform vec4 uRotation;uniform float uPass;uniform float uMenuOn;
uniform vec4 uRingIn;uniform vec4 uRingIn2;uniform vec4 uRingOut;uniform vec4 uRingOut2;uniform vec4 uRingLook;uniform vec4 uRingLook2;uniform vec4 uRingColor;uniform vec4 uRingColor2;uniform vec4 uRingReflect;uniform vec4 uRingReflect2;uniform vec4 uLayer;uniform vec4 uLayerTint;uniform vec4 uBulge;
 bool inside(vec2 p,vec4 r){return p.x>=r.x&&p.x<=r.x+r.z&&p.y>=r.y&&p.y<=r.y+r.w;}
 vec2 uv(vec2 p,vec4 r){return(p-r.xy)/r.zw;}
 vec2 rotatedUv(vec2 q,float r){if(r<0.5)return q;if(r<1.5)return vec2(1.0-q.y,q.x);if(r<2.5)return vec2(1.0-q.x,1.0-q.y);return vec2(q.y,1.0-q.x);}
 vec2 artUv(vec2 p,vec4 r){return vec2((p.x-r.x)/r.z,1.0-(p.y-r.y)/r.w);}
 vec4 plate(sampler2D t,vec2 p,vec4 r){return textureGrad(t,artUv(p,r),vec2(1.0/r.z,0.0),vec2(0.0,-1.0/r.w));}  // explicit gradients keep the mip level right at plate edges
 vec2 curved(vec2 q,float k){vec2 c=q*2.0-1.0;c*=(1.0+k*dot(c,c))/(1.0+k);return c*0.5+0.5;}
 vec2 unbulge(vec2 p,vec4 r,vec2 b){if(b.x<=0.0&&b.y<=0.0)return p;vec2 h=r.zw*0.5;vec2 c=r.xy+h;vec2 u=(p-c)/h;
  float sx=1.0+b.x*max(0.0,1.0-u.y*u.y);float sy=1.0+b.y*max(0.0,1.0-u.x*u.x);return c+vec2(u.x/sx,u.y/sy)*h;}
 float shapeMaskB(vec2 p,vec4 r,vec4 s,vec2 b){if(r.z<=0.0||r.w<=0.0)return 0.0;p=unbulge(p,r,b);if(s.x<0.5)return inside(p,r)?1.0:0.0;
  vec2 h=r.zw*0.5;float rad=clamp(s.y,0.0,0.5)*min(r.z,r.w);vec2 d=max(abs(p-(r.xy+h))-(h-rad),0.0);
  float n=s.x>1.5?max(s.z,2.0):2.0;float sd=pow(pow(d.x,n)+pow(d.y,n),1.0/n)-rad;return 1.0-smoothstep(-0.75,0.75,sd);}
 float shapeMask(vec2 p,vec4 r,vec4 s){return shapeMaskB(p,r,s,vec2(0.0));}
 vec3 blurred(sampler2D t,vec2 q,float lod){vec3 s=vec3(0.0);float o=0.035;for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)s+=textureLod(t,clamp(q+vec2(float(x),float(y))*o,0.0,1.0),lod).rgb;return s/9.0;}
 float lightLod(sampler2D shaded,sampler2D raw){vec2 cell=vec2(textureSize(shaded,0))/vec2(textureSize(raw,0));return log2(max(max(cell.x,cell.y),1.0));}  // one source pixel of the shaded picture, from its mip chain: the colour on screen without the mask, grid or scanlines
 float glowLod(sampler2D t){return log2(max(0.035*float(max(textureSize(t,0).x,textureSize(t,0).y)),1.0));}  // the mip whose texel spans one of blurred()'s steps: the glow is the picture's light, never its mask point-sampled through the bend
 float pictureLod(sampler2D t,vec4 rect,float rot){vec2 size=vec2(textureSize(t,0));vec2 span=(rot>0.5&&rot<1.5)||rot>2.5?rect.wz:rect.zw;return max(log2(max(size.x/span.x,size.y/span.y)),0.0);}  // 0 at 1x and above: the base level point-sampled; below 1x the mips average
 vec3 bentPicture(sampler2D t,vec2 p,vec4 tube,vec4 rect,vec4 fx,float rot,float lod){vec2 o[4]=vec2[4](vec2(0.15625,0.46875),vec2(-0.46875,0.15625),vec2(-0.15625,-0.46875),vec2(0.46875,-0.15625));vec3 s=vec3(0.0);  // the bend's scale drifts across the tube, so one tap lands on a fine mask's texel centres here and between them there, and its contrast rises and falls in rings: four rotated-grid taps over a 1.25 px footprint, each through the bend, keep it even
  for(int i=0;i<4;i++){vec2 g=uv(tube.xy+curved(uv(p+o[i],tube),fx.x)*tube.zw,rect);s+=textureLod(t,rotatedUv(clamp(g,0.0,1.0),rot),lod).rgb;}return s*0.25;}
 vec3 screenColor(sampler2D t,vec2 p,vec4 tube,vec4 rect,vec4 fx,vec4 surround,float rot,float edge){  // edge: how many pixels past the bent edge the picture still reaches, so a lip's antialiasing blends from it and never from the surround
  vec2 q0=uv(p,tube);vec2 q1=curved(q0,fx.x);bool on=q1.x>=0.0&&q1.x<=1.0&&q1.y>=0.0&&q1.y<=1.0;
  vec2 pc=tube.xy+q1*tube.zw;vec2 gq=uv(pc,rect);vec2 past=(gq-clamp(gq,0.0,1.0))*rect.zw;bool game=(on&&gq.x>=0.0&&gq.x<=1.0&&gq.y>=0.0&&gq.y<=1.0)||(edge>0.0&&dot(past,past)<=edge*edge);  // inside by comparison, never by a zero residue: compilers fuse (gq-clamp(gq))*size into multiply-adds that leave one, and every screen without a lip (computed layouts, bezels off) went black
  vec3 c=surround.rgb;
  if(game){vec2 q=rotatedUv(clamp(gq,0.0,1.0),rot);float lod=pictureLod(t,rect,rot);c=fx.x>0.0?bentPicture(t,p,tube,rect,fx,rot,lod):textureLod(t,q,lod).rgb;if(fx.z>0.0){vec3 b=blurred(t,q,glowLod(t));c=mix(c,max(c,b),min(fx.z*1.3,1.0));}}  // the full-size picture; a flat lane keeps its single tap on the texel centres
  else if(!on)c=vec3(0.0);
  vec2 cc=q1*2.0-1.0;c*=1.0-fx.y*smoothstep(0.45,1.7,dot(cc,cc));
  return c;}
 vec3 glassOver(vec3 c,sampler2D g,vec2 p,vec4 tube,float reflect,float hasGlass){if(hasGlass<0.5)return c;vec2 gu=uv(p,tube);gu.y=1.0-gu.y;vec4 gl=textureGrad(g,gu,vec2(1.0/tube.z,0.0),vec2(0.0,-1.0/tube.w));return 1.0-(1.0-c)*(1.0-gl.rgb*gl.a*clamp(reflect,0.0,1.0));}  // one glass over the whole opening: picture, corner wedges and lip alike
 vec4 frameRing(vec2 p,vec4 tube,vec4 shape){float w=uFrame.x;vec4 outer=vec4(tube.xy-w,tube.zw+2.0*w);vec4 os=vec4(shape.x>0.5?shape.x:1.0,uFrame.y,shape.z,0.0);
  float mo=shapeMask(p,outer,os);float mi=shapeMask(p,tube,shape);float ring=mo*(1.0-mi);if(ring<=0.0)return vec4(0.0);
  vec2 h=outer.zw*0.5;vec2 d=abs(p-(outer.xy+h))/h;float edge=max(d.x,d.y);
  float shade=0.7+0.4*smoothstep(0.82,1.0,edge);float bevel=smoothstep(0.985,1.0,edge)*0.22;
  return vec4(uFrameColor.rgb*shade+bevel,ring);}
 vec3 lipColor(vec2 p,vec4 outer,vec4 color){vec2 h=outer.zw*0.5;vec2 d=abs(p-(outer.xy+h))/h;float edge=max(d.x,d.y);return color.rgb*(0.7+0.4*smoothstep(0.82,1.0,edge))+smoothstep(0.985,1.0,edge)*0.22;}  // the lip's paint, as bezelRing shades it
 vec2 bentUv(vec2 p,vec4 tube,vec4 rect,vec4 fx){vec2 q=curved(uv(p,tube),fx.x);return uv(tube.xy+q*tube.zw,rect);}  // where p lands in the picture after the bend; outside 0..1 beyond the bent edge
 float edgeDistance(vec2 g,vec4 rect){vec2 e=clamp(g,0.0,1.0);vec2 depth=min(e,1.0-e)*rect.zw;return g==e?-min(depth.x,depth.y):length((g-e)*rect.zw);}  // signed pixels from the bent picture edge, negative inside
 vec3 reflectAt(sampler2D t,sampler2D raw,vec2 p,vec4 tube,vec4 rect,vec4 outer,vec4 fx,float rot,vec4 look){if(look.x<=0.0)return vec3(0.0);  // the bent picture mirrored across its bent edge: sharp there, blurrier and fainter outward
  vec2 g=bentUv(p,tube,rect,fx);vec2 e=clamp(g,0.0,1.0);if(g==e)return vec3(0.0);float light=lightLod(t,raw);float shaded=textureSize(t,0)==textureSize(raw,0)?0.0:1.0;vec2 o=(e-g)*rect.zw;vec2 src=e+(e-g)+o/max(length(o),1e-4)*shaded*0.003*min(rect.z,rect.w)/rect.zw;  // a shaded lane starts a few pixels in, past the dark rim its CRT shader leaves; the raw frame mirrors from its true edge
  vec2 span=max(vec2(g.x<0.0?rect.x-outer.x:outer.x+outer.z-rect.x-rect.z,g.y<0.0?rect.y-outer.y:outer.y+outer.w-rect.y-rect.w),vec2(1.0));float d=clamp(length((g-e)*rect.zw/span),0.0,1.0);  // each side fades over its own lip width
  float fade=1.0-look.z*smoothstep(0.0,1.0,d);vec2 spread=max(vec2(0.0006+mix(0.0,0.03,look.y)*d*d),1.0/rect.zw);vec2 reach=spread*0.5*vec2(textureSize(t,0));float lod=max(light,log2(max(max(reach.x,reach.y),1.0e-4)));vec3 c=vec3(0.0);  // at least a screen pixel; each tap averages its whole step, never a fan of copies
  for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++)c+=textureLod(t,rotatedUv(clamp(src+vec2(float(x),float(y))*spread*0.5,0.0,1.0),rot),lod).rgb;
  return c/25.0*look.x*fade;}
 vec4 bezelRing(vec2 p,vec4 inner,vec4 outer,vec4 look,vec4 color,vec2 b){if(look.z<0.5)return vec4(0.0);
  float ring=shapeMaskB(p,outer,vec4(1.0,look.y,2.0,0.0),b)*(1.0-shapeMaskB(p,inner,vec4(1.0,look.x,2.0,0.0),b));if(ring<=0.0)return vec4(0.0);
  vec2 h=outer.zw*0.5;vec2 d=abs(p-(outer.xy+h))/h;float edge=max(d.x,d.y);
  float shade=0.7+0.4*smoothstep(0.82,1.0,edge);float bevel=smoothstep(0.985,1.0,edge)*0.22;
  return vec4(color.rgb*shade+bevel,ring);}
 vec4 framePaint(sampler2D t,sampler2D raw,vec2 p,vec4 tube,vec4 rect,vec4 shape,vec4 fx,float rot,vec4 look){vec4 f=frameRing(p,tube,shape);if(f.a<=0.0)return f;vec4 outer=vec4(tube.xy-uFrame.x,tube.zw+2.0*uFrame.x);  // a drawn frame mirrors the picture like a lip: sharp at the edge, never its edge pixels smeared outward
  return vec4(1.0-(1.0-f.rgb)*(1.0-reflectAt(t,raw,p,tube,rect,outer,fx,rot,look)),f.a);}
void main(){
 vec2 p=gl_FragCoord.xy;float dual=uFlags.x;float hasArt=uFlags.y;float hasBg=uFlags.z;float layered=uFlags.w;
 if(uPass>3.5){if(!inside(p,uBezelRect))discard;vec4 t=plate(uBezel,p,uBezelRect);if(uLayerTint.w>0.0){float l=clamp((max(max(t.r,t.g),t.b)+min(min(t.r,t.g),t.b))*0.5*uLayerTint.w,0.0,1.0);t.rgb=l*uLayerTint.rgb;}t.rgb=mix(t.rgb,vec3(1.0),uLayer.z);float a=t.a*uLayer.x;if(uLayer.y>1.5)t.rgb*=a;frag=vec4(t.rgb,a);return;}
 if(uPass<0.5){vec3 c=vec3(0.0);if(hasBg>0.5&&inside(p,uBackgroundRect))c=plate(uBackground,p,uBackgroundRect).rgb;frag=vec4(c,1.0);return;}
 if(uPass<1.5){float m0=shapeMaskB(p,uTube,uShape,uBulge.xy);float m1=dual>0.5?shapeMaskB(p,uTube2,uShape2,uBulge.zw):0.0;
  if(m0<=0.0&&m1<=0.0)discard;
  vec3 c;float a;
  if(m0>=m1){bool lipAtEdge=layered>0.5&&uRingLook.z>0.5;c=screenColor(uGame,p,uTube,uRect,uFx,uSurround,uRotation.x,lipAtEdge?0.5:0.0);a=m0;
   if(uFrame.z>1.5){vec4 f=frameRing(p,uRect,vec4(1.0,uFrame.y,2.0,0.0));c=mix(c,f.rgb,f.a);}
   if(lipAtEdge){a=min(a,shapeMaskB(p,uRingOut,vec4(1.0,uRingLook.y,2.0,0.0),uBulge.xy));vec2 bg=bentUv(p,uTube,uRect,uFx);float off=clamp(edgeDistance(bg,uRect)+0.5,0.0,1.0);  // the lip starts at the bent picture edge: one surface, no second outline, antialiased half a pixel either side of it
    if(off>0.0){vec3 lip=lipColor(p,uRingOut,uRingColor)*uRingColor.a;vec3 f=reflectAt(uGame,uRaw,p,uTube,uRect,uRingOut,uFx,uRotation.x,uRingReflect);c=mix(c,1.0-(1.0-lip)*(1.0-f),off);}}
   else{vec4 r=bezelRing(p,uRingIn,uRingOut,uRingLook,uRingColor,uBulge.xy);if(r.a>0.0){vec3 lip=mix(c,r.rgb,uRingColor.a);if(uRingReflect.x>0.0){vec3 f=reflectAt(uGame,uRaw,p,uTube,uRect,uRingOut,uFx,uRotation.x,uRingReflect);lip=1.0-(1.0-lip)*(1.0-f);}c=mix(c,lip,r.a);}}c=glassOver(c,uGlass,p,uTube,uReflect.x,uReflect.z);}
  else{bool lipAtEdge=layered>0.5&&uRingLook2.z>0.5;c=screenColor(uGame2,p,uTube2,uRect2,uFx2,uSurround2,uRotation.y,lipAtEdge?0.5:0.0);a=m1;
   if(uFrame.z>1.5){vec4 f=frameRing(p,uRect2,vec4(1.0,uFrame.y,2.0,0.0));c=mix(c,f.rgb,f.a);}
   if(lipAtEdge){a=min(a,shapeMaskB(p,uRingOut2,vec4(1.0,uRingLook2.y,2.0,0.0),uBulge.zw));vec2 bg=bentUv(p,uTube2,uRect2,uFx2);float off=clamp(edgeDistance(bg,uRect2)+0.5,0.0,1.0);
    if(off>0.0){vec3 lip=lipColor(p,uRingOut2,uRingColor2)*uRingColor2.a;vec3 f=reflectAt(uGame2,uRaw2,p,uTube2,uRect2,uRingOut2,uFx2,uRotation.y,uRingReflect2);c=mix(c,1.0-(1.0-lip)*(1.0-f),off);}}
   else{vec4 r=bezelRing(p,uRingIn2,uRingOut2,uRingLook2,uRingColor2,uBulge.zw);if(r.a>0.0){vec3 lip=mix(c,r.rgb,uRingColor2.a);if(uRingReflect2.x>0.0){vec3 f=reflectAt(uGame2,uRaw2,p,uTube2,uRect2,uRingOut2,uFx2,uRotation.y,uRingReflect2);lip=1.0-(1.0-lip)*(1.0-f);}c=mix(c,lip,r.a);}}c=glassOver(c,uGlass2,p,uTube2,uReflect.y,uReflect.w);}
  frag=vec4(c,a);return;}
 if(uPass<2.5){vec4 c=vec4(0.0);
  if(hasArt>0.5&&inside(p,uBezelRect))c=plate(uBezel,p,uBezelRect);
  if(uFrame.z>0.5&&uFrame.z<1.5){vec4 f=framePaint(uGame,uRaw,p,uTube,uRect,uShape,uFx,uRotation.x,uRingReflect);c=mix(c,vec4(f.rgb,1.0),f.a);if(dual>0.5){vec4 f2=framePaint(uGame2,uRaw2,p,uTube2,uRect2,uShape2,uFx2,uRotation.y,uRingReflect2);c=mix(c,vec4(f2.rgb,1.0),f2.a);}}
  if(c.a<=0.0)discard;
  if(uRingLook.z>0.5){vec4 r=bezelRing(p,uRingIn,uRingOut,uRingLook,uRingColor,uBulge.xy);float band=r.a*(1.0-shapeMaskB(p,uTube,uShape,uBulge.xy));
   if(band>0.0){vec3 lip=mix(c.rgb,r.rgb,uRingColor.a);if(uRingReflect.x>0.0){vec3 f=reflectAt(uGame,uRaw,p,uTube,uRect,uRingOut,uFx,uRotation.x,uRingReflect);lip=1.0-(1.0-lip)*(1.0-f);}c.rgb=mix(c.rgb,lip,band);}}
  if(dual>0.5&&uRingLook2.z>0.5){vec4 r=bezelRing(p,uRingIn2,uRingOut2,uRingLook2,uRingColor2,uBulge.zw);float band=r.a*(1.0-shapeMaskB(p,uTube2,uShape2,uBulge.zw));
   if(band>0.0){vec3 lip=mix(c.rgb,r.rgb,uRingColor2.a);if(uRingReflect2.x>0.0){vec3 f=reflectAt(uGame2,uRaw2,p,uTube2,uRect2,uRingOut2,uFx2,uRotation.y,uRingReflect2);lip=1.0-(1.0-lip)*(1.0-f);}c.rgb=mix(c.rgb,lip,band);}}
  float mi=shapeMask(p,uTube,uShape);if(dual>0.5)mi=max(mi,shapeMask(p,uTube2,uShape2));c.a*=1.0-mi;
  frag=c;return;}
 if(uMenuOn<0.5||!inside(p,uMenuRect))discard;vec2 q=uv(p,uMenuRect);q.y=1.0-q.y;frag=texture(uMenu,q);
}
