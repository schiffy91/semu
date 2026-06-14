#version 450

layout(std140, set = 0, binding = 0) uniform UBO
{
   mat4 MVP;
   vec4 OutputSize;
   vec4 OriginalSize;
   vec4 SourceSize;
} global;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;

float box(vec2 uv, vec2 lo, vec2 hi)
{
   vec2 inside = step(lo, uv) * step(uv, hi);
   return inside.x * inside.y;
}

vec3 crtColor(vec2 sampleUv)
{
   vec3 color = texture(Source, sampleUv).rgb;
   float scan = 0.74 + 0.26 * sin(sampleUv.y * global.OriginalSize.y * 3.14159265);
   float mask = 0.90 + 0.10 * sin(sampleUv.x * global.OriginalSize.x * 6.2831853);
   vec2 centered = sampleUv * 2.0 - 1.0;
   float vignette = 1.0 - 0.18 * dot(centered, centered);
   return pow(color, vec3(1.08)) * scan * mask * vignette;
}

void main()
{
   vec2 uv = vTexCoord;
   vec2 screenLo = vec2(0.120, 0.110);
   vec2 screenHi = vec2(0.785, 0.885);
   float inScreen = box(uv, screenLo, screenHi);

   vec3 cabinet = vec3(0.050, 0.046, 0.040);
   cabinet *= 0.82 + 0.18 * smoothstep(0.0, 1.0, uv.y);
   float woodBand = box(uv, vec2(0.035, 0.060), vec2(0.940, 0.930));
   cabinet = mix(vec3(0.018, 0.018, 0.016), cabinet, woodBand);

   float screenFrame = box(uv, screenLo - vec2(0.035, 0.035), screenHi + vec2(0.035, 0.035)) * (1.0 - inScreen);
   cabinet = mix(cabinet, vec3(0.015, 0.015, 0.014), screenFrame);

   float sidePanel = box(uv, vec2(0.805, 0.170), vec2(0.930, 0.850));
   cabinet = mix(cabinet, vec3(0.090, 0.082, 0.068), sidePanel);
   float speaker = box(uv, vec2(0.825, 0.610), vec2(0.910, 0.790));
   cabinet = mix(cabinet, vec3(0.026, 0.025, 0.023), speaker);

   vec2 contentUv = clamp((uv - screenLo) / (screenHi - screenLo), vec2(0.0), vec2(1.0));
   vec2 curved = contentUv * 2.0 - 1.0;
   curved += curved * dot(curved, curved) * 0.028;
   curved = curved * 0.5 + 0.5;
   vec3 screen = crtColor(clamp(curved, vec2(0.0), vec2(1.0)));
   FragColor = vec4(mix(cabinet, screen, inScreen), 1.0);
}
