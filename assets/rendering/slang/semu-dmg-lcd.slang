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

void main()
{
   vec3 source = texture(Source, vTexCoord).rgb;
   float luma = dot(source, vec3(0.299, 0.587, 0.114));

   vec3 dark = vec3(0.075, 0.145, 0.075);
   vec3 mid = vec3(0.400, 0.560, 0.220);
   vec3 light = vec3(0.735, 0.820, 0.430);
   vec3 dmg = mix(dark, mid, smoothstep(0.08, 0.58, luma));
   dmg = mix(dmg, light, smoothstep(0.48, 1.00, luma));

   vec2 pixel = fract(vTexCoord * global.OriginalSize.xy);
   float grid = step(0.075, pixel.x) * step(0.075, pixel.y);
   float column = 0.92 + 0.08 * step(0.50, pixel.x);
   float line = 0.94 + 0.06 * step(0.50, pixel.y);

   FragColor = vec4(dmg * grid * column * line, 1.0);
}
