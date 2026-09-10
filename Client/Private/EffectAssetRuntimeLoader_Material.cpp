#include "pch.h"
#include "EffectAssetRuntimeLoader_Support.h"

NS_BEGIN(Client)

namespace EffectAssetRuntimeLoad::Material
{
    using namespace Curves;
    using namespace Json;

    bool Is_ScrollSpeedScaleTarget(EffectMaterialScalarModulationTarget target)
    {
        return target == EffectMaterialScalarModulationTarget::MainUVScrollSpeedScale ||
               target == EffectMaterialScalarModulationTarget::NoiseUVScrollSpeedScale ||
               target == EffectMaterialScalarModulationTarget::MaskUVScrollSpeedScale ||
               target == EffectMaterialScalarModulationTarget::FlowUVScrollSpeedScale;
    }

    void Apply_MaterialPayload(
        const json& material,
        EffectRequiredMaterialRuntimeDesc& materialDesc)
    {
        const EffectMaterialInstanceData defaultMaterial{};

        Read_Enum(material, "materialFamily", materialDesc.materialFamily);
        materialDesc.mainTextureGuid = Read_String(material, "mainTextureGuid", materialDesc.mainTextureGuid);
        materialDesc.mainTexturePath = Read_String(material, "mainTexturePath", materialDesc.mainTexturePath);
        materialDesc.noiseTextureGuid = Read_String(material, "noiseTextureGuid", materialDesc.noiseTextureGuid);
        materialDesc.noiseTexturePath = Read_String(material, "noiseTexturePath", materialDesc.noiseTexturePath);
        materialDesc.maskTextureGuid = Read_String(material, "maskTextureGuid", materialDesc.maskTextureGuid);
        materialDesc.maskTexturePath = Read_String(material, "maskTexturePath", materialDesc.maskTexturePath);
        materialDesc.flowTextureGuid = Read_String(material, "flowTextureGuid", materialDesc.flowTextureGuid);
        materialDesc.flowTexturePath = Read_String(material, "flowTexturePath", materialDesc.flowTexturePath);

        Read_Enum(material, "blendMode", materialDesc.blendMode);

        const string opacitySource = Read_String(material, "opacitySource");
        if (!opacitySource.empty())
            materialDesc.opacitySource = opacitySource;

        const auto tintIter = material.find("tint");
        if (tintIter != material.end())
            Read_Vec4(*tintIter, materialDesc.tint);

        materialDesc.intensity = Read_Float(material, "intensity", materialDesc.intensity);
        materialDesc.opacityPower = Read_Float(material, "opacityPower", materialDesc.opacityPower);
        materialDesc.alphaMultiplier = Read_Float(material, "alphaMultiplier", materialDesc.alphaMultiplier);
        materialDesc.noiseStrength = Read_Float(material, "noiseStrength", materialDesc.noiseStrength);
        materialDesc.alphaCutoff = Read_Float(material, "alphaCutoff", materialDesc.alphaCutoff);
        materialDesc.alphaErosion = Read_Float(material, "alphaErosion", materialDesc.alphaErosion);
        materialDesc.noiseSource = Read_String(material, "noiseSource", materialDesc.noiseSource);
        materialDesc.maskSource = Read_String(material, "maskSource", materialDesc.maskSource);
        materialDesc.noiseInvert = Read_Bool(material, "noiseInvert", materialDesc.noiseInvert);
        materialDesc.maskInvert = Read_Bool(material, "maskInvert", materialDesc.maskInvert);

        if (const auto coreEmissiveIter = material.find("coreEmissive"); coreEmissiveIter != material.end() && coreEmissiveIter->is_object())
        {
            const json& coreEmissive = *coreEmissiveIter;
            materialDesc.coreEmissive.enabled = Read_Bool(coreEmissive, "enabled", materialDesc.coreEmissive.enabled);
            if (const auto coreColorIter = coreEmissive.find("coreColor"); coreColorIter != coreEmissive.end())
                Read_Color(*coreColorIter, materialDesc.coreEmissive.coreColor);
            materialDesc.coreEmissive.corePower = Read_Float(coreEmissive, "corePower", materialDesc.coreEmissive.corePower);
            materialDesc.coreEmissive.coreIntensity = Read_Float(coreEmissive, "coreIntensity", materialDesc.coreEmissive.coreIntensity);
            materialDesc.coreEmissive.outerPower = Read_Float(coreEmissive, "outerPower", materialDesc.coreEmissive.outerPower);
            materialDesc.coreEmissive.outerIntensity = Read_Float(coreEmissive, "outerIntensity", materialDesc.coreEmissive.outerIntensity);
        }

        if (const auto mainUVOffsetIter = material.find("mainUVOffset"); mainUVOffsetIter != material.end())
            Read_Vec2(*mainUVOffsetIter, materialDesc.mainUVOffset);

        if (const auto mainUVScaleIter = material.find("mainUVScale"); mainUVScaleIter != material.end())
            Read_Vec2(*mainUVScaleIter, materialDesc.mainUVScale);

        if (const auto mainUVScrollSpeedIter = material.find("mainUVScrollSpeed"); mainUVScrollSpeedIter != material.end())
            Read_Vec2(*mainUVScrollSpeedIter, materialDesc.mainUVScrollSpeed);

        if (const auto noiseUVOffsetIter = material.find("noiseUVOffset"); noiseUVOffsetIter != material.end())
            Read_Vec2(*noiseUVOffsetIter, materialDesc.noiseUVOffset);

        if (const auto noiseUVScaleIter = material.find("noiseUVScale"); noiseUVScaleIter != material.end())
            Read_Vec2(*noiseUVScaleIter, materialDesc.noiseUVScale);

        if (const auto noiseUVScrollSpeedIter = material.find("noiseUVScrollSpeed"); noiseUVScrollSpeedIter != material.end())
            Read_Vec2(*noiseUVScrollSpeedIter, materialDesc.noiseUVScrollSpeed);

        if (const auto maskUVOffsetIter = material.find("maskUVOffset"); maskUVOffsetIter != material.end())
            Read_Vec2(*maskUVOffsetIter, materialDesc.maskUVOffset);

        if (const auto maskUVScaleIter = material.find("maskUVScale"); maskUVScaleIter != material.end())
            Read_Vec2(*maskUVScaleIter, materialDesc.maskUVScale);

        if (const auto maskUVScrollSpeedIter = material.find("maskUVScrollSpeed"); maskUVScrollSpeedIter != material.end())
            Read_Vec2(*maskUVScrollSpeedIter, materialDesc.maskUVScrollSpeed);

        if (const auto flowUVScaleIter = material.find("flowUVScale"); flowUVScaleIter != material.end())
            Read_Vec2(*flowUVScaleIter, materialDesc.flowUVScale);

        if (const auto flowUVOffsetIter = material.find("flowUVOffset"); flowUVOffsetIter != material.end())
            Read_Vec2(*flowUVOffsetIter, materialDesc.flowUVOffset);

        if (const auto flowUVScrollSpeedIter = material.find("flowUVScrollSpeed"); flowUVScrollSpeedIter != material.end())
            Read_Vec2(*flowUVScrollSpeedIter, materialDesc.flowUVScrollSpeed);

        materialDesc.mainUVTilingMode = defaultMaterial.mainUVTilingMode;
        materialDesc.noiseUVTilingMode = defaultMaterial.noiseUVTilingMode;
        materialDesc.maskUVTilingMode = defaultMaterial.maskUVTilingMode;
        materialDesc.flowUVTilingMode = defaultMaterial.flowUVTilingMode;
        Read_Enum(material, "mainUVTilingMode", materialDesc.mainUVTilingMode);
        Read_Enum(material, "noiseUVTilingMode", materialDesc.noiseUVTilingMode);
        Read_Enum(material, "maskUVTilingMode", materialDesc.maskUVTilingMode);
        Read_Enum(material, "flowUVTilingMode", materialDesc.flowUVTilingMode);
        Read_UVAxisPolicy(material, "mainUVPolicy", materialDesc.mainUVTilingMode, materialDesc.mainUVPolicy, materialDesc.mainUVTilingMode);
        Read_UVAxisPolicy(material, "noiseUVPolicy", materialDesc.noiseUVTilingMode, materialDesc.noiseUVPolicy, materialDesc.noiseUVTilingMode);
        Read_UVAxisPolicy(material, "maskUVPolicy", materialDesc.maskUVTilingMode, materialDesc.maskUVPolicy, materialDesc.maskUVTilingMode);
        Read_UVAxisPolicy(material, "flowUVPolicy", materialDesc.flowUVTilingMode, materialDesc.flowUVPolicy, materialDesc.flowUVTilingMode);
        Read_Enum(material, "mainUVRotation", materialDesc.mainUVRotation);
        Read_Enum(material, "noiseUVRotation", materialDesc.noiseUVRotation);
        Read_Enum(material, "maskUVRotation", materialDesc.maskUVRotation);
        Read_Enum(material, "flowUVRotation", materialDesc.flowUVRotation);
        materialDesc.refractionIntensity = Read_Float(material, "refractionIntensity", materialDesc.refractionIntensity);
        materialDesc.refractionPresence = Read_Float(material, "refractionPresence", materialDesc.refractionPresence);
        Read_Enum(material, "distortionShapeMode", materialDesc.distortionShapeMode);
        Read_Enum(material, "airSheathMapInterpretation", materialDesc.airSheathMapInterpretation);
        Read_Enum(material, "airSheathMapXSource", materialDesc.airSheathMapXSource);
        Read_Enum(material, "airSheathMapYSource", materialDesc.airSheathMapYSource);
        Read_Enum(material, "airSheathMapVectorSpace", materialDesc.airSheathMapVectorSpace);
        Read_Enum(material, "airSheathMapComposition", materialDesc.airSheathMapComposition);
        materialDesc.airSheathMapInfluence = Read_Float(material, "airSheathMapInfluence", materialDesc.airSheathMapInfluence);
        materialDesc.distortionShapeRadius = Read_Float(material, "distortionShapeRadius", materialDesc.distortionShapeRadius);
        materialDesc.distortionShapeThickness = Read_Float(material, "distortionShapeThickness", materialDesc.distortionShapeThickness);
        materialDesc.distortionShapeSoftness = Read_Float(material, "distortionShapeSoftness", materialDesc.distortionShapeSoftness);
        materialDesc.glassAlpha = Read_Float(material, "glassAlpha", materialDesc.glassAlpha);
        materialDesc.glassAlphaPower = Read_Float(material, "glassAlphaPower", materialDesc.glassAlphaPower);
        materialDesc.glassNormalStrength = Read_Float(material, "glassNormalStrength", materialDesc.glassNormalStrength);
        if (const auto glassRimColorIter = material.find("glassRimColor"); glassRimColorIter != material.end())
            Read_Color(*glassRimColorIter, materialDesc.glassRimColor);
        materialDesc.glassRimIntensity = Read_Float(material, "glassRimIntensity", materialDesc.glassRimIntensity);
        materialDesc.glassRimPower = Read_Float(material, "glassRimPower", materialDesc.glassRimPower);
        if (const auto glassLightDirectionIter = material.find("glassLightDirection"); glassLightDirectionIter != material.end())
            Read_Vec3(*glassLightDirectionIter, materialDesc.glassLightDirection);
        if (const auto glassLightColorIter = material.find("glassLightColor"); glassLightColorIter != material.end())
            Read_Color(*glassLightColorIter, materialDesc.glassLightColor);
        materialDesc.glassLightIntensity = Read_Float(material, "glassLightIntensity", materialDesc.glassLightIntensity);
        materialDesc.glassSpecularPower = Read_Float(material, "glassSpecularPower", materialDesc.glassSpecularPower);
        materialDesc.glassSpecularSoftness = Read_Float(material, "glassSpecularSoftness", materialDesc.glassSpecularSoftness);
        materialDesc.glassMainInfluence = Read_Float(material, "glassMainInfluence", materialDesc.glassMainInfluence);
        materialDesc.glassNoiseBreakup = Read_Float(material, "glassNoiseBreakup", materialDesc.glassNoiseBreakup);
        materialDesc.glassMaskStrength = Read_Float(material, "glassMaskStrength", materialDesc.glassMaskStrength);
        materialDesc.subUVRows = max(1u, Read_UInt(material, "subUVRows", materialDesc.subUVRows));
        materialDesc.subUVCols = max(1u, Read_UInt(material, "subUVCols", materialDesc.subUVCols));
        materialDesc.twoSided = Read_Bool(material, "twoSided", materialDesc.twoSided);

        const auto additiveIter = material.find("additive");
        if (additiveIter != material.end() && additiveIter->is_object())
        {
            const json& additive = *additiveIter;
            Read_Enum(additive, "colorSource", materialDesc.additive.colorSource);
            Read_Enum(additive, "amountSource", materialDesc.additive.amountSource);
            Read_Enum(additive, "coveragePolicy", materialDesc.additive.coveragePolicy);
            materialDesc.additive.intensityScale =
                Read_Float(additive, "intensityScale", materialDesc.additive.intensityScale);
            materialDesc.additive.blackNeutral =
                Read_Bool(additive, "blackNeutral", materialDesc.additive.blackNeutral);

            const auto emissiveColorIter = additive.find("emissiveColor");
            if (emissiveColorIter != additive.end())
                Read_Color(*emissiveColorIter, materialDesc.additive.emissiveColor);
            const auto constantColorIter = additive.find("constantColor");
            if (constantColorIter != additive.end())
                Read_Color(*constantColorIter, materialDesc.additive.constantColor);
        }
    }

    void Fill_MaterialScalarModulationPayload(
        EffectMaterialScalarModulationRuntimeDesc& desc,
        const json& materialScalarModulation)
    {
        desc = EffectMaterialScalarModulationRuntimeDesc{};

        const auto modulatorsIter = materialScalarModulation.find("modulators");
        if (modulatorsIter == materialScalarModulation.end() || !modulatorsIter->is_array())
            return;

        const uint32 count = min(
            EffectMaterialScalarModulationRuntimeDesc::kMaxModulators,
            static_cast<uint32>(modulatorsIter->size())
        );
        desc.count = count;

        for (uint32 index = 0; index < count; ++index)
        {
            const json& source = (*modulatorsIter)[index];
            EffectMaterialScalarModulatorRuntimeDesc& target = desc.modulators[index];
            target.enabled = Read_Bool(source, "enabled", target.enabled);
            Read_Enum(source, "targetField", target.targetField);
            Read_Enum(source, "operation", target.operation);
            Read_Enum(source, "timeSource", target.timeSource);
            if (Is_ScrollSpeedScaleTarget(target.targetField))
                target.timeSource = EffectMaterialScalarModulationTimeSource::EmitterTime;

            const auto distributionIter = source.find("distribution");
            if (distributionIter == source.end() || !distributionIter->is_object())
                continue;

            target.curve = EffectMaterialScalarModulationCurveDesc{};
            target.curve.enabled = true;

            const string mode = Read_String(*distributionIter, "mode");
            const json* payload = Find_DistributionPayload(*distributionIter);
            if (payload != nullptr && mode == "ConstantCurve")
            {
                const auto keysIter = payload->find("keys");
                if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
                {
                    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
                    target.curve.keyCount = keyCount;
                    target.curve.keyTimes = Vec4{};
                    target.curve.keyTimesBlock1 = Vec4{};
                    target.curve.keyValues = Vec4{};
                    target.curve.keyValuesBlock1 = Vec4{};
                    target.curve.keyArriveTangents = Vec4{};
                    target.curve.keyArriveTangentsBlock1 = Vec4{};
                    target.curve.keyLeaveTangents = Vec4{};
                    target.curve.keyLeaveTangentsBlock1 = Vec4{};
                    target.curve.keyModes = Vec4{};
                    target.curve.keyModesBlock1 = Vec4{};

                    for (uint32 keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                    {
                        const json& key = (*keysIter)[keyIndex];
                        Set_CurveComponent(target.curve.keyTimes, target.curve.keyTimesBlock1, keyIndex, clamp(Read_Float(key, "time"), 0.f, 1.f));
                        Set_CurveComponent(target.curve.keyValues, target.curve.keyValuesBlock1, keyIndex, Read_Float(key, "value", 1.f));
                        Set_CurveComponent(target.curve.keyArriveTangents, target.curve.keyArriveTangentsBlock1, keyIndex, Read_Float(key, "arriveTangent"));
                        Set_CurveComponent(target.curve.keyLeaveTangents, target.curve.keyLeaveTangentsBlock1, keyIndex, Read_Float(key, "leaveTangent"));
                        Set_CurveComponent(target.curve.keyModes, target.curve.keyModesBlock1, keyIndex, Resolve_CurveInterpolationMode(key));
                    }
                    continue;
                }
            }

            float value = 1.f;
            if (payload != nullptr && mode == "Constant")
                value = Read_Float(*payload, "value", value);
            else if (payload != nullptr && mode == "Uniform")
                value = (Read_Float(*payload, "minValue", value) + Read_Float(*payload, "maxValue", value)) * 0.5f;

            target.curve.keyCount = 2;
            target.curve.keyValues = Vec4{ value, value, 0.f, 0.f };
            target.curve.keyValuesBlock1 = Vec4{};
        }
    }

    void Fill_CoreColorRgbModulationPayload(
        EffectMaterialCoreColorRgbModulationRuntimeDesc& desc,
        const json& materialScalarModulation)
    {
        desc = EffectMaterialCoreColorRgbModulationRuntimeDesc{};

        const auto modulatorsIter = materialScalarModulation.find("coreColorRgbModulators");
        if (modulatorsIter == materialScalarModulation.end() || !modulatorsIter->is_array())
            return;

        const uint32 count = min(
            EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators,
            static_cast<uint32>(modulatorsIter->size())
        );
        desc.count = count;

        for (uint32 index = 0; index < count; ++index)
        {
            const json& source = (*modulatorsIter)[index];
            EffectMaterialCoreColorRgbModulatorRuntimeDesc& target = desc.modulators[index];
            target.enabled = Read_Bool(source, "enabled", target.enabled);
            Read_Enum(source, "timeSource", target.timeSource);

            const auto distributionIter = source.find("distribution");
            if (distributionIter == source.end() || !distributionIter->is_object())
                continue;

            target.curve = EffectMaterialCoreColorRgbModulationCurveDesc{};
            target.curve.enabled = true;

            const string mode = Read_String(*distributionIter, "mode");
            const json* payload = Find_DistributionPayload(*distributionIter);
            if (payload != nullptr && mode == "ConstantCurve")
            {
                const auto keysIter = payload->find("keys");
                if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
                {
                    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
                    target.curve.keyCount = keyCount;
                    target.curve.keyTimes = Vec4{};
                    target.curve.keyTimesBlock1 = Vec4{};
                    target.curve.keyValuesR = Vec4{};
                    target.curve.keyValuesRBlock1 = Vec4{};
                    target.curve.keyValuesG = Vec4{};
                    target.curve.keyValuesGBlock1 = Vec4{};
                    target.curve.keyValuesB = Vec4{};
                    target.curve.keyValuesBBlock1 = Vec4{};
                    target.curve.keyArriveTangentsR = Vec4{};
                    target.curve.keyArriveTangentsRBlock1 = Vec4{};
                    target.curve.keyArriveTangentsG = Vec4{};
                    target.curve.keyArriveTangentsGBlock1 = Vec4{};
                    target.curve.keyArriveTangentsB = Vec4{};
                    target.curve.keyArriveTangentsBBlock1 = Vec4{};
                    target.curve.keyLeaveTangentsR = Vec4{};
                    target.curve.keyLeaveTangentsRBlock1 = Vec4{};
                    target.curve.keyLeaveTangentsG = Vec4{};
                    target.curve.keyLeaveTangentsGBlock1 = Vec4{};
                    target.curve.keyLeaveTangentsB = Vec4{};
                    target.curve.keyLeaveTangentsBBlock1 = Vec4{};
                    target.curve.keyModes = Vec4{};
                    target.curve.keyModesBlock1 = Vec4{};

                    for (uint32 keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                    {
                        const json& key = (*keysIter)[keyIndex];
                        Vec3 value{ 1.f, 0.85f, 0.45f };
                        Vec3 arriveTangent{};
                        Vec3 leaveTangent{};
                        if (const auto valueIter = key.find("value"); valueIter != key.end())
                            Read_Vec3(*valueIter, value);
                        if (const auto arriveIter = key.find("arriveTangent"); arriveIter != key.end())
                            Read_Vec3(*arriveIter, arriveTangent);
                        if (const auto leaveIter = key.find("leaveTangent"); leaveIter != key.end())
                            Read_Vec3(*leaveIter, leaveTangent);

                        Set_CurveComponent(target.curve.keyTimes, target.curve.keyTimesBlock1, keyIndex, clamp(Read_Float(key, "time"), 0.f, 1.f));
                        Set_CurveComponent(target.curve.keyValuesR, target.curve.keyValuesRBlock1, keyIndex, value.x);
                        Set_CurveComponent(target.curve.keyValuesG, target.curve.keyValuesGBlock1, keyIndex, value.y);
                        Set_CurveComponent(target.curve.keyValuesB, target.curve.keyValuesBBlock1, keyIndex, value.z);
                        Set_CurveComponent(target.curve.keyArriveTangentsR, target.curve.keyArriveTangentsRBlock1, keyIndex, arriveTangent.x);
                        Set_CurveComponent(target.curve.keyArriveTangentsG, target.curve.keyArriveTangentsGBlock1, keyIndex, arriveTangent.y);
                        Set_CurveComponent(target.curve.keyArriveTangentsB, target.curve.keyArriveTangentsBBlock1, keyIndex, arriveTangent.z);
                        Set_CurveComponent(target.curve.keyLeaveTangentsR, target.curve.keyLeaveTangentsRBlock1, keyIndex, leaveTangent.x);
                        Set_CurveComponent(target.curve.keyLeaveTangentsG, target.curve.keyLeaveTangentsGBlock1, keyIndex, leaveTangent.y);
                        Set_CurveComponent(target.curve.keyLeaveTangentsB, target.curve.keyLeaveTangentsBBlock1, keyIndex, leaveTangent.z);
                        Set_CurveComponent(target.curve.keyModes, target.curve.keyModesBlock1, keyIndex, Resolve_CurveInterpolationMode(key));
                    }
                    continue;
                }
            }

            Vec3 value{ 1.f, 0.85f, 0.45f };
            if (payload != nullptr && mode == "Constant")
            {
                if (const auto valueIter = payload->find("value"); valueIter != payload->end())
                    Read_Vec3(*valueIter, value);
            }
            else if (payload != nullptr && mode == "Uniform")
            {
                Vec3 minValue = value;
                Vec3 maxValue = value;
                if (const auto minIter = payload->find("minValue"); minIter != payload->end())
                    Read_Vec3(*minIter, minValue);
                if (const auto maxIter = payload->find("maxValue"); maxIter != payload->end())
                    Read_Vec3(*maxIter, maxValue);
                target.curve.uniformEnabled = true;
                target.curve.uniformMin = minValue;
                target.curve.uniformMax = maxValue;
                target.curve.uniformSeed = Read_RandomSeedDesc(*payload);
                continue;
            }

            target.curve.keyCount = 2;
            target.curve.keyValuesR = Vec4{ value.x, value.x, 0.f, 0.f };
            target.curve.keyValuesRBlock1 = Vec4{};
            target.curve.keyValuesG = Vec4{ value.y, value.y, 0.f, 0.f };
            target.curve.keyValuesGBlock1 = Vec4{};
            target.curve.keyValuesB = Vec4{ value.z, value.z, 0.f, 0.f };
            target.curve.keyValuesBBlock1 = Vec4{};
        }
    }

    void Fill_MaterialVec2ModulationPayload(
        EffectMaterialVec2ModulationRuntimeDesc& desc,
        const json& materialScalarModulation)
    {
        desc = EffectMaterialVec2ModulationRuntimeDesc{};

        const auto modulatorsIter = materialScalarModulation.find("vec2Modulators");
        if (modulatorsIter == materialScalarModulation.end() || !modulatorsIter->is_array())
            return;

        const uint32 count = min(
            EffectMaterialVec2ModulationRuntimeDesc::kMaxModulators,
            static_cast<uint32>(modulatorsIter->size())
        );
        desc.count = count;

        for (uint32 index = 0; index < count; ++index)
        {
            const json& source = (*modulatorsIter)[index];
            EffectMaterialVec2ModulatorRuntimeDesc& target = desc.modulators[index];
            target.enabled = Read_Bool(source, "enabled", target.enabled);
            Read_Enum(source, "targetField", target.targetField);
            Read_Enum(source, "timeSource", target.timeSource);
            target.operation = EffectMaterialVec2ModulationOperation::Add;

            const auto distributionIter = source.find("distribution");
            if (distributionIter == source.end() || !distributionIter->is_object())
                continue;

            target.curve = EffectMaterialVec2ModulationCurveDesc{};
            target.curve.enabled = true;

            const string mode = Read_String(*distributionIter, "mode");
            const json* payload = Find_DistributionPayload(*distributionIter);
            if (payload != nullptr && mode == "ConstantCurve")
            {
                const auto keysIter = payload->find("keys");
                if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
                {
                    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
                    target.curve.keyCount = keyCount;
                    target.curve.keyTimes = Vec4{};
                    target.curve.keyTimesBlock1 = Vec4{};
                    target.curve.keyValuesX = Vec4{};
                    target.curve.keyValuesXBlock1 = Vec4{};
                    target.curve.keyValuesY = Vec4{};
                    target.curve.keyValuesYBlock1 = Vec4{};
                    target.curve.keyArriveTangentsX = Vec4{};
                    target.curve.keyArriveTangentsXBlock1 = Vec4{};
                    target.curve.keyArriveTangentsY = Vec4{};
                    target.curve.keyArriveTangentsYBlock1 = Vec4{};
                    target.curve.keyLeaveTangentsX = Vec4{};
                    target.curve.keyLeaveTangentsXBlock1 = Vec4{};
                    target.curve.keyLeaveTangentsY = Vec4{};
                    target.curve.keyLeaveTangentsYBlock1 = Vec4{};
                    target.curve.keyModes = Vec4{};
                    target.curve.keyModesBlock1 = Vec4{};

                    for (uint32 keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                    {
                        const json& key = (*keysIter)[keyIndex];
                        Vec2 value{};
                        Vec2 arriveTangent{};
                        Vec2 leaveTangent{};
                        if (const auto valueIter = key.find("value"); valueIter != key.end())
                            Read_Vec2(*valueIter, value);
                        if (const auto arriveIter = key.find("arriveTangent"); arriveIter != key.end())
                            Read_Vec2(*arriveIter, arriveTangent);
                        if (const auto leaveIter = key.find("leaveTangent"); leaveIter != key.end())
                            Read_Vec2(*leaveIter, leaveTangent);

                        Set_CurveComponent(target.curve.keyTimes, target.curve.keyTimesBlock1, keyIndex, clamp(Read_Float(key, "time"), 0.f, 1.f));
                        Set_CurveComponent(target.curve.keyValuesX, target.curve.keyValuesXBlock1, keyIndex, value.x);
                        Set_CurveComponent(target.curve.keyValuesY, target.curve.keyValuesYBlock1, keyIndex, value.y);
                        Set_CurveComponent(target.curve.keyArriveTangentsX, target.curve.keyArriveTangentsXBlock1, keyIndex, arriveTangent.x);
                        Set_CurveComponent(target.curve.keyArriveTangentsY, target.curve.keyArriveTangentsYBlock1, keyIndex, arriveTangent.y);
                        Set_CurveComponent(target.curve.keyLeaveTangentsX, target.curve.keyLeaveTangentsXBlock1, keyIndex, leaveTangent.x);
                        Set_CurveComponent(target.curve.keyLeaveTangentsY, target.curve.keyLeaveTangentsYBlock1, keyIndex, leaveTangent.y);
                        Set_CurveComponent(target.curve.keyModes, target.curve.keyModesBlock1, keyIndex, Resolve_CurveInterpolationMode(key));
                    }
                    continue;
                }
            }

            Vec2 value{};
            if (payload != nullptr && mode == "Constant")
            {
                if (const auto valueIter = payload->find("value"); valueIter != payload->end())
                    Read_Vec2(*valueIter, value);
            }
            else if (payload != nullptr && mode == "Uniform")
            {
                Vec2 minValue{};
                Vec2 maxValue{};
                if (const auto minIter = payload->find("minValue"); minIter != payload->end())
                    Read_Vec2(*minIter, minValue);
                if (const auto maxIter = payload->find("maxValue"); maxIter != payload->end())
                    Read_Vec2(*maxIter, maxValue);
                value = (minValue + maxValue) * 0.5f;
            }

            target.curve.keyCount = 2;
            target.curve.keyValuesX = Vec4{ value.x, value.x, 0.f, 0.f };
            target.curve.keyValuesY = Vec4{ value.y, value.y, 0.f, 0.f };
        }
    }
}

NS_END
