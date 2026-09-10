#include "EffectEmitterRegistration.h"

#include "ComputeGeneratedBeamEmitter.h"
#include "ComputeSourceHistoryRibbonEmitter.h"
#include "ComputeSourceHistorySpriteTrailEmitter.h"
#include "ComputeSpriteEmitter.h"
#include "ComputeTrailEmitter.h"
#include "EffectEmitterFactory.h"
#include "GameInstance.h"

NS_BEGIN(Client)

namespace EffectAssetRuntimeLoad
{
    constexpr auto kComputeSourceHistoryRibbonEmitterPrototypeTag = L"GameObject_Effect_ComputeSourceHistoryRibbonEmitter";
    constexpr auto kComputeSourceHistorySpriteTrailEmitterPrototypeTag = L"GameObject_Effect_ComputeSourceHistorySpriteTrailEmitter";
    constexpr auto kComputeBeamEmitterPrototypeTag = L"GameObject_Effect_ComputeGeneratedBeamEmitter";

    Shared<EffectEmitter> Create_ClientEffectEmitter(const EffectEmitterDefinition& emitterDefinition)
    {
        switch (emitterDefinition.kind)
        {
        case EffectEmitterKind::Sprite:
        {
            const auto* desc = get_if<ComputeSpriteEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeSpriteEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    L"GameObject_Effect_ComputeSpriteEmitter",
                    &concreteDesc
                )
            );

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }
        case EffectEmitterKind::Trail:
        {
            const auto* desc = get_if<ComputeTrailEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeTrailEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    L"GameObject_Effect_ComputeTrailEmitter",
                    &concreteDesc
                )
            );

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }
        case EffectEmitterKind::Ribbon:
        {
            const auto* desc = get_if<ComputeRibbonEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeRibbonEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    kComputeSourceHistoryRibbonEmitterPrototypeTag,
                    &concreteDesc
                )
            );
            if (nullptr == clonedObject)
            {
                LOG_WARN(
                    "ComputeSourceHistoryRibbonEmitter prototype missing: failed to clone source history ribbon emitter prototype. emitter='{}'",
                    emitterDefinition.name
                );
            }

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }
        case EffectEmitterKind::SourceHistorySpriteTrail:
        {
            const auto* desc = get_if<ComputeSourceHistorySpriteTrailEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeSourceHistorySpriteTrailEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    kComputeSourceHistorySpriteTrailEmitterPrototypeTag,
                    &concreteDesc
                )
            );
            if (nullptr == clonedObject)
            {
                LOG_WARN(
                    "ComputeSourceHistorySpriteTrailEmitter prototype missing: failed to clone source history sprite trail emitter prototype. emitter='{}'",
                    emitterDefinition.name
                );
            }

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }
        case EffectEmitterKind::Beam:
        {
            const auto* desc = get_if<ComputeBeamEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            ComputeBeamEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    kComputeBeamEmitterPrototypeTag,
                    &concreteDesc
                )
            );
            if (nullptr == clonedObject)
            {
                LOG_WARN(
                    "ComputeGeneratedBeamEmitter prototype missing: failed to clone beam emitter prototype. emitter='{}'",
                    emitterDefinition.name
                );
            }

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }
        case EffectEmitterKind::Mesh:
        {
            const auto* desc = get_if<MeshEmitterDesc>(&emitterDefinition.concreteDesc);
            if (nullptr == desc)
                return nullptr;

            MeshEmitterDesc concreteDesc = *desc;
            concreteDesc.renderLayerOverride = emitterDefinition.renderLayerOverride;
            const Shared<GameObject> clonedObject = dynamic_pointer_cast<GameObject>(
                GAME->Clone_Prototype(
                    Prototype::GameObject,
                    ETOI(LevelType::Static),
                    L"GameObject_Effect_MeshEmitter",
                    &concreteDesc
                )
            );
            if (nullptr == clonedObject)
            {
                LOG_WARN(
                    "MeshEmitter prototype missing: failed to clone mesh emitter prototype. emitter='{}'",
                    emitterDefinition.name
                );
            }

            return dynamic_pointer_cast<EffectEmitter>(clonedObject);
        }
        default:
            return nullptr;
        }
    }
}

void Register_ClientEffectEmitters()
{
    EffectEmitterFactory::Register_CreateEmitterFn(&EffectAssetRuntimeLoad::Create_ClientEffectEmitter);
}

NS_END
