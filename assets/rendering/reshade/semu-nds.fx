#include "ReShade.fxh"
#include "semu-common.fxh"

float edgeFrame(float2 uv)
{
    float l = 1.0 - smoothstep(0.040, 0.060, uv.x);
    float r = smoothstep(0.940, 0.960, uv.x);
    float t = 1.0 - smoothstep(0.035, 0.055, uv.y);
    float b = smoothstep(0.945, 0.965, uv.y);
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

void SemuNintendoDs(float4 pos : SV_Position, float2 uv : TEXCOORD, out float4 output : SV_Target0)
{
    float4 color = tex2D(ReShade::BackBuffer, uv);
    float screen = semuContentMask(uv, 0.006);
    float gridX = 0.95 + 0.05 * sin(uv.x * BUFFER_WIDTH * 0.80 * 3.14159265);
    float gridY = 0.95 + 0.05 * sin(uv.y * BUFFER_HEIGHT * 0.95 * 3.14159265);
    float gapShadow = 0.92 + 0.08 * smoothstep(0.05, 0.45, abs(uv.y - 0.365));
    color.rgb *= lerp(1.0, gridX * gridY * gapShadow, screen);

    float body = rectMask(uv, 0.075, 0.020, 0.925, 0.980);
    float topWell = rectMask(uv, 0.245, 0.010, 0.755, 0.356);
    float bottomWell = rectMask(uv, 0.235, 0.372, 0.765, 0.984);
    float hinge = rectMask(uv, 0.135, 0.345, 0.865, 0.392);
    float dpad = max(rectMask(uv, 0.135, 0.730, 0.260, 0.766), rectMask(uv, 0.180, 0.685, 0.216, 0.810));
    float buttons = max(circleMask(uv, float2(0.805, 0.732), 0.030), circleMask(uv, float2(0.855, 0.780), 0.030));
    float speaker = rectMask(uv, 0.105, 0.105, 0.195, 0.200) + rectMask(uv, 0.805, 0.105, 0.895, 0.200);
    speaker *= 0.45 + 0.55 * step(0.50, frac(uv.x * 52.0));

    float3 shell = lerp(float3(0.018, 0.018, 0.021), float3(0.185, 0.185, 0.195), body);
    shell = lerp(shell, float3(0.035, 0.035, 0.041), saturate(topWell + bottomWell));
    shell = lerp(shell, float3(0.095, 0.095, 0.105), hinge);
    shell = lerp(shell, float3(0.020, 0.020, 0.024), saturate(dpad + buttons));
    shell = lerp(shell, float3(0.030, 0.030, 0.034), saturate(speaker));
    shell *= 0.86 + 0.14 * smoothstep(0.0, 1.0, uv.x + uv.y);
    float frame = semuFrameMask(uv, 0.012);
    output = float4(lerp(color.rgb, shell, frame), color.a);
}

technique SemuNintendoDs
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader = SemuNintendoDs;
    }
}
