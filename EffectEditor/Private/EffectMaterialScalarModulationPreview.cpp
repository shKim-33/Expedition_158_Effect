#include "EffectMaterialScalarModulationPreview.h"

#include "EffectMaterialScalarModulationRuntime.h"

#include <cmath>

NS_BEGIN(EffectEditor)

namespace
{
    PointParticleRandomSeedRuntimeDesc Build_RandomSeedDesc(const DistributionRandomSeedData& data)
    {
        return PointParticleRandomSeedRuntimeDesc{
            .manualSeedEnabled = data.mode == DistributionRandomSeedMode::Manual,
            .seed = data.mode == DistributionRandomSeedMode::Manual ? data.manualSeed : 0u,
            .useInstanceSeed = data.useInstanceSeed
        };
    }

    void Set_Component(Vec4& value, uint32 index, float component)
    {
        switch (index)
        {
        case 0:
            value.x = component;
            break;
        case 1:
            value.y = component;
            break;
        case 2:
            value.z = component;
            break;
        case 3:
            value.w = component;
            break;
        default:
            break;
        }
    }

    void Set_Component(Vec4& block0, Vec4& block1, uint32 index, float component)
    {
        if (index < 4u)
        {
            Set_Component(block0, index, component);
            return;
        }

        Set_Component(block1, index - 4u, component);
    }

    EffectMaterialScalarModulationTarget To_RuntimeTarget(MaterialScalarModulationTargetField target)
    {
        switch (target)
        {
        case MaterialScalarModulationTargetField::OpacityPower:
            return EffectMaterialScalarModulationTarget::OpacityPower;
        case MaterialScalarModulationTargetField::NoiseStrength:
            return EffectMaterialScalarModulationTarget::NoiseStrength;
        case MaterialScalarModulationTargetField::AlphaErosion:
            return EffectMaterialScalarModulationTarget::AlphaErosion;
        case MaterialScalarModulationTargetField::AlphaCutoff:
            return EffectMaterialScalarModulationTarget::AlphaCutoff;
        case MaterialScalarModulationTargetField::AlphaMultiplier:
            return EffectMaterialScalarModulationTarget::AlphaMultiplier;
        case MaterialScalarModulationTargetField::CoreIntensity:
            return EffectMaterialScalarModulationTarget::CoreIntensity;
        case MaterialScalarModulationTargetField::OuterIntensity:
            return EffectMaterialScalarModulationTarget::OuterIntensity;
        case MaterialScalarModulationTargetField::CoreColor:
            return EffectMaterialScalarModulationTarget::CoreColor;
        case MaterialScalarModulationTargetField::RefractionIntensity:
            return EffectMaterialScalarModulationTarget::RefractionIntensity;
        case MaterialScalarModulationTargetField::MainUVScrollSpeedScale:
            return EffectMaterialScalarModulationTarget::MainUVScrollSpeedScale;
        case MaterialScalarModulationTargetField::NoiseUVScrollSpeedScale:
            return EffectMaterialScalarModulationTarget::NoiseUVScrollSpeedScale;
        case MaterialScalarModulationTargetField::MaskUVScrollSpeedScale:
            return EffectMaterialScalarModulationTarget::MaskUVScrollSpeedScale;
        case MaterialScalarModulationTargetField::FlowUVScrollSpeedScale:
            return EffectMaterialScalarModulationTarget::FlowUVScrollSpeedScale;
        case MaterialScalarModulationTargetField::Intensity:
        default:
            return EffectMaterialScalarModulationTarget::Intensity;
        }
    }

    bool Is_ScrollSpeedScaleTarget(MaterialScalarModulationTargetField target)
    {
        return target == MaterialScalarModulationTargetField::MainUVScrollSpeedScale ||
               target == MaterialScalarModulationTargetField::NoiseUVScrollSpeedScale ||
               target == MaterialScalarModulationTargetField::MaskUVScrollSpeedScale ||
               target == MaterialScalarModulationTargetField::FlowUVScrollSpeedScale;
    }

    EffectMaterialVec2ModulationTarget To_RuntimeTarget(MaterialVec2ModulationTargetField target)
    {
        switch (target)
        {
        case MaterialVec2ModulationTargetField::NoiseUVOffset:
            return EffectMaterialVec2ModulationTarget::NoiseUVOffset;
        case MaterialVec2ModulationTargetField::MaskUVOffset:
            return EffectMaterialVec2ModulationTarget::MaskUVOffset;
        case MaterialVec2ModulationTargetField::FlowUVOffset:
            return EffectMaterialVec2ModulationTarget::FlowUVOffset;
        case MaterialVec2ModulationTargetField::MainUVOffset:
        default:
            return EffectMaterialVec2ModulationTarget::MainUVOffset;
        }
    }

    EffectMaterialScalarModulationTimeSource To_RuntimeTimeSource(MaterialScalarModulationTimeSource timeSource)
    {
        switch (timeSource)
        {
        case MaterialScalarModulationTimeSource::EmitterTime:
            return EffectMaterialScalarModulationTimeSource::EmitterTime;
        case MaterialScalarModulationTimeSource::ParticleLife:
        default:
            return EffectMaterialScalarModulationTimeSource::ParticleLife;
        }
    }

    float To_RuntimeCurveMode(FloatCurveInterpolationMode mode)
    {
        switch (mode)
        {
        case FloatCurveInterpolationMode::Constant:
            return 0.f;
        case FloatCurveInterpolationMode::CurveAutoClamped:
            return 2.f;
        case FloatCurveInterpolationMode::Linear:
        default:
            return 1.f;
        }
    }

    void Fill_CurveComponent(
        EffectMaterialScalarModulationCurveDesc& desc,
        uint32 index,
        const FloatCurveKeyData& key)
    {
        Set_Component(desc.keyTimes, desc.keyTimesBlock1, index, clamp(key.time, 0.f, 1.f));
        Set_Component(desc.keyValues, desc.keyValuesBlock1, index, key.value);
        Set_Component(desc.keyArriveTangents, desc.keyArriveTangentsBlock1, index, key.arriveTangent);
        Set_Component(desc.keyLeaveTangents, desc.keyLeaveTangentsBlock1, index, key.leaveTangent);
        Set_Component(desc.keyModes, desc.keyModesBlock1, index, To_RuntimeCurveMode(key.interpolationMode));
    }

    void Fill_CoreColorRgbCurveComponent(
        EffectMaterialCoreColorRgbModulationCurveDesc& desc,
        uint32 index,
        const Vector3CurveKeyData& key)
    {
        Set_Component(desc.keyTimes, desc.keyTimesBlock1, index, clamp(key.time, 0.f, 1.f));
        Set_Component(desc.keyValuesR, desc.keyValuesRBlock1, index, key.value.x);
        Set_Component(desc.keyValuesG, desc.keyValuesGBlock1, index, key.value.y);
        Set_Component(desc.keyValuesB, desc.keyValuesBBlock1, index, key.value.z);
        Set_Component(desc.keyArriveTangentsR, desc.keyArriveTangentsRBlock1, index, key.arriveTangent.x);
        Set_Component(desc.keyArriveTangentsG, desc.keyArriveTangentsGBlock1, index, key.arriveTangent.y);
        Set_Component(desc.keyArriveTangentsB, desc.keyArriveTangentsBBlock1, index, key.arriveTangent.z);
        Set_Component(desc.keyLeaveTangentsR, desc.keyLeaveTangentsRBlock1, index, key.leaveTangent.x);
        Set_Component(desc.keyLeaveTangentsG, desc.keyLeaveTangentsGBlock1, index, key.leaveTangent.y);
        Set_Component(desc.keyLeaveTangentsB, desc.keyLeaveTangentsBBlock1, index, key.leaveTangent.z);
        Set_Component(desc.keyModes, desc.keyModesBlock1, index, To_RuntimeCurveMode(key.interpolationMode));
    }

    void Fill_Vec2CurveComponent(
        EffectMaterialVec2ModulationCurveDesc& desc,
        uint32 index,
        const Vector2CurveKeyData& key)
    {
        Set_Component(desc.keyTimes, desc.keyTimesBlock1, index, clamp(key.time, 0.f, 1.f));
        Set_Component(desc.keyValuesX, desc.keyValuesXBlock1, index, key.value.x);
        Set_Component(desc.keyValuesY, desc.keyValuesYBlock1, index, key.value.y);
        Set_Component(desc.keyArriveTangentsX, desc.keyArriveTangentsXBlock1, index, key.arriveTangent.x);
        Set_Component(desc.keyArriveTangentsY, desc.keyArriveTangentsYBlock1, index, key.arriveTangent.y);
        Set_Component(desc.keyLeaveTangentsX, desc.keyLeaveTangentsXBlock1, index, key.leaveTangent.x);
        Set_Component(desc.keyLeaveTangentsY, desc.keyLeaveTangentsYBlock1, index, key.leaveTangent.y);
        Set_Component(desc.keyModes, desc.keyModesBlock1, index, To_RuntimeCurveMode(key.interpolationMode));
    }

    EffectMaterialScalarModulationCurveDesc Build_CurveDesc(const FloatDistributionData& distribution)
    {
        constexpr uint32 kMaxCurveKeys{ ::Engine::kEffectDistributionCurveMaxKeys };
        EffectMaterialScalarModulationCurveDesc desc{};

        if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&distribution.payload);
            distribution.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
        {
            const uint32 keyCount = min(kMaxCurveKeys, static_cast<uint32>(curve->keys.size()));
            desc.enabled = true;
            desc.keyCount = keyCount;
            desc.keyTimes = Vec4{};
            desc.keyTimesBlock1 = Vec4{};
            desc.keyValues = Vec4{};
            desc.keyValuesBlock1 = Vec4{};
            desc.keyArriveTangents = Vec4{};
            desc.keyArriveTangentsBlock1 = Vec4{};
            desc.keyLeaveTangents = Vec4{};
            desc.keyLeaveTangentsBlock1 = Vec4{};
            desc.keyModes = Vec4{};
            desc.keyModesBlock1 = Vec4{};

            for (uint32 index = 0; index < keyCount; ++index)
                Fill_CurveComponent(desc, index, curve->keys[index]);

            return desc;
        }

        float value = 1.f;
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&distribution.payload))
            value = constant->value;
        else if (const auto* uniform = get_if<UniformFloatDistributionData>(&distribution.payload))
            value = (uniform->minValue + uniform->maxValue) * 0.5f;

        desc.enabled = true;
        desc.keyCount = 2;
        desc.keyValues = Vec4{ value, value, 0.f, 0.f };
        return desc;
    }

    EffectMaterialCoreColorRgbModulationCurveDesc Build_CoreColorRgbCurveDesc(const Vector3DistributionData& distribution)
    {
        constexpr uint32 kMaxCurveKeys{ ::Engine::kEffectDistributionCurveMaxKeys };
        EffectMaterialCoreColorRgbModulationCurveDesc desc{};

        if (const auto* curve = get_if<ConstantCurveVector3DistributionData>(&distribution.payload);
            distribution.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
        {
            const uint32 keyCount = min(kMaxCurveKeys, static_cast<uint32>(curve->keys.size()));
            desc.enabled = true;
            desc.keyCount = keyCount;
            desc.keyTimes = Vec4{};
            desc.keyTimesBlock1 = Vec4{};
            desc.keyValuesR = Vec4{};
            desc.keyValuesRBlock1 = Vec4{};
            desc.keyValuesG = Vec4{};
            desc.keyValuesGBlock1 = Vec4{};
            desc.keyValuesB = Vec4{};
            desc.keyValuesBBlock1 = Vec4{};
            desc.keyArriveTangentsR = Vec4{};
            desc.keyArriveTangentsRBlock1 = Vec4{};
            desc.keyArriveTangentsG = Vec4{};
            desc.keyArriveTangentsGBlock1 = Vec4{};
            desc.keyArriveTangentsB = Vec4{};
            desc.keyArriveTangentsBBlock1 = Vec4{};
            desc.keyLeaveTangentsR = Vec4{};
            desc.keyLeaveTangentsRBlock1 = Vec4{};
            desc.keyLeaveTangentsG = Vec4{};
            desc.keyLeaveTangentsGBlock1 = Vec4{};
            desc.keyLeaveTangentsB = Vec4{};
            desc.keyLeaveTangentsBBlock1 = Vec4{};
            desc.keyModes = Vec4{};
            desc.keyModesBlock1 = Vec4{};

            for (uint32 index = 0; index < keyCount; ++index)
                Fill_CoreColorRgbCurveComponent(desc, index, curve->keys[index]);

            return desc;
        }

        Vec3 value{ 1.f, 0.85f, 0.45f };
        if (const auto* constant = get_if<ConstantVector3DistributionData>(&distribution.payload))
            value = constant->value;
        else if (const auto* uniform = get_if<UniformVector3DistributionData>(&distribution.payload))
        {
            desc.enabled = true;
            desc.uniformEnabled = true;
            desc.uniformMin = uniform->minValue;
            desc.uniformMax = uniform->maxValue;
            desc.uniformSeed = Build_RandomSeedDesc(uniform->randomSeed);
            return desc;
        }

        desc.enabled = true;
        desc.keyCount = 2;
        desc.keyValuesR = Vec4{ value.x, value.x, 0.f, 0.f };
        desc.keyValuesG = Vec4{ value.y, value.y, 0.f, 0.f };
        desc.keyValuesB = Vec4{ value.z, value.z, 0.f, 0.f };
        return desc;
    }

    EffectMaterialVec2ModulationCurveDesc Build_Vec2CurveDesc(const Vector2DistributionData& distribution)
    {
        constexpr uint32 kMaxCurveKeys{ ::Engine::kEffectDistributionCurveMaxKeys };
        EffectMaterialVec2ModulationCurveDesc desc{};

        if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&distribution.payload);
            distribution.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
        {
            const uint32 keyCount = min(kMaxCurveKeys, static_cast<uint32>(curve->keys.size()));
            desc.enabled = true;
            desc.keyCount = keyCount;
            desc.keyTimes = Vec4{};
            desc.keyTimesBlock1 = Vec4{};
            desc.keyValuesX = Vec4{};
            desc.keyValuesXBlock1 = Vec4{};
            desc.keyValuesY = Vec4{};
            desc.keyValuesYBlock1 = Vec4{};
            desc.keyArriveTangentsX = Vec4{};
            desc.keyArriveTangentsXBlock1 = Vec4{};
            desc.keyArriveTangentsY = Vec4{};
            desc.keyArriveTangentsYBlock1 = Vec4{};
            desc.keyLeaveTangentsX = Vec4{};
            desc.keyLeaveTangentsXBlock1 = Vec4{};
            desc.keyLeaveTangentsY = Vec4{};
            desc.keyLeaveTangentsYBlock1 = Vec4{};
            desc.keyModes = Vec4{};
            desc.keyModesBlock1 = Vec4{};

            for (uint32 index = 0; index < keyCount; ++index)
                Fill_Vec2CurveComponent(desc, index, curve->keys[index]);

            return desc;
        }

        Vec2 value{ 0.f, 0.f };
        if (const auto* constant = get_if<ConstantVector2DistributionData>(&distribution.payload))
            value = constant->value;
        else if (const auto* uniform = get_if<UniformVector2DistributionData>(&distribution.payload))
            value = (uniform->minValue + uniform->maxValue) * 0.5f;

        desc.enabled = true;
        desc.keyCount = 2;
        desc.keyValuesX = Vec4{ value.x, value.x, 0.f, 0.f };
        desc.keyValuesY = Vec4{ value.y, value.y, 0.f, 0.f };
        return desc;
    }
}

const RequiredModuleData* Find_RequiredModuleData(const AuthoringEmitter& emitter)
{
    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type != AuthoringModuleType::Required)
            continue;

        return get_if<RequiredModuleData>(&module.data);
    }

    return nullptr;
}

const MaterialScalarModulationModuleData* Find_MaterialScalarModulationModuleData(const AuthoringEmitter& emitter)
{
    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type != AuthoringModuleType::MaterialScalarModulation || !module.enabled)
            continue;

        return get_if<MaterialScalarModulationModuleData>(&module.data);
    }

    return nullptr;
}

EffectMaterialScalarModulationRuntimeDesc Build_MaterialScalarModulationRuntimeDesc(const MaterialScalarModulationModuleData& data)
{
    EffectMaterialScalarModulationRuntimeDesc desc{};
    const uint32 count = min(
        EffectMaterialScalarModulationRuntimeDesc::kMaxModulators,
        static_cast<uint32>(data.modulators.size())
    );
    desc.count = count;

    for (uint32 index = 0; index < count; ++index)
    {
        const MaterialScalarModulatorData& source = data.modulators[index];
        EffectMaterialScalarModulatorRuntimeDesc& target = desc.modulators[index];
        target.enabled = source.enabled;
        target.targetField = To_RuntimeTarget(source.targetField);
        target.operation = EffectMaterialScalarModulationOperation::Multiply;
        target.timeSource = To_RuntimeTimeSource(source.timeSource);
        if (Is_ScrollSpeedScaleTarget(source.targetField))
            target.timeSource = EffectMaterialScalarModulationTimeSource::EmitterTime;
        target.curve = Build_CurveDesc(source.distribution);
    }

    return desc;
}

EffectMaterialCoreColorRgbModulationRuntimeDesc Build_CoreColorRgbModulationRuntimeDesc(const MaterialScalarModulationModuleData& data)
{
    EffectMaterialCoreColorRgbModulationRuntimeDesc desc{};
    const uint32 count = min(
        EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators,
        static_cast<uint32>(data.coreColorRgbModulators.size())
    );
    desc.count = count;

    for (uint32 index = 0; index < count; ++index)
    {
        const MaterialCoreColorRgbModulatorData& source = data.coreColorRgbModulators[index];
        EffectMaterialCoreColorRgbModulatorRuntimeDesc& target = desc.modulators[index];
        target.enabled = source.enabled;
        target.timeSource = To_RuntimeTimeSource(source.timeSource);
        target.curve = Build_CoreColorRgbCurveDesc(source.distribution);
    }

    return desc;
}

EffectMaterialVec2ModulationRuntimeDesc Build_MaterialVec2ModulationRuntimeDesc(const MaterialScalarModulationModuleData& data)
{
    EffectMaterialVec2ModulationRuntimeDesc desc{};
    const uint32 count = min(
        EffectMaterialVec2ModulationRuntimeDesc::kMaxModulators,
        static_cast<uint32>(data.vec2Modulators.size())
    );
    desc.count = count;

    for (uint32 index = 0; index < count; ++index)
    {
        const MaterialVec2ModulatorData& source = data.vec2Modulators[index];
        EffectMaterialVec2ModulatorRuntimeDesc& target = desc.modulators[index];
        target.enabled = source.enabled;
        target.targetField = To_RuntimeTarget(source.targetField);
        target.operation = EffectMaterialVec2ModulationOperation::Add;
        target.timeSource = To_RuntimeTimeSource(source.timeSource);
        target.curve = Build_Vec2CurveDesc(source.distribution);
    }

    return desc;
}

float Compute_MaterialScalarPreviewPhase(float elapsedTime, float duration)
{
    const float safeDuration = max(duration, 0.0001f);
    const float normalizedTime = max(0.f, elapsedTime) / safeDuration;
    return normalizedTime - std::floor(normalizedTime);
}

MaterialParameterModulationPreviewResult Evaluate_MaterialParameterModulationPreview(
    const EffectMaterialInstanceData& baseMaterial,
    const EffectMaterialCoreColorRgbModulationRuntimeDesc& coreColorRgbModulation,
    const EffectMaterialVec2ModulationRuntimeDesc& vec2Modulation,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float previewPhase,
    uint32 effectPlaybackSeed,
    bool evaluateParticleLife)
{
    MaterialScalarModulationPreviewResult result{};
    result.material = baseMaterial;

    const uint32 coreColorRgbCount = min(EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators, coreColorRgbModulation.count);
    for (uint32 index = 0; index < coreColorRgbCount; ++index)
    {
        const EffectMaterialCoreColorRgbModulatorRuntimeDesc& modulator = coreColorRgbModulation.modulators[index];
        if (!modulator.enabled)
            continue;

        if (modulator.timeSource == EffectMaterialScalarModulationTimeSource::ParticleLife)
        {
            result.skippedParticleLife = true;
            continue;
        }

        const Vec3 fallbackValue{
            result.material.coreEmissive.coreColor.x,
            result.material.coreEmissive.coreColor.y,
            result.material.coreEmissive.coreColor.z
        };
        const Vec3 coreColorRgb = Client::Evaluate_CoreColorRgbModulationCurve(modulator.curve, previewPhase, fallbackValue, effectPlaybackSeed);
        result.material.coreEmissive.coreColor.x = coreColorRgb.x;
        result.material.coreEmissive.coreColor.y = coreColorRgb.y;
        result.material.coreEmissive.coreColor.z = coreColorRgb.z;
        result.applied = true;
    }

    const uint32 vec2Count = min(EffectMaterialVec2ModulationRuntimeDesc::kMaxModulators, vec2Modulation.count);
    for (uint32 index = 0; index < vec2Count; ++index)
    {
        const EffectMaterialVec2ModulatorRuntimeDesc& modulator = vec2Modulation.modulators[index];
        if (!modulator.enabled)
            continue;

        if (modulator.timeSource == EffectMaterialScalarModulationTimeSource::ParticleLife)
        {
            result.skippedParticleLife = true;
            continue;
        }

        const Vec2 value = Client::Evaluate_MaterialVec2ModulationCurve(modulator.curve, previewPhase, Vec2{ 0.f, 0.f });
        Client::Apply_MaterialVec2ModulationAdd(result.material, modulator.targetField, value);
        result.applied = true;
    }

    const uint32 count = min(EffectMaterialScalarModulationRuntimeDesc::kMaxModulators, modulation.count);
    for (uint32 index = 0; index < count; ++index)
    {
        const EffectMaterialScalarModulatorRuntimeDesc& modulator = modulation.modulators[index];
        if (!modulator.enabled)
            continue;

        if (modulator.timeSource == EffectMaterialScalarModulationTimeSource::ParticleLife && !evaluateParticleLife)
        {
            result.skippedParticleLife = true;
            continue;
        }

        const float multiplier = Client::Evaluate_MaterialScalarModulationCurve(modulator.curve, previewPhase, 1.f);
        Client::Apply_MaterialScalarModulationMultiplier(result.material, modulator.targetField, multiplier);
        result.applied = true;
    }

    return result;
}

MaterialScalarModulationPreviewResult Evaluate_MaterialScalarModulationPreview(
    const EffectMaterialInstanceData& baseMaterial,
    const EffectMaterialCoreColorRgbModulationRuntimeDesc& coreColorRgbModulation,
    const EffectMaterialVec2ModulationRuntimeDesc& vec2Modulation,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float previewPhase,
    uint32 effectPlaybackSeed,
    bool evaluateParticleLife)
{
    return Evaluate_MaterialParameterModulationPreview(
        baseMaterial,
        coreColorRgbModulation,
        vec2Modulation,
        modulation,
        previewPhase,
        effectPlaybackSeed,
        evaluateParticleLife
    );
}

NS_END
