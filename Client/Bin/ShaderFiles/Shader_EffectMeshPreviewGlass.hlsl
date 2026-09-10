#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectMeshPreviewGlassMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshAlphaParams = { 0.f, 0.f, 0.f, 1.f };
    float4 g_EffectMeshMainUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectMeshUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshUVModeParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshMaskUVAxisPolicyParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshUVRotationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshSourceParams = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshAdditiveParams = { 0.f, 0.f, 0.f, 1.f };
    float4 g_EffectMeshAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };
    float4 g_EffectMeshCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f };
    float4 g_EffectMeshPreviewScreenSize = { 512.f, 512.f, 0.f, 0.f };
    float4 g_EffectMeshModelMaterialFlags = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMeshGlassSurfaceParams = { 0.18f, 1.f, 1.f, 0.25f };
    float4 g_EffectMeshGlassRimColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshGlassRimParams = { 1.5f, 3.f, 0.35f, 1.f };
    float4 g_EffectMeshGlassLightDirection = { -0.35f, 0.65f, -0.65f, 1.2f };
    float4 g_EffectMeshGlassLightColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectMeshGlassSpecularParams = { 48.f, 1.f, 0.f, 0.f };
    int g_OpacitySource = 0;
    float g_EffectMeshMaterialTime = 0.f;
    float2 g_EffectMeshGlassPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);
Texture2D g_ModelNormalTexture : register(t3);
Texture2D g_ModelEmissiveTexture : register(t4);

float Resolve_EffectMeshAxisUV(float rawValue, float transformedValue, float policy)
{
    const int mode = (int)(policy + 0.5f);
    return mode == 3 ? rawValue : transformedValue;
}

float2 Rotate_MaterialUV(float2 uv, float rotation)
{
    const int mode = (int)(rotation + 0.5f);
    if (mode == 1)
        return float2(1.f - uv.y, uv.x);
    if (mode == 2)
        return 1.f - uv;
    if (mode == 3)
        return float2(uv.y, 1.f - uv.x);
    return uv;
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
    const float3 baseNormal = normalize(normal);
    if (g_EffectMeshModelMaterialFlags.y <= 0.5f || g_EffectMeshGlassSurfaceParams.z <= 0.f)
        return baseNormal;

    const float3 tangentNormal = Decode_EffectMeshModelNormal(g_ModelNormalTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(uv, policy)));
    const float3 resolvedNormal = normalize(
        tangentNormal.x * normalize(tangent) +
        tangentNormal.y * normalize(binormal) +
        tangentNormal.z * baseNormal
    );
    return normalize(lerp(baseNormal, resolvedNormal, saturate(g_EffectMeshGlassSurfaceParams.z)));
}

float Resolve_SourceScalar(float4 texel, float sourceIndex)
{
    return Effect_SelectScalarSource(texel, (int)(sourceIndex + 0.5f));
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
    float3 vWorldPosition : TEXCOORD0;
    float2 vTexcoord : TEXCOORD1;
    float3 vNormal : TEXCOORD2;
    float3 vTangent : TEXCOORD3;
    float3 vBinormal : TEXCOORD4;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;
    const float4 worldPosition = mul(float4(In.vPosition, 1.f), g_WorldMatrix);

    Out.vPosition = mul(worldPosition, mul(g_ViewMatrix, g_ProjMatrix));
    Out.vWorldPosition = worldPosition.xyz;
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

PS_OUT PS_GLASS(VS_OUT In)
{
    PS_OUT Out = (PS_OUT)0;

    const float2 mainUVPolicy = g_EffectMeshUVAxisPolicyParams.xy;
    const float2 noiseUVPolicy = g_EffectMeshUVAxisPolicyParams.zw;
    const float2 maskUVPolicy = g_EffectMeshMaskUVAxisPolicyParams.xy;
    const float2 mainUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMainUVParams, g_EffectMeshUVOffsetParams.xy, mainUVPolicy, g_EffectMeshUVRotationParams.x);
    const float2 noiseUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshNoiseUVParams, g_EffectMeshUVOffsetParams.zw, noiseUVPolicy, g_EffectMeshUVRotationParams.y);
    const float2 maskUV = Build_EffectMeshUV(In.vTexcoord, g_EffectMeshMaskUVParams, g_EffectMeshMaskUVOffsetParams.xy, maskUVPolicy, g_EffectMeshUVRotationParams.z);
    const float4 mainTexel = Sample_EffectMeshMainTexture(mainUV, mainUVPolicy);
    const float4 noiseTexel =
        0 != g_EffectMeshParams.w
        ? Sample_EffectMeshNoiseTexture(noiseUV, noiseUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);
    const float4 maskTexel =
        0 != g_EffectMeshAlphaParams.z
        ? Sample_EffectMeshMaskTexture(maskUV, maskUVPolicy)
        : float4(1.f, 1.f, 1.f, 1.f);
    const float noiseScalar = Effect_ApplySourceInvert(Resolve_SourceScalar(noiseTexel, g_EffectMeshSourceParams.x), g_EffectMeshSourceParams.z);
    const float maskScalar = Effect_ApplySourceInvert(Resolve_SourceScalar(maskTexel, g_EffectMeshSourceParams.y), g_EffectMeshSourceParams.w);
    const float noiseBreakup = lerp(1.f, noiseScalar, saturate(g_EffectMeshGlassRimParams.z));
    const float maskCoverage = lerp(1.f, maskScalar, saturate(g_EffectMeshGlassRimParams.w));
    const float coverage = pow(saturate(maskCoverage * noiseBreakup * mainTexel.a), max(g_EffectMeshGlassSurfaceParams.y, 0.0001f));

    const float3 normal = Resolve_EffectMeshModelNormal(mainUV, mainUVPolicy, In.vNormal, In.vTangent, In.vBinormal);
    const float3 viewDir = normalize(g_CamPosition.xyz - In.vWorldPosition);
    const float3 lightDir = normalize(g_EffectMeshGlassLightDirection.xyz);
    const float fresnel = pow(1.f - saturate(dot(viewDir, normal)), max(g_EffectMeshGlassRimParams.y, 0.0001f));
    const float3 halfVector = normalize(viewDir + lightDir);
    const float specBase = pow(saturate(dot(normal, halfVector)), max(g_EffectMeshGlassSpecularParams.x, 0.0001f));
    const float specular = pow(specBase, 1.f / max(g_EffectMeshGlassSpecularParams.y, 0.0001f));

    const float3 patternedTint = lerp(g_Tint.rgb, g_Tint.rgb * mainTexel.rgb, saturate(g_EffectMeshGlassSurfaceParams.w));
    const float3 rimColor = g_EffectMeshGlassRimColor.rgb * fresnel * max(g_EffectMeshGlassRimParams.x, 0.f);
    const float3 lightColor = g_EffectMeshGlassLightColor.rgb * specular * max(g_EffectMeshGlassLightDirection.w, 0.f);
    const float3 emissive = g_EffectMeshModelMaterialFlags.z > 0.5f
                            ? g_ModelEmissiveTexture.Sample(LinearClampSampler, Apply_EffectMeshAxisAddress(mainUV, mainUVPolicy)).rgb
                            : float3(0.f, 0.f, 0.f);
    const float alpha = saturate((g_EffectMeshGlassSurfaceParams.x + fresnel * 0.35f + specular * 0.25f) * coverage * g_Tint.a);

    Out.color = float4((patternedTint * 0.18f + rimColor + lightColor + emissive) * max(g_EffectMeshParams.x, 0.f), alpha);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(GlassAlphaBlendPass, RS_Default, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, PS_GLASS)
}
