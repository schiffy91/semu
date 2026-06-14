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
   color = mix(color, pow(color, vec3(1.12)) * vec3(0.92, 0.90, 0.84), 0.32);
   vec2 pixel = fract(lcdUv * global.OriginalSize.xy);
   float grid = step(0.040, pixel.x) * step(0.050, pixel.y);
   return color * grid;
}

void main()
{
   vec2 uv = vTexCoord;
   vec2 screenLo = vec2(0.195, 0.210);
   vec2 screenHi = vec2(0.805, 0.705);
   float inScreen = box(uv, screenLo, screenHi);

   vec3 shell = vec3(0.420, 0.250, 0.650);
   shell *= 0.90 + 0.10 * smoothstep(0.0, 1.0, uv.y);

   float frame = box(uv, vec2(0.035, 0.130), vec2(0.965, 0.840));
   float inner = box(uv, vec2(0.070, 0.165), vec2(0.930, 0.805));
   shell = mix(vec3(0.065, 0.060, 0.075), shell, frame);
   shell = mix(shell, vec3(0.300, 0.180, 0.500), frame * (1.0 - inner) * 0.55);

   float screenFrame = box(uv, screenLo - vec2(0.025, 0.025), screenHi + vec2(0.025, 0.025)) * (1.0 - inScreen);
   shell = mix(shell, vec3(0.075, 0.075, 0.085), screenFrame);

   float a = circle(uv, vec2(0.840, 0.485), 0.040);
   float b = circle(uv, vec2(0.765, 0.555), 0.034);
   shell = mix(shell, vec3(0.130, 0.090, 0.180), max(a, b));

   float dpadH = box(uv, vec2(0.105, 0.490), vec2(0.245, 0.535));
   float dpadV = box(uv, vec2(0.150, 0.445), vec2(0.195, 0.580));
   shell = mix(shell, vec3(0.070, 0.070, 0.080), max(dpadH, dpadV));

   vec2 lcdUv = clamp((uv - screenLo) / (screenHi - screenLo), vec2(0.0), vec2(1.0));
   vec3 screen = lcdColor(lcdUv, lcdUv);
   FragColor = vec4(mix(shell, screen, inScreen), 1.0);
}
