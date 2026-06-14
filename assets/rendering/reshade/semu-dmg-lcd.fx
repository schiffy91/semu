#include "ReShade.fxh"
#include "semu-common.fxh"

void SemuDmgLcd(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float luma = dot(color.rgb, float3(0.299, 0.587, 0.114));
    float content = semuContentMask(uv, 0.006);
    float gridX = 0.90 + 0.10 * sin(uv.x * BUFFER_WIDTH * 0.50 * 3.14159265);
    float gridY = 0.88 + 0.12 * sin(uv.y * BUFFER_HEIGHT * 0.72 * 3.14159265);
    float ghost = 0.04 * sin((uv.x + uv.y) * 16.0);
    float shade = saturate(luma * gridX * gridY + ghost);
    float3 dark = float3(0.22, 0.31, 0.12);
    float3 light = float3(0.62, 0.72, 0.38);
    output = float4(lerp(color.rgb, lerp(dark, light, shade), content), color.a);
}

technique SemuDmgLcd
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuDmgLcd;
    }
}
