#version 330 core
out vec4 frag;
uniform sampler2D uGame;uniform sampler2D uGame2;uniform sampler2D uBezel;uniform sampler2D uGlass;uniform sampler2D uGlass2;uniform sampler2D uBackground;uniform sampler2D uMenu;
uniform vec4 uRect;uniform vec4 uRect2;uniform vec4 uTube;uniform vec4 uTube2;uniform vec4 uShape;uniform vec4 uShape2;uniform vec4 uFx;uniform vec4 uFx2;
uniform vec4 uSurround;uniform vec4 uSurround2;uniform vec4 uReflect;uniform vec4 uBezelRect;uniform vec4 uBackgroundRect;uniform vec4 uMenuRect;
uniform vec4 uFlags;uniform vec4 uFrame;uniform vec4 uFrameColor;uniform vec4 uRotation;uniform float uPass;uniform float uMenuOn;
uniform vec4 uRingIn;uniform vec4 uRingIn2;uniform vec4 uRingOut;uniform vec4 uRingOut2;uniform vec4 uRingLook;uniform vec4 uRingLook2;uniform vec4 uRingColor;uniform vec4 uRingColor2;uniform vec4 uRingReflect;uniform vec4 uRingReflect2;uniform vec4 uRingReach;uniform vec4 uLayer;uniform vec4 uBulge;
 bool inside(vec2 p,vec4 r){return p.x>=r.x&&p.x<=r.x+r.z&&p.y>=r.y&&p.y<=r.y+r.w;}
 vec2 uv(vec2 p,vec4 r){return(p-r.xy)/r.zw;}
 vec2 rotatedUv(vec2 q,float r){if(r<0.5)return q;if(r<1.5)return vec2(1.0-q.y,q.x);if(r<2.5)return vec2(1.0-q.x,1.0-q.y);return vec2(q.y,1.0-q.x);}
 vec2 artUv(vec2 p,vec4 r){return vec2((p.x-r.x)/r.z,1.0-(p.y-r.y)/r.w);}
 vec2 curved(vec2 q,float k){vec2 c=q*2.0-1.0;c*=(1.0+k*dot(c,c))/(1.0+k);return c*0.5+0.5;}
 vec2 unbulge(vec2 p,vec4 r,vec2 b){if(b.x<=0.0&&b.y<=0.0)return p;vec2 h=r.zw*0.5;vec2 c=r.xy+h;vec2 u=(p-c)/h;
  float sx=1.0+b.x*max(0.0,1.0-u.y*u.y);float sy=1.0+b.y*max(0.0,1.0-u.x*u.x);return c+vec2(u.x/sx,u.y/sy)*h;}
 float shapeMaskB(vec2 p,vec4 r,vec4 s,vec2 b){if(r.z<=0.0||r.w<=0.0)return 0.0;p=unbulge(p,r,b);if(s.x<0.5)return inside(p,r)?1.0:0.0;
  vec2 h=r.zw*0.5;float rad=clamp(s.y,0.0,0.5)*min(r.z,r.w);vec2 d=max(abs(p-(r.xy+h))-(h-rad),0.0);
  float n=s.x>1.5?max(s.z,2.0):2.0;float sd=pow(pow(d.x,n)+pow(d.y,n),1.0/n)-rad;return 1.0-smoothstep(-0.75,0.75,sd);}
 float shapeMask(vec2 p,vec4 r,vec4 s){return shapeMaskB(p,r,s,vec2(0.0));}
 vec3 blurred(sampler2D t,vec2 q){vec3 s=vec3(0.0);float o=0.035;for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)s+=texture(t,clamp(q+vec2(float(x),float(y))*o,0.0,1.0)).rgb;return s/9.0;}
 vec3 screenColor(sampler2D t,sampler2D g,vec2 p,vec4 tube,vec4 rect,vec4 fx,vec4 surround,float rot,float reflect,float hasGlass){
  vec2 q0=uv(p,tube);vec2 q1=curved(q0,fx.x);bool on=q1.x>=0.0&&q1.x<=1.0&&q1.y>=0.0&&q1.y<=1.0;
  vec2 pc=tube.xy+q1*tube.zw;vec2 gq=uv(pc,rect);bool game=on&&gq.x>=0.0&&gq.x<=1.0&&gq.y>=0.0&&gq.y<=1.0;
  vec3 c=surround.rgb;
  if(game){vec2 q=rotatedUv(gq,rot);c=texture(t,q).rgb;if(fx.z>0.0){vec3 b=blurred(t,q);c+=fx.z*max(b-0.4,0.0)*1.3;}}
  else if(!on)c=vec3(0.0);
  vec2 cc=q1*2.0-1.0;c*=1.0-fx.y*smoothstep(0.45,1.7,dot(cc,cc));
  if(hasGlass>0.5){vec2 gu=q0;gu.y=1.0-gu.y;vec4 gl=texture(g,gu);c=1.0-(1.0-c)*(1.0-gl.rgb*gl.a*clamp(reflect,0.0,1.0));}
  return c;}
 float halo(vec2 p,vec4 r,out vec2 q){q=clamp(uv(p,r),0.0,1.0);vec2 n=r.xy+q*r.zw;float d=length(p-n);float band=0.16*min(r.z,r.w);float k=1.0-clamp(d/band,0.0,1.0);return k*k;}
 vec4 frameRing(vec2 p,vec4 tube,vec4 shape){float w=uFrame.x;vec4 outer=vec4(tube.xy-w,tube.zw+2.0*w);vec4 os=vec4(shape.x>0.5?shape.x:1.0,uFrame.y,shape.z,0.0);
  float mo=shapeMask(p,outer,os);float mi=shapeMask(p,tube,shape);float ring=mo*(1.0-mi);if(ring<=0.0)return vec4(0.0);
  vec2 h=outer.zw*0.5;vec2 d=abs(p-(outer.xy+h))/h;float edge=max(d.x,d.y);
  float shade=0.7+0.4*smoothstep(0.82,1.0,edge);float bevel=smoothstep(0.985,1.0,edge)*0.22;
  return vec4(uFrameColor.rgb*shade+bevel,ring);}
 vec3 ringReflection(sampler2D t,vec2 p,vec4 inner,vec4 rect,vec4 outer,vec3 edge,float rot,vec4 look){vec2 q=uv(p,inner);vec2 o=max(max(-q,q-1.0),vec2(0.0));
  vec2 m=q;if(q.x<0.0)m.x=-q.x;else if(q.x>1.0)m.x=2.0-q.x;if(q.y<0.0)m.y=-q.y;else if(q.y>1.0)m.y=2.0-q.y;m=clamp(m,0.0,1.0);
  vec2 thick=max((outer.zw-inner.zw)*0.5,vec2(1.0));float d=clamp(length(o*inner.zw/thick),0.0,1.0);
  float falloff=pow(1.0-d,1.6);float fade=1.0-look.z*(1.0-falloff);
  float spread=0.0004+mix(0.0,0.008,look.y)*d*d;vec3 direct=vec3(0.0);
  for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){vec2 mp=inner.xy+clamp(m+vec2(float(x),float(y))*spread*0.5,0.0,1.0)*inner.zw;vec2 g=uv(mp,rect);
   direct+=(g.x>=0.0&&g.x<=1.0&&g.y>=0.0&&g.y<=1.0)?texture(t,rotatedUv(g,rot)).rgb:edge;}
  direct/=25.0;vec3 diffused=vec3(0.0);
  for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)diffused+=texture(t,rotatedUv(vec2(0.5)+vec2(float(x),float(y))*0.3,rot)).rgb;
  diffused/=9.0;return (direct*0.88+diffused*0.12)*look.x*fade;}
 vec4 reachRect(vec4 outer,vec2 reach){vec2 grow=outer.zw*reach;return vec4(outer.xy-grow,outer.zw+2.0*grow);}
 float reachBand(vec2 p,vec4 outer,vec2 reach,float radius,vec2 b){if(reach.x<=0.0&&reach.y<=0.0)return 0.0;vec4 far=reachRect(outer,reach);
  return shapeMaskB(p,far,vec4(1.0,radius,2.0,0.0),b)*(1.0-shapeMaskB(p,outer,vec4(1.0,radius,2.0,0.0),b));}
 vec4 bezelRing(vec2 p,vec4 inner,vec4 outer,vec4 look,vec4 color,vec2 b){if(look.z<0.5)return vec4(0.0);
  float ring=shapeMaskB(p,outer,vec4(1.0,look.y,2.0,0.0),b)*(1.0-shapeMaskB(p,inner,vec4(1.0,look.x,2.0,0.0),b));if(ring<=0.0)return vec4(0.0);
  vec2 h=outer.zw*0.5;vec2 d=abs(p-(outer.xy+h))/h;float edge=max(d.x,d.y);
  float shade=0.7+0.4*smoothstep(0.82,1.0,edge);float bevel=smoothstep(0.985,1.0,edge)*0.22;
  return vec4(color.rgb*shade+bevel,ring);}
void main(){
 vec2 p=gl_FragCoord.xy;float dual=uFlags.x;float hasArt=uFlags.y;float hasBg=uFlags.z;float layered=uFlags.w;
 if(uPass>3.5){if(!inside(p,uBezelRect))discard;vec4 t=texture(uBezel,artUv(p,uBezelRect));if(uLayer.y>0.5&&uLayer.y<1.5)t.rgb*=t.a;frag=vec4(t.rgb,t.a*uLayer.x);return;}
 if(uPass<0.5){vec3 c=vec3(0.0);if(hasBg>0.5&&inside(p,uBackgroundRect))c=texture(uBackground,artUv(p,uBackgroundRect)).rgb;frag=vec4(c,1.0);return;}
 if(uPass<1.5){float m0=shapeMaskB(p,uTube,uShape,uBulge.xy);float m1=dual>0.5?shapeMaskB(p,uTube2,uShape2,uBulge.zw):0.0;
  if(layered>0.5){m0=max(m0,uRingLook.z>0.5&&uRingReflect.x>0.0?reachBand(p,uRingOut,uRingReach.xy,uRingLook.y,uBulge.xy):0.0);if(dual>0.5)m1=max(m1,uRingLook2.z>0.5&&uRingReflect2.x>0.0?reachBand(p,uRingOut2,uRingReach.zw,uRingLook2.y,uBulge.zw):0.0);}
  if(m0<=0.0&&m1<=0.0)discard;
  vec3 c;float a;
  if(m0>=m1){c=screenColor(uGame,uGlass,p,uTube,uRect,uFx,uSurround,uRotation.x,uReflect.x,uReflect.z);a=m0;
   if(uFrame.z>1.5){vec4 f=frameRing(p,uRect,vec4(1.0,uFrame.y,2.0,0.0));c=mix(c,f.rgb,f.a);}
   vec4 far=reachRect(uRingOut,uRingReach.xy);float beyond=uRingLook.z>0.5&&uRingReflect.x>0.0?reachBand(p,uRingOut,uRingReach.xy,uRingLook.y,uBulge.xy):0.0;
   if(layered>0.5&&uRingLook.z>0.5){if(!inside(p,uRect))c=vec3(0.0);a=min(a,shapeMaskB(p,uRingOut,vec4(1.0,uRingLook.y,2.0,0.0),uBulge.xy));}
   vec4 r=bezelRing(p,uRingIn,uRingOut,uRingLook,uRingColor,uBulge.xy);c=mix(c,r.rgb,r.a*uRingColor.a);if(r.a>0.0&&uRingReflect.x>0.0){vec3 f=ringReflection(uGame,p,uRect,uRect,far,vec3(0.0),uRotation.x,uRingReflect);c=mix(c,1.0-(1.0-c)*(1.0-f),r.a);}
   if(layered>0.5&&beyond>0.0){vec3 f=ringReflection(uGame,p,uRect,uRect,far,vec3(0.0),uRotation.x,uRingReflect);c=f;a=max(a,max(f.r,max(f.g,f.b))*beyond);}}
  else{c=screenColor(uGame2,uGlass2,p,uTube2,uRect2,uFx2,uSurround2,uRotation.y,uReflect.y,uReflect.w);a=m1;
   if(uFrame.z>1.5){vec4 f=frameRing(p,uRect2,vec4(1.0,uFrame.y,2.0,0.0));c=mix(c,f.rgb,f.a);}
   vec4 far=reachRect(uRingOut2,uRingReach.zw);float beyond=uRingLook2.z>0.5&&uRingReflect2.x>0.0?reachBand(p,uRingOut2,uRingReach.zw,uRingLook2.y,uBulge.zw):0.0;
   if(layered>0.5&&uRingLook2.z>0.5){if(!inside(p,uRect2))c=vec3(0.0);a=min(a,shapeMaskB(p,uRingOut2,vec4(1.0,uRingLook2.y,2.0,0.0),uBulge.zw));}
   vec4 r=bezelRing(p,uRingIn2,uRingOut2,uRingLook2,uRingColor2,uBulge.zw);c=mix(c,r.rgb,r.a*uRingColor2.a);if(r.a>0.0&&uRingReflect2.x>0.0){vec3 f=ringReflection(uGame2,p,uRect2,uRect2,far,vec3(0.0),uRotation.y,uRingReflect2);c=mix(c,1.0-(1.0-c)*(1.0-f),r.a);}
   if(layered>0.5&&beyond>0.0){vec3 f=ringReflection(uGame2,p,uRect2,uRect2,far,vec3(0.0),uRotation.y,uRingReflect2);c=f;a=max(a,max(f.r,max(f.g,f.b))*beyond);}}
  frag=vec4(c,a);return;}
 if(uPass<2.5){vec4 c=vec4(0.0);
  if(hasArt>0.5&&inside(p,uBezelRect))c=texture(uBezel,artUv(p,uBezelRect));
  if(uFrame.z>0.5&&uFrame.z<1.5){vec4 f=frameRing(p,uTube,uShape);c=mix(c,vec4(f.rgb,1.0),f.a);if(dual>0.5){vec4 f2=frameRing(p,uTube2,uShape2);c=mix(c,vec4(f2.rgb,1.0),f2.a);}}
  if(c.a<=0.0)discard;
  vec2 q;float k=halo(p,uTube,q);vec3 g=blurred(uGame,rotatedUv(q,uRotation.x))*k*uFx.w;
  if(dual>0.5){vec2 q2;float k2=halo(p,uTube2,q2);g=max(g,blurred(uGame2,rotatedUv(q2,uRotation.y))*k2*uFx2.w);}
  c.rgb=1.0-(1.0-c.rgb)*(1.0-g);
  if(uRingLook.z>0.5){vec4 r=bezelRing(p,uRingIn,uRingOut,uRingLook,uRingColor,uBulge.xy);float outsideTube=1.0-shapeMaskB(p,uTube,uShape,uBulge.xy);float band=r.a*outsideTube;
   vec4 far=reachRect(uRingOut,uRingReach.xy);float glow=(band+reachBand(p,uRingOut,uRingReach.xy,uRingLook.y,uBulge.xy))*outsideTube;
   if(band>0.0)c.rgb=mix(c.rgb,r.rgb,band*uRingColor.a);
   if(glow>0.0&&uRingReflect.x>0.0){vec3 f=ringReflection(uGame,p,uRect,uRect,far,vec3(0.0),uRotation.x,uRingReflect);c.rgb=mix(c.rgb,1.0-(1.0-c.rgb)*(1.0-f),clamp(glow,0.0,1.0));}}
  if(dual>0.5&&uRingLook2.z>0.5){vec4 r=bezelRing(p,uRingIn2,uRingOut2,uRingLook2,uRingColor2,uBulge.zw);float outsideTube=1.0-shapeMaskB(p,uTube2,uShape2,uBulge.zw);float band=r.a*outsideTube;
   vec4 far=reachRect(uRingOut2,uRingReach.zw);float glow=(band+reachBand(p,uRingOut2,uRingReach.zw,uRingLook2.y,uBulge.zw))*outsideTube;
   if(band>0.0)c.rgb=mix(c.rgb,r.rgb,band*uRingColor2.a);
   if(glow>0.0&&uRingReflect2.x>0.0){vec3 f=ringReflection(uGame2,p,uRect2,uRect2,far,vec3(0.0),uRotation.y,uRingReflect2);c.rgb=mix(c.rgb,1.0-(1.0-c.rgb)*(1.0-f),clamp(glow,0.0,1.0));}}
  float mi=shapeMask(p,uTube,uShape);if(dual>0.5)mi=max(mi,shapeMask(p,uTube2,uShape2));c.a*=1.0-mi;
  frag=c;return;}
 if(uMenuOn<0.5||!inside(p,uMenuRect))discard;vec2 q=uv(p,uMenuRect);q.y=1.0-q.y;frag=texture(uMenu,q);
}
