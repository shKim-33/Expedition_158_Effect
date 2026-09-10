#ifndef SHADER_EFFECT_SPRITE_COMMON_HLSLI
#define SHADER_EFFECT_SPRITE_COMMON_HLSLI

// ComputeSpriteEmitter billboard geometry가 공통으로 소비하는 runtime 값이다.
cbuffer cbEffectSpriteRuntime : register(b3)
{
    int g_ScreenAlignmentMode = 0;
    int g_DirectionalAlignmentMode = 1;
    int g_SpriteTextureAxis = 1;
    int g_UseInstanceLookDirectionBasis = 0;
    float g_SpriteRollOffsetRadians = 0.f;
    float4 g_EffectSpriteEmitterCenter = { 0.f, 0.f, 0.f, 1.f };
};

#endif
