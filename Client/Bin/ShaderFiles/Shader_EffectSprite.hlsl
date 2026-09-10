#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"
#include "Shader_EffectSpriteCommon.hlsli"

cbuffer cbEffectSpriteMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteParams = { 1.f, 1.f, 0.f, 0.f };                 // x: authored energy scalar, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectSpriteAlphaParams = { 0.f, 0.f, 0.f, 1.f };            // x: alphaCutoff, y: alphaErosion, z: useMask, w: alphaMultiplier
    float4 g_EffectSpriteMainUVParams = { 1.f, 1.f, 0.f, 0.f };           // xy: scale, zw: scroll speed
    float4 g_EffectSpriteNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };         // xy: main offset, zw: noise offset
    float4 g_EffectSpriteMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteUVModeParams = { 0.f, 0.f, 0.f, 0.f };           // x: main, y: noise, z: mask
    float4 g_EffectSpriteUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main U/V policy, zw: noise U/V policy
    float4 g_EffectSpriteMaskUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f }; // xy: mask U/V policy, zw: reserved
    float4 g_EffectSpriteUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteSourceParams = { 1.f, 0.f, 0.f, 0.f };           // x: noise source, y: mask source, z: noise invert, w: mask invert
    float4 g_EffectSpriteAdditiveParams = { 0.f, 0.f, 0.f, 1.f };         // x: colorSource, y: amountSource, z: coveragePolicy, w: intensityScale
    float4 g_EffectSpriteAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };          // x: blackNeutral
    float4 g_EffectSpriteCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };     // x: enabled, y: corePower, z: coreIntensity, w: outerPower
    float4 g_EffectSpriteCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };  // rgb: coreColor, a: outerIntensity
    float4 g_EffectInfluenceDebugParams = { 0.f, 0.5f, 2.f, 0.f };        // x: influence outline enabled, y: reserved, z: pixel width scale, w: reserved
    float4 g_EffectMaterialScalarModulationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMaterialScalarModulationMeta[8];
    float4 g_EffectMaterialScalarModulationKeyTimes[8];
    float4 g_EffectMaterialScalarModulationKeyTimesBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyValues[8];
    float4 g_EffectMaterialScalarModulationKeyValuesBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyArriveTangents[8];
    float4 g_EffectMaterialScalarModulationKeyArriveTangentsBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyLeaveTangents[8];
    float4 g_EffectMaterialScalarModulationKeyLeaveTangentsBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyModes[8];
    float4 g_EffectMaterialScalarModulationKeyModesBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMaterialCoreColorRgbModulationMeta[8];
    float4 g_EffectMaterialCoreColorRgbModulationUniformMin[8];
    float4 g_EffectMaterialCoreColorRgbModulationUniformMax[8];
    float4 g_EffectMaterialCoreColorRgbModulationUniformSeed[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyTimes[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyTimesBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesR[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesRBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesG[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesGBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesB[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyValuesBBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsR[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsRBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsG[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsGBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsB[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyArriveTangentsBBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsR[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsRBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsG[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsGBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsB[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyLeaveTangentsBBlock1[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyModes[8];
    float4 g_EffectMaterialCoreColorRgbModulationKeyModesBlock1[8];
    int g_OpacitySource = 0;
    float g_EffectSpriteMaterialTime = 0.f;
    float3 g_EffectSpritePadding;
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_Texture : register(t0); // sprite effect의 primary texture다. 현재 경로에서는 RGB를 emissive-like color source로 해석한다.
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

static const float3 kInfluenceOutlineColor = float3(1.f, 0.f, 0.f);

float Resolve_EffectSpriteAxisUV(float rawValue, float transformedValue, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = transformedValue;
    if (mode == 3)
        result = rawValue;
    return result;
}

float2 Rotate_MaterialUV(float2 uv, float rotation)
{
    const int mode = (int)(rotation + 0.5f);
    float2 result = uv;
    if (mode == 1)
        result = float2(1.f - uv.y, uv.x);
    else if (mode == 2)
        result = 1.f - uv;
    else if (mode == 3)
        result = float2(uv.y, 1.f - uv.x);
    return result;
}

float2 Build_EffectSpriteUV(float2 baseUV, float4 uvParams, float2 uvOffset, float2 policy, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(baseUV, rotation);
    const float2 transformedUV = rotatedUV * uvParams.xy + uvOffset + uvParams.zw * g_EffectSpriteMaterialTime;
    return float2(
        Resolve_EffectSpriteAxisUV(rotatedUV.x, transformedUV.x, policy.x),
        Resolve_EffectSpriteAxisUV(rotatedUV.y, transformedUV.y, policy.y)
    );
}

float Apply_EffectSpriteAxisAddressValue(float value, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = frac(value);
    if (mode == 1 || mode == 2)
        result = saturate(value);
    else if (mode == 4)
        result = 1.f - abs(frac(value * 0.5f) * 2.f - 1.f);
    return result;
}

float2 Apply_EffectSpriteAxisAddress(float2 uv, float2 policy)
{
    return float2(
        Apply_EffectSpriteAxisAddressValue(uv.x, policy.x),
        Apply_EffectSpriteAxisAddressValue(uv.y, policy.y)
    );
}

float4 Sample_EffectSpriteMainTexture(float2 uv, float2 policy)
{
    return g_Texture.Sample(LinearClampSampler, Apply_EffectSpriteAxisAddress(uv, policy));
}

float4 Sample_EffectSpriteNoiseTexture(float2 uv, float2 policy)
{
    return g_NoiseTexture.Sample(LinearClampSampler, Apply_EffectSpriteAxisAddress(uv, policy));
}

float4 Sample_EffectSpriteMaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_EffectSpriteAxisAddress(uv, policy));
}

float Compute_InfluenceOutlineWidth(float2 cardUV)
{
    const float pixelUvWidth = max(fwidth(cardUV.x), fwidth(cardUV.y));
    const float pixelScale = max(g_EffectInfluenceDebugParams.z, 0.0001f);
    return clamp(pixelUvWidth * pixelScale, 0.0001f, 0.035f);
}

float Compute_CardInfluenceOutline(float2 cardUV)
{
    const float outlineWidth = Compute_InfluenceOutlineWidth(cardUV);
    const float edgeDistance = min(min(cardUV.x, 1.f - cardUV.x), min(cardUV.y, 1.f - cardUV.y));
    return 1.f - smoothstep(outlineWidth, outlineWidth * 1.5f, edgeDistance);
}

struct VS_IN
{
    float3 position : POSITION;
    row_major float4x4 InstanceMatrix : WORLD;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float2 spriteTiltDegrees : TEXCOORD2;
    float4 coreColorRgb : TEXCOORD3;
};

struct VS_OUT
{
    float4 worldPosition : POSITION;
    float2 size : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float rotationRadians : TEXCOORD3;
    float3 velocityDirection : TEXCOORD4;
    float2 spriteTiltDegrees : TEXCOORD5;
    float4 coreColorRgb : TEXCOORD6;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    const float4 worldPosition = mul(float4(In.position, 1.f), In.InstanceMatrix);
    Out.worldPosition = worldPosition;
    Out.size = float2(length(In.InstanceMatrix[0].xyz), length(In.InstanceMatrix[1].xyz));
    Out.lifeTime = In.lifeTime;
    Out.startColor = In.startColor;
    Out.endColor = In.endColor;
    Out.subUVRect = In.subUVRect;
    Out.rotationRadians = atan2(In.InstanceMatrix[0].y, In.InstanceMatrix[0].x);
    Out.velocityDirection = In.InstanceMatrix[2].xyz;
    Out.spriteTiltDegrees = In.spriteTiltDegrees;
    Out.coreColorRgb = In.coreColorRgb;

    return Out;
}

struct GS_IN
{
    float4 worldPosition : POSITION;
    float2 size : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float rotationRadians : TEXCOORD3;
    float3 velocityDirection : TEXCOORD4;
    float2 spriteTiltDegrees : TEXCOORD5;
    float4 coreColorRgb : TEXCOORD6;
};

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float4 coreColorRgb : TEXCOORD3;
};

// 파티클 위치에서 카메라를 향하는 정규화 look 방향을 구한다.
float3 ResolveFacingLookDirection(float3 particlePosition)
{
    float3 lookDir = g_CamPosition.xyz - particlePosition;
    const float lookLength = length(lookDir);
    return lookLength < 0.0001f ? float3(0.f, 0.f, 1.f) : lookDir / lookLength;
}

bool TryNormalizeDirection(float3 direction, out float3 normalizedDirection)
{
    normalizedDirection = float3(0.f, 0.f, 0.f);

    const float directionLength = length(direction);
    const bool valid = directionLength >= 0.0001f;
    if (valid)
        normalizedDirection = direction / directionLength;

    return valid;
}

bool TryResolveAwayFromCenterLookDirection(float3 particlePosition, out float3 lookDir)
{
    return TryNormalizeDirection(particlePosition - g_EffectSpriteEmitterCenter.xyz, lookDir);
}

bool TryResolveVelocityLookDirection(float3 velocityDirection, out float3 lookDir)
{
    return TryNormalizeDirection(velocityDirection, lookDir);
}

bool TryResolveDirectionalLookDirection(float3 particlePosition, float3 velocityDirection, out float3 lookDir)
{
    lookDir = float3(0.f, 0.f, 0.f);
    bool resolved = false;

    if (g_ScreenAlignmentMode == 3)
    {
        resolved = TryResolveAwayFromCenterLookDirection(particlePosition, lookDir);
        if (!resolved)
            resolved = TryResolveVelocityLookDirection(velocityDirection, lookDir);
    }
    else if (g_ScreenAlignmentMode == 4)
    {
        resolved = TryResolveVelocityLookDirection(velocityDirection, lookDir);
        if (!resolved)
            resolved = TryResolveAwayFromCenterLookDirection(particlePosition, lookDir);
    }

    return resolved;
}

// look 방향을 기준으로 billboard right/up 축을 만든다.
void ResolveLookAlignedBasis(float3 lookDir, out float3 rightDir, out float3 upDir)
{
    float3 worldUp = float3(0.f, 1.f, 0.f);
    rightDir = cross(lookDir, worldUp);

    if (length(rightDir) < 0.0001f)
        rightDir = cross(lookDir, float3(0.f, 0.f, 1.f));

    rightDir = normalize(rightDir);
    upDir = normalize(cross(rightDir, lookDir));
}

// 현재 카메라 평면 기준의 right/up 축을 가져온다.
void ResolveCameraPlaneBasis(out float3 rightDir, out float3 upDir)
{
    rightDir = normalize(float3(g_ViewMatrix[0].x, g_ViewMatrix[1].x, g_ViewMatrix[2].x));
    upDir = normalize(float3(g_ViewMatrix[0].y, g_ViewMatrix[1].y, g_ViewMatrix[2].y));
}

void ResolveWorldUpFacingCameraBasis(float3 center, out float3 rightDir, out float3 upDir)
{
    ResolveCameraPlaneBasis(rightDir, upDir);

    const float3 worldUp = float3(0.f, 1.f, 0.f);
    float3 cameraLook = float3(0.f, 0.f, 0.f);
    if (!TryNormalizeDirection(g_CamPosition.xyz - center, cameraLook))
        return;

    float3 worldRight = float3(0.f, 0.f, 0.f);
    if (!TryNormalizeDirection(cross(worldUp, cameraLook), worldRight))
        return;

    rightDir = worldRight;
    upDir = worldUp;
}

// texture 축 하나를 지정 방향에 맞추고, 나머지 축은 카메라 가시성을 유지하도록 만든다.
bool TryResolveTextureAxisBasis(float3 axisDir, float3 particlePosition, out float3 rightDir, out float3 upDir)
{
    ResolveCameraPlaneBasis(rightDir, upDir);

    float3 axis = float3(0.f, 0.f, 0.f);
    float3 cameraLook = float3(0.f, 0.f, 0.f);
    bool resolved = TryNormalizeDirection(axisDir, axis);
    if (resolved)
        resolved = TryNormalizeDirection(g_CamPosition.xyz - particlePosition, cameraLook);

    if (resolved && g_SpriteTextureAxis == 0)
    {
        rightDir = axis;
        resolved = TryNormalizeDirection(cross(rightDir, cameraLook), upDir);
    }
    else if (resolved)
    {
        upDir = axis;
        resolved = TryNormalizeDirection(cross(cameraLook, upDir), rightDir);
    }

    return resolved;
}

void ApplySpriteTilt(inout float3 rightDir, inout float3 upDir, float2 tiltDegrees)
{
    const float tiltX = radians(tiltDegrees.x);
    const float tiltY = radians(tiltDegrees.y);
    float3 normalDir = normalize(cross(rightDir, upDir));
    rightDir = normalize(rightDir * cos(tiltX) + normalDir * sin(tiltX));
    normalDir = normalize(cross(rightDir, upDir));
    upDir = normalize(upDir * cos(tiltY) + normalDir * sin(tiltY));
}

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const float3 particlePosition = In[0].worldPosition.xyz;
    float3 rightDir = float3(0.f, 0.f, 0.f);
    float3 upDir = float3(0.f, 0.f, 0.f);
    ResolveCameraPlaneBasis(rightDir, upDir);
    bool usedInstanceLookBasis = false;
    if (g_UseInstanceLookDirectionBasis != 0)
    {
        float3 lookDir = float3(0.f, 0.f, 0.f);
        usedInstanceLookBasis = TryNormalizeDirection(In[0].velocityDirection, lookDir);
        if (usedInstanceLookBasis)
            ResolveLookAlignedBasis(lookDir, rightDir, upDir);
    }

    if (!usedInstanceLookBasis && g_ScreenAlignmentMode == 5)
    {
        rightDir = float3(1.f, 0.f, 0.f);
        upDir = float3(0.f, 1.f, 0.f);
    }
    else if (!usedInstanceLookBasis && g_ScreenAlignmentMode == 6)
    {
        rightDir = float3(1.f, 0.f, 0.f);
        upDir = float3(0.f, 0.f, 1.f);
    }
    else if (!usedInstanceLookBasis && g_ScreenAlignmentMode == 7)
        ResolveWorldUpFacingCameraBasis(particlePosition, rightDir, upDir);
    else if (!usedInstanceLookBasis && (g_ScreenAlignmentMode == 3 || g_ScreenAlignmentMode == 4))
    {
        float3 lookDir = float3(0.f, 0.f, 0.f);
        const bool hasLookDir = TryResolveDirectionalLookDirection(particlePosition, In[0].velocityDirection, lookDir);

        if (g_DirectionalAlignmentMode == 1) // 스프라이트의 X 또는 Y 가 바라보도록
        {
            if (hasLookDir)
                TryResolveTextureAxisBasis(lookDir, particlePosition, rightDir, upDir);
        }
        else // 스프라이트의 look 이 바라보도록
        {
            if (!hasLookDir)
                lookDir = ResolveFacingLookDirection(particlePosition);
            ResolveLookAlignedBasis(lookDir, rightDir, upDir);
        }
    }
    else if (!usedInstanceLookBasis && g_ScreenAlignmentMode != 1 && g_ScreenAlignmentMode != 2)
    {
        const float3 lookDir = ResolveFacingLookDirection(particlePosition);
        ResolveLookAlignedBasis(lookDir, rightDir, upDir);
    }

    const float rotationRadians = In[0].rotationRadians + g_SpriteRollOffsetRadians;
    const float rotationCos = cos(rotationRadians);
    const float rotationSin = sin(rotationRadians);
    const float3 billboardRight = rightDir;
    const float3 billboardUp = upDir;
    rightDir = billboardRight * rotationCos + billboardUp * rotationSin;
    upDir = -billboardRight * rotationSin + billboardUp * rotationCos;
    ApplySpriteTilt(rightDir, upDir, In[0].spriteTiltDegrees);
    float2 spriteSize = In[0].size;
    if (g_ScreenAlignmentMode == 2)
    {
        const float squareExtent = max(spriteSize.x, spriteSize.y);
        spriteSize = float2(squareExtent, squareExtent);
    }

    rightDir *= spriteSize.x * 0.5f;
    upDir *= spriteSize.y * 0.5f;

    const matrix ViewProjMatrix = mul(g_ViewMatrix, g_ProjMatrix);
    GS_OUT Out[4] = (GS_OUT[4])0;

    Out[0].position = mul(float4(particlePosition - rightDir + upDir, 1.f), ViewProjMatrix);
    Out[0].texCoord = float2(0.f, 0.f);
    Out[0].lifeTime = In[0].lifeTime;
    Out[0].startColor = In[0].startColor;
    Out[0].endColor = In[0].endColor;
    Out[0].subUVRect = In[0].subUVRect;
    Out[0].coreColorRgb = In[0].coreColorRgb;

    Out[1].position = mul(float4(particlePosition + rightDir + upDir, 1.f), ViewProjMatrix);
    Out[1].texCoord = float2(1.f, 0.f);
    Out[1].lifeTime = In[0].lifeTime;
    Out[1].startColor = In[0].startColor;
    Out[1].endColor = In[0].endColor;
    Out[1].subUVRect = In[0].subUVRect;
    Out[1].coreColorRgb = In[0].coreColorRgb;

    Out[2].position = mul(float4(particlePosition - rightDir - upDir, 1.f), ViewProjMatrix);
    Out[2].texCoord = float2(0.f, 1.f);
    Out[2].lifeTime = In[0].lifeTime;
    Out[2].startColor = In[0].startColor;
    Out[2].endColor = In[0].endColor;
    Out[2].subUVRect = In[0].subUVRect;
    Out[2].coreColorRgb = In[0].coreColorRgb;

    Out[3].position = mul(float4(particlePosition + rightDir - upDir, 1.f), ViewProjMatrix);
    Out[3].texCoord = float2(1.f, 1.f);
    Out[3].lifeTime = In[0].lifeTime;
    Out[3].startColor = In[0].startColor;
    Out[3].endColor = In[0].endColor;
    Out[3].subUVRect = In[0].subUVRect;
    Out[3].coreColorRgb = In[0].coreColorRgb;

    OutStream.Append(Out[0]);
    OutStream.Append(Out[1]);
    OutStream.Append(Out[2]);
    OutStream.Append(Out[3]);
}

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float4 coreColorRgb : TEXCOORD3;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

EffectMaterialSurface Resolve_EffectMaterialSurface(PS_IN In)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float lifeMax = max(In.lifeTime.x, 0.0001f);
    const float lifeProgress = saturate(In.lifeTime.y / lifeMax);
    const float4 particleColor = lerp(In.startColor, In.endColor, lifeProgress);
    const float2 sampleUV = lerp(In.subUVRect.xy, In.subUVRect.zw, In.texCoord);
    const float2 mainUVPolicy = g_EffectSpriteUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectSpriteUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectSpriteMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMainUVParams, g_EffectSpriteUVOffsetParams.xy, mainUVPolicy, g_EffectSpriteUVRotationParams.x);
    const float4 symbol = Sample_EffectSpriteMainTexture(mainUV, mainUVPolicy);
    const float resolvedOpacityPower = g_EffectSpriteParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER, lifeProgress);
    const float resolvedNoiseStrength = 0 != g_EffectSpriteParams.w && g_EffectSpriteParams.z > 0.f
                                        ? g_EffectSpriteParams.z * EffectMaterialScalar_ResolveMultiplier(
                                              EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH,
                                              lifeProgress
                                          )
                                        : 0.f;
    const float resolvedAlphaErosion = g_EffectSpriteAlphaParams.y != 0.f
                                       ? g_EffectSpriteAlphaParams.y * EffectMaterialScalar_ResolveMultiplier(
                                             EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION,
                                             lifeProgress
                                         )
                                       : 0.f;
    const float resolvedAlphaCutoff = g_EffectSpriteAlphaParams.x != 0.f
                                      ? g_EffectSpriteAlphaParams.x * EffectMaterialScalar_ResolveMultiplier(
                                            EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF,
                                            lifeProgress
                                        )
                                      : 0.f;
    const float resolvedCoreIntensity = g_EffectSpriteCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectSpriteCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectSpriteCoreEmissiveColor.rgb, lifeProgress, In.coreColorRgb.rgb);
    const float opacityPower = max(resolvedOpacityPower, 0.0001f);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectSpriteCoreEmissiveParams.x,
        g_EffectSpriteCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectSpriteCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        resolvedCoreColorRgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectSpriteParams.w && resolvedNoiseStrength > 0.f)
    {
        const float2 noiseUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteNoiseUVParams, g_EffectSpriteUVOffsetParams.zw, noiseUVPolicy, g_EffectSpriteUVRotationParams.y);
        const float4 noiseTexel = Sample_EffectSpriteNoiseTexture(noiseUV, noiseUVPolicy);
        const float noise = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectSpriteSourceParams.x),
            g_EffectSpriteSourceParams.z
        );
        noiseFactor = lerp(1.f, noise, saturate(resolvedNoiseStrength));
    }

    if (0 != g_EffectSpriteAlphaParams.z)
    {
        const float2 maskUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMaskUVParams, g_EffectSpriteMaskUVOffsetParams.xy, maskUVPolicy, g_EffectSpriteUVRotationParams.z);
        const float4 maskTexel = Sample_EffectSpriteMaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectSpriteSourceParams.y),
            g_EffectSpriteSourceParams.w
        );
    }

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float shapeCoverage = Effect_BuildCoverage(selectedOpacity, opacityPower, maskCoverage, noiseFactor, g_EffectSpriteAlphaParams.w);
    const float3 baseColor = Effect_ApplyCoreEmissiveShaping(
        symbol.rgb * particleColor.rgb * g_Tint.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );
    surface.color = baseColor * noiseFactor;
    surface.coverage = shapeCoverage * saturate(particleColor.a * g_Tint.a);

    if (shapeCoverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float resolvedIntensity = g_EffectSpriteParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    Out.color = Effect_ComposeAlphaBlend(surface, resolvedIntensity);
    if (g_EffectInfluenceDebugParams.x > 0.5f)
    {
        const float outlineMask = Compute_CardInfluenceOutline(In.texCoord);
        Out.color.rgb = lerp(Out.color.rgb, kInfluenceOutlineColor, outlineMask);
        Out.color.a = max(Out.color.a, outlineMask);
    }
    return Out;
}

PS_OUT PS_MASKED(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float resolvedIntensity = g_EffectSpriteParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    Out.color = float4(surface.color * max(resolvedIntensity, 0.f), 1.f);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float lifeMax = max(In.lifeTime.x, 0.0001f);
    const float lifeProgress = saturate(In.lifeTime.y / lifeMax);
    const float4 particleColor = lerp(In.startColor, In.endColor, lifeProgress);
    const float2 sampleUV = lerp(In.subUVRect.xy, In.subUVRect.zw, In.texCoord);
    const float2 mainUVPolicy = g_EffectSpriteUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectSpriteUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectSpriteMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMainUVParams, g_EffectSpriteUVOffsetParams.xy, mainUVPolicy, g_EffectSpriteUVRotationParams.x);
    const float2 noiseUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteNoiseUVParams, g_EffectSpriteUVOffsetParams.zw, noiseUVPolicy, g_EffectSpriteUVRotationParams.y);
    const float2 maskUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMaskUVParams, g_EffectSpriteMaskUVOffsetParams.xy, maskUVPolicy, g_EffectSpriteUVRotationParams.z);
    const float4 symbol = Sample_EffectSpriteMainTexture(mainUV, mainUVPolicy);
    const float resolvedNoiseStrength = g_EffectSpriteParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion = g_EffectSpriteAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectSpriteAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity = g_EffectSpriteCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectSpriteCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float3 resolvedCoreColorRgb =
        EffectMaterialCoreColor_ResolveRgb(g_EffectSpriteCoreEmissiveColor.rgb, lifeProgress, In.coreColorRgb.rgb);
    const float resolvedIntensity = g_EffectSpriteParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectSpriteCoreEmissiveParams.x,
        g_EffectSpriteCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectSpriteCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        resolvedCoreColorRgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectSpriteParams.w && resolvedNoiseStrength > 0.f)
    {
        const float4 noiseTexel = Sample_EffectSpriteNoiseTexture(noiseUV, noiseUVPolicy);
        const float noise = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectSpriteSourceParams.x),
            g_EffectSpriteSourceParams.z
        );
        noiseFactor = lerp(1.f, noise, saturate(resolvedNoiseStrength));
    }

    if (0 != g_EffectSpriteAlphaParams.z)
    {
        const float4 maskTexel = Sample_EffectSpriteMaskTexture(maskUV, maskUVPolicy);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectSpriteSourceParams.y),
            g_EffectSpriteSourceParams.w
        );
    }

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float coverageBase = saturate(maskCoverage * noiseFactor * g_EffectSpriteAlphaParams.w);
    const float additiveLifeAlpha = saturate(particleColor.a * g_Tint.a);
    const int colorSource = (int)g_EffectSpriteAdditiveParams.x;
    const int amountSource = (int)g_EffectSpriteAdditiveParams.y;
    const int coveragePolicy = (int)g_EffectSpriteAdditiveParams.z;
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    const float amount = Effect_SelectAdditiveAmount(symbol, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(symbol.rgb, colorSource, g_EffectSpriteAdditiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        symbol.rgb,
        symbol.rgb * g_Tint.rgb,
        g_EffectSpriteAdditiveEmissiveColor,
        g_EffectSpriteAdditiveConstantColor,
        colorSource
    );
    const float3 shapedMaterialColor = Effect_ApplyCoreEmissiveShaping(
        materialColor * particleColor.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );
    const float additiveEnergyScalar = max(resolvedIntensity, 0.f) * max(g_EffectSpriteAdditiveParams.w, 0.f);
    const float3 contribution = shapedMaterialColor * noiseFactor * policyAmount * additiveEnergyScalar * blackGate * additiveLifeAlpha;
    Out.color = float4(contribution, 1.f);
    if (g_EffectInfluenceDebugParams.x > 0.5f)
    {
        const float outlineMask = Compute_CardInfluenceOutline(In.texCoord);
        Out.color.rgb = lerp(Out.color.rgb, kInfluenceOutlineColor, outlineMask);
    }
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN) // 0
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, GS_MAIN, PS_ADDITIVE) // 1
    PASS_RS_DS_BS_VGP(MaskedPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, GS_MAIN, PS_MASKED)              // 2
}
