#ifndef SHADER_EFFECT_MESH_COMMON_HLSLI
#define SHADER_EFFECT_MESH_COMMON_HLSLI

struct EffectMeshCommonSurfaceInput
{
    float4 symbol;
    float4 noiseTexel;
    float4 maskTexel;
    float4 instanceColor;
    float4 tint;
    float4 params;
    float4 alphaParams;
    float4 sourceParams;
    float4 coreEmissiveColor;
    float4 coreEmissiveParams;
    int opacitySource;
};

struct EffectMeshCommonAdditiveInput
{
    float4 symbol;
    float4 noiseTexel;
    float4 maskTexel;
    float4 instanceColor;
    float4 tint;
    float4 params;
    float4 alphaParams;
    float4 sourceParams;
    float4 additiveParams;
    float4 additiveEmissiveColor;
    float4 additiveConstantColor;
    float4 additiveFlags;
    float4 coreEmissiveColor;
    float4 coreEmissiveParams;
    int opacitySource;
};

float EffectMeshCommon_ResolveNoiseFactor(float4 noiseTexel, float4 params, float4 sourceParams)
{
    float noiseFactor = 1.f;
    if (0 != params.w && params.z > 0.f)
    {
        const float noise = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)sourceParams.x),
            sourceParams.z
        );
        noiseFactor = lerp(1.f, noise, saturate(params.z));
    }

    return noiseFactor;
}

float EffectMeshCommon_ResolveMaskAlpha(float4 maskTexel, float4 alphaParams, float4 sourceParams)
{
    float maskAlpha = 1.f;
    if (0 != alphaParams.z)
    {
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)sourceParams.y),
            sourceParams.w
        );
    }

    return maskAlpha;
}

EffectMaterialSurface EffectMeshCommon_ResolveSurface(EffectMeshCommonSurfaceInput input)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float opacityPower = max(input.params.y, 0.0001f);
    const float alphaCutoff = saturate(input.alphaParams.x);
    const float alphaErosion = saturate(input.alphaParams.y);
    const float selectedOpacity = Effect_SelectOpacity(input.symbol, input.opacitySource);
    const float noiseFactor = EffectMeshCommon_ResolveNoiseFactor(input.noiseTexel, input.params, input.sourceParams);
    const float maskAlpha = EffectMeshCommon_ResolveMaskAlpha(input.maskTexel, input.alphaParams, input.sourceParams);
    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float coverage = Effect_BuildCoverage(
        selectedOpacity,
        opacityPower,
        maskCoverage,
        noiseFactor,
        input.instanceColor.a * input.tint.a * input.alphaParams.w
    );
    const float3 baseColor = Effect_ApplyCoreEmissiveShaping(
        input.symbol.rgb * input.instanceColor.rgb * input.tint.rgb,
        input.coreEmissiveColor,
        selectedOpacity,
        input.coreEmissiveParams
    );

    surface.color = baseColor * noiseFactor;
    surface.coverage = coverage;

    if (coverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

float3 EffectMeshCommon_ResolveAdditiveContribution(EffectMeshCommonAdditiveInput input)
{
    const float alphaCutoff = saturate(input.alphaParams.x);
    const float alphaErosion = saturate(input.alphaParams.y);
    const float noiseFactor = EffectMeshCommon_ResolveNoiseFactor(input.noiseTexel, input.params, input.sourceParams);
    const float maskAlpha = EffectMeshCommon_ResolveMaskAlpha(input.maskTexel, input.alphaParams, input.sourceParams);
    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float coverageBase = saturate(maskCoverage * noiseFactor * input.instanceColor.a * input.tint.a * input.alphaParams.w);
    const int colorSource = (int)input.additiveParams.x;
    const int amountSource = (int)input.additiveParams.y;
    const int coveragePolicy = (int)input.additiveParams.z;
    const float selectedOpacity = Effect_SelectOpacity(input.symbol, input.opacitySource);
    const float amount = Effect_SelectAdditiveAmount(input.symbol, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(input.symbol.rgb, colorSource, input.additiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        input.symbol.rgb,
        input.symbol.rgb * input.tint.rgb,
        input.additiveEmissiveColor,
        input.additiveConstantColor,
        colorSource
    );
    const float3 shapedMaterialColor = Effect_ApplyCoreEmissiveShaping(
        materialColor * input.instanceColor.rgb,
        input.coreEmissiveColor,
        selectedOpacity,
        input.coreEmissiveParams
    );
    const float additiveEnergyScalar = max(input.params.x, 0.f) * max(input.additiveParams.w, 0.f);
    return shapedMaterialColor * noiseFactor * policyAmount * additiveEnergyScalar * blackGate * coverageBase;
}

#endif
