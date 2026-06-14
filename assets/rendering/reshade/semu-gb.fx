#include "ReShade.fxh"
#include "semu-common.fxh"

float semuFrame(float2 uv)
{
    float l = 1.0 - smoothstep(0.070, 0.092, uv.x);
    float r = smoothstep(0.908, 0.930, uv.x);
    float t = 1.0 - smoothstep(0.060, 0.084, uv.y);
    float b = smoothstep(0.916, 0.940, uv.y);
    return saturate(max(max(l, r), max(t, b)));
}

void SemuGameBoy(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float luma = dot(color.rgb, float3(0.299, 0.587, 0.114));
    float content = semuContentMask(uv, 0.006);
    float gridX = 0.90 + 0.10 * sin(uv.x * BUFFER_WIDTH * 0.50 * 3.14159265);
    float gridY = 0.88 + 0.12 * sin(uv.y * BUFFER_HEIGHT * 0.72 * 3.14159265);
    float shade = saturate(luma * gridX * gridY + 0.035 * sin((uv.x + uv.y) * 16.0));
    float3 lcd = lerp(float3(0.20, 0.29, 0.11), float3(0.64, 0.74, 0.39), shade);
    float bevel = smoothstep(0.0, 1.0, uv.x * (1.0 - uv.y));
    float3 shell = lerp(float3(0.44, 0.43, 0.39), float3(0.78, 0.76, 0.68), bevel);
    output = float4(lerp(lerp(color.rgb, lcd, content), shell, semuFrameMask(uv, 0.012)), color.a);
}

technique SemuGameBoy
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuGameBoy;
    }
}
