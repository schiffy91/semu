#include "ReShade.fxh"
#include "semu-common.fxh"

float semuFrame(float2 uv)
{
    float l = 1.0 - smoothstep(0.060, 0.082, uv.x);
    float r = smoothstep(0.918, 0.940, uv.x);
    float t = 1.0 - smoothstep(0.050, 0.074, uv.y);
    float b = smoothstep(0.926, 0.950, uv.y);
    return saturate(max(max(l, r), max(t, b)));
}

void SemuGameBoyColor(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float content = semuContentMask(uv, 0.006);
    float gridX = 0.94 + 0.06 * sin(uv.x * BUFFER_WIDTH * 0.75 * 3.14159265);
    float gridY = 0.94 + 0.06 * sin(uv.y * BUFFER_HEIGHT * 0.80 * 3.14159265);
    color.rgb *= lerp(1.0, gridX * gridY, content);
    float3 shell = lerp(float3(0.39, 0.30, 0.58), float3(0.72, 0.62, 0.90), smoothstep(0.02, 0.92, uv.x));
    shell = lerp(shell, float3(0.17, 0.13, 0.25), smoothstep(0.86, 1.0, uv.y) * 0.28);
    output = float4(lerp(color.rgb, shell, semuFrameMask(uv, 0.012)), color.a);
}

technique SemuGameBoyColor
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuGameBoyColor;
    }
}
