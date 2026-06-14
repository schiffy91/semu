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

float circle(vec2 uv, vec2 center, float radius)
{
   return 1.0 - smoothstep(radius - 0.006, radius + 0.006, length(uv - center));
}

void main()
{
   vec2 uv = vTexCoord;
   vec2 screenLo = vec2(0.205, 0.310);
   vec2 screenHi = vec2(0.795, 0.685);
   float inScreen = box(uv, screenLo, screenHi);

   vec3 shell = vec3(0.530, 0.030, 0.030);
   shell *= 0.82 + 0.18 * smoothstep(0.0, 1.0, uv.y);
   shell = mix(vec3(0.018, 0.018, 0.020), shell, box(uv, vec2(0.030, 0.225), vec2(0.970, 0.780)));

   float frame = box(uv, screenLo - vec2(0.018, 0.020), screenHi + vec2(0.018, 0.020)) * (1.0 - inScreen);
   shell = mix(shell, vec3(0.015, 0.015, 0.018), frame);

   shell = mix(shell, vec3(0.030, 0.030, 0.035), circle(uv, vec2(0.115, 0.500), 0.072));
   shell = mix(shell, vec3(0.035, 0.035, 0.040), circle(uv, vec2(0.885, 0.500), 0.060));
   shell = mix(shell, vec3(0.750, 0.720, 0.640), box(uv, vec2(0.458, 0.725), vec2(0.542, 0.738)));

   vec2 lcdUv = clamp((uv - screenLo) / (screenHi - screenLo), vec2(0.0), vec2(1.0));
   vec3 color = texture(Source, lcdUv).rgb;
   vec2 pixel = fract(lcdUv * global.OriginalSize.xy);
   float grid = step(0.035, pixel.x) * step(0.035, pixel.y);
   FragColor = vec4(mix(shell, color * grid, inScreen), 1.0);
}
