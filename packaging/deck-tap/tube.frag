// tube.frag — source-of-truth for the Semu compositor fragment shader (v8).
// Mirrors the FS string embedded in libsemutap.c. GEOMETRY: game-first integer scaling.
//   uRect = the LARGEST-INTEGER-scaled, CENTERED game rect (computed in C from native + fb).
//   uSrc  = the emulator's reported content rect (the actual game pixels), GL bottom-left.
//   uHole = the bezel art's TV-glass rect (normalized, top-left); the art is scaled so this
//           hole lands exactly on uRect. Keep uHole the game's display aspect (4:3 n64) → uniform.
// Inside uRect: draw the game (CRT/LCD fx, rounded corners). Outside: sample the art, hole-mapped.
#version 330 core
out vec4 frag;
uniform sampler2D uGame; uniform sampler2D uBezel;
uniform vec2 uWin; uniform vec4 uRect; uniform vec4 uSrc; uniform vec4 uHole;
uniform float uNative; uniform float uStyle; uniform float uHasArt; uniform vec3 uShell;
uniform vec4 uTV;  // y=mask z=scanlineDepth w=bezelWidthFrac
uniform vec4 uRef; // z=halation w=cornerRadius
float sdRound(vec2 p, vec2 b, float r){ vec2 d=abs(p)-b+r; return length(max(d,vec2(0.0)))+min(max(d.x,d.y),0.0)-r; }
vec3 gameAt(vec2 uv, float lod){ return textureLod(uGame,(uSrc.xy+uv*uSrc.zw)/uWin,lod).rgb; }
void main(){
  vec2 px=gl_FragCoord.xy; vec2 cen=uRect.xy+uRect.zw*0.5; vec2 hf=uRect.zw*0.5;
  vec2 c=(px-cen)/hf;                                          // -1..1 across the integer game rect
  bool inGame = sdRound(c, vec2(1.0), clamp(uRef.w,0.0,0.4)) < 0.0;
  if(inGame){
    vec2 uv=c*0.5+0.5; vec3 g=gameAt(uv,0.0);
    if(uStyle<0.5){                                            // CRT
      g*=1.0-uTV.z*(1.0-abs(cos(uv.y*uNative*3.14159265)));
      float m=mod(px.x,3.0); vec3 mask=(m<1.0)?vec3(1.0,0.7,0.7):(m<2.0)?vec3(0.7,1.0,0.7):vec3(0.7,0.7,1.0);
      g*=mix(vec3(1.0),mask,uTV.y); g+=gameAt(uv,3.0)*uRef.z;
    } else {                                                   // LCD
      float nx=uNative*uRect.z/uRect.w; g*=0.90+0.10*sqrt(abs(cos(uv.x*nx*3.14159265))*abs(cos(uv.y*uNative*3.14159265)));
    }
    frag=vec4(g,1.0); return;
  }
  vec3 base;
  if(uHasArt>0.5){                                             // map art so its HOLE == the game rect
    vec2 rel=(px-uRect.xy)/uRect.zw;                           // game-rect-relative (GL, y up)
    vec2 auv=uHole.xy + vec2(rel.x, 1.0-rel.y)*uHole.zw;       // -> art uv (stb top-left)
    base=texture(uBezel, auv).rgb;
  } else {
    vec2 outd=max(abs(c)-1.0,vec2(0.0)); float d=length(outd); float bevel=1.0-clamp(d/uTV.w,0.0,1.0);
    base = (uStyle<0.5) ? mix(vec3(0.04),vec3(0.14),bevel*bevel) : uShell*mix(0.78,1.06,bevel);
  }
  frag=vec4(base,1.0);
}
