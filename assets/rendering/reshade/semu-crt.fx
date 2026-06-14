#include "ReShade.fxh"
#include "semu-common.fxh"

void SemuCrt(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float content = semuContentMask(uv, 0.006);
    float scanline = 0.86 + 0.14 * sin(uv.y * BUFFER_HEIGHT * 3.14159265);
    float mask = 0.92 + 0.08 * sin(uv.x * BUFFER_WIDTH * 3.14159265);
    float vignette = smoothstep(0.02, 0.24, uv.x) * smoothstep(0.02, 0.24, 1.0 - uv.x)
        * smoothstep(0.02, 0.20, uv.y) * smoothstep(0.02, 0.20, 1.0 - uv.y);
    color.rgb *= lerp(1.0, scanline * mask * (0.84 + 0.16 * vignette), content);
    color.rgb = pow(saturate(color.rgb), 1.0 / 1.04);
    output = color;
}

technique SemuCrt
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuCrt;
    }
}
