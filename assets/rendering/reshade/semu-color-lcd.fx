#include "ReShade.fxh"
#include "semu-common.fxh"

void SemuColorLcd(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float content = semuContentMask(uv, 0.006);
    float gridX = 0.94 + 0.06 * sin(uv.x * BUFFER_WIDTH * 0.75 * 3.14159265);
    float gridY = 0.94 + 0.06 * sin(uv.y * BUFFER_HEIGHT * 0.75 * 3.14159265);
    color.rgb *= lerp(1.0, gridX * gridY, content);
    color.rgb = lerp(color.rgb, float3(dot(color.rgb, float3(0.2126, 0.7152, 0.0722))), 0.04);
    output = color;
}

technique SemuColorLcd
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuColorLcd;
    }
}
