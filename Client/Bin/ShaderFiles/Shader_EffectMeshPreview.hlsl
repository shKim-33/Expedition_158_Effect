#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"
#include "Shader_EffectMeshCommon.hlsli"

cbuffer cbEffectMeshMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshParams = { 1.f, 1.f, 0.f, 0.f };                 // x: intensity, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectMeshAlphaParams = { 0.f, 0.f, 0.f, 1.f };            // x: alphaCutoff, y: alphaErosion, z: useMask, w: alphaMultiplier
    float4 g_EffectMeshMainUVParams = { 1.f, 1.f, 0.f, 0.f };           // xy: scale, zw: scroll speed
    float4 g_EffectMeshNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshUVModeParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };     // xy: main U/V policy, zw: noise U/V policy
    float4 g_EffectMeshMaskUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f }; // xy: mask U/V policy, zw: reserved
    float4 g_EffectMeshUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshSourceParams = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshAdditiveParams = { 0.f, 0.f, 0.f, 1.f };
    float4 g_EffectMeshAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };
    float4 g_EffectMeshCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };
    float4 g_EffectMeshPreviewScreenSize = { 512.f, 512.f, 0.f, 0.f };
    float4 g_EffectMeshModelMaterialFlags = { 0.f, 0.f, 0.f, 0.f };      // x: BaseColor, y: Normal, z: Emissive, w: ORM
    int g_OpacitySource = 0;
    float g_EffectMeshMaterialTime = 0.f;
    float2 g_EffectMeshPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);
Texture2D g_ModelNormalTexture : register(t3);
Texture2D g_ModelEmissiveTexture : register(t4);
Texture2D g_ModelOrmTexture : register(t5);

float Resolve_EffectMeshAxisUV(float rawValue, float transformedValue, float policy)
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

float2 Build_EffectMeshUV(float2 baseUV, float4 uvParams, float2 uvOffset, float2 policy, float rotation)
{
    const float2 rotatedUV = Rotate_MaterialUV(baseUV, rotation);
    const float2 transformedUV = rotatedUV * uvParams.xy + uvOffset + uvParams.zw * g_EffectMeshMaterialTime;
    return float2(
        Resolve_EffectMeshAxisUV(rotatedUV.x, transformedUV.x, policy.x),
        Resolve_EffectMeshAxisUV(rotatedUV.y, transformedUV.y, policy.y)
    );
}

float Apply_EffectMeshAxisAddressValue(float value, float policy)
{
    const int mode = (int)(policy + 0.5f);
    float result = frac(value);
    if (mode == 1 || mode == 2)
        result = saturate(value);
    else if (mode == 4)
        result = 1.f - abs(frac(value * 0.5f) * 2.f - 1.f);
    return result;
}

float2 Apply_EffectMeshAxisAddress(float2 uv, float2 policy)
{
    return float2(
        Apply_EffectMeshAxisAddressValue(uv.x, policy.x),
        Apply_EffectMeshAxisAddressValue(uv.y, policy.y)
    );
}

float4 Sample_EffectMeshMainTexture(float2 uv, float2 policy)
{
    return g_Texture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy));
}

float4 Sample_EffectMeshNoiseTexture(float2 uv, float2 policy)
{
    return g_NoiseTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy));
}

float4 Sample_EffectMeshMaskTexture(float2 uv, float2 policy)
{
    return g_MaskTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy));
}

float3 Decode_EffectMeshModelNormal(float4 normalTexel)
{
    const float2 normalXY = normalTexel.rg * 2.f - 1.f;
    return normalize(float3(normalXY, sqrt(saturate(1.f - dot(normalXY, normalXY)))));
}

float3 Resolve_EffectMeshModelNormal(float2 uv, float2 policy, float3 normal, float3 tangent, float3 binormal)
{
    if (g_EffectMeshModelMaterialFlags.y <= 0.5f)
        return normalize(normal);

    const float3 tangentNormal = Decode_EffectMeshModelNormal(g_ModelNormalTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy)));
    const float3 resolvedTangent = normalize(tangent);
    const float3 resolvedBinormal = normalize(binormal);
    const float3 resolvedNormal = normalize(normal);
    return normalize(
        tangentNormal.x * resolvedTangent +
        tangentNormal.y * resolvedBinormal +
        tangentNormal.z * resolvedNormal
    );
}

float Resolve_EffectMeshModelRoughness(float2 uv, float2 policy)
{
    if (g_EffectMeshModelMaterialFlags.w <= 0.5f)
        return 0.65f;

    return saturate(g_ModelOrmTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy)).g);
}

float3 Apply_EffectMeshModelMaterialLighting(float3 color, float3 normal, float2 uv, float2 policy)
{
    if (g_EffectMeshModelMaterialFlags.y <= 0.5f && g_EffectMeshModelMaterialFlags.w <= 0.5f)
        return color;

    const float3 lightDirection = normalize(float3(0.35f, 0.65f, -0.6f));
    const float3 resolvedNormal = normalize(normal);
    const float lighting =
        g_EffectMeshModelMaterialFlags.y > 0.5f
        ? 0.55f + 0.65f * saturate(dot(resolvedNormal, lightDirection))
        : 1.f;

    const float roughness = Resolve_EffectMeshModelRoughness(uv, policy);
    const float visualRoughness = saturate(roughness * 0.75f);
    const float3 viewDirection = normalize(float3(0.f, 0.f, -1.f));
    const float3 halfDirection = normalize(lightDirection + viewDirection);
    const float specularPower = lerp(64.f, 8.f, visualRoughness);
    const float specularStrength = g_EffectMeshModelMaterialFlags.w > 0.5f ? (1.f - visualRoughness) * 0.35f : 0.f;
    const float specular = pow(saturate(dot(resolvedNormal, halfDirection)), specularPower) * specularStrength;
    return color * lighting + float3(specular, specular, specular);
}

float3 Resolve_EffectMeshModelEmissive(float2 uv, float2 policy, float3 modulationColor)
{
    if (g_EffectMeshModelMaterialFlags.z <= 0.5f)
        return float3(0.f, 0.f, 0.f);

    return g_ModelEmissiveTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy)).rgb * modulationColor;
}

struct VS_IN
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float3 vTangent : TANGENT;
    float3 vBinormal : BINORMAL;
    float2 vTexcoord : TEXCOORD;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
    float3 vNormal : TEXCOORD1;
    float3 vTangent : TEXCOORD2;
    float3 vBinormal : TEXCOORD3;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;
    const float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    const float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vTexcoord = In.vTexcoord;
    Out.vNormal = normalize(mul(float4(In.vNormal, 0.f), g_WorldMatrix)).xyz;
    Out.vTangent = normalize(mul(float4(In.vTangent, 0.f), g_WorldMatrix)).xyz;
    Out.vBinormal = normalize(mul(float4(In.vBinormal, 0.f), g_WorldMatrix)).xyz;
    return Out;
}

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

EffectMaterialSurface Resolve_EffectMeshSurface(VS_OUT In)
{
    const float2 mainUVPolicy = g_EffectMeshUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectMeshUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectMeshMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMainUVParams, g_EffectMeshUVOffsetParams.xy, mainUVPolicy, g_EffectMeshUVRotationParams.x);
    const float2 noiseUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshNoiseUVParams, g_EffectMeshUVOffsetParams.zw, noiseUVPolicy, g_EffectMeshUVRotationParams.y);
    const float2 maskUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMaskUVParams, g_EffectMeshMaskUVOffsetParams.xy, maskUVPolicy, g_EffectMeshUVRotationParams.z);
    const float4 symbol = Sample_EffectMeshMainTexture(mainUV, mainUVPolicy);
    const float4 noiseTexel =
        0 != g_EffectMeshParams.w && g_EffectMeshParams.z > 0.f
        ? Sample_EffectMeshNoiseTexture(noiseUV, noiseUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);
    const float4 maskTexel =
        0 != g_EffectMeshAlphaParams.z
        ? Sample_EffectMeshMaskTexture(maskUV, maskUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);

    EffectMeshCommonSurfaceInput surfaceInput = (EffectMeshCommonSurfaceInput)0;
    surfaceInput.symbol = symbol;
    surfaceInput.noiseTexel = noiseTexel;
    surfaceInput.maskTexel = maskTexel;
    surfaceInput.instanceColor = float4(1.f, 1.f, 1.f, 1.f);
    surfaceInput.tint = g_Tint;
    surfaceInput.params = g_EffectMeshParams;
    surfaceInput.alphaParams = g_EffectMeshAlphaParams;
    surfaceInput.sourceParams = g_EffectMeshSourceParams;
    surfaceInput.coreEmissiveColor = g_EffectMeshCoreEmissiveColor;
    surfaceInput.coreEmissiveParams = g_EffectMeshCoreEmissiveParams;
    surfaceInput.opacitySource = g_OpacitySource;
    EffectMaterialSurface surface = EffectMeshCommon_ResolveSurface(surfaceInput);
    const float3 modelNormal = Resolve_EffectMeshModelNormal(mainUV, mainUVPolicy, In.vNormal, In.vTangent, In.vBinormal);
    surface.color = Apply_EffectMeshModelMaterialLighting(surface.color, modelNormal, mainUV, mainUVPolicy);
    surface.color += Resolve_EffectMeshModelEmissive(mainUV, mainUVPolicy, g_Tint.rgb);
    return surface;
}

PS_OUT PS_ALPHA(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMeshSurface(In);
    const float4 materialColor = Effect_ComposeAlphaBlend(surface, g_EffectMeshParams.x);
    Out.color = materialColor;
    return Out;
}

PS_OUT PS_MASKED(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMeshSurface(In);
    Out.color = float4(surface.color * max(g_EffectMeshParams.x, 0.f), 1.f);
    return Out;
}

PS_OUT PS_ADDITIVE(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 mainUVPolicy = g_EffectMeshUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectMeshUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectMeshMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMainUVParams, g_EffectMeshUVOffsetParams.xy, mainUVPolicy, g_EffectMeshUVRotationParams.x);
    const float2 noiseUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshNoiseUVParams, g_EffectMeshUVOffsetParams.zw, noiseUVPolicy, g_EffectMeshUVRotationParams.y);
    const float2 maskUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMaskUVParams, g_EffectMeshMaskUVOffsetParams.xy, maskUVPolicy, g_EffectMeshUVRotationParams.z);
    const float4 symbol = Sample_EffectMeshMainTexture(mainUV, mainUVPolicy);
    const float4 noiseTexel =
        0 != g_EffectMeshParams.w && g_EffectMeshParams.z > 0.f
        ? Sample_EffectMeshNoiseTexture(noiseUV, noiseUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);
    const float4 maskTexel =
        0 != g_EffectMeshAlphaParams.z
        ? Sample_EffectMeshMaskTexture(maskUV, maskUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);

    EffectMeshCommonAdditiveInput additiveInput = (EffectMeshCommonAdditiveInput)0;
    additiveInput.symbol = symbol;
    additiveInput.noiseTexel = noiseTexel;
    additiveInput.maskTexel = maskTexel;
    additiveInput.instanceColor = float4(1.f, 1.f, 1.f, 1.f);
    additiveInput.tint = g_Tint;
    additiveInput.params = g_EffectMeshParams;
    additiveInput.alphaParams = g_EffectMeshAlphaParams;
    additiveInput.sourceParams = g_EffectMeshSourceParams;
    additiveInput.additiveParams = g_EffectMeshAdditiveParams;
    additiveInput.additiveEmissiveColor = g_EffectMeshAdditiveEmissiveColor;
    additiveInput.additiveConstantColor = g_EffectMeshAdditiveConstantColor;
    additiveInput.additiveFlags = g_EffectMeshAdditiveFlags;
    additiveInput.coreEmissiveColor = g_EffectMeshCoreEmissiveColor;
    additiveInput.coreEmissiveParams = g_EffectMeshCoreEmissiveParams;
    additiveInput.opacitySource = g_OpacitySource;
    float3 additiveColor = EffectMeshCommon_ResolveAdditiveContribution(additiveInput);
    const float3 modelNormal = Resolve_EffectMeshModelNormal(mainUV, mainUVPolicy, In.vNormal, In.vTangent, In.vBinormal);
    additiveColor = Apply_EffectMeshModelMaterialLighting(additiveColor, modelNormal, mainUV, mainUVPolicy);
    additiveColor += Resolve_EffectMeshModelEmissive(mainUV, mainUVPolicy, g_Tint.rgb) * max(g_EffectMeshParams.x, 0.f);
    Out.color = float4(additiveColor, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(AlphaBlendPass, RS_Default, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, PS_ALPHA)           // 0
    PASS_RS_DS_BS_VP(AdditivePass, RS_Default, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE)             // 1
    PASS_RS_DS_BS_VP(AlphaBlendTwoSidedPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, PS_ALPHA)  // 2
    PASS_RS_DS_BS_VP(AdditiveTwoSidedPass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, PS_ADDITIVE)   // 3
    PASS_RS_DS_BS_VP(MaskedPass, RS_Default, DSS_Default, BS_Default, VS_MAIN, PS_MASKED)                // 4
    PASS_RS_DS_BS_VP(MaskedTwoSidedPass, RS_CullNone, DSS_Default, BS_Default, VS_MAIN, PS_MASKED)       // 5
}
