#include "ReShade.fxh"
#include "semu-common.fxh"

float edgeFrame(float2 uv)
{
    float l = 1.0 - smoothstep(0.035, 0.055, uv.x);
    float r = smoothstep(0.945, 0.965, uv.x);
    float t = 1.0 - smoothstep(0.030, 0.052, uv.y);
    float b = smoothstep(0.948, 0.970, uv.y);
    return saturate(max(max(l, r), max(t, b)));
}

float rectMask(float2 uv, float left, float top, float right, float bottom)
{
    float x = smoothstep(left, left + 0.018, uv.x) * (1.0 - smoothstep(right - 0.018, right, uv.x));
    float y = smoothstep(top, top + 0.018, uv.y) * (1.0 - smoothstep(bottom - 0.018, bottom, uv.y));
    return saturate(x * y);
}

float circleMask(float2 uv, float2 center, float radius)
{
    return 1.0 - smoothstep(radius - 0.006, radius + 0.006, length(uv - center));
}

void SemuNintendo3Ds(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float screen = semuContentMask(uv, 0.006);
    float gridX = 0.96 + 0.04 * sin(uv.x * BUFFER_WIDTH * 0.86 * 3.14159265);
    float gridY = 0.96 + 0.04 * sin(uv.y * BUFFER_HEIGHT * 3.14159265);
    float localGlow = 0.96 + 0.04 * smoothstep(0.15, 0.70, uv.y);
    color.rgb *= lerp(1.0, gridX * gridY * localGlow, screen);

    float body = rectMask(uv, 0.055, 0.018, 0.945, 0.982);
    float topLid = rectMask(uv, 0.170, 0.010, 0.830, 0.355);
    float bottomDeck = rectMask(uv, 0.115, 0.372, 0.885, 0.985);
    float hinge = rectMask(uv, 0.170, 0.342, 0.830, 0.395);
    float circlePad = circleMask(uv, float2(0.190, 0.665), 0.054);
    float dpad = max(rectMask(uv, 0.145, 0.807, 0.260, 0.840), rectMask(uv, 0.185, 0.766, 0.220, 0.883));
    float buttons = max(circleMask(uv, float2(0.790, 0.720), 0.026), circleMask(uv, float2(0.840, 0.682), 0.026));
    buttons = max(buttons, max(circleMask(uv, float2(0.840, 0.758), 0.026), circleMask(uv, float2(0.890, 0.720), 0.026)));
    float camera = circleMask(uv, float2(0.500, 0.050), 0.014);
    float speaker = rectMask(uv, 0.250, 0.070, 0.345, 0.150) + rectMask(uv, 0.655, 0.070, 0.750, 0.150);
    speaker *= 0.45 + 0.55 * step(0.50, frac(uv.x * 58.0));

    float3 shell = lerp(float3(0.014, 0.014, 0.017), float3(0.130, 0.135, 0.150), body);
    shell = lerp(shell, float3(0.028, 0.029, 0.034), saturate(topLid + bottomDeck));
    shell = lerp(shell, float3(0.075, 0.078, 0.088), hinge);
    shell = lerp(shell, float3(0.012, 0.012, 0.015), saturate(circlePad + dpad + buttons + camera));
    shell = lerp(shell, float3(0.040, 0.042, 0.048), saturate(speaker));
    shell *= 0.84 + 0.16 * smoothstep(0.0, 1.0, uv.x + uv.y);
    float frame = semuFrameMask(uv, 0.012);
    output = float4(lerp(color.rgb, shell, frame), color.a);
}

technique SemuNintendo3Ds
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuNintendo3Ds;
    }
}
