#include "ReShade.fxh"
#include "semu-common.fxh"

float semuFrame(float2 uv)
{
    float l = 1.0 - smoothstep(0.050, 0.075, uv.x);
    float r = smoothstep(0.925, 0.950, uv.x);
    float t = 1.0 - smoothstep(0.060, 0.085, uv.y);
    float b = smoothstep(0.915, 0.940, uv.y);
    return saturate(max(max(l, r), max(t, b)));
}

void SemuPsp(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float content = semuContentMask(uv, 0.006);
    float gridX = 0.97 + 0.03 * sin(uv.x * BUFFER_WIDTH * 3.14159265);
    float gridY = 0.97 + 0.03 * sin(uv.y * BUFFER_HEIGHT * 0.90 * 3.14159265);
    color.rgb *= lerp(1.0, gridX * gridY, content);
    float redAccent = smoothstep(0.00, 0.28, uv.x) * (1.0 - smoothstep(0.44, 0.80, uv.x));
    float3 shell = lerp(float3(0.015, 0.015, 0.018), float3(0.42, 0.02, 0.025), redAccent);
    output = float4(lerp(color.rgb, shell, semuFrameMask(uv, 0.012)), color.a);
}

technique SemuPsp
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuPsp;
    }
}
