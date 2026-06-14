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

vec3 dmgColor(vec2 sampleUv, vec2 lcdUv)
{
   vec3 source = texture(Source, sampleUv).rgb;
   float luma = dot(source, vec3(0.299, 0.587, 0.114));
   vec3 dark = vec3(0.075, 0.145, 0.075);
   vec3 mid = vec3(0.400, 0.560, 0.220);
   vec3 light = vec3(0.735, 0.820, 0.430);
   vec3 dmg = mix(dark, mid, smoothstep(0.08, 0.58, luma));
   dmg = mix(dmg, light, smoothstep(0.48, 1.00, luma));

   vec2 pixel = fract(lcdUv * global.OriginalSize.xy);
   float grid = step(0.075, pixel.x) * step(0.075, pixel.y);
   float column = 0.92 + 0.08 * step(0.50, pixel.x);
   float line = 0.94 + 0.06 * step(0.50, pixel.y);
   return dmg * grid * column * line;
}

void main()
{
   vec2 uv = vTexCoord;
   vec2 screenLo = vec2(0.145, 0.105);
   vec2 screenHi = vec2(0.855, 0.705);
   float inScreen = box(uv, screenLo, screenHi);

   vec3 shell = vec3(0.560, 0.565, 0.535);
   shell *= 0.92 + 0.08 * smoothstep(0.0, 1.0, uv.y);

   float outer = box(uv, vec2(0.030, 0.025), vec2(0.970, 0.970));
   float inner = box(uv, vec2(0.055, 0.055), vec2(0.945, 0.940));
   float bevel = outer * (1.0 - inner);
   shell = mix(shell, vec3(0.315, 0.320, 0.300), bevel * 0.55);

   float screenFrame = box(uv, screenLo - vec2(0.025, 0.025), screenHi + vec2(0.025, 0.025)) * (1.0 - inScreen);
   shell = mix(shell, vec3(0.150, 0.155, 0.145), screenFrame);

   float a = circle(uv, vec2(0.770, 0.830), 0.040);
   float b = circle(uv, vec2(0.670, 0.865), 0.034);
   shell = mix(shell, vec3(0.185, 0.070, 0.105), max(a, b));

   float dpadH = box(uv, vec2(0.180, 0.805), vec2(0.325, 0.850));
   float dpadV = box(uv, vec2(0.230, 0.755), vec2(0.275, 0.900));
   shell = mix(shell, vec3(0.115, 0.120, 0.115), max(dpadH, dpadV));

   float start = box(uv, vec2(0.430, 0.865), vec2(0.520, 0.885));
   float select = box(uv, vec2(0.305, 0.865), vec2(0.395, 0.885));
   shell = mix(shell, vec3(0.275, 0.280, 0.265), max(start, select));

   vec2 lcdUv = clamp((uv - screenLo) / (screenHi - screenLo), vec2(0.0), vec2(1.0));
   vec3 screen = dmgColor(lcdUv, lcdUv);
   FragColor = vec4(mix(shell, screen, inScreen), 1.0);
}
