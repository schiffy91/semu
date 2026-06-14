#include "ReShade.fxh"
#include "semu-common.fxh"

float semuFrame(float2 uv, float left, float right, float top, float bottom)
{
    float l = 1.0 - smoothstep(left, left + 0.018, uv.x);
    float r = smoothstep(right - 0.018, right, uv.x);
    float t = 1.0 - smoothstep(top, top + 0.018, uv.y);
    float b = smoothstep(bottom - 0.018, bottom, uv.y);
    return saturate(max(max(l, r), max(t, b)));
}

void SemuCrtBezel(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float content = semuContentMask(uv, 0.006);
    float scanline = 0.86 + 0.14 * sin(uv.y * BUFFER_HEIGHT * 3.14159265);
    float mask = 0.92 + 0.08 * sin(uv.x * BUFFER_WIDTH * 3.14159265);
    float vignette = smoothstep(0.02, 0.24, uv.x) * smoothstep(0.02, 0.24, 1.0 - uv.x)
        * smoothstep(0.02, 0.20, uv.y) * smoothstep(0.02, 0.20, 1.0 - uv.y);
    color.rgb *= lerp(1.0, scanline * mask * (0.84 + 0.16 * vignette), content);
    color.rgb = pow(saturate(color.rgb), 1.0 / 1.04);

    float frame = semuFrameMask(uv, 0.012);
    float4 tube = semuContentRect();
    float tubeGlass = semuRectMask(uv, tube + float4(-0.020, -0.026, 0.020, 0.026), 0.016) * (1.0 - content);
    float cabinetBox = semuRectMask(uv, float4(0.035, 0.035, 0.965, 0.965), 0.018);
    float speaker = semuRectMask(uv, float4(0.805, 0.245, 0.925, 0.780), 0.010);
    float dialA = 1.0 - smoothstep(0.024, 0.030, length(uv - float2(0.865, 0.835)));
    float dialB = 1.0 - smoothstep(0.018, 0.024, length(uv - float2(0.915, 0.835)));
    float grill = 0.35 + 0.65 * step(0.55, frac(uv.y * 72.0));
    float shine = smoothstep(0.04, 0.28, uv.x) * (1.0 - smoothstep(0.12, 0.62, uv.y));
    float wood = 0.82 + 0.18 * sin((uv.x * 5.0 + uv.y * 17.0) * 3.14159265);
    float3 bezel = lerp(float3(0.020, 0.019, 0.017), float3(0.115, 0.086, 0.058) * wood, cabinetBox);
    bezel = lerp(bezel, float3(0.012, 0.012, 0.011), tubeGlass);
    bezel = lerp(bezel, float3(0.030, 0.028, 0.024) * grill, speaker);
    bezel = lerp(bezel, float3(0.70, 0.64, 0.50), saturate(dialA + dialB));
    bezel = lerp(bezel, float3(0.29, 0.27, 0.22), shine * 0.24);
    output = float4(lerp(color.rgb, bezel, frame), color.a);
}

technique SemuCrtBezel
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuCrtBezel;
    }
}
