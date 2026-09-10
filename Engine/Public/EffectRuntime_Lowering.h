#pragma once

// Effect authoring/material instance 값을 runtime desc와 runtime policy로 접는 공용 helper를 정의한다.

#include "EffectMaterial_Types.h"
#include "EffectRuntime_Types.h"

namespace Engine::EffectRuntime
{
// authoring material instance 값을 runtime material desc로 복사한다.
inline void Copy_EffectMaterialInstanceToRuntimeDesc(
    const EffectMaterialInstanceData& material,
    EffectRequiredMaterialRuntimeDesc& outMaterial)
{
    outMaterial.materialFamily = material.materialFamily;
    outMaterial.mainTextureGuid = material.mainTextureGuid;
    outMaterial.mainTexturePath = material.mainTexturePath;
    outMaterial.noiseTextureGuid = material.noiseTextureGuid;
    outMaterial.noiseTexturePath = material.noiseTexturePath;
    outMaterial.maskTextureGuid = material.maskTextureGuid;
    outMaterial.maskTexturePath = material.maskTexturePath;
    outMaterial.flowTextureGuid = material.flowTextureGuid;
    outMaterial.flowTexturePath = material.flowTexturePath;

    outMaterial.blendMode = material.blendMode;
    outMaterial.opacitySource = material.opacitySource.empty() ? "Alpha" : material.opacitySource;
    outMaterial.tint = material.tint;
    outMaterial.intensity = material.intensity;
    outMaterial.opacityPower = material.opacityPower;
    outMaterial.alphaMultiplier = material.alphaMultiplier;
    outMaterial.noiseStrength = material.noiseStrength;
    outMaterial.alphaCutoff = material.alphaCutoff;
    outMaterial.alphaErosion = material.alphaErosion;
    outMaterial.noiseSource = material.noiseSource.empty() ? "Red" : material.noiseSource;
    outMaterial.maskSource = material.maskSource.empty() ? "Alpha" : material.maskSource;
    outMaterial.noiseInvert = material.noiseInvert;
    outMaterial.maskInvert = material.maskInvert;

    outMaterial.mainUVScale = material.mainUVScale;
    outMaterial.mainUVOffset = material.mainUVOffset;
    outMaterial.mainUVScrollSpeed = material.mainUVScrollSpeed;
    outMaterial.mainUVTilingMode = material.mainUVTilingMode;
    outMaterial.mainUVPolicy = material.mainUVPolicy;
    outMaterial.mainUVRotation = material.mainUVRotation;
    outMaterial.noiseUVScale = material.noiseUVScale;
    outMaterial.noiseUVOffset = material.noiseUVOffset;
    outMaterial.noiseUVScrollSpeed = material.noiseUVScrollSpeed;
    outMaterial.noiseUVTilingMode = material.noiseUVTilingMode;
    outMaterial.noiseUVPolicy = material.noiseUVPolicy;
    outMaterial.noiseUVRotation = material.noiseUVRotation;
    outMaterial.maskUVScale = material.maskUVScale;
    outMaterial.maskUVOffset = material.maskUVOffset;
    outMaterial.maskUVScrollSpeed = material.maskUVScrollSpeed;
    outMaterial.maskUVTilingMode = material.maskUVTilingMode;
    outMaterial.maskUVPolicy = material.maskUVPolicy;
    outMaterial.maskUVRotation = material.maskUVRotation;
    outMaterial.flowUVScale = material.flowUVScale;
    outMaterial.flowUVOffset = material.flowUVOffset;
    outMaterial.flowUVScrollSpeed = material.flowUVScrollSpeed;
    outMaterial.flowUVTilingMode = material.flowUVTilingMode;
    outMaterial.flowUVPolicy = material.flowUVPolicy;
    outMaterial.flowUVRotation = material.flowUVRotation;

    outMaterial.additive = material.additive;
    outMaterial.coreEmissive = material.coreEmissive;
    outMaterial.refractionIntensity = material.refractionIntensity;
    outMaterial.refractionPresence = material.refractionPresence;
    outMaterial.distortionShapeMode = material.distortionShapeMode;
    outMaterial.airSheathMapInterpretation = material.airSheathMapInterpretation;
    outMaterial.airSheathMapXSource = material.airSheathMapXSource;
    outMaterial.airSheathMapYSource = material.airSheathMapYSource;
    outMaterial.airSheathMapVectorSpace = material.airSheathMapVectorSpace;
    outMaterial.airSheathMapComposition = material.airSheathMapComposition;
    outMaterial.airSheathMapInfluence = clamp(material.airSheathMapInfluence, 0.f, 1.f);
    outMaterial.distortionShapeRadius = material.distortionShapeRadius;
    outMaterial.distortionShapeThickness = material.distortionShapeThickness;
    outMaterial.distortionShapeSoftness = material.distortionShapeSoftness;
    outMaterial.glassAlpha = material.glassAlpha;
    outMaterial.glassAlphaPower = material.glassAlphaPower;
    outMaterial.glassNormalStrength = material.glassNormalStrength;
    outMaterial.glassRimColor = material.glassRimColor;
    outMaterial.glassRimIntensity = material.glassRimIntensity;
    outMaterial.glassRimPower = material.glassRimPower;
    outMaterial.glassLightDirection = material.glassLightDirection;
    outMaterial.glassLightColor = material.glassLightColor;
    outMaterial.glassLightIntensity = material.glassLightIntensity;
    outMaterial.glassSpecularPower = material.glassSpecularPower;
    outMaterial.glassSpecularSoftness = material.glassSpecularSoftness;
    outMaterial.glassMainInfluence = material.glassMainInfluence;
    outMaterial.glassNoiseBreakup = material.glassNoiseBreakup;
    outMaterial.glassMaskStrength = material.glassMaskStrength;
    outMaterial.twoSided = material.twoSided;
    outMaterial.subUVRows = max(1u, material.subUVRows);
    outMaterial.subUVCols = max(1u, material.subUVCols);
}

// Trail runtime이 지원하지 않는 material 정책을 runtime-safe 값으로 보정한다.
inline void Apply_TrailMaterialRuntimePolicy(EffectRequiredMaterialRuntimeDesc& material)
{
    if (material.blendMode == EffectMaterialBlendMode::Masked)
        material.blendMode = EffectMaterialBlendMode::AlphaBlend;
}
}
