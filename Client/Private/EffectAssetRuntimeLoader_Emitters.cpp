#include "pch.h"
#include "EffectAssetRuntimeLoader_Support.h"

NS_BEGIN(Client)

namespace EffectAssetRuntimeLoad::Emitters
{
    using namespace Assets;
    using namespace Curves;
    using namespace Json;
    using namespace Material;

    constexpr uint32 kRibbonAutoHistoryMaxCount{ 4096 };
    constexpr float kRibbonAutoHistoryFrameStep{ 0.016f };
    constexpr uint32 kRibbonAutoHistoryMargin{ 8 };

    EffectEmitterHistoryBudgetRuntimeDesc Read_HistoryBudgetUsage(const json& emitter)
    {
        EffectEmitterHistoryBudgetRuntimeDesc usage{};
        const auto budgetIter = emitter.find("historyBudget");
        if (budgetIter == emitter.end() || !budgetIter->is_object())
            return usage;

        const json& budget = *budgetIter;
        Read_Enum(budget, "priority", usage.priority);
        Read_Enum(budget, "densityBias", usage.densityBias);
        usage.lengthScale = max(0.f, Read_Float(budget, "lengthScale", usage.lengthScale));
        return usage;
    }

    const json* Find_ModuleData(const json& emitter, const string& moduleType)
    {
        const auto modulesIter = emitter.find("modules");
        if (modulesIter == emitter.end() || !modulesIter->is_array())
            return nullptr;

        for (const json& module : *modulesIter)
        {
            if (!module.is_object())
                continue;

            if (!Read_Bool(module, "enabled", true))
                continue;

            if (Read_String(module, "type") != moduleType)
                continue;

            const auto dataIter = module.find("data");
            if (dataIter == module.end() || !dataIter->is_object())
                return nullptr;

            return &*dataIter;
        }

        return nullptr;
    }

    constexpr uint32 kVelocityOverLifeApplyChannelInitialVelocityMask{ 1u << 0 };
    constexpr uint32 kVelocityOverLifeApplyChannelInitialRadialVelocityMask{ 1u << 1 };
    constexpr uint32 kVelocityOverLifeApplyChannelVelocityConeMask{ 1u << 2 };
    constexpr uint32 kVelocityOverLifeApplyChannelSourceMotionVelocityMask{ 1u << 3 };
    constexpr uint32 kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask{ 1u << 4 };
    constexpr uint32 kVelocityOverLifeApplyChannelAllMask{
        kVelocityOverLifeApplyChannelInitialVelocityMask |
        kVelocityOverLifeApplyChannelInitialRadialVelocityMask |
        kVelocityOverLifeApplyChannelVelocityConeMask |
        kVelocityOverLifeApplyChannelSourceMotionVelocityMask |
        kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask
    };
    constexpr uint32 kVelocityOverLifeApplyChannelDefaultMask{
        kVelocityOverLifeApplyChannelAllMask & ~kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask
    };

    uint32 To_VelocityOverLifeApplyChannelMask(PointParticleVelocityScaleChannel channel)
    {
        switch (channel)
        {
        case PointParticleVelocityScaleChannel::InitialVelocity:
            return kVelocityOverLifeApplyChannelInitialVelocityMask;
        case PointParticleVelocityScaleChannel::InitialRadialVelocity:
            return kVelocityOverLifeApplyChannelInitialRadialVelocityMask;
        case PointParticleVelocityScaleChannel::VelocityCone:
            return kVelocityOverLifeApplyChannelVelocityConeMask;
        case PointParticleVelocityScaleChannel::SourceMotionVelocity:
            return kVelocityOverLifeApplyChannelSourceMotionVelocityMask;
        case PointParticleVelocityScaleChannel::AccelerationIntegratedVelocity:
            return kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask;
        default:
            return 0u;
        }
    }

    uint32 Read_VelocityOverLifeApplyChannelMask(const json& velocityOverLife)
    {
        const auto iter = velocityOverLife.find("applyChannels");
        if (iter == velocityOverLife.end() || !iter->is_array())
            return kVelocityOverLifeApplyChannelDefaultMask;

        uint32 mask = 0u;
        for (const json& channelNode : *iter)
        {
            if (!channelNode.is_string())
                continue;

            const optional<PointParticleVelocityScaleChannel> parsed =
                magic_enum::enum_cast<PointParticleVelocityScaleChannel>(channelNode.get<string>());
            if (parsed.has_value())
                mask |= To_VelocityOverLifeApplyChannelMask(parsed.value());
        }

        return mask != 0u ? mask : kVelocityOverLifeApplyChannelDefaultMask;
    }

    PointParticleFloatCurveRuntimeDesc& Resolve_VelocityScaleChannelPayload(
        PointParticleMotionDesc& desc,
        PointParticleVelocityScaleChannel channel)
    {
        switch (channel)
        {
        case PointParticleVelocityScaleChannel::InitialVelocity:
            return desc.initialVelocityScaleByLife;
        case PointParticleVelocityScaleChannel::InitialRadialVelocity:
            return desc.initialRadialVelocityScaleByLife;
        case PointParticleVelocityScaleChannel::VelocityCone:
            return desc.velocityConeScaleByLife;
        case PointParticleVelocityScaleChannel::SourceMotionVelocity:
            return desc.sourceMotionVelocityScaleByLife;
        case PointParticleVelocityScaleChannel::AccelerationIntegratedVelocity:
        default:
            return desc.accelerationIntegratedVelocityScaleByLife;
        }
    }

    void Apply_VelocityOverLifeOwnership(PointParticleMotionDesc& desc, const json& emitter)
    {
        const auto modulesIter = emitter.find("modules");
        if (modulesIter == emitter.end() || !modulesIter->is_array())
            return;

        const PointParticleVelocityScaleChannel channels[] = {
            PointParticleVelocityScaleChannel::InitialVelocity,
            PointParticleVelocityScaleChannel::InitialRadialVelocity,
            PointParticleVelocityScaleChannel::VelocityCone,
            PointParticleVelocityScaleChannel::SourceMotionVelocity,
            PointParticleVelocityScaleChannel::AccelerationIntegratedVelocity,
        };
        const uint32 channelMasks[] = {
            kVelocityOverLifeApplyChannelInitialVelocityMask,
            kVelocityOverLifeApplyChannelInitialRadialVelocityMask,
            kVelocityOverLifeApplyChannelVelocityConeMask,
            kVelocityOverLifeApplyChannelSourceMotionVelocityMask,
            kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask,
        };
        const json* owners[5]{};

        for (const json& module : *modulesIter)
        {
            if (!module.is_object() || !Read_Bool(module, "enabled", true))
                continue;
            if (Read_String(module, "type") != "VelocityOverLife")
                continue;

            const auto dataIter = module.find("data");
            if (dataIter == module.end() || !dataIter->is_object())
                continue;

            const json& velocityOverLife = *dataIter;
            const uint32 mask = Read_VelocityOverLifeApplyChannelMask(velocityOverLife);
            for (uint32 index = 0u; index < 5u; ++index)
            {
                if ((mask & channelMasks[index]) != 0u && owners[index] == nullptr)
                    owners[index] = &velocityOverLife;
            }
        }

        const json* sharedOwner = owners[0];
        bool hasAnyOwner = sharedOwner != nullptr;
        bool hasSingleOwnerForAllChannels = sharedOwner != nullptr;
        for (uint32 index = 0u; index < 5u; ++index)
        {
            if (owners[index] != nullptr)
            {
                hasAnyOwner = true;
                Fill_VelocityScaleByLifeChannelPayload(
                    Resolve_VelocityScaleChannelPayload(desc, channels[index]),
                    *owners[index]
                );
            }

            if (owners[index] != sharedOwner)
                hasSingleOwnerForAllChannels = false;
        }

        if (hasAnyOwner)
            desc.enabled = true;
        if (hasSingleOwnerForAllChannels)
            Fill_VelocityScaleByLifePayload(desc, *sharedOwner);
    }

    void Fill_SourceMotionVelocityPayload(PointParticleMotionDesc& desc, const json& sourceMotionVelocity)
    {
        desc.enabled = true;
        desc.sourceMotionVelocityEnabled = true;
        Read_Enum(sourceMotionVelocity, "directionMode", desc.sourceMotionVelocityDirectionMode);
        desc.sourceMotionVelocitySourceSpeedScale =
            max(0.f, Read_Float(sourceMotionVelocity, "sourceSpeedScale", desc.sourceMotionVelocitySourceSpeedScale));
        desc.sourceMotionVelocitySpreadAngleDegrees =
            clamp(Read_Float(sourceMotionVelocity, "spreadAngleDegrees", desc.sourceMotionVelocitySpreadAngleDegrees), 0.f, 180.f);

        if (const auto speedIter = sourceMotionVelocity.find("speed"); speedIter != sourceMotionVelocity.end())
        {
            desc.sourceMotionVelocitySpeed = Vec2{
                Evaluate_FloatDistributionMin(*speedIter, desc.sourceMotionVelocitySpeed.x),
                Evaluate_FloatDistributionMax(*speedIter, desc.sourceMotionVelocitySpeed.y)
            };
            desc.sourceMotionVelocitySeed = Read_DistributionRandomSeedDesc(*speedIter, &sourceMotionVelocity);
        }
    }

    Vec2 Read_Normalized01Range(const json& distribution, Vec2 fallbackValue)
    {
        Vec2 range{
            Evaluate_FloatDistributionMin(distribution, fallbackValue.x),
            Evaluate_FloatDistributionMax(distribution, fallbackValue.y)
        };
        range.x = clamp(range.x, 0.f, 1.f);
        range.y = clamp(range.y, 0.f, 1.f);
        if (range.x > range.y)
            swap(range.x, range.y);
        return range;
    }

    void Fill_MeshDirectionAlignPayload(MeshDirectionAlignRuntimeDesc& desc, const json& data)
    {
        desc.enabled = true;
        Read_Enum(data, "targetMode", desc.targetMode);
        Read_Enum(data, "space", desc.space);
        Read_Enum(data, "meshForwardAxis", desc.meshForwardAxis);
        Read_Enum(data, "meshUpAxis", desc.meshUpAxis);
        Read_Enum(data, "blendMode", desc.blendMode);

        if (const auto targetIter = data.find("target"); targetIter != data.end())
            Read_Vec3(*targetIter, desc.target);
        if (const auto progressIter = data.find("alignmentProgress"); progressIter != data.end())
            Fill_FloatCurvePayload(desc.alignmentProgress, *progressIter, 0.f);
        if (const auto delayIter = data.find("randomDelay"); delayIter != data.end())
        {
            desc.randomDelay = Read_Normalized01Range(*delayIter, desc.randomDelay);
            desc.randomDelaySeed = Read_DistributionRandomSeedDesc(*delayIter, &data);
        }
        if (const auto weightScaleIter = data.find("randomWeightScale"); weightScaleIter != data.end())
        {
            desc.randomWeightScale = Read_Normalized01Range(*weightScaleIter, desc.randomWeightScale);
            desc.randomWeightScaleSeed = Read_DistributionRandomSeedDesc(*weightScaleIter, &data);
        }
    }

    string Resolve_RendererType(const json& emitter)
    {
        string rendererType = Read_String(emitter, "rendererType");
        if (!rendererType.empty())
            return rendererType;

        if (const json* required = Find_ModuleData(emitter, "Required"))
        {
            rendererType = Read_String(*required, "rendererType");
            if (!rendererType.empty())
                return rendererType;
        }

        const auto typeDataIter = emitter.find("typeData");
        if (typeDataIter != emitter.end() && typeDataIter->is_object())
            return Read_String(*typeDataIter, "kind");

        return {};
    }

    EffectSourceHistorySpriteTrailArrivalMode Resolve_SourceHistorySpriteTrailArrivalMode(const json& data)
    {
        const string arrivalMode = Read_String(data, "arrivalMode");
        if (arrivalMode == "ClampAtEnd")
            return EffectSourceHistorySpriteTrailArrivalMode::ClampAtEnd;
        if (arrivalMode == "KillOnArrive")
            return EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive;

        const bool killOnArrive = Read_Bool(data, "killOnArrive", true);
        const bool clampAtEnd = Read_Bool(data, "clampAtEnd", true);
        if (!killOnArrive && clampAtEnd)
            return EffectSourceHistorySpriteTrailArrivalMode::ClampAtEnd;

        return EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive;
    }

    uint32 Resolve_RibbonAutoHistoryCount(
        uint32 authoredCount,
        float sampleLifetime,
        float sampleSpacing,
        float maxLength)
    {
        const uint32 authored = max(2u, authoredCount);
        const uint32 timeBased =
            static_cast<uint32>(ceilf(max(0.0001f, sampleLifetime) / kRibbonAutoHistoryFrameStep)) + kRibbonAutoHistoryMargin;
        uint32 required = max(authored, timeBased);

        if (maxLength > 0.f)
        {
            const uint32 distanceBased =
                static_cast<uint32>(ceilf(maxLength / max(0.001f, sampleSpacing))) + kRibbonAutoHistoryMargin;
            required = max(required, distanceBased);
        }

        return clamp(required, 2u, kRibbonAutoHistoryMaxCount);
    }

    EffectRibbonSpreadBasis Resolve_RibbonSpreadBasis(const string& token)
    {
        if (token == "ViewUp")
            return EffectRibbonSpreadBasis::ViewUp;
        if (token == "WorldUp")
            return EffectRibbonSpreadBasis::WorldUp;
        if (token == "SourceUp")
            return EffectRibbonSpreadBasis::SourceUp;
        if (token == "SourceRight")
            return EffectRibbonSpreadBasis::SourceRight;

        return EffectRibbonSpreadBasis::CameraFacing;
    }

    uint32 Resolve_SourceHistoryCurveSubdivisionPreset(float sampleSpacing)
    {
        const float smoothDelta = sampleSpacing - 0.04f;
        if (smoothDelta >= -0.0001f && smoothDelta <= 0.0001f)
            return 8u;
        const float highQualityDelta = sampleSpacing - 0.02f;
        if (highQualityDelta >= -0.0001f && highQualityDelta <= 0.0001f)
            return 12u;
        return 4u;
    }

    void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeSpriteEmitterDesc& desc)
    {
        const auto materialIter = data.find("material");
        if (materialIter != data.end() && materialIter->is_object())
            Apply_MaterialPayload(*materialIter, desc.required.material);

        Read_Enum(data, "screenAlignment", desc.required.spriteRender.screenAlignment);
        Read_Enum(data, "directionalAlignmentMode", desc.required.spriteRender.directionalAlignmentMode);
        Read_Enum(data, "spriteTextureAxis", desc.required.spriteRender.spriteTextureAxis);
        desc.required.spriteRender.spriteRollOffsetDegrees =
            Read_Float(data, "spriteRollOffsetDegrees", desc.required.spriteRender.spriteRollOffsetDegrees);
        Apply_SortPayload(data, desc.required.sort);
        desc.required.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.required.playback.duration));
        desc.required.playback.loopCount = Read_UInt(data, "loopCount", desc.required.playback.loopCount);
        desc.required.playback.delay = max(0.f, Read_Float(data, "delay", desc.required.playback.delay));
        desc.required.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.required.playback.delayFirstLoopOnly);
        desc.required.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.required.playback.killOnDeactivate);
        desc.required.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.required.playback.killOnCompleted);
        desc.required.drawLimit.useMaxDrawCount = Read_Bool(data, "useMaxDrawCount", desc.required.drawLimit.useMaxDrawCount);
        desc.required.drawLimit.maxDrawCount = max(1u, Read_UInt(data, "maxDrawCount", desc.required.drawLimit.maxDrawCount));

        const auto originIter = data.find("emitterOrigin");
        if (originIter != data.end())
            Read_Vec3(*originIter, definition.localPosition);

        const auto rotationIter = data.find("emitterRotationDegrees");
        if (rotationIter != data.end())
            Read_Vec3(*rotationIter, definition.localRotationDegrees);

        definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
    }

    void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeTrailEmitterDesc& desc)
    {
        const auto materialIter = data.find("material");
        if (materialIter != data.end() && materialIter->is_object())
            Apply_MaterialPayload(*materialIter, desc.material);
        Engine::EffectRuntime::Apply_TrailMaterialRuntimePolicy(desc.material);

        desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
        desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
        desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
        desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
        desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
        desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);

        const auto originIter = data.find("emitterOrigin");
        if (originIter != data.end())
            Read_Vec3(*originIter, definition.localPosition);

        const auto rotationIter = data.find("emitterRotationDegrees");
        if (rotationIter != data.end())
            Read_Vec3(*rotationIter, definition.localRotationDegrees);

        definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
    }

    void Apply_RequiredModule(
        const json& data,
        EffectEmitterDefinition& definition,
        ComputeRibbonEmitterDesc& desc)
    {
        const auto materialIter = data.find("material");
        if (materialIter != data.end() && materialIter->is_object())
            Apply_MaterialPayload(*materialIter, desc.material);

        Apply_SortPayload(data, desc.sort);
        desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
        desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
        desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
        desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
        desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
        desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);

        const auto originIter = data.find("emitterOrigin");
        if (originIter != data.end())
            Read_Vec3(*originIter, definition.localPosition);

        const auto rotationIter = data.find("emitterRotationDegrees");
        if (rotationIter != data.end())
            Read_Vec3(*rotationIter, definition.localRotationDegrees);

        definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
    }

    void Apply_RequiredModule(
        const json& data,
        EffectEmitterDefinition& definition,
        ComputeSourceHistorySpriteTrailEmitterDesc& desc)
    {
        const auto materialIter = data.find("material");
        if (materialIter != data.end() && materialIter->is_object())
            Apply_MaterialPayload(*materialIter, desc.material);

        Read_Enum(data, "screenAlignment", desc.spriteRender.screenAlignment);
        Read_Enum(data, "directionalAlignmentMode", desc.spriteRender.directionalAlignmentMode);
        Read_Enum(data, "spriteTextureAxis", desc.spriteRender.spriteTextureAxis);
        desc.spriteRender.spriteRollOffsetDegrees =
            Read_Float(data, "spriteRollOffsetDegrees", desc.spriteRender.spriteRollOffsetDegrees);
        Apply_SortPayload(data, desc.sort);
        desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
        desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
        desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
        desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
        desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
        desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);
        desc.drawLimit.useMaxDrawCount = Read_Bool(data, "useMaxDrawCount", desc.drawLimit.useMaxDrawCount);
        desc.drawLimit.maxDrawCount = max(1u, Read_UInt(data, "maxDrawCount", desc.drawLimit.maxDrawCount));

        const auto originIter = data.find("emitterOrigin");
        if (originIter != data.end())
            Read_Vec3(*originIter, definition.localPosition);

        const auto rotationIter = data.find("emitterRotationDegrees");
        if (rotationIter != data.end())
            Read_Vec3(*rotationIter, definition.localRotationDegrees);

        definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
    }

    void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeBeamEmitterDesc& desc)
    {
        const auto materialIter = data.find("material");
        if (materialIter != data.end() && materialIter->is_object())
            Apply_MaterialPayload(*materialIter, desc.material);

        Apply_SortPayload(data, desc.sort);
        desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
        desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
        desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
        desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
        desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
        desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);

        if (const auto originIter = data.find("emitterOrigin"); originIter != data.end())
            Read_Vec3(*originIter, definition.localPosition);
        if (const auto rotationIter = data.find("emitterRotationDegrees"); rotationIter != data.end())
            Read_Vec3(*rotationIter, definition.localRotationDegrees);
        definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
    }

    void Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, MeshEmitterDesc& desc)
    {
        const auto materialIter = data.find("material");
        if (materialIter != data.end() && materialIter->is_object())
            Apply_MaterialPayload(*materialIter, desc.material);

        Read_Enum(data, "screenAlignment", desc.meshTransform.alignment);
        Apply_SortPayload(data, desc.sort);
        desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
        desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
        desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
        desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
        desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
        desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);
        desc.drawLimit.useMaxDrawCount = Read_Bool(data, "useMaxDrawCount", desc.drawLimit.useMaxDrawCount);
        desc.drawLimit.maxDrawCount = max(1u, Read_UInt(data, "maxDrawCount", desc.drawLimit.maxDrawCount));

        const auto originIter = data.find("emitterOrigin");
        if (originIter != data.end())
            Read_Vec3(*originIter, definition.localPosition);

        const auto rotationIter = data.find("emitterRotationDegrees");
        if (rotationIter != data.end())
            Read_Vec3(*rotationIter, definition.localRotationDegrees);

        definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
    }

    void Apply_SpawnModule(const json& data, ComputeSpriteEmitterDesc& desc)
    {
        desc.spawn.instanceCount = max(1u, Read_UInt(data, "maxParticleCount", desc.spawn.instanceCount));
        desc.spawn.particleSpawn.processSpawnRate = Read_Bool(data, "processSpawnRate", desc.spawn.particleSpawn.processSpawnRate);
        desc.spawn.particleSpawn.processBurstList = Read_Bool(data, "processBurstList", desc.spawn.particleSpawn.processBurstList);
        desc.spawn.particleSpawn.maxActiveCount = desc.spawn.instanceCount;

        const auto spawnRateIter = data.find("spawnRate");
        if (spawnRateIter != data.end())
        {
            desc.spawn.particleSpawn.spawnRateRange = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*spawnRateIter, desc.spawn.particleSpawn.spawnRate)),
                max(0.f, Evaluate_FloatDistributionMax(*spawnRateIter, desc.spawn.particleSpawn.spawnRate))
            };
            desc.spawn.particleSpawn.spawnRate = desc.spawn.particleSpawn.spawnRateRange.y;
            desc.spawn.particleSpawn.spawnRateSeed = Read_DistributionRandomSeedDesc(*spawnRateIter, &data);
            Fill_FloatCurvePayload(desc.spawn.particleSpawn.spawnRateCurve, *spawnRateIter, desc.spawn.particleSpawn.spawnRate);
        }

        const auto spawnRateScaleIter = data.find("spawnRateScale");
        if (spawnRateScaleIter != data.end())
        {
            desc.spawn.particleSpawn.spawnRateScaleRange = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale)),
                max(0.f, Evaluate_FloatDistributionMax(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale))
            };
            desc.spawn.particleSpawn.spawnRateScale = desc.spawn.particleSpawn.spawnRateScaleRange.y;
            desc.spawn.particleSpawn.spawnRateScaleSeed = Read_DistributionRandomSeedDesc(*spawnRateScaleIter, &data);
            Fill_FloatCurvePayload(desc.spawn.particleSpawn.spawnRateScaleCurve, *spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale);
        }

        const auto burstScaleIter = data.find("burstScale");
        if (burstScaleIter != data.end())
        {
            desc.spawn.particleSpawn.burstScaleRange = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*burstScaleIter, desc.spawn.particleSpawn.burstScale)),
                max(0.f, Evaluate_FloatDistributionMax(*burstScaleIter, desc.spawn.particleSpawn.burstScale))
            };
            desc.spawn.particleSpawn.burstScale = desc.spawn.particleSpawn.burstScaleRange.y;
            desc.spawn.particleSpawn.burstScaleSeed = Read_DistributionRandomSeedDesc(*burstScaleIter, &data);
            Fill_FloatCurvePayload(desc.spawn.particleSpawn.burstScaleCurve, *burstScaleIter, desc.spawn.particleSpawn.burstScale);
        }

        const auto burstListIter = data.find("burstList");
        if (burstListIter == data.end() || !burstListIter->is_array())
            return;

        desc.spawn.particleSpawn.bursts.clear();
        desc.spawn.particleSpawn.bursts.reserve(burstListIter->size());
        for (const json& burst : *burstListIter)
        {
            if (!burst.is_object())
                continue;

            desc.spawn.particleSpawn.bursts.push_back(
                PointParticleBurstDesc{
                    .time = max(0.f, Read_Float(burst, "time")),
                    .count = Read_UInt(burst, "count")
                }
            );
        }
    }

    void Apply_SpawnModule(const json& data, MeshEmitterDesc& desc)
    {
        desc.spawn.instanceCount = max(1u, Read_UInt(data, "maxParticleCount", desc.spawn.instanceCount));
        desc.spawn.particleSpawn.processSpawnRate = Read_Bool(data, "processSpawnRate", desc.spawn.particleSpawn.processSpawnRate);
        desc.spawn.particleSpawn.processBurstList = Read_Bool(data, "processBurstList", desc.spawn.particleSpawn.processBurstList);
        desc.spawn.particleSpawn.maxActiveCount = desc.spawn.instanceCount;

        const auto spawnRateIter = data.find("spawnRate");
        if (spawnRateIter != data.end())
        {
            desc.spawn.particleSpawn.spawnRateRange = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*spawnRateIter, desc.spawn.particleSpawn.spawnRate)),
                max(0.f, Evaluate_FloatDistributionMax(*spawnRateIter, desc.spawn.particleSpawn.spawnRate))
            };
            desc.spawn.particleSpawn.spawnRate = desc.spawn.particleSpawn.spawnRateRange.y;
            desc.spawn.particleSpawn.spawnRateSeed = Read_DistributionRandomSeedDesc(*spawnRateIter, &data);
            Fill_FloatCurvePayload(desc.spawn.particleSpawn.spawnRateCurve, *spawnRateIter, desc.spawn.particleSpawn.spawnRate);
        }

        const auto spawnRateScaleIter = data.find("spawnRateScale");
        if (spawnRateScaleIter != data.end())
        {
            desc.spawn.particleSpawn.spawnRateScaleRange = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale)),
                max(0.f, Evaluate_FloatDistributionMax(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale))
            };
            desc.spawn.particleSpawn.spawnRateScale = desc.spawn.particleSpawn.spawnRateScaleRange.y;
            desc.spawn.particleSpawn.spawnRateScaleSeed = Read_DistributionRandomSeedDesc(*spawnRateScaleIter, &data);
            Fill_FloatCurvePayload(desc.spawn.particleSpawn.spawnRateScaleCurve, *spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale);
        }

        const auto burstScaleIter = data.find("burstScale");
        if (burstScaleIter != data.end())
        {
            desc.spawn.particleSpawn.burstScaleRange = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*burstScaleIter, desc.spawn.particleSpawn.burstScale)),
                max(0.f, Evaluate_FloatDistributionMax(*burstScaleIter, desc.spawn.particleSpawn.burstScale))
            };
            desc.spawn.particleSpawn.burstScale = desc.spawn.particleSpawn.burstScaleRange.y;
            desc.spawn.particleSpawn.burstScaleSeed = Read_DistributionRandomSeedDesc(*burstScaleIter, &data);
            Fill_FloatCurvePayload(desc.spawn.particleSpawn.burstScaleCurve, *burstScaleIter, desc.spawn.particleSpawn.burstScale);
        }

        const auto burstListIter = data.find("burstList");
        if (burstListIter == data.end() || !burstListIter->is_array())
            return;

        desc.spawn.particleSpawn.bursts.clear();
        desc.spawn.particleSpawn.bursts.reserve(burstListIter->size());
        for (const json& burst : *burstListIter)
        {
            if (!burst.is_object())
                continue;

            desc.spawn.particleSpawn.bursts.push_back(
                PointParticleBurstDesc{
                    .time = max(0.f, Read_Float(burst, "time")),
                    .count = Read_UInt(burst, "count")
                }
            );
        }
    }

    bool Build_EmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
    {
        outDefinition = EffectEmitterDefinition{};
        outDefinition.id = Read_UInt(emitter, "id");
        outDefinition.name = Read_String(emitter, "name");
        outDefinition.kind = EffectEmitterKind::Sprite;
        outDefinition.enabled = true;
        Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);

        ComputeSpriteEmitterDesc desc{};
        desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
        desc.renderLayerOverride = outDefinition.renderLayerOverride;

        if (const json* required = Find_ModuleData(emitter, "Required"))
            Apply_RequiredModule(*required, outDefinition, desc);

        if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
        {
            Fill_MaterialScalarModulationPayload(desc.required.material.scalarModulation, *materialScalarModulation);
            Fill_CoreColorRgbModulationPayload(desc.required.material.coreColorRgbModulation, *materialScalarModulation);
            Fill_MaterialVec2ModulationPayload(desc.required.material.vec2Modulation, *materialScalarModulation);
        }

        if (const json* spawn = Find_ModuleData(emitter, "Spawn"))
            Apply_SpawnModule(*spawn, desc);

        if (const json* initialSize = Find_ModuleData(emitter, "InitialSize"))
        {
            desc.initialSize.enabled = true;
            const auto sizeIter = initialSize->find("size");
            if (sizeIter != initialSize->end())
            {
                const Vec2 minSize = Evaluate_Vector2DistributionMin(*sizeIter, desc.initialSize.sizeMin);
                const Vec2 maxSize = Evaluate_Vector2DistributionMax(*sizeIter, desc.initialSize.sizeMax);
                desc.initialSize.sizeMin = Vec2{ min(minSize.x, maxSize.x), min(minSize.y, maxSize.y) };
                desc.initialSize.sizeMax = Vec2{ max(minSize.x, maxSize.x), max(minSize.y, maxSize.y) };
                desc.initialSize.sizeSeed = Read_DistributionRandomSeedDesc(*sizeIter, initialSize);
            }
        }

        if (const json* initialLocation = Find_ModuleData(emitter, "InitialLocation"))
        {
            const auto locationIter = initialLocation->find("location");
            if (locationIter != initialLocation->end())
            {
                const Vec3 minOffset = Evaluate_Vector3DistributionMin(*locationIter, desc.initialLocation.minOffset);
                const Vec3 maxOffset = Evaluate_Vector3DistributionMax(*locationIter, desc.initialLocation.maxOffset);
                desc.initialLocation.enabled = true;
                desc.initialLocation.minOffset = Vec3{
                    min(minOffset.x, maxOffset.x),
                    min(minOffset.y, maxOffset.y),
                    min(minOffset.z, maxOffset.z)
                };
                desc.initialLocation.maxOffset = Vec3{
                    max(minOffset.x, maxOffset.x),
                    max(minOffset.y, maxOffset.y),
                    max(minOffset.z, maxOffset.z)
                };
            }
            if (locationIter != initialLocation->end())
                desc.initialLocationSeed = Read_DistributionRandomSeedDesc(*locationIter, initialLocation);
        }

        if (const json* sphereLocation = Find_ModuleData(emitter, "SphereLocation"))
        {
            desc.sphereLocation.enabled = true;

            const auto offsetIter = sphereLocation->find("offset");
            if (offsetIter != sphereLocation->end())
                Read_Vec3(*offsetIter, desc.sphereLocation.offset);

            desc.sphereLocation.radius = max(0.f, Read_Float(*sphereLocation, "radius", desc.sphereLocation.radius));
            Read_Enum(*sphereLocation, "spawnMode", desc.sphereLocation.mode);
            Read_Enum(*sphereLocation, "placementMode", desc.sphereLocation.placementMode);
            desc.sphereLocationSeed = Read_RandomSeedDesc(*sphereLocation);
        }

        if (const json* planeRadialLocation = Find_ModuleData(emitter, "PlaneRadialLocation"))
            Fill_PlaneRadialLocationPayload(desc.planeRadialLocation, *planeRadialLocation);

        if (const json* cylinderLocation = Find_ModuleData(emitter, "CylinderLocation"))
            Fill_CylinderLocationPayload(desc.cylinderLocation, *cylinderLocation);

        if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
        {
            const auto lifeTimeIter = lifetime->find("lifeTime");
            if (lifeTimeIter != lifetime->end())
            {
                desc.lifetime.lifeTime = Vec2{
                    Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                    Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
                };
                Fill_FloatCurvePayload(desc.lifetime.lifeTimeCurve, *lifeTimeIter, desc.lifetime.lifeTime.y);
                desc.lifetimeSeed = Read_DistributionRandomSeedDesc(*lifeTimeIter, lifetime);
            }
        }

        if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
        {
            const auto colorIter = initialColor->find("color");
            if (colorIter != initialColor->end())
            {
                desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
                desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
                desc.initialColor.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, initialColor);
            }

            const auto alphaIter = initialColor->find("alpha");
            if (alphaIter != initialColor->end())
            {
                desc.initialColor.startColorMin.w =
                    clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
                desc.initialColor.startColorMax.w =
                    clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
                desc.initialColor.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, initialColor);
            }

            desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
            desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
        }

        if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
        {
            const auto colorIter = colorOverLife->find("colorOverLife");
            if (colorIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
                desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
                desc.colorOverLife.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, colorOverLife);
            }

            const auto alphaIter = colorOverLife->find("alphaOverLife");
            if (alphaIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
                desc.colorOverLife.endColorMax.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
                desc.colorOverLife.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, colorOverLife);
            }

            Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
        }

        if (const json* subUvFrameOverLife = Find_ModuleData(emitter, "SubUVFrameOverLife"))
        {
            desc.subUVFrameOverLife.enabled = true;
            desc.subUVFrameOverLife.startFrame = Read_UInt(*subUvFrameOverLife, "startFrame", desc.subUVFrameOverLife.startFrame);
            desc.subUVFrameOverLife.endFrame = Read_UInt(*subUvFrameOverLife, "endFrame", desc.subUVFrameOverLife.endFrame);
            desc.subUVFrameOverLife.loop = Read_Bool(*subUvFrameOverLife, "loop", desc.subUVFrameOverLife.loop);
            Read_Enum(*subUvFrameOverLife, "playbackMode", desc.subUVFrameOverLife.playbackMode);
            desc.subUVFrameOverLife.framesPerSecond =
                max(0.f, Read_Float(*subUvFrameOverLife, "framesPerSecond", desc.subUVFrameOverLife.framesPerSecond));
            desc.subUVFrameOverLife.randomStartPhase =
                Read_Bool(*subUvFrameOverLife, "randomStartPhase", desc.subUVFrameOverLife.randomStartPhase);

            const auto frameIndexIter = subUvFrameOverLife->find("frameIndex");
            if (frameIndexIter != subUvFrameOverLife->end())
                Fill_SubUVFrameCurvePayload(desc.subUVFrameOverLife, *frameIndexIter);
            if (desc.subUVFrameOverLife.playbackMode == SubUVFramePlaybackMode::RandomFrame ||
                desc.subUVFrameOverLife.randomStartPhase)
                desc.subUVRandomFrameSeed = Read_RandomSeedDesc(*subUvFrameOverLife);

            const uint32 frameCount = max(1u, desc.required.material.subUVRows * desc.required.material.subUVCols);
            const uint32 lastFrame = frameCount - 1u;
            desc.subUVFrameOverLife.startFrame = min(desc.subUVFrameOverLife.startFrame, lastFrame);
            desc.subUVFrameOverLife.endFrame = min(desc.subUVFrameOverLife.endFrame, lastFrame);

            if (0u == Read_UInt(*subUvFrameOverLife, "startFrame") &&
                0u == Read_UInt(*subUvFrameOverLife, "endFrame") &&
                frameCount > 1u)
                desc.subUVFrameOverLife.endFrame = lastFrame;
        }

        if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
            Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

        if (const json* initialVelocity = Find_ModuleData(emitter, "InitialVelocity"))
        {
            desc.motion.enabled = true;
            desc.motion.initialVelocityEnabled = true;
            desc.motion.initialVelocityInWorldSpace = Read_Bool(*initialVelocity, "inWorldSpace", desc.motion.initialVelocityInWorldSpace);

            const auto velocityIter = initialVelocity->find("velocity");
            if (velocityIter != initialVelocity->end())
            {
                desc.motion.initialVelocityMin = Evaluate_Vector3DistributionMin(*velocityIter, desc.motion.initialVelocityMin);
                desc.motion.initialVelocityMax = Evaluate_Vector3DistributionMax(*velocityIter, desc.motion.initialVelocityMax);
            }
            if (velocityIter != initialVelocity->end())
                desc.motion.initialVelocitySeed = Read_DistributionRandomSeedDesc(*velocityIter, initialVelocity);
        }

        if (const json* initialRadialVelocity = Find_ModuleData(emitter, "InitialRadialVelocity"))
        {
            desc.motion.enabled = true;
            desc.motion.initialRadialVelocityEnabled = true;
            desc.motion.initialRadialVelocityInWorldSpace =
                Read_Bool(*initialRadialVelocity, "inWorldSpace", desc.motion.initialRadialVelocityInWorldSpace);

            const auto radialPivotIter = initialRadialVelocity->find("radialPivot");
            if (radialPivotIter != initialRadialVelocity->end())
                Read_Vec3(*radialPivotIter, desc.motion.radialPivot);
            Read_Enum(
                *initialRadialVelocity,
                "centerDirectionMode",
                desc.motion.initialRadialVelocityCenterDirectionMode
            );

            const auto speedIter = initialRadialVelocity->find("speed");
            if (speedIter != initialRadialVelocity->end())
            {
                desc.motion.radialSpeed = Vec2{
                    Evaluate_FloatDistributionMin(*speedIter, desc.motion.radialSpeed.x),
                    Evaluate_FloatDistributionMax(*speedIter, desc.motion.radialSpeed.y)
                };
            }
            if (speedIter != initialRadialVelocity->end())
                desc.motion.initialRadialVelocitySeed = Read_DistributionRandomSeedDesc(*speedIter, initialRadialVelocity);
        }

        if (const json* velocityCone = Find_ModuleData(emitter, "VelocityCone"))
        {
            desc.motion.enabled = true;
            Fill_VelocityConePayload(desc.motion, *velocityCone);
            if (const auto speedIter = velocityCone->find("speed"); speedIter != velocityCone->end())
                desc.motion.velocityConeSeed = Read_DistributionRandomSeedDesc(*speedIter, velocityCone);
        }

        if (const json* sourceMotionVelocity = Find_ModuleData(emitter, "SourceMotionVelocity"))
            Fill_SourceMotionVelocityPayload(desc.motion, *sourceMotionVelocity);

        if (const json* acceleration = Find_ModuleData(emitter, "Acceleration"))
        {
            desc.motion.enabled = true;
            desc.motion.accelerationInWorldSpace =
                Read_Bool(*acceleration, "inWorldSpace", desc.motion.accelerationInWorldSpace);
            Read_Enum(*acceleration, "timeBasis", desc.motion.accelerationTimeBasis);
            const auto accelerationIter = acceleration->find("acceleration");
            if (accelerationIter != acceleration->end())
            {
                desc.motion.accelerationMin = Evaluate_Vector3DistributionMin(*accelerationIter, desc.motion.accelerationMin);
                desc.motion.accelerationMax = Evaluate_Vector3DistributionMax(*accelerationIter, desc.motion.accelerationMax);
                Fill_AccelerationCurvePayload(desc.motion, *accelerationIter);
            }
            if (accelerationIter != acceleration->end())
                desc.motion.accelerationSeed = Read_DistributionRandomSeedDesc(*accelerationIter, acceleration);
        }

        if (const json* drag = Find_ModuleData(emitter, "Drag"))
        {
            desc.motion.enabled = true;
            const auto dragIter = drag->find("drag");
            if (dragIter != drag->end())
            {
                desc.motion.drag = Vec2{
                    max(0.f, Evaluate_FloatDistributionMin(*dragIter, desc.motion.drag.x)),
                    max(0.f, Evaluate_FloatDistributionMax(*dragIter, desc.motion.drag.y))
                };
            }
            if (dragIter != drag->end())
                desc.motion.dragSeed = Read_DistributionRandomSeedDesc(*dragIter, drag);
        }

        Apply_VelocityOverLifeOwnership(desc.motion, emitter);

        if (const json* orbitOverLife = Find_ModuleData(emitter, "OrbitOverLife"))
            Fill_OrbitOverLifePayload(desc.orbitOverLife, *orbitOverLife);

        if (const json* initialRotation = Find_ModuleData(emitter, "InitialRotation"))
        {
            desc.rotation.enabled = true;
            const auto rotationIter = initialRotation->find("rotationDegrees");
            if (rotationIter != initialRotation->end())
            {
                desc.rotation.initialRotationDegrees = Vec2{
                    Evaluate_FloatDistributionMin(*rotationIter, desc.rotation.initialRotationDegrees.x),
                    Evaluate_FloatDistributionMax(*rotationIter, desc.rotation.initialRotationDegrees.y)
                };
            }
            if (rotationIter != initialRotation->end())
                desc.rotation.initialRotationSeed = Read_DistributionRandomSeedDesc(*rotationIter, initialRotation);
        }

        if (const json* planeRadialOrientation = Find_ModuleData(emitter, "PlaneRadialOrientation"))
            Fill_PlaneRadialOrientationPayload(desc.planeRadialOrientation, *planeRadialOrientation);
        if (const json* cylinderOrientation = Find_ModuleData(emitter, "CylinderOrientation"))
            Fill_CylinderOrientationPayload(desc.cylinderOrientation, *cylinderOrientation);

        if (const json* rotationOverLife = Find_ModuleData(emitter, "RotationOverLife"))
        {
            desc.rotation.enabled = true;
            Fill_RotationOverLifePayload(desc.rotation, *rotationOverLife);
        }

        Fill_SpriteTiltPayload(
            desc.spriteTilt,
            Find_ModuleData(emitter, "SpriteTilt"),
            Find_ModuleData(emitter, "SpriteTiltOverLife")
        );

        if (const json* initialRotationRate = Find_ModuleData(emitter, "InitialRotationRate"))
        {
            desc.rotation.enabled = true;
            const auto rotationRateIter = initialRotationRate->find("rotationRateDegrees");
            if (rotationRateIter != initialRotationRate->end())
            {
                desc.rotation.initialRotationRateDegrees = Vec2{
                    Evaluate_FloatDistributionMin(*rotationRateIter, desc.rotation.initialRotationRateDegrees.x),
                    Evaluate_FloatDistributionMax(*rotationRateIter, desc.rotation.initialRotationRateDegrees.y)
                };
            }
            if (rotationRateIter != initialRotationRate->end())
                desc.rotation.initialRotationRateSeed = Read_DistributionRandomSeedDesc(*rotationRateIter, initialRotationRate);
        }

        if (const json* rotationRateScaleByLife = Find_ModuleData(emitter, "RotationRateScaleByLife"))
        {
            desc.rotation.enabled = true;
            Fill_RotationRateScaleByLifePayload(desc.rotation, *rotationRateScaleByLife);
        }

        outDefinition.concreteDesc = desc;
        return true;
    }

    bool Build_TrailEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
    {
        outDefinition = EffectEmitterDefinition{};
        outDefinition.id = Read_UInt(emitter, "id");
        outDefinition.name = Read_String(emitter, "name");
        outDefinition.kind = EffectEmitterKind::Trail;
        outDefinition.enabled = true;
        Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);
        outDefinition.historyBudget = Read_HistoryBudgetUsage(emitter);

        ComputeTrailEmitterDesc desc{};
        desc.renderLayerOverride = outDefinition.renderLayerOverride;

        const auto typeDataIter = emitter.find("typeData");
        if (typeDataIter != emitter.end() && typeDataIter->is_object())
        {
            const auto dataIter = typeDataIter->find("data");
            if (dataIter != typeDataIter->end() && dataIter->is_object())
            {
                const json& typeData = *dataIter;
                desc.width = max(0.001f, Read_Float(typeData, "width", desc.width));
                desc.segmentLifetime = max(0.001f, Read_Float(typeData, "segmentLifetime", desc.segmentLifetime));
                desc.historyCount = max(2u, Read_UInt(typeData, "historyCount", desc.historyCount));
                desc.uvTiling = max(0.001f, Read_Float(typeData, "uvTiling", desc.uvTiling));
                desc.maxTrailLength = max(0.f, Read_Float(typeData, "maxTrailLength", desc.maxTrailLength));
                desc.tailFadeLength = max(0.f, Read_Float(typeData, "tailFadeLength", desc.tailFadeLength));
                desc.autoLifeFade = Read_Bool(typeData, "autoLifeFade", desc.autoLifeFade);
                desc.sampleSpacing = max(0.001f, Read_Float(typeData, "sampleSpacing", desc.sampleSpacing));
                desc.curveSubdivision = max(0u, Read_UInt(typeData, "curveSubdivision", desc.curveSubdivision));
                desc.smoothTangent = Read_Bool(typeData, "smoothTangent", desc.smoothTangent);
                desc.sideFade = clamp(Read_Float(typeData, "sideFade", desc.sideFade), 0.001f, 0.49f);
            }
        }

        if (const json* required = Find_ModuleData(emitter, "Required"))
            Apply_RequiredModule(*required, outDefinition, desc);

        if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
        {
            Fill_MaterialScalarModulationPayload(desc.material.scalarModulation, *materialScalarModulation);
            Fill_CoreColorRgbModulationPayload(desc.material.coreColorRgbModulation, *materialScalarModulation);
            Fill_MaterialVec2ModulationPayload(desc.material.vec2Modulation, *materialScalarModulation);
        }

        if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
        {
            const auto lifeTimeIter = lifetime->find("lifeTime");
            if (lifeTimeIter != lifetime->end())
            {
                desc.useLifetimeSegmentLifetime = true;
                desc.lifetime.lifeTime = Vec2{
                    Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                    Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
                };
                desc.lifetimeSeed = Read_DistributionRandomSeedDesc(*lifeTimeIter, lifetime);
                desc.segmentLifetime = max(0.001f, desc.lifetime.lifeTime.y);
            }
        }

        if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
        {
            const auto colorIter = initialColor->find("color");
            if (colorIter != initialColor->end())
            {
                desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
                desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
            }

            const auto alphaIter = initialColor->find("alpha");
            if (alphaIter != initialColor->end())
            {
                desc.initialColor.startColorMin.w =
                    clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
                desc.initialColor.startColorMax.w =
                    clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
            }

            desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
            desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
        }

        if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
        {
            const auto colorIter = colorOverLife->find("colorOverLife");
            if (colorIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
                desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
            }

            const auto alphaIter = colorOverLife->find("alphaOverLife");
            if (alphaIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
                desc.colorOverLife.endColorMax.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
            }

            Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
        }

        if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
            Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

        if (const json* spawnPerUnit = Find_ModuleData(emitter, "SpawnPerUnit"))
        {
            desc.spawnPerUnit.enabled = true;

            const auto spawnPerUnitIter = spawnPerUnit->find("spawnPerUnit");
            if (spawnPerUnitIter != spawnPerUnit->end())
            {
                desc.spawnPerUnit.spawnPerUnit =
                    max(0.f, Evaluate_FloatDistributionMax(*spawnPerUnitIter, desc.spawnPerUnit.spawnPerUnit));
            }

            desc.spawnPerUnit.unitScalar = max(0.0001f, Read_Float(*spawnPerUnit, "unitScalar", desc.spawnPerUnit.unitScalar));
            desc.spawnPerUnit.movementTolerance =
                max(0.f, Read_Float(*spawnPerUnit, "movementTolerance", desc.spawnPerUnit.movementTolerance));
            desc.spawnPerUnit.maxFrameDistance =
                max(0.f, Read_Float(*spawnPerUnit, "maxFrameDistance", desc.spawnPerUnit.maxFrameDistance));
        }

        outDefinition.concreteDesc = desc;
        return true;
    }

    bool Build_SourceHistoryRibbonEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
    {
        outDefinition = EffectEmitterDefinition{};
        outDefinition.id = Read_UInt(emitter, "id");
        outDefinition.name = Read_String(emitter, "name");
        outDefinition.kind = EffectEmitterKind::Ribbon;
        outDefinition.enabled = true;
        Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);
        outDefinition.historyBudget = Read_HistoryBudgetUsage(emitter);

        ComputeRibbonEmitterDesc desc{};
        desc.renderLayerOverride = outDefinition.renderLayerOverride;

        const auto typeDataIter = emitter.find("typeData");
        if (typeDataIter != emitter.end() && typeDataIter->is_object())
        {
            const auto dataIter = typeDataIter->find("data");
            if (dataIter != typeDataIter->end() && dataIter->is_object())
            {
                const json& typeData = *dataIter;
                const string sourceMode = Read_String(typeData, "sourceMode", "SelfRoot");
                desc.sourceMode = sourceMode == "ParticleEmitter" || sourceMode == "SourceEmitter"
                                  ? EffectSourceHistoryRibbonSourceMode::ParticleEmitter
                                  : EffectSourceHistoryRibbonSourceMode::SelfRoot;
                desc.sourceEmitterId =
                    desc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter
                    ? Read_UInt(typeData, "sourceEmitterId", 0u)
                    : 0u;
                desc.followerLaneCount = max(1u, Read_UInt(typeData, "followerLaneCount", desc.followerLaneCount));
                desc.maxSampleCount = max(2u, Read_UInt(typeData, "maxSampleCount", desc.maxSampleCount));
                desc.sampleSpacing = max(0.001f, Read_Float(typeData, "sampleSpacing", desc.sampleSpacing));
                const bool hasCurveSubdivision = typeData.find("curveSubdivision") != typeData.end();
                desc.curveSubdivision = max(0u, Read_UInt(typeData, "curveSubdivision", desc.curveSubdivision));
                desc.smoothTangent = Read_Bool(typeData, "smoothTangent", desc.smoothTangent);
                if (!hasCurveSubdivision)
                    desc.curveSubdivision = Resolve_SourceHistoryCurveSubdivisionPreset(desc.sampleSpacing);
                desc.sampleInterval = max(0.f, Read_Float(typeData, "sampleInterval", desc.sampleInterval));
                desc.maxLength = max(0.f, Read_Float(typeData, "maxLength", desc.maxLength));
                desc.tailFadeLength = max(0.f, Read_Float(typeData, "tailFadeLength", desc.tailFadeLength));
                desc.laneSpawnFadeInEnabled = Read_Bool(typeData, "laneSpawnFadeInEnabled", desc.laneSpawnFadeInEnabled);
                desc.laneSpawnFadeInDuration = max(0.f, Read_Float(typeData, "laneSpawnFadeInDuration", desc.laneSpawnFadeInDuration));
                desc.baseWidth = max(0.001f, Read_Float(typeData, "baseWidth", desc.baseWidth));
                desc.tilingDistance = max(0.f, Read_Float(typeData, "tilingDistance", desc.tilingDistance));
                desc.autoLifeFade = Read_Bool(typeData, "autoLifeFade", desc.autoLifeFade);
                desc.tailCollapseOnIdle = Read_Bool(typeData, "tailCollapseOnIdle", desc.tailCollapseOnIdle);
                desc.tailCollapseSpeed = max(0.f, Read_Float(typeData, "tailCollapseSpeed", desc.tailCollapseSpeed));
                desc.useManualRoll = Read_Bool(typeData, "useManualRoll", desc.useManualRoll);
                desc.manualRollDegrees = Read_Float(typeData, "manualRollDegrees", desc.manualRollDegrees);
            }
        }

        if (const json* ribbonOrientation = Find_ModuleData(emitter, "RibbonOrientation"))
        {
            desc.spreadBasis = Resolve_RibbonSpreadBasis(Read_String(*ribbonOrientation, "spreadBasis"));
            desc.spreadAngleDegrees = Read_Float(*ribbonOrientation, "spreadAngleDegrees", desc.spreadAngleDegrees);
            desc.useManualRoll = false;
            desc.manualRollDegrees = 0.f;
        }

        if (const json* required = Find_ModuleData(emitter, "Required"))
            Apply_RequiredModule(*required, outDefinition, desc);

        if (const json* initialLocation = Find_ModuleData(emitter, "InitialLocation"))
        {
            const auto locationIter = initialLocation->find("location");
            if (locationIter != initialLocation->end())
            {
                const Vec3 minOffset = Evaluate_Vector3DistributionMin(*locationIter, desc.headOffset.minOffset);
                const Vec3 maxOffset = Evaluate_Vector3DistributionMax(*locationIter, desc.headOffset.maxOffset);
                desc.headOffset.enabled = true;
                desc.headOffset.minOffset = Vec3{
                    min(minOffset.x, maxOffset.x),
                    min(minOffset.y, maxOffset.y),
                    min(minOffset.z, maxOffset.z)
                };
                desc.headOffset.maxOffset = Vec3{
                    max(minOffset.x, maxOffset.x),
                    max(minOffset.y, maxOffset.y),
                    max(minOffset.z, maxOffset.z)
                };
                desc.headOffsetSeed = Read_DistributionRandomSeedDesc(*locationIter, initialLocation);
            }
        }

        if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
        {
            Fill_MaterialScalarModulationPayload(desc.material.scalarModulation, *materialScalarModulation);
            Fill_CoreColorRgbModulationPayload(desc.material.coreColorRgbModulation, *materialScalarModulation);
            Fill_MaterialVec2ModulationPayload(desc.material.vec2Modulation, *materialScalarModulation);
        }

        if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
        {
            const auto lifeTimeIter = lifetime->find("lifeTime");
            if (lifeTimeIter != lifetime->end())
            {
                desc.useLifetimeSampleLifetime = true;
                desc.lifetime.lifeTime = Vec2{
                    Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                    Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
                };
                desc.lifetimeSeed = Read_DistributionRandomSeedDesc(*lifeTimeIter, lifetime);
                desc.sampleLifetime = max(0.0001f, desc.lifetime.lifeTime.y);
            }
        }

        if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
        {
            const auto colorIter = initialColor->find("color");
            if (colorIter != initialColor->end())
            {
                desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
                desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
                desc.initialColor.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, initialColor);
            }

            const auto alphaIter = initialColor->find("alpha");
            if (alphaIter != initialColor->end())
            {
                desc.initialColor.startColorMin.w =
                    clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
                desc.initialColor.startColorMax.w =
                    clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
                desc.initialColor.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, initialColor);
            }

            desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
            desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
        }

        if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
        {
            const auto colorIter = colorOverLife->find("colorOverLife");
            if (colorIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
                desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
                desc.colorOverLife.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, colorOverLife);
            }

            const auto alphaIter = colorOverLife->find("alphaOverLife");
            if (alphaIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
                desc.colorOverLife.endColorMax.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
                desc.colorOverLife.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, colorOverLife);
            }

            Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
        }

        if (const json* subUvFrameOverLife = Find_ModuleData(emitter, "SubUVFrameOverLife"))
        {
            desc.subUVFrameOverLife.enabled = true;
            desc.subUVFrameOverLife.startFrame = Read_UInt(*subUvFrameOverLife, "startFrame", desc.subUVFrameOverLife.startFrame);
            desc.subUVFrameOverLife.endFrame = Read_UInt(*subUvFrameOverLife, "endFrame", desc.subUVFrameOverLife.endFrame);
            desc.subUVFrameOverLife.loop = Read_Bool(*subUvFrameOverLife, "loop", desc.subUVFrameOverLife.loop);
            Read_Enum(*subUvFrameOverLife, "playbackMode", desc.subUVFrameOverLife.playbackMode);
            desc.subUVFrameOverLife.framesPerSecond =
                max(0.f, Read_Float(*subUvFrameOverLife, "framesPerSecond", desc.subUVFrameOverLife.framesPerSecond));
            desc.subUVFrameOverLife.randomStartPhase =
                Read_Bool(*subUvFrameOverLife, "randomStartPhase", desc.subUVFrameOverLife.randomStartPhase);

            const auto frameIndexIter = subUvFrameOverLife->find("frameIndex");
            if (frameIndexIter != subUvFrameOverLife->end())
                Fill_SubUVFrameCurvePayload(desc.subUVFrameOverLife, *frameIndexIter);
            if (desc.subUVFrameOverLife.playbackMode == SubUVFramePlaybackMode::RandomFrame ||
                desc.subUVFrameOverLife.randomStartPhase)
                desc.subUVRandomFrameSeed = Read_RandomSeedDesc(*subUvFrameOverLife);

            const uint32 frameCount = max(1u, desc.material.subUVRows * desc.material.subUVCols);
            const uint32 lastFrame = frameCount - 1u;
            desc.subUVFrameOverLife.startFrame = min(desc.subUVFrameOverLife.startFrame, lastFrame);
            desc.subUVFrameOverLife.endFrame = min(desc.subUVFrameOverLife.endFrame, lastFrame);

            if (0u == Read_UInt(*subUvFrameOverLife, "startFrame") &&
                0u == Read_UInt(*subUvFrameOverLife, "endFrame") &&
                frameCount > 1u)
                desc.subUVFrameOverLife.endFrame = lastFrame;
        }

        if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
            Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

        desc.maxSampleCount = Resolve_RibbonAutoHistoryCount(
            desc.maxSampleCount,
            desc.sampleLifetime,
            desc.sampleSpacing,
            desc.maxLength
        );

        outDefinition.concreteDesc = desc;
        return true;
    }

    bool Build_SourceHistorySpriteTrailEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
    {
        outDefinition = EffectEmitterDefinition{};
        outDefinition.id = Read_UInt(emitter, "id");
        outDefinition.name = Read_String(emitter, "name");
        outDefinition.kind = EffectEmitterKind::SourceHistorySpriteTrail;
        outDefinition.enabled = true;
        Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);
        outDefinition.historyBudget = Read_HistoryBudgetUsage(emitter);

        ComputeSourceHistorySpriteTrailEmitterDesc desc{};
        desc.renderLayerOverride = outDefinition.renderLayerOverride;

        const auto typeDataIter = emitter.find("typeData");
        if (typeDataIter != emitter.end() && typeDataIter->is_object())
        {
            const auto dataIter = typeDataIter->find("data");
            if (dataIter != typeDataIter->end() && dataIter->is_object())
            {
                const json& typeData = *dataIter;
                const string sourceMode = Read_String(typeData, "sourceMode", "SelfRoot");
                desc.sourceMode = sourceMode == "ParticleEmitter" || sourceMode == "SourceEmitter"
                                  ? EffectSourceHistoryRibbonSourceMode::ParticleEmitter
                                  : EffectSourceHistoryRibbonSourceMode::SelfRoot;
                desc.sourceEmitterId =
                    desc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter
                    ? Read_UInt(typeData, "sourceEmitterId", 0u)
                    : 0u;
                desc.followerLaneCount = max(1u, Read_UInt(typeData, "followerLaneCount", desc.followerLaneCount));
                desc.sampleLifetime = max(0.0001f, Read_Float(typeData, "sampleLifetime", desc.sampleLifetime));
                desc.sampleSpacing = max(0.001f, Read_Float(typeData, "sampleSpacing", desc.sampleSpacing));
                const bool hasCurveSubdivision = typeData.find("curveSubdivision") != typeData.end();
                desc.curveSubdivision = max(0u, Read_UInt(typeData, "curveSubdivision", desc.curveSubdivision));
                desc.smoothTangent = Read_Bool(typeData, "smoothTangent", desc.smoothTangent);
                if (!hasCurveSubdivision)
                    desc.curveSubdivision = Resolve_SourceHistoryCurveSubdivisionPreset(desc.sampleSpacing);
                desc.maxLength = max(0.f, Read_Float(typeData, "maxLength", desc.maxLength));

                const string stampSpawnMode = Read_String(typeData, "stampSpawnMode", "Distance");
                desc.stampSpawnMode = stampSpawnMode == "Time"
                                      ? EffectSourceHistorySpriteTrailStampSpawnMode::Time
                                      : EffectSourceHistorySpriteTrailStampSpawnMode::Distance;
                desc.stampSpacing = max(0.001f, Read_Float(typeData, "stampSpacing", desc.stampSpacing));
                desc.stampInterval = max(0.001f, Read_Float(typeData, "stampInterval", desc.stampInterval));
                desc.maxStampCount = max(1u, Read_UInt(typeData, "maxStampCount", desc.maxStampCount));
                desc.cardLength = max(0.001f, Read_Float(typeData, "cardLength", desc.cardLength));
                desc.cardWidth = max(0.001f, Read_Float(typeData, "cardWidth", desc.cardWidth));
                desc.flipU = Read_Bool(typeData, "flipU", desc.flipU);
                desc.flipV = Read_Bool(typeData, "flipV", desc.flipV);
                desc.rotationOffsetDegrees = Read_Float(typeData, "rotationOffsetDegrees", desc.rotationOffsetDegrees);
                desc.spawnJitter = max(0.f, Read_Float(typeData, "spawnJitter", desc.spawnJitter));
            }
        }

        if (const json* required = Find_ModuleData(emitter, "Required"))
            Apply_RequiredModule(*required, outDefinition, desc);

        if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
        {
            Fill_MaterialScalarModulationPayload(desc.material.scalarModulation, *materialScalarModulation);
            Fill_CoreColorRgbModulationPayload(desc.material.coreColorRgbModulation, *materialScalarModulation);
            Fill_MaterialVec2ModulationPayload(desc.material.vec2Modulation, *materialScalarModulation);
        }

        if (const json* initialSize = Find_ModuleData(emitter, "InitialSize"))
        {
            desc.initialSize.enabled = true;
            const auto sizeIter = initialSize->find("size");
            if (sizeIter != initialSize->end())
            {
                const Vec2 minSize = Evaluate_Vector2DistributionMin(*sizeIter, desc.initialSize.sizeMin);
                const Vec2 maxSize = Evaluate_Vector2DistributionMax(*sizeIter, desc.initialSize.sizeMax);
                desc.initialSize.sizeMin = Vec2{ min(minSize.x, maxSize.x), min(minSize.y, maxSize.y) };
                desc.initialSize.sizeMax = Vec2{ max(minSize.x, maxSize.x), max(minSize.y, maxSize.y) };
                desc.initialSize.sizeSeed = Read_DistributionRandomSeedDesc(*sizeIter, initialSize);
            }
        }

        if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
        {
            const auto lifeTimeIter = lifetime->find("lifeTime");
            if (lifeTimeIter != lifetime->end())
            {
                desc.useLifetimeStampLifetime = true;
                desc.lifetime.lifeTime = Vec2{
                    Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                    Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
                };
                Fill_FloatCurvePayload(desc.lifetime.lifeTimeCurve, *lifeTimeIter, desc.lifetime.lifeTime.y);
                desc.lifetimeSeed = Read_DistributionRandomSeedDesc(*lifeTimeIter, lifetime);
            }
        }

        if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
        {
            const auto colorIter = initialColor->find("color");
            if (colorIter != initialColor->end())
            {
                desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
                desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
                desc.initialColor.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, initialColor);
            }

            const auto alphaIter = initialColor->find("alpha");
            if (alphaIter != initialColor->end())
            {
                desc.initialColor.startColorMin.w =
                    clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
                desc.initialColor.startColorMax.w =
                    clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
                desc.initialColor.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, initialColor);
            }

            desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
            desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
        }

        if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
        {
            const auto colorIter = colorOverLife->find("colorOverLife");
            if (colorIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
                desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
                desc.colorOverLife.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, colorOverLife);
            }

            const auto alphaIter = colorOverLife->find("alphaOverLife");
            if (alphaIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
                desc.colorOverLife.endColorMax.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
                desc.colorOverLife.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, colorOverLife);
            }

            Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
        }

        if (const json* subUvFrameOverLife = Find_ModuleData(emitter, "SubUVFrameOverLife"))
        {
            desc.subUVFrameOverLife.enabled = true;
            desc.subUVFrameOverLife.startFrame = Read_UInt(*subUvFrameOverLife, "startFrame", desc.subUVFrameOverLife.startFrame);
            desc.subUVFrameOverLife.endFrame = Read_UInt(*subUvFrameOverLife, "endFrame", desc.subUVFrameOverLife.endFrame);
            desc.subUVFrameOverLife.loop = Read_Bool(*subUvFrameOverLife, "loop", desc.subUVFrameOverLife.loop);
            Read_Enum(*subUvFrameOverLife, "playbackMode", desc.subUVFrameOverLife.playbackMode);
            desc.subUVFrameOverLife.framesPerSecond =
                max(0.f, Read_Float(*subUvFrameOverLife, "framesPerSecond", desc.subUVFrameOverLife.framesPerSecond));
            desc.subUVFrameOverLife.randomStartPhase =
                Read_Bool(*subUvFrameOverLife, "randomStartPhase", desc.subUVFrameOverLife.randomStartPhase);

            const auto frameIndexIter = subUvFrameOverLife->find("frameIndex");
            if (frameIndexIter != subUvFrameOverLife->end())
                Fill_SubUVFrameCurvePayload(desc.subUVFrameOverLife, *frameIndexIter);
            if (desc.subUVFrameOverLife.playbackMode == SubUVFramePlaybackMode::RandomFrame ||
                desc.subUVFrameOverLife.randomStartPhase)
                desc.subUVRandomFrameSeed = Read_RandomSeedDesc(*subUvFrameOverLife);

            const uint32 frameCount = max(1u, desc.material.subUVRows * desc.material.subUVCols);
            const uint32 lastFrame = frameCount - 1u;
            desc.subUVFrameOverLife.startFrame = min(desc.subUVFrameOverLife.startFrame, lastFrame);
            desc.subUVFrameOverLife.endFrame = min(desc.subUVFrameOverLife.endFrame, lastFrame);

            if (0u == Read_UInt(*subUvFrameOverLife, "startFrame") &&
                0u == Read_UInt(*subUvFrameOverLife, "endFrame") &&
                frameCount > 1u)
                desc.subUVFrameOverLife.endFrame = lastFrame;
        }

        if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
            Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

        const json* pathReplay = Find_ModuleData(emitter, "SourceHistorySpriteTrailPathReplay");
        if (pathReplay != nullptr)
        {
            desc.pathReplay.enabled = true;
            desc.pathReplay.delayTime = max(0.f, Read_Float(*pathReplay, "delayTime", desc.pathReplay.delayTime));
            const string replayMode = Read_String(*pathReplay, "replayMode", "RecordedSpeed");
            desc.pathReplay.replayMode = replayMode == "FitDuration"
                                         ? EffectSourceHistorySpriteTrailPathReplayMode::FitDuration
                                         : EffectSourceHistorySpriteTrailPathReplayMode::RecordedSpeed;
            desc.pathReplay.speedScale = max(0.f, Read_Float(*pathReplay, "speedScale", desc.pathReplay.speedScale));
            desc.pathReplay.drainDuration = max(0.0001f, Read_Float(*pathReplay, "drainDuration", desc.pathReplay.drainDuration));
            if (const auto drainCurveIter = pathReplay->find("drainCurve"); drainCurveIter != pathReplay->end())
                Fill_FloatCurvePayload(desc.pathReplay.drainCurve, *drainCurveIter, 1.f);
            const string startMode = Read_String(*pathReplay, "startMode", "TailFirst");
            desc.pathReplay.startMode = startMode == "AllAtOnce"
                                        ? EffectSourceHistorySpriteTrailPathReplayStartMode::AllAtOnce
                                        : EffectSourceHistorySpriteTrailPathReplayStartMode::TailFirst;
            desc.pathReplay.arrivalMode = Resolve_SourceHistorySpriteTrailArrivalMode(*pathReplay);
        }

        if (pathReplay == nullptr)
        {
            if (const json* pathFollow = Find_ModuleData(emitter, "SourceHistorySpriteTrailPathFollow"))
            {
                desc.pathFollow.enabled = true;
                const string direction = Read_String(*pathFollow, "direction", "TowardHead");
                desc.pathFollow.direction = direction == "TowardTail"
                                            ? EffectSourceHistorySpriteTrailPathFollowDirection::TowardTail
                                            : EffectSourceHistorySpriteTrailPathFollowDirection::TowardHead;
                if (const auto speedIter = pathFollow->find("speed"); speedIter != pathFollow->end())
                {
                    desc.pathFollow.speed = Vec2{
                        max(0.f, Evaluate_FloatDistributionMin(*speedIter, desc.pathFollow.speed.x)),
                        max(0.f, Evaluate_FloatDistributionMax(*speedIter, desc.pathFollow.speed.y))
                    };
                    desc.pathFollow.speedSeed = Read_DistributionRandomSeedDesc(*speedIter, pathFollow);
                }
                if (const auto startDelayIter = pathFollow->find("startDelay"); startDelayIter != pathFollow->end())
                {
                    desc.pathFollow.startDelay = Vec2{
                        max(0.f, Evaluate_FloatDistributionMin(*startDelayIter, desc.pathFollow.startDelay.x)),
                        max(0.f, Evaluate_FloatDistributionMax(*startDelayIter, desc.pathFollow.startDelay.y))
                    };
                    desc.pathFollow.startDelaySeed = Read_DistributionRandomSeedDesc(*startDelayIter, pathFollow);
                }
                desc.pathFollow.arrivalMode = Resolve_SourceHistorySpriteTrailArrivalMode(*pathFollow);
            }
        }

        if (const json* initialVelocity = Find_ModuleData(emitter, "InitialVelocity"))
        {
            desc.motion.enabled = true;
            desc.motion.initialVelocityEnabled = true;
            desc.motion.initialVelocityInWorldSpace = Read_Bool(*initialVelocity, "inWorldSpace", desc.motion.initialVelocityInWorldSpace);

            const auto velocityIter = initialVelocity->find("velocity");
            if (velocityIter != initialVelocity->end())
            {
                desc.motion.initialVelocityMin = Evaluate_Vector3DistributionMin(*velocityIter, desc.motion.initialVelocityMin);
                desc.motion.initialVelocityMax = Evaluate_Vector3DistributionMax(*velocityIter, desc.motion.initialVelocityMax);
                desc.motion.initialVelocitySeed = Read_DistributionRandomSeedDesc(*velocityIter, initialVelocity);
            }
        }

        if (const json* initialRadialVelocity = Find_ModuleData(emitter, "InitialRadialVelocity"))
        {
            desc.motion.enabled = true;
            desc.motion.initialRadialVelocityEnabled = true;
            desc.motion.initialRadialVelocityInWorldSpace =
                Read_Bool(*initialRadialVelocity, "inWorldSpace", desc.motion.initialRadialVelocityInWorldSpace);

            const auto radialPivotIter = initialRadialVelocity->find("radialPivot");
            if (radialPivotIter != initialRadialVelocity->end())
                Read_Vec3(*radialPivotIter, desc.motion.radialPivot);
            Read_Enum(*initialRadialVelocity, "centerDirectionMode", desc.motion.initialRadialVelocityCenterDirectionMode);

            const auto speedIter = initialRadialVelocity->find("speed");
            if (speedIter != initialRadialVelocity->end())
            {
                desc.motion.radialSpeed = Vec2{
                    Evaluate_FloatDistributionMin(*speedIter, desc.motion.radialSpeed.x),
                    Evaluate_FloatDistributionMax(*speedIter, desc.motion.radialSpeed.y)
                };
                desc.motion.initialRadialVelocitySeed = Read_DistributionRandomSeedDesc(*speedIter, initialRadialVelocity);
            }
        }

        if (const json* velocityCone = Find_ModuleData(emitter, "VelocityCone"))
        {
            desc.motion.enabled = true;
            Fill_VelocityConePayload(desc.motion, *velocityCone);
            if (const auto speedIter = velocityCone->find("speed"); speedIter != velocityCone->end())
                desc.motion.velocityConeSeed = Read_DistributionRandomSeedDesc(*speedIter, velocityCone);
        }

        if (const json* sourceMotionVelocity = Find_ModuleData(emitter, "SourceMotionVelocity"))
            Fill_SourceMotionVelocityPayload(desc.motion, *sourceMotionVelocity);

        if (const json* acceleration = Find_ModuleData(emitter, "Acceleration"))
        {
            desc.motion.enabled = true;
            desc.motion.accelerationInWorldSpace =
                Read_Bool(*acceleration, "inWorldSpace", desc.motion.accelerationInWorldSpace);
            Read_Enum(*acceleration, "timeBasis", desc.motion.accelerationTimeBasis);
            const auto accelerationIter = acceleration->find("acceleration");
            if (accelerationIter != acceleration->end())
            {
                desc.motion.accelerationMin = Evaluate_Vector3DistributionMin(*accelerationIter, desc.motion.accelerationMin);
                desc.motion.accelerationMax = Evaluate_Vector3DistributionMax(*accelerationIter, desc.motion.accelerationMax);
                Fill_AccelerationCurvePayload(desc.motion, *accelerationIter);
                desc.motion.accelerationSeed = Read_DistributionRandomSeedDesc(*accelerationIter, acceleration);
            }
        }

        if (const json* drag = Find_ModuleData(emitter, "Drag"))
        {
            desc.motion.enabled = true;
            const auto dragIter = drag->find("drag");
            if (dragIter != drag->end())
            {
                desc.motion.drag = Vec2{
                    max(0.f, Evaluate_FloatDistributionMin(*dragIter, desc.motion.drag.x)),
                    max(0.f, Evaluate_FloatDistributionMax(*dragIter, desc.motion.drag.y))
                };
                desc.motion.dragSeed = Read_DistributionRandomSeedDesc(*dragIter, drag);
            }
        }

        Apply_VelocityOverLifeOwnership(desc.motion, emitter);

        if (const json* initialRotation = Find_ModuleData(emitter, "InitialRotation"))
        {
            desc.rotation.enabled = true;
            const auto rotationIter = initialRotation->find("rotationDegrees");
            if (rotationIter != initialRotation->end())
            {
                desc.rotation.initialRotationDegrees = Vec2{
                    Evaluate_FloatDistributionMin(*rotationIter, desc.rotation.initialRotationDegrees.x),
                    Evaluate_FloatDistributionMax(*rotationIter, desc.rotation.initialRotationDegrees.y)
                };
                desc.rotation.initialRotationSeed = Read_DistributionRandomSeedDesc(*rotationIter, initialRotation);
            }
        }

        if (const json* rotationOverLife = Find_ModuleData(emitter, "RotationOverLife"))
        {
            desc.rotation.enabled = true;
            Fill_RotationOverLifePayload(desc.rotation, *rotationOverLife);
        }

        Fill_SpriteTiltPayload(
            desc.spriteTilt,
            Find_ModuleData(emitter, "SpriteTilt"),
            Find_ModuleData(emitter, "SpriteTiltOverLife")
        );

        if (const json* initialRotationRate = Find_ModuleData(emitter, "InitialRotationRate"))
        {
            desc.rotation.enabled = true;
            const auto rotationRateIter = initialRotationRate->find("rotationRateDegrees");
            if (rotationRateIter != initialRotationRate->end())
            {
                desc.rotation.initialRotationRateDegrees = Vec2{
                    Evaluate_FloatDistributionMin(*rotationRateIter, desc.rotation.initialRotationRateDegrees.x),
                    Evaluate_FloatDistributionMax(*rotationRateIter, desc.rotation.initialRotationRateDegrees.y)
                };
                desc.rotation.initialRotationRateSeed = Read_DistributionRandomSeedDesc(*rotationRateIter, initialRotationRate);
            }
        }

        if (const json* rotationRateScaleByLife = Find_ModuleData(emitter, "RotationRateScaleByLife"))
        {
            desc.rotation.enabled = true;
            Fill_RotationRateScaleByLifePayload(desc.rotation, *rotationRateScaleByLife);
        }

        outDefinition.concreteDesc = desc;
        return true;
    }

    bool Build_BeamEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
    {
        outDefinition = EffectEmitterDefinition{};
        outDefinition.id = Read_UInt(emitter, "id");
        outDefinition.name = Read_String(emitter, "name");
        outDefinition.kind = EffectEmitterKind::Beam;
        outDefinition.enabled = true;
        Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);

        ComputeBeamEmitterDesc desc{};
        desc.renderLayerOverride = outDefinition.renderLayerOverride;

        const auto typeDataIter = emitter.find("typeData");
        if (typeDataIter != emitter.end() && typeDataIter->is_object())
        {
            const auto dataIter = typeDataIter->find("data");
            if (dataIter != typeDataIter->end() && dataIter->is_object())
            {
                const json& typeData = *dataIter;
                Read_Enum(typeData, "endpointMode", desc.endpointMode);

                const auto localStartIter = typeData.find("localStart");
                if (localStartIter != typeData.end())
                    Read_Vec3(*localStartIter, desc.localStart);

                const auto localEndIter = typeData.find("localEnd");
                if (localEndIter != typeData.end())
                    Read_Vec3(*localEndIter, desc.localEnd);

                const auto localDirectionIter = typeData.find("localDirection");
                if (localDirectionIter != typeData.end())
                    Read_Vec3(*localDirectionIter, desc.localDirection);

                desc.length = max(0.f, Read_Float(typeData, "length", desc.length));
                desc.segmentCount = max(1u, Read_UInt(typeData, "segmentCount", desc.segmentCount));
                desc.noiseAmplitude = max(0.f, Read_Float(typeData, "noiseAmplitude", desc.noiseAmplitude));
                desc.seed = Read_UInt(typeData, "seed", desc.seed);
                desc.stripCount = max(1u, Read_UInt(typeData, "stripCount", desc.stripCount));
                desc.endSpreadRadius = max(0.f, Read_Float(typeData, "endSpreadRadius", desc.endSpreadRadius));
                desc.lengthVariance = max(0.f, Read_Float(typeData, "lengthVariance", desc.lengthVariance));
                Read_Enum(typeData, "branchPreset", desc.branchPreset);
                desc.branchEnabled = Read_Bool(typeData, "branchEnabled", desc.branchEnabled);
                desc.branchCount = Read_UInt(typeData, "branchCount", desc.branchCount);
                desc.branchChance = clamp(Read_Float(typeData, "branchChance", desc.branchChance), 0.f, 1.f);
                desc.branchSegmentCount = max(1u, Read_UInt(typeData, "branchSegmentCount", desc.branchSegmentCount));
                desc.branchLength = max(0.f, Read_Float(typeData, "branchLength", desc.branchLength));
                desc.branchLengthVariance = max(0.f, Read_Float(typeData, "branchLengthVariance", desc.branchLengthVariance));
                desc.branchStartMin = clamp(Read_Float(typeData, "branchStartMin", desc.branchStartMin), 0.f, 1.f);
                desc.branchStartMax = clamp(Read_Float(typeData, "branchStartMax", desc.branchStartMax), 0.f, 1.f);
                if (desc.branchStartMin > desc.branchStartMax)
                    swap(desc.branchStartMin, desc.branchStartMax);
                desc.branchSpreadRadius = max(0.f, Read_Float(typeData, "branchSpreadRadius", desc.branchSpreadRadius));
                if (typeData.contains("branchEndSpreadRadius"))
                    desc.branchEndSpreadRadius = max(0.f, Read_Float(typeData, "branchEndSpreadRadius", desc.branchEndSpreadRadius));
                else
                    desc.branchEndSpreadRadius = desc.branchSpreadRadius;
                desc.branchOutwardAmount = max(0.f, Read_Float(typeData, "branchOutwardAmount", desc.branchOutwardAmount));
                desc.branchCurveAmount = max(0.f, Read_Float(typeData, "branchCurveAmount", desc.branchCurveAmount));
                desc.branchDownLength = max(0.f, Read_Float(typeData, "branchDownLength", desc.branchDownLength));
                desc.branchEntangleRadius = max(0.f, Read_Float(typeData, "branchEntangleRadius", desc.branchEntangleRadius));
                desc.branchEntangleAdvance = max(0.f, Read_Float(typeData, "branchEntangleAdvance", desc.branchEntangleAdvance));
                desc.branchCrackLength = max(0.f, Read_Float(typeData, "branchCrackLength", desc.branchCrackLength));
                desc.branchCrackSpreadRadius = max(0.f, Read_Float(typeData, "branchCrackSpreadRadius", desc.branchCrackSpreadRadius));
                desc.branchWidthScale = max(0.f, Read_Float(typeData, "branchWidthScale", desc.branchWidthScale));
                desc.branchSeedOffset = Read_UInt(typeData, "branchSeedOffset", desc.branchSeedOffset);
                desc.baseWidth = max(0.001f, Read_Float(typeData, "baseWidth", desc.baseWidth));
                desc.tilingDistance = max(0.f, Read_Float(typeData, "tilingDistance", desc.tilingDistance));
            }
        }

        if (const json* required = Find_ModuleData(emitter, "Required"))
            Apply_RequiredModule(*required, outDefinition, desc);

        if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
        {
            Fill_MaterialScalarModulationPayload(desc.material.scalarModulation, *materialScalarModulation);
            Fill_CoreColorRgbModulationPayload(desc.material.coreColorRgbModulation, *materialScalarModulation);
            Fill_MaterialVec2ModulationPayload(desc.material.vec2Modulation, *materialScalarModulation);
        }

        if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
        {
            const auto lifeTimeIter = lifetime->find("lifeTime");
            if (lifeTimeIter != lifetime->end())
            {
                desc.useLifetimeVisualLife = true;
                desc.lifetime.lifeTime = Vec2{
                    Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                    Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
                };
                Fill_FloatCurvePayload(desc.lifetime.lifeTimeCurve, *lifeTimeIter, desc.lifetime.lifeTime.y);
                desc.lifetimeSeed = Read_DistributionRandomSeedDesc(*lifeTimeIter, lifetime);
            }
        }

        if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
        {
            const auto colorIter = initialColor->find("color");
            if (colorIter != initialColor->end())
            {
                desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
                desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
                desc.initialColor.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, initialColor);
            }

            const auto alphaIter = initialColor->find("alpha");
            if (alphaIter != initialColor->end())
            {
                desc.initialColor.startColorMin.w =
                    clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
                desc.initialColor.startColorMax.w =
                    clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
                desc.initialColor.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, initialColor);
            }

            desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
            desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
        }

        if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
        {
            const auto colorIter = colorOverLife->find("colorOverLife");
            if (colorIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
                desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
                desc.colorOverLife.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, colorOverLife);
            }

            const auto alphaIter = colorOverLife->find("alphaOverLife");
            if (alphaIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
                desc.colorOverLife.endColorMax.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
                desc.colorOverLife.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, colorOverLife);
            }

            Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
        }

        if (const json* subUvFrameOverLife = Find_ModuleData(emitter, "SubUVFrameOverLife"))
        {
            desc.subUVFrameOverLife.enabled = Read_Bool(*subUvFrameOverLife, "enabled", true);
            desc.subUVFrameOverLife.startFrame =
                Read_UInt(*subUvFrameOverLife, "startFrame", desc.subUVFrameOverLife.startFrame);
            desc.subUVFrameOverLife.endFrame =
                Read_UInt(*subUvFrameOverLife, "endFrame", desc.subUVFrameOverLife.endFrame);
            desc.subUVFrameOverLife.loop = Read_Bool(*subUvFrameOverLife, "loop", desc.subUVFrameOverLife.loop);
            Read_Enum(*subUvFrameOverLife, "playbackMode", desc.subUVFrameOverLife.playbackMode);
            desc.subUVFrameOverLife.framesPerSecond =
                max(0.f, Read_Float(*subUvFrameOverLife, "framesPerSecond", desc.subUVFrameOverLife.framesPerSecond));
            const bool hasRandomStartPhase = subUvFrameOverLife->find("randomStartPhase") != subUvFrameOverLife->end();
            desc.subUVFrameOverLife.randomStartPhase =
                Read_Bool(*subUvFrameOverLife, "randomStartPhase", desc.subUVFrameOverLife.randomStartPhase);
            if (!hasRandomStartPhase)
            {
                desc.subUVFrameOverLife.randomStartPhase =
                    Read_Bool(*subUvFrameOverLife, "perStripSubUVVariation", desc.subUVFrameOverLife.randomStartPhase);
            }
            desc.usePerStripSubUVVariation = desc.subUVFrameOverLife.randomStartPhase;

            const auto frameIndexIter = subUvFrameOverLife->find("frameIndex");
            if (frameIndexIter != subUvFrameOverLife->end())
                Fill_SubUVFrameCurvePayload(desc.subUVFrameOverLife, *frameIndexIter);
            if (desc.subUVFrameOverLife.playbackMode == SubUVFramePlaybackMode::RandomFrame ||
                desc.subUVFrameOverLife.randomStartPhase)
                desc.subUVRandomFrameSeed = Read_RandomSeedDesc(*subUvFrameOverLife);

            const uint32 frameCount = max(1u, desc.material.subUVRows * desc.material.subUVCols);
            const uint32 lastFrame = frameCount - 1u;
            desc.subUVFrameOverLife.startFrame = min(desc.subUVFrameOverLife.startFrame, lastFrame);
            desc.subUVFrameOverLife.endFrame = min(desc.subUVFrameOverLife.endFrame, lastFrame);

            if (0u == Read_UInt(*subUvFrameOverLife, "startFrame") &&
                0u == Read_UInt(*subUvFrameOverLife, "endFrame") &&
                frameCount > 1u)
                desc.subUVFrameOverLife.endFrame = lastFrame;
        }

        if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
            Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

        if (const json* beamEnvelope = Find_ModuleData(emitter, "BeamEnvelopeOverLife"))
            Fill_BeamEnvelopeOverLifePayload(desc.beamEnvelopeOverLife, *beamEnvelope);

        outDefinition.concreteDesc = desc;
        return true;
    }

    bool Build_MeshEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
    {
        outDefinition = EffectEmitterDefinition{};
        outDefinition.id = Read_UInt(emitter, "id");
        outDefinition.name = Read_String(emitter, "name");
        outDefinition.kind = EffectEmitterKind::Mesh;
        outDefinition.enabled = true;
        Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);

        MeshEmitterDesc desc{};
        desc.renderLayerOverride = outDefinition.renderLayerOverride;
        string assignedEffectMaterialGuid{};
        string assignedEffectMaterialPath{};
        bool hasAssignedMaterialInstance = false;
        json assignedMaterialPayload{};

        const auto typeDataIter = emitter.find("typeData");
        if (typeDataIter != emitter.end() && typeDataIter->is_object())
        {
            const auto dataIter = typeDataIter->find("data");
            if (dataIter != typeDataIter->end() && dataIter->is_object())
            {
                const json& typeData = *dataIter;
                desc.modelGuid = Read_String(typeData, "modelGuid", desc.modelGuid);
                desc.modelPath = Read_String(typeData, "modelPath", desc.modelPath);
                desc.useModelMaterials = Read_Bool(typeData, "useModelMaterials", desc.useModelMaterials);
                assignedEffectMaterialGuid = Read_String(typeData, "assignedEffectMaterialGuid", assignedEffectMaterialGuid);
                assignedEffectMaterialPath = Read_String(typeData, "assignedEffectMaterialPath", assignedEffectMaterialPath);
                hasAssignedMaterialInstance = Read_Bool(typeData, "hasAssignedMaterialInstance", hasAssignedMaterialInstance);
                if (const auto assignedMaterialIter = typeData.find("assignedMaterial");
                    assignedMaterialIter != typeData.end() && assignedMaterialIter->is_object())
                {
                    assignedMaterialPayload = *assignedMaterialIter;
                    hasAssignedMaterialInstance = true;
                }

                const auto previewScaleIter = typeData.find("previewScale");
                if (previewScaleIter != typeData.end())
                    Read_Vec3(*previewScaleIter, desc.previewScale);
            }
        }

        if (const json* required = Find_ModuleData(emitter, "Required"))
            Apply_RequiredModule(*required, outDefinition, desc);

        if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
        {
            Fill_MaterialScalarModulationPayload(desc.material.scalarModulation, *materialScalarModulation);
            Fill_CoreColorRgbModulationPayload(desc.material.coreColorRgbModulation, *materialScalarModulation);
            Fill_MaterialVec2ModulationPayload(desc.material.vec2Modulation, *materialScalarModulation);
        }

        if (hasAssignedMaterialInstance && assignedMaterialPayload.is_object())
            Apply_MaterialPayload(assignedMaterialPayload, desc.material);
        else
        {
            fs::path assignedMaterialResolvedPath{};
            if (!assignedEffectMaterialGuid.empty() && GAME != nullptr)
            {
                const AssetMeta* assetMeta = GAME->Find_AssetByGUID(assignedEffectMaterialGuid);
                if (assetMeta != nullptr && assetMeta->type == "EffectMaterial")
                {
                    const wstring resolvedPath = GAME->Resolve_AssetPath(assignedEffectMaterialGuid);
                    if (!resolvedPath.empty())
                        assignedMaterialResolvedPath = fs::path(resolvedPath).lexically_normal();
                }
            }

            if (assignedMaterialResolvedPath.empty() && !assignedEffectMaterialPath.empty())
            {
                assignedMaterialResolvedPath = fs::path(String::ToWString(assignedEffectMaterialPath));
                if (assignedMaterialResolvedPath.is_relative() && GAME != nullptr)
                    assignedMaterialResolvedPath = fs::path(GAME->Get_AssetRoot()) / assignedMaterialResolvedPath;
                assignedMaterialResolvedPath = assignedMaterialResolvedPath.lexically_normal();
            }

            if (!assignedMaterialResolvedPath.empty() && fs::exists(assignedMaterialResolvedPath))
            {
                ifstream materialFile{ assignedMaterialResolvedPath };
                json materialRoot{};
                try
                {
                    materialFile >> materialRoot;
                    if (materialRoot.is_object())
                        Apply_MaterialPayload(materialRoot, desc.material);
                }
                catch (...)
                {
                    LOG_WARN(
                        "EffectAssetRuntimeLoader failed to read assigned mesh effect material. emitter='{}', materialPath='{}'",
                        outDefinition.name,
                        To_LogPath(assignedMaterialResolvedPath)
                    );
                }
            }
        }

        if (const json* required = Find_ModuleData(emitter, "Required");
            required != nullptr && required->is_object())
        {
            if (const auto materialIter = required->find("material");
                materialIter != required->end() && materialIter->is_object())
            {
                Read_Enum(*materialIter, "blendMode", desc.material.blendMode);
                desc.material.alphaCutoff = Read_Float(*materialIter, "alphaCutoff", desc.material.alphaCutoff);
            }
        }

        if (const json* spawn = Find_ModuleData(emitter, "Spawn"))
            Apply_SpawnModule(*spawn, desc);

        if (const json* initialMeshSize = Find_ModuleData(emitter, "InitialMeshSize"))
        {
            const auto sizeIter = initialMeshSize->find("size");
            if (sizeIter != initialMeshSize->end())
            {
                desc.meshTransform.initialScaleEnabled = true;
                const Vec3 minSize = Evaluate_Vector3DistributionMin(*sizeIter, desc.meshTransform.initialScaleMin);
                const Vec3 maxSize = Evaluate_Vector3DistributionMax(*sizeIter, desc.meshTransform.initialScaleMax);
                desc.meshTransform.initialScaleMin = Vec3{ min(minSize.x, maxSize.x), min(minSize.y, maxSize.y), min(minSize.z, maxSize.z) };
                desc.meshTransform.initialScaleMax = Vec3{ max(minSize.x, maxSize.x), max(minSize.y, maxSize.y), max(minSize.z, maxSize.z) };
                desc.meshTransform.initialScaleSeed = Read_DistributionRandomSeedDesc(*sizeIter, initialMeshSize);
            }
        }

        if (const json* initialLocation = Find_ModuleData(emitter, "InitialLocation"))
        {
            const auto locationIter = initialLocation->find("location");
            if (locationIter != initialLocation->end())
            {
                const Vec3 minOffset = Evaluate_Vector3DistributionMin(*locationIter, desc.initialLocation.minOffset);
                const Vec3 maxOffset = Evaluate_Vector3DistributionMax(*locationIter, desc.initialLocation.maxOffset);
                desc.initialLocation.enabled = true;
                desc.initialLocation.minOffset = Vec3{
                    min(minOffset.x, maxOffset.x),
                    min(minOffset.y, maxOffset.y),
                    min(minOffset.z, maxOffset.z)
                };
                desc.initialLocation.maxOffset = Vec3{
                    max(minOffset.x, maxOffset.x),
                    max(minOffset.y, maxOffset.y),
                    max(minOffset.z, maxOffset.z)
                };
                desc.initialLocationSeed = Read_DistributionRandomSeedDesc(*locationIter, initialLocation);
            }
        }

        if (const json* sphereLocation = Find_ModuleData(emitter, "SphereLocation"))
        {
            desc.sphereLocation.enabled = true;

            const auto offsetIter = sphereLocation->find("offset");
            if (offsetIter != sphereLocation->end())
                Read_Vec3(*offsetIter, desc.sphereLocation.offset);

            desc.sphereLocation.radius = max(0.f, Read_Float(*sphereLocation, "radius", desc.sphereLocation.radius));
            Read_Enum(*sphereLocation, "spawnMode", desc.sphereLocation.mode);
            Read_Enum(*sphereLocation, "placementMode", desc.sphereLocation.placementMode);
            desc.sphereLocationSeed = Read_RandomSeedDesc(*sphereLocation);
        }

        if (const json* planeRadialLocation = Find_ModuleData(emitter, "PlaneRadialLocation"))
            Fill_PlaneRadialLocationPayload(desc.planeRadialLocation, *planeRadialLocation);

        if (const json* cylinderLocation = Find_ModuleData(emitter, "CylinderLocation"))
            Fill_CylinderLocationPayload(desc.cylinderLocation, *cylinderLocation);

        if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
        {
            const auto lifeTimeIter = lifetime->find("lifeTime");
            if (lifeTimeIter != lifetime->end())
            {
                desc.lifetime.lifeTime = Vec2{
                    Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                    Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
                };
                Fill_FloatCurvePayload(desc.lifetime.lifeTimeCurve, *lifeTimeIter, desc.lifetime.lifeTime.y);
                desc.lifetimeSeed = Read_DistributionRandomSeedDesc(*lifeTimeIter, lifetime);
            }
        }

        if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
        {
            const auto colorIter = initialColor->find("color");
            if (colorIter != initialColor->end())
            {
                desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
                desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
                desc.initialColor.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, initialColor);
            }

            const auto alphaIter = initialColor->find("alpha");
            if (alphaIter != initialColor->end())
            {
                desc.initialColor.startColorMin.w =
                    clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
                desc.initialColor.startColorMax.w =
                    clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
                desc.initialColor.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, initialColor);
            }

            desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
            desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
        }

        if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
        {
            const auto colorIter = colorOverLife->find("colorOverLife");
            if (colorIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
                desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
                desc.colorOverLife.colorSeed = Read_DistributionRandomSeedDesc(*colorIter, colorOverLife);
            }

            const auto alphaIter = colorOverLife->find("alphaOverLife");
            if (alphaIter != colorOverLife->end())
            {
                desc.colorOverLife.endColorMin.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
                desc.colorOverLife.endColorMax.w =
                    clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
                desc.colorOverLife.alphaSeed = Read_DistributionRandomSeedDesc(*alphaIter, colorOverLife);
            }

            Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
        }

        if (const json* meshSizeByLife = Find_ModuleData(emitter, "MeshSizeByLife"))
        {
            const auto scaleIter = meshSizeByLife->find("scaleOverLife");
            if (scaleIter != meshSizeByLife->end())
            {
                Fill_MeshVector3CurvePayload(desc.meshTransform.scaleByLife, *scaleIter, Vec3{ 1.f, 1.f, 1.f });
                if (!Read_Bool(*meshSizeByLife, "multiplyX", true))
                {
                    desc.meshTransform.scaleByLife.curveKeyValuesX = Vec4{ 1.f, 1.f, 1.f, 1.f };
                    desc.meshTransform.scaleByLife.curveKeyValuesXBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
                    desc.meshTransform.scaleByLife.curveKeyArriveTangentsX = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyArriveTangentsXBlock1 = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyLeaveTangentsX = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyLeaveTangentsXBlock1 = Vec4{};
                }
                if (!Read_Bool(*meshSizeByLife, "multiplyY", true))
                {
                    desc.meshTransform.scaleByLife.curveKeyValuesY = Vec4{ 1.f, 1.f, 1.f, 1.f };
                    desc.meshTransform.scaleByLife.curveKeyValuesYBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
                    desc.meshTransform.scaleByLife.curveKeyArriveTangentsY = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyArriveTangentsYBlock1 = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyLeaveTangentsY = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyLeaveTangentsYBlock1 = Vec4{};
                }
                if (!Read_Bool(*meshSizeByLife, "multiplyZ", true))
                {
                    desc.meshTransform.scaleByLife.curveKeyValuesZ = Vec4{ 1.f, 1.f, 1.f, 1.f };
                    desc.meshTransform.scaleByLife.curveKeyValuesZBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
                    desc.meshTransform.scaleByLife.curveKeyArriveTangentsZ = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyArriveTangentsZBlock1 = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyLeaveTangentsZ = Vec4{};
                    desc.meshTransform.scaleByLife.curveKeyLeaveTangentsZBlock1 = Vec4{};
                }
            }
        }

        if (const json* initialVelocity = Find_ModuleData(emitter, "InitialVelocity"))
        {
            desc.motion.enabled = true;
            desc.motion.initialVelocityEnabled = true;
            desc.motion.initialVelocityInWorldSpace = Read_Bool(*initialVelocity, "inWorldSpace", desc.motion.initialVelocityInWorldSpace);

            const auto velocityIter = initialVelocity->find("velocity");
            if (velocityIter != initialVelocity->end())
            {
                desc.motion.initialVelocityMin = Evaluate_Vector3DistributionMin(*velocityIter, desc.motion.initialVelocityMin);
                desc.motion.initialVelocityMax = Evaluate_Vector3DistributionMax(*velocityIter, desc.motion.initialVelocityMax);
                desc.motion.initialVelocitySeed = Read_DistributionRandomSeedDesc(*velocityIter, initialVelocity);
            }
        }

        if (const json* initialRadialVelocity = Find_ModuleData(emitter, "InitialRadialVelocity"))
        {
            desc.motion.enabled = true;
            desc.motion.initialRadialVelocityEnabled = true;
            desc.motion.initialRadialVelocityInWorldSpace =
                Read_Bool(*initialRadialVelocity, "inWorldSpace", desc.motion.initialRadialVelocityInWorldSpace);

            const auto radialPivotIter = initialRadialVelocity->find("radialPivot");
            if (radialPivotIter != initialRadialVelocity->end())
                Read_Vec3(*radialPivotIter, desc.motion.radialPivot);
            Read_Enum(
                *initialRadialVelocity,
                "centerDirectionMode",
                desc.motion.initialRadialVelocityCenterDirectionMode
            );

            const auto speedIter = initialRadialVelocity->find("speed");
            if (speedIter != initialRadialVelocity->end())
            {
                desc.motion.radialSpeed = Vec2{
                    Evaluate_FloatDistributionMin(*speedIter, desc.motion.radialSpeed.x),
                    Evaluate_FloatDistributionMax(*speedIter, desc.motion.radialSpeed.y)
                };
                desc.motion.initialRadialVelocitySeed = Read_DistributionRandomSeedDesc(*speedIter, initialRadialVelocity);
            }
        }

        if (const json* velocityCone = Find_ModuleData(emitter, "VelocityCone"))
        {
            desc.motion.enabled = true;
            Fill_VelocityConePayload(desc.motion, *velocityCone);
            if (const auto speedIter = velocityCone->find("speed"); speedIter != velocityCone->end())
                desc.motion.velocityConeSeed = Read_DistributionRandomSeedDesc(*speedIter, velocityCone);
        }

        if (const json* sourceMotionVelocity = Find_ModuleData(emitter, "SourceMotionVelocity"))
            Fill_SourceMotionVelocityPayload(desc.motion, *sourceMotionVelocity);

        if (const json* acceleration = Find_ModuleData(emitter, "Acceleration"))
        {
            desc.motion.enabled = true;
            desc.motion.accelerationInWorldSpace =
                Read_Bool(*acceleration, "inWorldSpace", desc.motion.accelerationInWorldSpace);
            Read_Enum(*acceleration, "timeBasis", desc.motion.accelerationTimeBasis);
            const auto accelerationIter = acceleration->find("acceleration");
            if (accelerationIter != acceleration->end())
            {
                desc.motion.accelerationMin = Evaluate_Vector3DistributionMin(*accelerationIter, desc.motion.accelerationMin);
                desc.motion.accelerationMax = Evaluate_Vector3DistributionMax(*accelerationIter, desc.motion.accelerationMax);
                Fill_AccelerationCurvePayload(desc.motion, *accelerationIter);
                desc.motion.accelerationSeed = Read_DistributionRandomSeedDesc(*accelerationIter, acceleration);
            }
        }

        if (const json* drag = Find_ModuleData(emitter, "Drag"))
        {
            desc.motion.enabled = true;
            const auto dragIter = drag->find("drag");
            if (dragIter != drag->end())
            {
                desc.motion.drag = Vec2{
                    max(0.f, Evaluate_FloatDistributionMin(*dragIter, desc.motion.drag.x)),
                    max(0.f, Evaluate_FloatDistributionMax(*dragIter, desc.motion.drag.y))
                };
                desc.motion.dragSeed = Read_DistributionRandomSeedDesc(*dragIter, drag);
            }
        }

        Apply_VelocityOverLifeOwnership(desc.motion, emitter);

        if (const json* orbitOverLife = Find_ModuleData(emitter, "OrbitOverLife"))
            Fill_OrbitOverLifePayload(desc.orbitOverLife, *orbitOverLife);

        if (const json* initialMeshRotation = Find_ModuleData(emitter, "InitialMeshRotation"))
        {
            const auto rotationIter = initialMeshRotation->find("rotationDegrees");
            if (rotationIter != initialMeshRotation->end())
            {
                const Vec3 minRotation = Evaluate_Vector3DistributionMin(*rotationIter, Vec3{});
                const Vec3 maxRotation = Evaluate_Vector3DistributionMax(*rotationIter, Vec3{});
                desc.meshTransform.rotationEnabled = true;
                desc.meshTransform.initialRotationDegreesMin = Vec3{
                    min(minRotation.x, maxRotation.x), min(minRotation.y, maxRotation.y), min(minRotation.z, maxRotation.z)
                };
                desc.meshTransform.initialRotationDegreesMax = Vec3{
                    max(minRotation.x, maxRotation.x), max(minRotation.y, maxRotation.y), max(minRotation.z, maxRotation.z)
                };
                desc.meshTransform.initialRotationSeed = Read_DistributionRandomSeedDesc(*rotationIter, initialMeshRotation);
            }
        }

        if (const json* sphereRadialOrientation = Find_ModuleData(emitter, "SphereRadialOrientation"))
            Fill_SphereRadialOrientationPayload(desc.sphereRadialOrientation, *sphereRadialOrientation);
        if (const json* planeRadialOrientation = Find_ModuleData(emitter, "PlaneRadialOrientation"))
            Fill_PlaneRadialOrientationPayload(desc.planeRadialOrientation, *planeRadialOrientation);
        if (const json* cylinderOrientation = Find_ModuleData(emitter, "CylinderOrientation"))
            Fill_CylinderOrientationPayload(desc.cylinderOrientation, *cylinderOrientation);

        if (const json* meshRotationOverLife = Find_ModuleData(emitter, "MeshRotationOverLife"))
        {
            const auto rotationIter = meshRotationOverLife->find("rotationOverLife");
            if (rotationIter != meshRotationOverLife->end())
            {
                desc.meshTransform.rotationEnabled = true;
                Fill_MeshVector3CurvePayload(desc.meshTransform.rotationByLife, *rotationIter, Vec3{});
            }
        }

        if (const json* meshDirectionAlign = Find_ModuleData(emitter, "MeshDirectionAlignOverLife"))
            Fill_MeshDirectionAlignPayload(desc.meshDirectionAlign, *meshDirectionAlign);

        if (const json* initialMeshRotationRate = Find_ModuleData(emitter, "InitialMeshRotationRate"))
        {
            const auto rotationRateIter = initialMeshRotationRate->find("rotationRateDegrees");
            if (rotationRateIter != initialMeshRotationRate->end())
            {
                const Vec3 minRate = Evaluate_Vector3DistributionMin(*rotationRateIter, Vec3{});
                const Vec3 maxRate = Evaluate_Vector3DistributionMax(*rotationRateIter, Vec3{});
                desc.meshTransform.rotationEnabled = true;
                desc.meshTransform.initialAngularVelocityDegreesMin = Vec3{ min(minRate.x, maxRate.x), min(minRate.y, maxRate.y), min(minRate.z, maxRate.z) };
                desc.meshTransform.initialAngularVelocityDegreesMax = Vec3{ max(minRate.x, maxRate.x), max(minRate.y, maxRate.y), max(minRate.z, maxRate.z) };
                desc.meshTransform.initialAngularVelocitySeed = Read_DistributionRandomSeedDesc(*rotationRateIter, initialMeshRotationRate);
                desc.meshTransform.initialAngularVelocityInWorldSpace = Read_Bool(
                    *initialMeshRotationRate,
                    "inWorldSpace",
                    desc.meshTransform.initialAngularVelocityInWorldSpace
                );
            }
        }

        if (const json* meshRotationRateScaleByLife = Find_ModuleData(emitter, "MeshRotationRateScaleByLife"))
        {
            const auto scaleIter = meshRotationRateScaleByLife->find("scaleOverLife");
            if (scaleIter != meshRotationRateScaleByLife->end())
            {
                desc.meshTransform.rotationEnabled = true;
                Fill_MeshVector3CurvePayload(desc.meshTransform.angularVelocityScaleByLife, *scaleIter, Vec3{ 1.f, 1.f, 1.f });
            }
        }

        if (desc.modelGuid.empty() && desc.modelPath.empty())
        {
            LOG_WARN(
                "EffectAssetRuntimeLoader skipped mesh emitter with empty model. emitter='{}'",
                outDefinition.name
            );
            return false;
        }

        bool hasResolvableModel = false;
        if (!desc.modelGuid.empty() && GAME != nullptr)
        {
            const AssetMeta* assetMeta = GAME->Find_AssetByGUID(desc.modelGuid);
            const wstring resolvedPath = assetMeta != nullptr && assetMeta->type == "Model"
                                         ? GAME->Resolve_AssetPath(desc.modelGuid)
                                         : wstring{};
            hasResolvableModel = !resolvedPath.empty() && fs::exists(resolvedPath);
        }

        if (!hasResolvableModel && !desc.modelPath.empty())
        {
            fs::path modelPath = String::ToWString(desc.modelPath);
            if (modelPath.is_relative() && GAME != nullptr)
                modelPath = fs::path(GAME->Get_AssetRoot()) / modelPath;

            hasResolvableModel = fs::exists(modelPath.lexically_normal());
        }

        if (!hasResolvableModel)
        {
            LOG_WARN(
                "EffectAssetRuntimeLoader skipped mesh emitter with missing model. emitter='{}', modelGuid='{}', modelPath='{}'",
                outDefinition.name,
                desc.modelGuid,
                desc.modelPath
            );
            return false;
        }

        outDefinition.concreteDesc = desc;
        return true;
    }
}

NS_END
