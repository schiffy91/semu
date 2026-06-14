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

vec3 lcdColor(vec2 sampleUv, vec2 lcdUv)
{
   vec3 color = texture(Source, sampleUv).rgb;
   color = pow(color, vec3(0.94));
   vec2 pixel = fract(lcdUv * global.OriginalSize.xy);
   float grid = step(0.055, pixel.x) * step(0.055, pixel.y);
   float sub = 0.94 + 0.06 * sin(pixel.x * 18.8496);
   return color * grid * sub;
}

void main()
{
   vec2 uv = vTexCoord;
   vec2 screenLo = vec2(0.145, 0.108);
   vec2 screenHi = vec2(0.855, 0.710);
   float inScreen = box(uv, screenLo, screenHi);

   vec3 shell = vec3(0.440, 0.315, 0.720);
   shell *= 0.88 + 0.12 * smoothstep(0.0, 1.0, uv.y);

   float outer = box(uv, vec2(0.030, 0.025), vec2(0.970, 0.970));
   float inner = box(uv, vec2(0.055, 0.055), vec2(0.945, 0.940));
   shell = mix(shell, vec3(0.300, 0.210, 0.500), outer * (1.0 - inner) * 0.65);

   float screenFrame = box(uv, screenLo - vec2(0.025, 0.025), screenHi + vec2(0.025, 0.025)) * (1.0 - inScreen);
   shell = mix(shell, vec3(0.115, 0.115, 0.125), screenFrame);

   float a = circle(uv, vec2(0.770, 0.830), 0.040);
   float b = circle(uv, vec2(0.670, 0.865), 0.034);
   shell = mix(shell, vec3(0.105, 0.070, 0.155), max(a, b));

   float dpadH = box(uv, vec2(0.180, 0.805), vec2(0.325, 0.850));
   float dpadV = box(uv, vec2(0.230, 0.755), vec2(0.275, 0.900));
   shell = mix(shell, vec3(0.095, 0.095, 0.105), max(dpadH, dpadV));

   vec2 lcdUv = clamp((uv - screenLo) / (screenHi - screenLo), vec2(0.0), vec2(1.0));
   vec3 screen = lcdColor(lcdUv, lcdUv);
   FragColor = vec4(mix(shell, screen, inScreen), 1.0);
}
