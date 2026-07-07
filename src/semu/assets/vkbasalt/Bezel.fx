// Semu standalone-emulator bezel (ReShade effect, loaded by vkBasalt). Shrinks the emulator's
// full-screen frame into a cutout rectangle and draws a bezel texture around it. Runs INSIDE
// the game's Vulkan swapchain, which gamescope presents (unlike a separate overlay window).
#include "ReShade.fxh"

#ifndef SEMU_BEZEL_TEXTURE
#define SEMU_BEZEL_TEXTURE "bezel.png"
#endif
#ifndef SEMU_CUT_LEFT
#define SEMU_CUT_LEFT   0.125
#endif
#ifndef SEMU_CUT_TOP
#define SEMU_CUT_TOP    0.050
#endif
#ifndef SEMU_CUT_RIGHT
#define SEMU_CUT_RIGHT  0.875
#endif
#ifndef SEMU_CUT_BOTTOM
#define SEMU_CUT_BOTTOM 0.950
#endif

texture SemuBezelTex < source = SEMU_BEZEL_TEXTURE; > { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
sampler SemuBezel { Texture = SemuBezelTex; };

float4 PS_Bezel(float4 vpos : SV_Position, float2 uv : TexCoord) : SV_Target
{
    float4 bez = tex2D(SemuBezel, uv);
    if (uv.x >= SEMU_CUT_LEFT && uv.x <= SEMU_CUT_RIGHT && uv.y >= SEMU_CUT_TOP && uv.y <= SEMU_CUT_BOTTOM)
    {
        float2 guv = float2((uv.x - SEMU_CUT_LEFT) / (SEMU_CUT_RIGHT - SEMU_CUT_LEFT),
                            (uv.y - SEMU_CUT_TOP)  / (SEMU_CUT_BOTTOM - SEMU_CUT_TOP));
        float4 game = tex2D(ReShade::BackBuffer, guv);
        return lerp(game, bez, bez.a);
    }
    float4 backdrop = float4(0.04, 0.04, 0.07, 1.0);
    return lerp(backdrop, bez, bez.a);
}

technique SemuBezel { pass { VertexShader = PostProcessVS; PixelShader = PS_Bezel; } }
