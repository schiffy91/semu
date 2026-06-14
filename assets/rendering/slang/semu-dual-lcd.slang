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

vec3 lcd(vec2 sampleUv, vec2 localUv)
{
   vec3 color = texture(Source, sampleUv).rgb;
   vec2 pixel = fract(localUv * global.OriginalSize.xy);
   float grid = step(0.045, pixel.x) * step(0.045, pixel.y);
   return color * grid;
}

void main()
{
   vec2 uv = vTexCoord;
   vec2 topLo = vec2(0.185, 0.080);
   vec2 topHi = vec2(0.815, 0.445);
   vec2 bottomLo = vec2(0.185, 0.555);
   vec2 bottomHi = vec2(0.815, 0.920);
   float inTop = box(uv, topLo, topHi);
   float inBottom = box(uv, bottomLo, bottomHi);

   vec3 shell = vec3(0.045, 0.045, 0.052);
   shell = mix(shell, vec3(0.155, 0.155, 0.165), box(uv, vec2(0.100, 0.035), vec2(0.900, 0.965)));
   shell = mix(shell, vec3(0.030, 0.030, 0.036), box(uv, topLo - vec2(0.020), topHi + vec2(0.020)) * (1.0 - inTop));
   shell = mix(shell, vec3(0.030, 0.030, 0.036), box(uv, bottomLo - vec2(0.020), bottomHi + vec2(0.020)) * (1.0 - inBottom));
   shell = mix(shell, vec3(0.090, 0.090, 0.100), box(uv, vec2(0.130, 0.482), vec2(0.870, 0.518)));

   vec3 top = lcd(clamp((uv - topLo) / (topHi - topLo), vec2(0.0), vec2(1.0)) * vec2(1.0, 0.5), clamp((uv - topLo) / (topHi - topLo), vec2(0.0), vec2(1.0)));
   vec3 bottom = lcd(vec2(clamp((uv - bottomLo) / (bottomHi - bottomLo), vec2(0.0), vec2(1.0)).x, 0.5 + 0.5 * clamp((uv - bottomLo) / (bottomHi - bottomLo), vec2(0.0), vec2(1.0)).y), clamp((uv - bottomLo) / (bottomHi - bottomLo), vec2(0.0), vec2(1.0)));
   vec3 color = mix(shell, top, inTop);
   color = mix(color, bottom, inBottom);
   FragColor = vec4(color, 1.0);
}
