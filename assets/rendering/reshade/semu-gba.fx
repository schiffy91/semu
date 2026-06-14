#include "ReShade.fxh"
#include "semu-common.fxh"

float semuFrame(float2 uv)
{
    float l = 1.0 - smoothstep(0.055, 0.080, uv.x);
    float r = smoothstep(0.920, 0.945, uv.x);
    float t = 1.0 - smoothstep(0.070, 0.092, uv.y);
    float b = smoothstep(0.908, 0.930, uv.y);
    return saturate(max(max(l, r), max(t, b)));
}

void SemuGameBoyAdvance(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float content = semuContentMask(uv, 0.006);
    float gridX = 0.95 + 0.05 * sin(uv.x * BUFFER_WIDTH * 0.75 * 3.14159265);
    float gridY = 0.95 + 0.05 * sin(uv.y * BUFFER_HEIGHT * 0.80 * 3.14159265);
    color.rgb *= lerp(1.0, gridX * gridY, content);
    float3 shell = lerp(float3(0.31, 0.24, 0.65), float3(0.55, 0.44, 0.89), smoothstep(0.0, 1.0, uv.x));
    output = float4(lerp(color.rgb, shell, semuFrameMask(uv, 0.012)), color.a);
}

technique SemuGameBoyAdvance
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuGameBoyAdvance;
    }
}
