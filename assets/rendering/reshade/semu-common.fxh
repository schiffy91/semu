#ifndef SEMU_CONTENT_ASPECT
#define SEMU_CONTENT_ASPECT 1.333333333
#endif

#ifndef SEMU_LAYOUT
#define SEMU_LAYOUT 0
#endif

#ifndef SEMU_SCREEN_GAP
#define SEMU_SCREEN_GAP 0.045
#endif

float semuScreenAspect()
{
    return BUFFER_WIDTH / BUFFER_HEIGHT;
}

float4 semuFitRect(float aspect, float margin)
{
    float screenAspect = semuScreenAspect();
    float width = 1.0 - margin * 2.0;
    float height = 1.0 - margin * 2.0;
    if (screenAspect > aspect) {
        width = height * aspect / screenAspect;
    } else {
        height = width * screenAspect / aspect;
    }
    float left = (1.0 - width) * 0.5;
    float top = (1.0 - height) * 0.5;
    return float4(left, top, left + width, top + height);
}

float semuRectMask(float2 uv, float4 rect, float feather)
{
    float x = smoothstep(rect.x, rect.x + feather, uv.x)
        * (1.0 - smoothstep(rect.z - feather, rect.z, uv.x));
    float y = smoothstep(rect.y, rect.y + feather, uv.y)
        * (1.0 - smoothstep(rect.w - feather, rect.w, uv.y));
    return saturate(x * y);
}

float semuOutsideRect(float2 uv, float4 rect, float feather)
{
    return 1.0 - semuRectMask(uv, rect, feather);
}

float4 semuContentRect()
{
    return semuFitRect(SEMU_CONTENT_ASPECT, 0.055);
}

float4 semuTopScreenRect()
{
    if (SEMU_LAYOUT == 2) {
        return float4(0.225, 0.030, 0.775, 0.330);
    }
    return float4(0.285, 0.030, 0.715, 0.335);
}

float4 semuBottomScreenRect()
{
    if (SEMU_LAYOUT == 2) {
        return float4(0.300, 0.390, 0.700, 0.960);
    }
    return float4(0.295, 0.390, 0.705, 0.960);
}

float semuContentMask(float2 uv, float feather)
{
    if (SEMU_LAYOUT == 1 || SEMU_LAYOUT == 2) {
        return saturate(max(
            semuRectMask(uv, semuTopScreenRect(), feather),
            semuRectMask(uv, semuBottomScreenRect(), feather)
        ));
    }
    return semuRectMask(uv, semuContentRect(), feather);
}

float semuFrameMask(float2 uv, float feather)
{
    if (SEMU_LAYOUT == 1 || SEMU_LAYOUT == 2) {
        float hinge = 1.0 - smoothstep(SEMU_SCREEN_GAP * 0.25, SEMU_SCREEN_GAP, abs(uv.y - 0.365));
        return saturate(max(hinge * 0.75, 1.0 - semuContentMask(uv, feather)));
    }
    return semuOutsideRect(uv, semuContentRect(), feather);
}
