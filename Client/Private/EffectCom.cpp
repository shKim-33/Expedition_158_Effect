#include "EffectCom.h"

#include "ClientInstance.h"
#include "Camera.h"
#include "ContainerObject.h"
#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "PlayableParts.h"
#include "Player_WorldBase.h"
#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"
#include "SourcePointSampleProvider_ByPart.h"
#include "TrailAnchor_Asset.h"
#include "TrailSampleProvider_ByPart.h"
#include "Mon_Dualliste_Sword_L.h"
#include "Mon_Dualliste_Sword_R.h"
#include "Mon_PotatoBag_Weapon.h"
#include "Mon_Simon_Weapon.h"
#include "Monster_Part.h"
#include "Weapon.h"

NS_BEGIN(Client)

namespace
{
string Format_SourceKeyVec3(const Vec3& value)
{
    return format("{:.4f},{:.4f},{:.4f}", value.x, value.y, value.z);
}

bool Is_SameTrailAnchor(const TrailAnchorAsset& lhs, const TrailAnchorAsset& rhs)
{
    return lhs.modelGuid == rhs.modelGuid &&
        lhs.base.boneName == rhs.base.boneName &&
        lhs.tip.boneName == rhs.tip.boneName &&
        Format_SourceKeyVec3(lhs.base.localPosition) == Format_SourceKeyVec3(rhs.base.localPosition) &&
        Format_SourceKeyVec3(lhs.tip.localPosition) == Format_SourceKeyVec3(rhs.tip.localPosition);
}

EffectHistorySourceGroupKey Build_TrailPairHistoryKey(
    const wstring& partTag,
    const TrailAnchorAsset& trailAnchorAsset,
    uint64 trailNotifyToken)
{
    EffectHistorySourceGroupKey key{};
    key.kind = EffectHistorySourceGroupKind::TrailPairHistory;
    key.stableId = format(
        "TrailPair|part={}|token={}|model={}|base={}:{}|tip={}:{}",
        String::ToString(partTag),
        trailNotifyToken,
        trailAnchorAsset.modelGuid,
        trailAnchorAsset.base.boneName,
        Format_SourceKeyVec3(trailAnchorAsset.base.localPosition),
        trailAnchorAsset.tip.boneName,
        Format_SourceKeyVec3(trailAnchorAsset.tip.localPosition)
    );
    return key;
}

EffectHistorySourceGroupKey Build_SourcePointHistoryKey(
    const wstring& partTag,
    SourceHistorySourceBindingMode sourceBindingMode,
    const string& socketName,
    const string& sourceHistoryAnchorName,
    const Vec3& sourceHistoryLocalOffset)
{
    EffectHistorySourceGroupKey key{};
    key.kind = EffectHistorySourceGroupKind::SourcePointHistory;
    key.stableId = format(
        "SourcePoint|part={}|binding={}|socket={}|anchor={}|offset={}",
        String::ToString(partTag),
        static_cast<int32>(sourceBindingMode),
        socketName,
        sourceHistoryAnchorName,
        Format_SourceKeyVec3(sourceHistoryLocalOffset)
    );
    return key;
}

Quat Resolve_NormalizedWorldRotation(const Matrix& worldMatrix)
{
    Matrix decomposableWorldMatrix = worldMatrix;
    Vec3 scale = Vec3::One;
    Vec3 position = Vec3::Zero;
    Quat rotation = Quat::Identity;
    if (!decomposableWorldMatrix.Decompose(scale, rotation, position) || rotation.LengthSquared() <= 0.000001f)
        return Quat::Identity;

    rotation.Normalize();
    return rotation;
}

class TrailTipSourcePointSampleProvider final : public IEffectSourcePointSampleProvider
{
public:
    explicit TrailTipSourcePointSampleProvider(const Weak<IEffectTrailSampleProvider>& trailProvider)
        : _trailProvider(trailProvider)
    {
    }

    bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const override
    {
        const Shared<IEffectTrailSampleProvider> trailProvider = _trailProvider.lock();
        if (trailProvider == nullptr)
            return false;

        EffectTrailSample trailSample{};
        if (!trailProvider->Try_GetTrailSample(trailSample))
            return false;

        outSample.worldPosition = trailSample.tipWorldPosition;
        outSample.worldRotation = trailSample.tipWorldRotation;
        return true;
    }

private:
    Weak<IEffectTrailSampleProvider> _trailProvider{};
};

Vec3 Resolve_CameraDirectionFromAnchor(const Vec3& anchorPosition, const Vec3& fallback)
{
    if (GAME != nullptr)
    {
        const ObjectType cameraTypes[] = {
            ObjectType::BattleTurnCamera,
            ObjectType::TargetCamera,
            ObjectType::CinematicCamera,
            ObjectType::FreeCamera,
        };

        for (const ObjectType cameraType : cameraTypes)
        {
            const Shared<Camera> camera = GAME->Get_CameraByType(cameraType);
            if (camera == nullptr || camera->Get_Transform() == nullptr)
                continue;

            Vec3 direction = camera->Get_Transform()->Get_WorldPosition() - anchorPosition;
            if (direction.LengthSquared() <= 0.000001f)
                continue;

            direction.Normalize();
            return direction;
        }
    }

    return fallback;
}

float Resolve_TrailDrainDuration(const Shared<const EffectDefinition>& definition)
{
    constexpr float fallbackDuration = 0.25f;
    if (definition == nullptr)
        return fallbackDuration;

    float duration = fallbackDuration;
    for (const EffectEmitterDefinition& emitter : definition->emitters)
    {
        if (!emitter.enabled || emitter.kind != EffectEmitterKind::Trail)
            continue;

        const ComputeTrailEmitterDesc* trailDesc = get_if<ComputeTrailEmitterDesc>(&emitter.concreteDesc);
        if (trailDesc == nullptr)
            continue;

        duration = max(duration, trailDesc->segmentLifetime);
    }

    return max(0.001f, duration);
}

float Resolve_SourceHistoryDrainDuration(const Shared<const EffectDefinition>& definition)
{
    constexpr float fallbackDuration = 0.5f;
    if (definition == nullptr)
        return fallbackDuration;

    float duration = fallbackDuration;
    for (const EffectEmitterDefinition& emitter : definition->emitters)
    {
        if (!emitter.enabled)
            continue;

        if (emitter.kind == EffectEmitterKind::Ribbon)
        {
            const ComputeRibbonEmitterDesc* ribbonDesc = get_if<ComputeRibbonEmitterDesc>(&emitter.concreteDesc);
            if (ribbonDesc == nullptr || ribbonDesc->sourceMode != EffectSourceHistoryRibbonSourceMode::SelfRoot)
                continue;

            duration = max(duration, ribbonDesc->sampleLifetime);
        }
        else if (emitter.kind == EffectEmitterKind::SourceHistorySpriteTrail)
        {
            const ComputeSourceHistorySpriteTrailEmitterDesc* spriteTrailDesc =
                get_if<ComputeSourceHistorySpriteTrailEmitterDesc>(&emitter.concreteDesc);
            if (spriteTrailDesc == nullptr || spriteTrailDesc->sourceMode != EffectSourceHistoryRibbonSourceMode::SelfRoot)
                continue;

            float spriteDuration =
                spriteTrailDesc->sampleLifetime +
                spriteTrailDesc->stampLifetime;
            if (spriteTrailDesc->pathReplay.enabled)
            {
                spriteDuration +=
                    spriteTrailDesc->pathReplay.delayTime +
                    spriteTrailDesc->pathReplay.drainDuration;
            }

            duration = max(duration, spriteDuration);
        }
    }

    return max(0.001f, duration + 0.1f);
}

Shared<ModelCom> Resolve_TrailPartModel(const PartObject* partObject)
{
    if (partObject == nullptr)
        return nullptr;

    if (const Weapon* weaponPart = dynamic_cast<const Weapon*>(partObject))
        return weaponPart->Get_Model();

    if (const Client::Monster_Part* monsterPart = dynamic_cast<const Client::Monster_Part*>(partObject))
        return monsterPart->Get_Model();

    if (const PlayableParts* playablePart = dynamic_cast<const PlayableParts*>(partObject))
        return playablePart->Get_Model();

    return nullptr;
}

const Matrix* Resolve_TrailPartSocketMatrix(const PartObject* partObject, const string& socketName)
{
    if (partObject == nullptr)
        return nullptr;

    if (const Weapon* weaponPart = dynamic_cast<const Weapon*>(partObject))
        return weaponPart->Get_SocketBoneMatrixPtr(socketName);

    if (const Client::Monster_Part* monsterPart = dynamic_cast<const Client::Monster_Part*>(partObject))
        return monsterPart->Get_SocketBoneMatrixPtr(socketName);

    if (const PlayableParts* playablePart = dynamic_cast<const PlayableParts*>(partObject))
        return playablePart->Get_SocketBoneMatrixPtr(socketName);

    return nullptr;
}

wstring Resolve_DefaultTrailEffectName(const PartObject* partObject)
{
    if (partObject == nullptr)
        return {};

    if (const Weapon* weaponPart = dynamic_cast<const Weapon*>(partObject))
        return weaponPart->Get_TrailEffectName();

    if (const Client::Mon_Simon_Weapon* simonWeapon = dynamic_cast<const Client::Mon_Simon_Weapon*>(partObject))
        return simonWeapon->Get_TrailEffectName();

    if (const Client::Mon_Dualliste_Sword_L* duallisteSwordL = dynamic_cast<const Client::Mon_Dualliste_Sword_L*>(partObject))
        return duallisteSwordL->Get_TrailEffectName();

    if (const Client::Mon_Dualliste_Sword_R* duallisteSwordR = dynamic_cast<const Client::Mon_Dualliste_Sword_R*>(partObject))
        return duallisteSwordR->Get_TrailEffectName();

    if (const Client::Mon_PotatoBag_Weapon* potatoBagWeapon = dynamic_cast<const Client::Mon_PotatoBag_Weapon*>(partObject))
        return potatoBagWeapon->Get_TrailEffectName();

    return {};
}
}

IMPLEMENT_REFLECTION(EffectCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "EffectCom";
    info.displayName = "EffectCom";
    info.category = "Component";

    PROPERTY_BOOL("Auto Play", _inspectorAutoPlayEffect);
    PROPERTY_WSTRING("Effect Name", _inspectorEffectName);
    PROPERTY_VEC3("Position", _inspectorLocalOffset, 0.01f);
    PROPERTY_VEC3("Rotation", _inspectorLocalRotation, 0.1f);
    PROPERTY_VEC3("Scale", _inspectorScale, 0.01f);
    PROPERTY_BOOL("Follow Target", _inspectorFollowTarget);
    PROPERTY_BOOL("Manual Release", _inspectorRequiresManualRelease);

    return true;
}

EffectCom::EffectCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

EffectCom::~EffectCom()
{
    Stop_AllAttachedEffects();
}

HRESULT EffectCom::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT EffectCom::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    _switchDissolve = {};
    _previewRim = {};
    _attachedEffects.clear();
    _vanishComplete = false;
    _inspectorEffectPlayed = false;
    _attachedEffectRevealAlphaMultiplier = 1.f;
    _attachedEffectRevealAlphaErosion = 0.f;

    return S_OK;
}

void EffectCom::BeginPlay()
{
    __super::BeginPlay();
    Play_InspectorEffectIfEnabled();
}

void EffectCom::Update(float timeDelta)
{
    const bool wasDissolving = _switchDissolve.enabled;
    const bool wasVanish = !_switchDissolve.reveal;

    _switchDissolve.Update(timeDelta);

    if (_vanishComplete &&
        wasDissolving &&
        wasVanish &&
        !_switchDissolve.enabled)
    {
        if (const Shared<GameObject> owner = Get_Owner())
        {
            owner->Set_Visible(false);

            if (const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(owner))
            {
                for (const auto& [partTag, partObject] : container->Get_PartObjects())
                {
                    UNREFERENCED_PARAMETER(partTag);

                    if (partObject == nullptr)
                        continue;

                    if (const Shared<EffectCom> partEffectCom = partObject->Get_Component<EffectCom>())
                        partEffectCom->Stop_AllAttachedEffects();
                }
            }
        }

        Stop_AllAttachedEffects();
        _vanishComplete = false;
    }

    Update_AttachedEffects(timeDelta);
}

void EffectCom::Tick_AttachedEffectInstances(float timeDelta)
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        if (nullptr == attached.poolHandle.effectObject)
            continue;

        attached.poolHandle.effectObject->Try_BeginPlay();
        attached.poolHandle.effectObject->Priority_Update(timeDelta);
        attached.poolHandle.effectObject->Update(timeDelta);
        attached.poolHandle.effectObject->Late_Update(timeDelta);
    }
}

void EffectCom::Capture_AttachedEffectPauseStates(vector<AttachedEffectPauseState>& outStates)
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        const Shared<EffectInstance> effect =
            dynamic_pointer_cast<EffectInstance>(attached.poolHandle.effectObject);
        if (effect == nullptr)
            continue;

        const bool alreadyCaptured = ranges::any_of(
            outStates,
            [&effect](const AttachedEffectPauseState& state)
            {
                return state.effect == effect;
            }
        );

        if (alreadyCaptured)
            continue;

        AttachedEffectPauseState state{};
        state.effect = effect;
        state.wasPaused = effect->Is_PlaybackPaused();

        effect->Set_PlaybackPaused(true);
        outStates.push_back(state);
    }
}

void EffectCom::Restore_AttachedEffectPauseStates(vector<AttachedEffectPauseState>& states)
{
    for (AttachedEffectPauseState& state : states)
    {
        if (state.effect == nullptr)
            continue;

        state.effect->Set_PlaybackPaused(state.wasPaused);
    }

    states.clear();
}

EffectCom::AttachedEffectDebugSummary EffectCom::Get_AttachedEffectDebugSummary() const
{
    AttachedEffectDebugSummary summary{};
    summary.totalCount = static_cast<uint32>(_attachedEffects.size());

    for (const AttachedEffect& attached : _attachedEffects)
    {
        if (attached.poolHandle.effectObject != nullptr)
            ++summary.activeCount;

        if (attached.role == AttachedEffectRole::Trail)
        {
            ++summary.trailCount;
            if (attached.isTrailDraining)
                ++summary.trailDrainingCount;
        }
        else if (attached.role == AttachedEffectRole::SourceHistoryPoint)
        {
            ++summary.sourceHistoryCount;
            if (attached.isSourceHistoryDraining)
                ++summary.sourceHistoryDrainingCount;
        }
    }

    return summary;
}

void EffectCom::Start_SwitchDissolve(float durationSec, const string& noiseName)
{
    const uint32 noiseHandle = GAME->Get_Handle(noiseName);

    _vanishComplete = false;
    _switchDissolve.Start_Reveal(durationSec, noiseHandle, 0.08f, 0.02f, Vec3(0.10f, 0.10f, 0.10f));
}

void EffectCom::Start_VanishDissolve(float durationSec, const string& noiseName, bool vanishComplete)
{
    const uint32 noiseHandle = GAME->Get_Handle(noiseName);

    _vanishComplete = vanishComplete;
    _switchDissolve.Start_Vanish(durationSec, noiseHandle, 0.08f, 0.02f, Vec3(0.10f, 0.10f, 0.10f));
}

void EffectCom::Set_PreviewRim(const Vec3& color, float intensity, float power, float hairIntensity)
{
    _previewRim.Start(color, intensity, power, hairIntensity);
}

void EffectCom::Clear_PreviewRim()
{
    _previewRim.Stop();
}

void EffectCom::Stop_AllAttachedEffects()
{
    for (AttachedEffect& attached : _attachedEffects)
        Release_AttachedEffect(attached);

    Prune_ReleasedAttachedEffects();
}

HRESULT EffectCom::Bind_DrawFeatures(ShaderCom* shader, uint32 supportFeatureMask)
{
    CHECK_NULL_THROTTLED(shader, 60, E_FAIL);

    DrawFeatureCB drawFeatureCB{};

    constexpr uint32 featureTextureCount =
        static_cast<uint32>(ShaderSRV::DrawFeatureTextureSlotNames.size());

    array<ShaderResourceView*, featureTextureCount> featureSRVs{};
    drawFeatureCB.supportFeatureMask = supportFeatureMask;

    if (_switchDissolve.enabled == true &&
        (supportFeatureMask & DrawFeatureFlagDissolve) != 0u)
    {
        drawFeatureCB.activeFeatureMask |= DrawFeatureFlagDissolve;
        drawFeatureCB.param0.x = _switchDissolve.progress;
        drawFeatureCB.param0.y = _switchDissolve.edgeWidth;
        drawFeatureCB.param0.z = _switchDissolve.edgeSoftness;
        drawFeatureCB.param0.w = _switchDissolve.reveal ? 1.f : 0.f;
        drawFeatureCB.param1 = Vec4(_switchDissolve.edgeColor.x, _switchDissolve.edgeColor.y, _switchDissolve.edgeColor.z, 0.45f);

        if (ShaderResourceView* noiseSRV = GAME->Get_SRV(_switchDissolve.noiseHandle))
        {
            featureSRVs[DrawFeatureTexDissolveNoise] = noiseSRV;
            drawFeatureCB.textureSlotMask |= 1u << DrawFeatureTexDissolveNoise;
        }
    }

    if (_previewRim.enabled == true &&
        (supportFeatureMask & DrawFeatureFlagPreviewRim) != 0u)
    {
        drawFeatureCB.activeFeatureMask |= DrawFeatureFlagPreviewRim;
        drawFeatureCB.param2 = Vec4(_previewRim.color.x, _previewRim.color.y, _previewRim.color.z, _previewRim.intensity);
        drawFeatureCB.param3 = Vec4(_previewRim.power, _previewRim.hairIntensity, 0.f, 0.f);
    }

    return shader->Bind_DrawFeatures(drawFeatureCB, featureSRVs.data(), featureTextureCount);
}

HRESULT EffectCom::Bind_DrawFeatures(const Shared<GameObject>& owner, ShaderCom* shader, uint32 supportFeatureMask)
{
    CHECK_NULL_THROTTLED(shader, 60, E_FAIL);

    Shared<EffectCom> effectCom{};

    if (owner)
        effectCom = owner->Get_Component<EffectCom>();

    if (effectCom)
        return effectCom->Bind_DrawFeatures(shader, supportFeatureMask);

    return shader->Reset_DrawFeatures();
}

Shared<EffectCom> EffectCom::Find_FromNotifyOwner(GameObject* notifyOwner)
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (Shared<EffectCom> effectCom = current->Get_Component<EffectCom>())
            return effectCom;

        current = current->Get_Owner();
    }

    return nullptr;
}

Shared<EffectCom> EffectCom::Find_FromNotifyOwner(GameObject* notifyOwner, EffectPlayTarget target)
{
    if (target == EffectPlayTarget::OwnerRoot)
        return Find_FromNotifyOwner(notifyOwner);

    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;
    while (current)
    {
        if (dynamic_pointer_cast<ContainerObject>(current))
        {
            if (Shared<EffectCom> effectCom = current->Get_Component<EffectCom>())
                return effectCom;
        }

        current = current->Get_Owner();
    }

    return Find_FromNotifyOwner(notifyOwner);
}

EffectCom::InspectorEffectSettings EffectCom::Get_InspectorEffectSettings() const
{
    InspectorEffectSettings settings{};
    settings.autoPlay = _inspectorAutoPlayEffect;
    settings.effectName = _inspectorEffectName;
    settings.target = static_cast<EffectPlayTarget>(_inspectorEffectTarget);
    settings.partTag = _inspectorPartTag;
    settings.socketName = _inspectorSocketName;
    settings.layerTag = _inspectorLayerTag;
    settings.localOffset = _inspectorLocalOffset;
    settings.localRotation = _inspectorLocalRotation;
    settings.scale = _inspectorScale;
    settings.followTarget = _inspectorFollowTarget;
    settings.requiresManualRelease = _inspectorRequiresManualRelease;
    return settings;
}

void EffectCom::Set_InspectorEffectSettings(const InspectorEffectSettings& settings)
{
    _inspectorAutoPlayEffect = settings.autoPlay;
    _inspectorEffectName = settings.effectName;
    _inspectorEffectTarget = static_cast<int>(settings.target);
    _inspectorPartTag = settings.partTag;
    _inspectorSocketName = settings.socketName;
    _inspectorLayerTag = settings.layerTag;
    _inspectorLocalOffset = settings.localOffset;
    _inspectorLocalRotation = settings.localRotation;
    _inspectorScale = settings.scale;
    _inspectorFollowTarget = settings.followTarget;
    _inspectorRequiresManualRelease = settings.requiresManualRelease;
    _inspectorEffectPlayed = false;
}

void EffectCom::Set_InspectorEffectLocalOffset(const Vec3& localOffset)
{
    _inspectorLocalOffset = localOffset;

    const EffectPlayTarget inspectorTarget = static_cast<EffectPlayTarget>(_inspectorEffectTarget);
    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.role != AttachedEffectRole::Normal)
            continue;
        if (attached.effectName != _inspectorEffectName ||
            attached.target != inspectorTarget ||
            attached.partTag != _inspectorPartTag ||
            attached.socketName != _inspectorSocketName)
            continue;

        attached.localOffset = localOffset;
        if (attached.poolHandle.effectObject == nullptr)
            continue;

        Vec3 targetWorldPosition{};
        Quat targetWorldRotation{ Quat::Identity };
        Vec3 targetWorldScale{};
        if (!Try_ResolveEffectWorldTransform(
            attached.target,
            attached.partTag,
            attached.socketName,
            attached.localOffset,
            attached.localRotation,
            attached.scale,
            attached.transformInheritance,
            attached.localOffsetZTowardCamera,
            targetWorldPosition,
            targetWorldRotation,
            targetWorldScale
        ))
            continue;

        const Shared<TransformCom> effectTransform = attached.poolHandle.effectObject->Get_Transform();
        CHECK_NULL(effectTransform);

        effectTransform->Set_WorldPosition(targetWorldPosition);
        effectTransform->Set_WorldRotationQuaternion(targetWorldRotation);
        effectTransform->Set_Scale(targetWorldScale);
    }
}

void EffectCom::Set_AttachedEffectMaterialReveal(float alphaMultiplier, float alphaErosion)
{
    _attachedEffectRevealAlphaMultiplier = clamp(alphaMultiplier, 0.f, 1.f);
    _attachedEffectRevealAlphaErosion = clamp(alphaErosion, 0.f, 1.f);

    for (AttachedEffect& attached : _attachedEffects)
        Apply_AttachedEffectMaterialReveal(attached.poolHandle.effectObject);
}

void EffectCom::Set_AttachedEffectMaterialTintOverride(
    const vector<uint32>& emitterIds,
    const Vec4& targetTint,
    float strength)
{
    const float clampedStrength = clamp(strength, 0.f, 1.f);

    for (AttachedEffect& attached : _attachedEffects)
    {
        const Shared<EffectInstance> effectInstance =
            dynamic_pointer_cast<EffectInstance>(attached.poolHandle.effectObject);
        if (effectInstance == nullptr)
            continue;

        effectInstance->Set_MaterialTintOverride(emitterIds, targetTint, clampedStrength);
    }
}

HRESULT EffectCom::Play_InspectorEffect()
{
    if (_inspectorEffectName.empty())
        return E_FAIL;

    EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = _inspectorEffectName;
    desc.target = static_cast<EffectPlayTarget>(_inspectorEffectTarget);
    desc.partTag = _inspectorPartTag;
    desc.socketName = _inspectorSocketName;
    desc.layerTag = _inspectorLayerTag;
    desc.localOffset = _inspectorLocalOffset;
    desc.localRotation = _inspectorLocalRotation;
    desc.scale = _inspectorScale;
    desc.followTarget = _inspectorFollowTarget;
    desc.requiresManualRelease = _inspectorRequiresManualRelease;

    return Play_Effect(desc);
}

HRESULT EffectCom::Play_Effect(const EffectPlayDesc& desc)
{
    return Play_Effect(desc, nullptr);
}

HRESULT EffectCom::Play_Effect(const EffectPlayDesc& desc, EffectInstancePool::AttachedHandle* outHandle)
{
    if (desc.effectName.empty() || desc.layerTag.empty())
    {
        LOG_WARN(
            "[EffectCom] Play_Effect failed: empty effectName or layerTag. effect={}, layer={}",
            String::ToString(desc.effectName),
            String::ToString(desc.layerTag)
        );
        return E_FAIL;
    }

    Vec3 spawnWorldPosition{};
    Quat spawnWorldRotation{ Quat::Identity };
    Vec3 spawnWorldScale{};
    if (!Try_ResolveEffectWorldTransform(
        desc.target,
        desc.partTag,
        desc.socketName,
        desc.localOffset,
        desc.localRotation,
        desc.scale,
        desc.transformInheritance,
        desc.localOffsetZTowardCamera,
        spawnWorldPosition,
        spawnWorldRotation,
        spawnWorldScale
    ))
        return E_FAIL;

    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    if (effectPool == nullptr)
    {
        LOG_WARN(
            "[EffectCom] Play_Effect failed: effect pool is null. effect={}, layer={}",
            String::ToString(desc.effectName),
            String::ToString(desc.layerTag)
        );
        return E_FAIL;
    }

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = desc.effectName;
    acquireDesc.layerLevelIndex = desc.levelIndex;
    acquireDesc.layerTag = desc.layerTag;
    acquireDesc.worldPosition = spawnWorldPosition;
    acquireDesc.worldRotation = spawnWorldRotation;
    acquireDesc.scale = spawnWorldScale;
    acquireDesc.playbackSpeed = desc.playbackSpeed;
    acquireDesc.trailSampleProvider = desc.trailSampleProvider;
    acquireDesc.sourcePointSampleProvider = desc.sourcePointSampleProvider;
    const bool shouldTrackAttached =
        outHandle != nullptr ||
        desc.followTarget ||
        desc.requiresManualRelease ||
        !desc.trailSampleProvider.expired() ||
        !desc.sourcePointSampleProvider.expired();
    acquireDesc.autoReleaseOnFinished = !shouldTrackAttached;

    EffectInstancePool::AttachedHandle handle{};
    const HRESULT acquireResult = effectPool->Acquire(acquireDesc, handle);
    if (FAILED(acquireResult))
    {
        LOG_WARN(
            "[EffectCom] Play_Effect failed: pool acquire. hr={}, effect={}, level={}, layer={}, pos=({:.3f},{:.3f},{:.3f})",
            acquireResult,
            String::ToString(desc.effectName),
            desc.levelIndex,
            String::ToString(desc.layerTag),
            spawnWorldPosition.x,
            spawnWorldPosition.y,
            spawnWorldPosition.z
        );
        return E_FAIL;
    }
    if (desc.requiresManualRelease)
        handle.requiresManualRelease = true;

    Apply_AttachedEffectMaterialReveal(handle.effectObject);

    if (!shouldTrackAttached)
        return S_OK;

    AttachedEffect attached{};
    attached.poolHandle = handle;
    attached.effectName = desc.effectName;
    attached.target = desc.target;
    attached.partTag = desc.partTag;
    attached.socketName = desc.socketName;
    attached.layerTag = desc.layerTag;
    attached.localOffset = desc.localOffset;
    attached.localRotation = desc.localRotation;
    attached.scale = desc.scale;
    attached.transformInheritance = desc.transformInheritance;
    attached.localOffsetZTowardCamera = desc.localOffsetZTowardCamera;
    attached.followTarget = desc.followTarget;
    attached.role = AttachedEffectRole::Normal;
    attached.trailSampleProvider = desc.trailSampleProvider.lock();
    attached.sourcePointSampleProvider = desc.sourcePointSampleProvider.lock();
    _attachedEffects.push_back(attached);

    if (outHandle != nullptr)
        *outHandle = handle;

    return S_OK;
}

void EffectCom::Stop_Effect(
    const wstring& effectName,
    EffectPlayTarget target,
    const wstring& partTag,
    const string& socketName)
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.role == AttachedEffectRole::Trail ||
            attached.role == AttachedEffectRole::SourceHistoryPoint)
            continue;
        if (attached.effectName != effectName)
            continue;
        if (attached.target != target)
            continue;
        if (attached.partTag != partTag)
            continue;
        if (attached.socketName != socketName)
            continue;

        Release_AttachedEffect(attached);
    }

    Prune_ReleasedAttachedEffects();
}

void EffectCom::Stop_Effect(const EffectInstancePool::AttachedHandle& handle)
{
    if (handle.effectObject == nullptr)
        return;

    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.poolHandle.effectObject != handle.effectObject)
            continue;

        Release_AttachedEffect(attached);
        break;
    }

    Prune_ReleasedAttachedEffects();
}

void EffectCom::Play_InspectorEffectIfEnabled()
{
    if (!_inspectorAutoPlayEffect || _inspectorEffectPlayed || _inspectorEffectName.empty())
        return;

    if (SUCCEEDED(Play_InspectorEffect()))
        _inspectorEffectPlayed = true;
}

bool EffectCom::Try_ResolveEffectWorldTransform(
    EffectPlayTarget target,
    const wstring& partTag,
    const string& socketName,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale,
    const EffectTransformInheritance& transformInheritance,
    bool localOffsetZTowardCamera,
    Vec3& outWorldPosition,
    Quat& outWorldRotation,
    Vec3& outWorldScale) const
{
    Matrix baseWorldMatrix = Matrix::Identity;

    if (target == EffectPlayTarget::OwnerRoot)
    {
        if (!Try_GetOwnerRootWorldMatrix(baseWorldMatrix))
            return false;
    }
    else if (target == EffectPlayTarget::PartSocket)
    {
        if (!Try_GetSocketWorldMatrix(partTag, socketName, baseWorldMatrix))
            return false;
    }
    else
    {
        if (!Try_GetPartRootWorldMatrix(partTag, baseWorldMatrix))
            return false;
    }

    Vec3 baseScale = Vec3::One;
    Quat baseRotation = Quat::Identity;
    Vec3 baseTranslation = Vec3::Zero;
    if (!baseWorldMatrix.Decompose(baseScale, baseRotation, baseTranslation))
        return false;

    const Vec3 localRotationRad(
        XMConvertToRadians(localRotation.x),
        XMConvertToRadians(localRotation.y),
        XMConvertToRadians(localRotation.z)
    );

    // Local offset과 rotation은 선택된 position/rotation 상속 basis를 기준으로 합성한다.
    const Matrix localMatrix =
        Matrix::CreateFromYawPitchRoll(localRotationRad.y, localRotationRad.x, localRotationRad.z) *
        Matrix::CreateTranslation(localOffset);

    auto normalize_axis = [](Vec3 axis, const Vec3& fallback) -> Vec3
    {
        if (axis.LengthSquared() <= 0.000001f)
            return fallback;

        axis.Normalize();
        return axis;
    };

    Matrix rotationBasisMatrix = baseWorldMatrix;
    if (!transformInheritance.inheritRotation)
        Try_GetOwnerRootWorldMatrix(rotationBasisMatrix);

    const Vec3 basisRight = transformInheritance.inheritRotation
                            ? normalize_axis(Vec3(baseWorldMatrix._11, baseWorldMatrix._12, baseWorldMatrix._13), Vec3::Right)
                            : normalize_axis(Vec3(rotationBasisMatrix._11, rotationBasisMatrix._12, rotationBasisMatrix._13), Vec3::Right);
    const Vec3 basisUp = transformInheritance.inheritRotation
                         ? normalize_axis(Vec3(baseWorldMatrix._21, baseWorldMatrix._22, baseWorldMatrix._23), Vec3::Up)
                         : normalize_axis(Vec3(rotationBasisMatrix._21, rotationBasisMatrix._22, rotationBasisMatrix._23), Vec3::Up);
    const Vec3 basisLook = transformInheritance.inheritRotation
                           ? normalize_axis(Vec3(baseWorldMatrix._31, baseWorldMatrix._32, baseWorldMatrix._33), Vec3::Look)
                           : normalize_axis(Vec3(rotationBasisMatrix._31, rotationBasisMatrix._32, rotationBasisMatrix._33), Vec3::Look);

    Matrix basisWorldMatrix = Matrix::Identity;
    basisWorldMatrix._11 = basisRight.x;
    basisWorldMatrix._12 = basisRight.y;
    basisWorldMatrix._13 = basisRight.z;
    basisWorldMatrix._21 = basisUp.x;
    basisWorldMatrix._22 = basisUp.y;
    basisWorldMatrix._23 = basisUp.z;
    basisWorldMatrix._31 = basisLook.x;
    basisWorldMatrix._32 = basisLook.y;
    basisWorldMatrix._33 = basisLook.z;
    if (transformInheritance.inheritPosition)
    {
        basisWorldMatrix._41 = baseTranslation.x;
        basisWorldMatrix._42 = baseTranslation.y;
        basisWorldMatrix._43 = baseTranslation.z;
    }

    Matrix finalWorldMatrix = localMatrix * basisWorldMatrix;
    if (localOffsetZTowardCamera)
    {
        const Vec3 fallbackDirection = basisLook;
        const Vec3 cameraDirection = Resolve_CameraDirectionFromAnchor(baseTranslation, fallbackDirection);
        const Vec3 origin = transformInheritance.inheritPosition ? baseTranslation : Vec3::Zero;
        const Vec3 customTranslation =
            origin +
            basisRight * localOffset.x +
            basisUp * localOffset.y +
            cameraDirection * localOffset.z;

        finalWorldMatrix._41 = customTranslation.x;
        finalWorldMatrix._42 = customTranslation.y;
        finalWorldMatrix._43 = customTranslation.z;
    }

    Vec3 decomposedScale = Vec3::One;
    Quat decomposedRotation = Quat::Identity;
    Vec3 decomposedTranslation = Vec3::Zero;
    if (!finalWorldMatrix.Decompose(decomposedScale, decomposedRotation, decomposedTranslation))
        return false;

    outWorldPosition = decomposedTranslation;
    outWorldRotation = decomposedRotation;
    if (outWorldRotation.LengthSquared() <= 0.000001f)
        outWorldRotation = Quat::Identity;
    else
        outWorldRotation.Normalize();
    outWorldScale = scale;
    if (transformInheritance.inheritScale)
    {
        outWorldScale.x *= baseScale.x;
        outWorldScale.y *= baseScale.y;
        outWorldScale.z *= baseScale.z;
    }
    return true;
}

HRESULT EffectCom::Play_AttachedEffect(
    uint32 levelIndex,
    const wstring& effectName,
    const wstring& partTag,
    const string& socketName,
    const wstring& layerTag,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale,
    const Weak<IEffectTrailSampleProvider>& trailSampleProvider)
{
    EffectPlayDesc desc{};
    desc.levelIndex = levelIndex;
    desc.effectName = effectName;
    desc.target = EffectPlayTarget::PartSocket;
    desc.partTag = partTag;
    desc.socketName = socketName;
    desc.layerTag = layerTag;
    desc.localOffset = localOffset;
    desc.localRotation = localRotation;
    desc.scale = scale;
    desc.followTarget = true;
    desc.trailSampleProvider = trailSampleProvider;

    return Play_Effect(desc);
}

HRESULT EffectCom::Play_AttachedPartEffect(
    uint32 levelIndex,
    const wstring& effectName,
    const wstring& partTag,
    const string& socketName,
    const wstring& layerTag,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale)
{
    EffectPlayDesc desc{};
    desc.levelIndex = levelIndex;
    desc.effectName = effectName;
    desc.target = socketName.empty() ? EffectPlayTarget::PartRoot : EffectPlayTarget::PartSocket;
    desc.partTag = partTag;
    desc.socketName = socketName;
    desc.layerTag = layerTag;
    desc.localOffset = localOffset;
    desc.localRotation = localRotation;
    desc.scale = scale;
    desc.followTarget = true;

    return Play_Effect(desc);
}

HRESULT EffectCom::Play_AttachedPartEffect(
    uint32 levelIndex,
    const wstring& effectName,
    const wstring& partTag,
    const wstring& layerTag,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale)
{
    return Play_AttachedPartEffect(levelIndex, effectName, partTag, "", layerTag, localOffset, localRotation, scale);
}

HRESULT EffectCom::Play_TrailEffect(
    const wstring& effectName,
    const wstring& partTag,
    const wstring& layerTag,
    uint64 trailNotifyToken,
    const TrailOffsetRuntimeDesc& offsetDesc,
    const TrailAnchorRuntimeDesc& anchorDesc)
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    CHECK_NULL(ownerContainer, E_FAIL);

    const PartObject* trailPart = ownerContainer->Get_PartObject(partTag);
    CHECK_NULL(trailPart, E_FAIL);

    wstring resolvedEffectName{};
    if (!Try_ResolveTrailEffectName(effectName, partTag, resolvedEffectName))
        return E_FAIL;

    const Shared<ModelCom> weaponModel = Resolve_TrailPartModel(trailPart);
    CHECK_NULL(weaponModel, E_FAIL);

    AttachedEffect attached{};
    // Stop에서도 동일한 pool key를 사용하도록 해석이 끝난 effect 이름을 저장한다.
    attached.effectName = resolvedEffectName;
    attached.partTag = partTag;
    attached.layerTag = layerTag;
    attached.role = AttachedEffectRole::Trail;
    attached.trailNotifyToken = trailNotifyToken;
    attached.trailOffset = offsetDesc;
    attached.trailOffset.phase = std::clamp(attached.trailOffset.phase, 0.f, 1.f);
    attached.trailSampleEnabled = make_shared<bool>(true);

    attached.trailEchoEligible = anchorDesc.source == ANS_Trail::TrailAnchorSource::ModelDefault;

    if (anchorDesc.source == ANS_Trail::TrailAnchorSource::NotifyBonePair)
    {
        attached.trailAnchorAsset = {};
        attached.trailAnchorAsset.modelGuid.clear();
        attached.trailAnchorAsset.base.boneName = anchorDesc.baseBoneName;
        attached.trailAnchorAsset.base.localPosition = anchorDesc.baseLocalOffset;
        attached.trailAnchorAsset.tip.boneName = anchorDesc.tipBoneName;
        attached.trailAnchorAsset.tip.localPosition = anchorDesc.tipLocalOffset;
    }
    else
    {
        const wstring& sourceModelPath = weaponModel->Get_SourceModelPath();
        if (sourceModelPath.empty())
            return E_FAIL;

        const string modelGuid = GAME->Ensure_AssetGUID(sourceModelPath, "Model");
        if (modelGuid.empty())
            return E_FAIL;

        const fs::path anchorPath = TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(modelGuid);
        if (!TrailAnchor_Parser::Load_FromFile(anchorPath.wstring(), attached.trailAnchorAsset))
            return E_FAIL;
    }

    EffectTrailSample initialSample{};
    if (!Try_BuildTrailSample(attached.partTag, attached.trailAnchorAsset, &attached.trailOffset, initialSample))
        return E_FAIL;

    attached.trailSampleProvider = make_shared<TrailSampleProvider_ByPart>(
        static_pointer_cast<EffectCom>(GetSharedPtr<EffectCom>()),
        partTag,
        attached.trailAnchorAsset,
        trailNotifyToken,
        attached.trailSampleEnabled
    );
    CHECK_NULL(attached.trailSampleProvider, E_FAIL);
    attached.ribbonSourcePointSampleProvider = make_shared<TrailTipSourcePointSampleProvider>(attached.trailSampleProvider);
    CHECK_NULL(attached.ribbonSourcePointSampleProvider, E_FAIL);

    EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool();
    CHECK_NULL(effectPool, E_FAIL);

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = resolvedEffectName;
    acquireDesc.layerLevelIndex = GAME->Current_LevelIndex();
    acquireDesc.layerTag = layerTag;
    acquireDesc.worldPosition = initialSample.tipWorldPosition;
    acquireDesc.trailSampleProvider = attached.trailSampleProvider;
    acquireDesc.ribbonSourcePointSampleProvider = attached.ribbonSourcePointSampleProvider;
    acquireDesc.trailSampleGroupKey = Build_TrailPairHistoryKey(
        attached.partTag,
        attached.trailAnchorAsset,
        attached.trailNotifyToken
    );
    acquireDesc.autoReleaseOnFinished = false;

    attached.trailDrainDuration = Resolve_TrailDrainDuration(EffectAssetRuntimeLoader::Load_DefinitionByName(resolvedEffectName));

    for (AttachedEffect& existing : _attachedEffects)
    {
        if (existing.role != AttachedEffectRole::Trail || !existing.isTrailDraining)
            continue;
        if (existing.effectName != attached.effectName)
            continue;
        if (existing.partTag != attached.partTag)
            continue;
        if (existing.layerTag != attached.layerTag)
            continue;
        if (!Is_SameTrailAnchor(existing.trailAnchorAsset, attached.trailAnchorAsset))
            continue;

        Release_AttachedEffect(existing);
    }
    Prune_ReleasedAttachedEffects();

    CHECK_FAILED(effectPool->Acquire(acquireDesc, attached.poolHandle), E_FAIL);

    _attachedEffects.push_back(attached);

    return S_OK;
}

void EffectCom::Update_TrailEffectOffset(
    const wstring& effectName,
    const wstring& partTag,
    uint64 trailNotifyToken,
    const TrailOffsetRuntimeDesc& offsetDesc)
{
    if (trailNotifyToken == 0)
        return;

    wstring resolvedEffectName{};
    if (!Try_ResolveTrailEffectName(effectName, partTag, resolvedEffectName))
        return;

    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.role != AttachedEffectRole::Trail)
            continue;
        if (attached.effectName != resolvedEffectName)
            continue;
        if (attached.partTag != partTag)
            continue;
        if (attached.trailNotifyToken != trailNotifyToken)
            continue;

        attached.trailOffset = offsetDesc;
        attached.trailOffset.phase = std::clamp(attached.trailOffset.phase, 0.f, 1.f);
    }
}

void EffectCom::Stop_TrailEffect(const wstring& effectName, const wstring& partTag, uint64 trailNotifyToken)
{
    wstring resolvedEffectName{};
    if (!Try_ResolveTrailEffectName(effectName, partTag, resolvedEffectName))
        return;

    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.role != AttachedEffectRole::Trail)
            continue;
        if (attached.effectName != resolvedEffectName)
            continue;
        if (attached.partTag != partTag)
            continue;
        if (trailNotifyToken != 0 && attached.trailNotifyToken != trailNotifyToken)
            continue;

        _recentTrailEffect.valid = true;
        _recentTrailEffect.partTag = attached.partTag;
        _recentTrailEffect.effectName = attached.effectName;
        _recentTrailEffect.layerTag = attached.layerTag;
        _recentTrailEffect.offsetDesc = attached.trailOffset;
        _recentTrailEffect.echoEligible = attached.trailEchoEligible;
        _recentTrailEffect.ageSec = 0.f;

        Begin_TrailDrain(attached);
    }

    Prune_ReleasedAttachedEffects();
}

bool EffectCom::Try_GetTrailSampleForPart(
    const wstring& partTag,
    const TrailAnchorAsset& trailAnchorAsset,
    uint64 trailNotifyToken,
    EffectTrailSample& outSample) const
{
    const TrailOffsetRuntimeDesc* offsetDesc = nullptr;
    if (trailNotifyToken != 0)
    {
        for (const AttachedEffect& attached : _attachedEffects)
        {
            if (attached.role != AttachedEffectRole::Trail)
                continue;
            if (attached.trailNotifyToken != trailNotifyToken)
                continue;

            offsetDesc = &attached.trailOffset;
            break;
        }
    }

    return Try_BuildTrailSample(partTag, trailAnchorAsset, offsetDesc, outSample);
}

bool EffectCom::Try_GetActiveTrailEffect(
    const wstring& partTag,
    wstring& outEffectName,
    wstring& outLayerTag,
    TrailOffsetRuntimeDesc& outOffsetDesc) const
{
    outEffectName.clear();
    outLayerTag.clear();
    outOffsetDesc = {};

    for (const AttachedEffect& attached : _attachedEffects)
    {
        if (attached.role != AttachedEffectRole::Trail)
            continue;
        if (attached.partTag != partTag)
            continue;
        if (attached.isTrailDraining)
            continue;
        if (!attached.trailEchoEligible)
            continue;
        if (attached.poolHandle.effectObject == nullptr)
            continue;

        outEffectName = attached.effectName;
        outLayerTag = attached.layerTag;
        outOffsetDesc = attached.trailOffset;
        return !outEffectName.empty() && !outLayerTag.empty();
    }

    constexpr float recentTrailGraceSec = 0.5f;
    if (_recentTrailEffect.valid &&
        _recentTrailEffect.echoEligible &&
        _recentTrailEffect.partTag == partTag &&
        _recentTrailEffect.ageSec <= recentTrailGraceSec)
    {
        outEffectName = _recentTrailEffect.effectName;
        outLayerTag = _recentTrailEffect.layerTag;
        outOffsetDesc = _recentTrailEffect.offsetDesc;
        return !outEffectName.empty() && !outLayerTag.empty();
    }

    return false;
}

HRESULT EffectCom::Play_SourceHistoryEffect(
    const wstring& effectName,
    const wstring& partTag,
    const wstring& layerTag,
    SourceHistorySourceBindingMode sourceBindingMode,
    const string& socketName,
    const string& sourceHistoryAnchorName,
    const Vec3& sourceHistoryLocalOffset)
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
    {
        LOG_WARN(
            "[EffectCom] Play_SourceHistoryEffect failed: owner is not container. effect={}, part={}, layer={}",
            String::ToString(effectName),
            String::ToString(partTag),
            String::ToString(layerTag)
        );
        return E_FAIL;
    }

    wstring resolvedEffectName{};
    if (!Try_ResolveSourceHistoryEffectName(effectName, resolvedEffectName))
    {
        LOG_WARN(
            "[EffectCom] Play_SourceHistoryEffect failed: effect name is empty. part={}, layer={}",
            String::ToString(partTag),
            String::ToString(layerTag)
        );
        return E_FAIL;
    }

    AttachedEffect attached{};
    attached.effectName = resolvedEffectName;
    attached.partTag = partTag;
    attached.socketName = socketName;
    attached.layerTag = layerTag;
    attached.role = AttachedEffectRole::SourceHistoryPoint;
    attached.sourceHistorySourceBindingMode = sourceBindingMode;
    attached.sourceHistoryAnchorName = sourceHistoryAnchorName;
    attached.sourceHistoryLocalOffset = sourceHistoryLocalOffset;
    attached.sourcePointSampleEnabled = make_shared<bool>(true);

    if (static_cast<int32>(sourceBindingMode) == 3)
    {
        const PartObject* trailPart = ownerContainer->Get_PartObject(partTag);
        if (Resolve_TrailPartModel(trailPart) == nullptr)
        {
            LOG_WARN(
                "[EffectCom] Play_SourceHistoryEffect failed: weapon part not found. effect={}, part={}, layer={}",
                String::ToString(effectName),
                String::ToString(partTag),
                String::ToString(layerTag)
            );
            return E_FAIL;
        }

        const Shared<ModelCom> weaponModel = Resolve_TrailPartModel(trailPart);
        if (weaponModel == nullptr)
        {
            LOG_WARN(
                "[EffectCom] Play_SourceHistoryEffect failed: weapon model is null. effect={}, part={}",
                String::ToString(resolvedEffectName),
                String::ToString(partTag)
            );
            return E_FAIL;
        }

        const wstring& sourceModelPath = weaponModel->Get_SourceModelPath();
        if (sourceModelPath.empty())
        {
            LOG_WARN(
                "[EffectCom] Play_SourceHistoryEffect failed: source model path is empty. effect={}, part={}",
                String::ToString(resolvedEffectName),
                String::ToString(partTag)
            );
            return E_FAIL;
        }

        const string modelGuid = GAME->Ensure_AssetGUID(sourceModelPath, "Model");
        if (modelGuid.empty())
        {
            LOG_WARN(
                "[EffectCom] Play_SourceHistoryEffect failed: model guid is empty. effect={}, part={}, model={}",
                String::ToString(resolvedEffectName),
                String::ToString(partTag),
                String::ToString(sourceModelPath)
            );
            return E_FAIL;
        }

        const fs::path anchorPath = TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(modelGuid);
        if (!TrailAnchor_Parser::Load_FromFile(anchorPath.wstring(), attached.trailAnchorAsset))
        {
            LOG_WARN(
                "[EffectCom] Play_SourceHistoryEffect failed: trail anchor load. effect={}, part={}, path={}",
                String::ToString(resolvedEffectName),
                String::ToString(partTag),
                String::ToString(anchorPath.wstring())
            );
            return E_FAIL;
        }
    }

    EffectSourcePointSample initialSample{};
    if (!Try_BuildSourcePointSample(
        attached.partTag,
        sourceBindingMode,
        socketName,
        sourceHistoryAnchorName,
        sourceHistoryLocalOffset,
        &attached.trailAnchorAsset,
        initialSample))
    {
        LOG_WARN(
            "[EffectCom] Play_SourceHistoryEffect failed: source point sample. effect={}, part={}, binding={}, socket={}, anchor={}",
            String::ToString(resolvedEffectName),
            String::ToString(partTag),
            static_cast<int>(sourceBindingMode),
            socketName,
            sourceHistoryAnchorName
        );
        return E_FAIL;
    }

    attached.sourcePointSampleProvider = make_shared<SourcePointSampleProvider_ByPart>(
        static_pointer_cast<EffectCom>(GetSharedPtr<EffectCom>()),
        partTag,
        sourceBindingMode,
        socketName,
        sourceHistoryAnchorName,
        sourceHistoryLocalOffset,
        attached.sourcePointSampleEnabled
    );
    CHECK_NULL(attached.sourcePointSampleProvider, E_FAIL);
    attached.ribbonSourcePointSampleProvider = attached.sourcePointSampleProvider;

    EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool();
    if (effectPool == nullptr)
    {
        LOG_WARN(
            "[EffectCom] Play_SourceHistoryEffect failed: effect pool is null. effect={}, part={}, layer={}",
            String::ToString(resolvedEffectName),
            String::ToString(partTag),
            String::ToString(layerTag)
        );
        return E_FAIL;
    }

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = resolvedEffectName;
    acquireDesc.layerLevelIndex = GAME->Current_LevelIndex();
    acquireDesc.layerTag = layerTag;
    acquireDesc.worldPosition = initialSample.worldPosition;
    acquireDesc.sourcePointSampleProvider = attached.sourcePointSampleProvider;
    acquireDesc.ribbonSourcePointSampleProvider = attached.ribbonSourcePointSampleProvider;
    acquireDesc.sourcePointGroupKey = Build_SourcePointHistoryKey(
        attached.partTag,
        attached.sourceHistorySourceBindingMode,
        attached.socketName,
        attached.sourceHistoryAnchorName,
        attached.sourceHistoryLocalOffset
    );
    acquireDesc.autoReleaseOnFinished = false;

    attached.sourceHistoryDrainDuration =
        Resolve_SourceHistoryDrainDuration(EffectAssetRuntimeLoader::Load_DefinitionByName(resolvedEffectName));

    const HRESULT acquireResult = effectPool->Acquire(acquireDesc, attached.poolHandle);
    if (FAILED(acquireResult))
    {
        LOG_WARN(
            "[EffectCom] Play_SourceHistoryEffect failed: pool acquire. hr={}, effect={}, part={}, layer={}, pos=({:.3f},{:.3f},{:.3f})",
            acquireResult,
            String::ToString(resolvedEffectName),
            String::ToString(partTag),
            String::ToString(layerTag),
            initialSample.worldPosition.x,
            initialSample.worldPosition.y,
            initialSample.worldPosition.z
        );
        return E_FAIL;
    }

    _attachedEffects.push_back(attached);

    return S_OK;
}

void EffectCom::Stop_SourceHistoryEffect(
    const wstring& effectName,
    const wstring& partTag,
    SourceHistorySourceBindingMode sourceBindingMode,
    const string& socketName,
    const string& sourceHistoryAnchorName)
{
    wstring resolvedEffectName{};
    if (!Try_ResolveSourceHistoryEffectName(effectName, resolvedEffectName))
        return;

    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.role != AttachedEffectRole::SourceHistoryPoint)
            continue;
        if (attached.effectName != resolvedEffectName)
            continue;
        if (attached.partTag != partTag)
            continue;
        if (attached.sourceHistorySourceBindingMode != sourceBindingMode)
            continue;
        if (attached.socketName != socketName)
            continue;
        if (attached.sourceHistoryAnchorName != sourceHistoryAnchorName)
            continue;

        Begin_SourceHistoryDrain(attached);
    }

    Prune_ReleasedAttachedEffects();
}

bool EffectCom::Try_GetSourcePointSampleForPart(
    const wstring& partTag,
    SourceHistorySourceBindingMode sourceBindingMode,
    const string& socketName,
    const string& sourceHistoryAnchorName,
    const Vec3& sourceHistoryLocalOffset,
    EffectSourcePointSample& outSample) const
{
    const TrailAnchorAsset* trailAnchorAsset = nullptr;
    if (static_cast<int32>(sourceBindingMode) == 3)
    {
        for (const AttachedEffect& attached : _attachedEffects)
        {
            if (attached.role != AttachedEffectRole::SourceHistoryPoint)
                continue;
            if (attached.partTag != partTag)
                continue;
            if (attached.sourceHistorySourceBindingMode != sourceBindingMode)
                continue;
            if (attached.socketName != socketName)
                continue;
            if (attached.sourceHistoryAnchorName != sourceHistoryAnchorName)
                continue;

            trailAnchorAsset = &attached.trailAnchorAsset;
            break;
        }

        if (trailAnchorAsset == nullptr)
            return false;
    }

    return Try_BuildSourcePointSample(
        partTag,
        sourceBindingMode,
        socketName,
        sourceHistoryAnchorName,
        sourceHistoryLocalOffset,
        trailAnchorAsset,
        outSample);
}

bool EffectCom::Try_ResolveTrailEffectName(
    const wstring& effectNameOverride,
    const wstring& partTag,
    wstring& outEffectName) const
{
    outEffectName.clear();

    if (!effectNameOverride.empty())
    {
        outEffectName = effectNameOverride;
        return true;
    }

    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    const PartObject* trailPart = ownerContainer->Get_PartObject(partTag);
    outEffectName = Resolve_DefaultTrailEffectName(trailPart);
    return !outEffectName.empty();
}

void EffectCom::Update_AttachedEffects(float timeDelta)
{
    if (_recentTrailEffect.valid)
    {
        _recentTrailEffect.ageSec += max(0.f, timeDelta);
        if (_recentTrailEffect.ageSec > 0.5f)
            _recentTrailEffect = {};
    }

    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.poolHandle.effectObject == nullptr)
            continue;

        const Shared<TransformCom> effectTransform = attached.poolHandle.effectObject->Get_Transform();
        if (effectTransform == nullptr)
        {
            Release_AttachedEffect(attached);
            continue;
        }

        if (attached.isTrailDraining)
        {
            attached.trailDrainElapsed += max(0.f, timeDelta);
            if (attached.trailDrainElapsed >= attached.trailDrainDuration)
                Release_AttachedEffect(attached);

            continue;
        }

        if (attached.isSourceHistoryDraining)
        {
            attached.sourceHistoryDrainElapsed += max(0.f, timeDelta);
            if (attached.sourceHistoryDrainElapsed >= attached.sourceHistoryDrainDuration)
                Release_AttachedEffect(attached);

            continue;
        }

        if (Should_ReleaseAttachedEffect(attached))
        {
            Release_AttachedEffect(attached);
            continue;
        }

        if (attached.role == AttachedEffectRole::Trail)
        {
            EffectTrailSample trailSample{};
            if (!Try_BuildTrailSample(attached.partTag, attached.trailAnchorAsset, &attached.trailOffset, trailSample))
                continue;

            effectTransform->Set_WorldPosition(trailSample.tipWorldPosition);
            continue;
        }

        // 단발 effect는 시작 transform만 사용하고 이후 owner 이동을 추적하지 않는다.
        if (!attached.followTarget)
            continue;

        Vec3 targetWorldPosition{};
        Quat targetWorldRotation{ Quat::Identity };
        Vec3 targetWorldScale{};
        if (!Try_ResolveEffectWorldTransform(
            attached.target,
            attached.partTag,
            attached.socketName,
            attached.localOffset,
            attached.localRotation,
            attached.scale,
            attached.transformInheritance,
            attached.localOffsetZTowardCamera,
            targetWorldPosition,
            targetWorldRotation,
            targetWorldScale
        ))
            continue;

        effectTransform->Set_WorldPosition(targetWorldPosition);
        effectTransform->Set_WorldRotationQuaternion(targetWorldRotation);
        effectTransform->Set_Scale(targetWorldScale);
    }

    Prune_ReleasedAttachedEffects();
}

void EffectCom::Release_AttachedEffect(AttachedEffect& attached)
{
    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    if (nullptr == effectPool)
        return;

    if (nullptr == attached.poolHandle.effectObject)
        return;

    effectPool->Release(attached.poolHandle);
    attached.poolHandle = {};
    attached.trailSampleEnabled.reset();
    attached.trailSampleProvider.reset();
    attached.isTrailDraining = false;
    attached.trailDrainElapsed = 0.f;
    attached.sourcePointSampleEnabled.reset();
    attached.sourcePointSampleProvider.reset();
    attached.isSourceHistoryDraining = false;
    attached.sourceHistoryDrainElapsed = 0.f;
}

void EffectCom::Begin_TrailDrain(AttachedEffect& attached)
{
    if (attached.role != AttachedEffectRole::Trail || attached.isTrailDraining)
        return;

    if (attached.trailSampleEnabled != nullptr)
        *attached.trailSampleEnabled = false;

    attached.isTrailDraining = true;
    attached.trailDrainElapsed = 0.f;
}

void EffectCom::Begin_SourceHistoryDrain(AttachedEffect& attached)
{
    if (attached.role != AttachedEffectRole::SourceHistoryPoint || attached.isSourceHistoryDraining)
        return;

    if (attached.sourcePointSampleEnabled != nullptr)
        *attached.sourcePointSampleEnabled = false;

    attached.isSourceHistoryDraining = true;
    attached.sourceHistoryDrainElapsed = 0.f;
}

bool EffectCom::Should_ReleaseAttachedEffect(const AttachedEffect& attached) const
{
    if (nullptr == attached.poolHandle.effectObject)
        return false;

    if (attached.poolHandle.requiresManualRelease)
        return false;

    const Shared<EffectInstance> effectInstance =
        dynamic_pointer_cast<EffectInstance>(attached.poolHandle.effectObject);
    if (nullptr == effectInstance)
        return false;

    return effectInstance->Is_Finished();
}

void EffectCom::Prune_ReleasedAttachedEffects()
{
    erase_if(
        _attachedEffects,
        [](const AttachedEffect& attached)
        {
            return attached.poolHandle.effectObject == nullptr;
        }
    );
}

void EffectCom::Apply_AttachedEffectMaterialReveal(const Shared<GameObject>& effectObject) const
{
    const Shared<EffectInstance> effectInstance = dynamic_pointer_cast<EffectInstance>(effectObject);
    if (effectInstance == nullptr)
        return;

    effectInstance->Set_MaterialRevealOverride(
        _attachedEffectRevealAlphaMultiplier,
        _attachedEffectRevealAlphaErosion
    );
}

bool EffectCom::Try_GetOwnerRootWorldMatrix(Matrix& outWorldMatrix) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    if (ownerObject == nullptr)
        return false;

    if (const Shared<PartObject> ownerPart = dynamic_pointer_cast<PartObject>(ownerObject))
    {
        outWorldMatrix = ownerPart->Get_CombinedWorldMatrix();
        return true;
    }

    const Shared<TransformCom> ownerTransform = ownerObject->Get_Transform();
    if (ownerTransform == nullptr)
        return false;

    outWorldMatrix = ownerTransform->Get_WorldMatrix();
    return true;
}

bool EffectCom::Try_GetSocketWorldMatrix(
    const wstring& partTag,
    const string& socketName,
    Matrix& outWorldMatrix) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    if (const auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * playablePart->Get_CombinedWorldMatrix();

        outWorldMatrix = socketWorldMatrix;
        return true;
    }

    if (Resolve_TrailPartModel(partObject) != nullptr)
    {
        const Matrix* socketMatrix = Resolve_TrailPartSocketMatrix(partObject, socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * partObject->Get_CombinedWorldMatrix();
        outWorldMatrix = socketWorldMatrix;
        return true;
    }

    return false;
}

bool EffectCom::Try_GetPartRootWorldMatrix(const wstring& partTag, Matrix& outWorldMatrix) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    const PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    outWorldMatrix = partObject->Get_CombinedWorldMatrix();
    return true;
}

bool EffectCom::Try_BuildTrailSample(
    const wstring& partTag,
    const TrailAnchorAsset& trailAnchorAsset,
    const TrailOffsetRuntimeDesc* offsetDesc,
    EffectTrailSample& outSample) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    const PartObject* trailPart = ownerContainer->Get_PartObject(partTag);
    if (Resolve_TrailPartModel(trailPart) == nullptr)
        return false;

    const TrailAnchorAsset sampleAnchorAsset =
        offsetDesc != nullptr
        ? ANS_Trail::Make_OffsetTrailAnchorAsset(
            trailAnchorAsset,
            offsetDesc->tipExtendCurve,
            offsetDesc->baseLocalOffsetCurve,
            offsetDesc->tipLocalOffsetCurve,
            offsetDesc->phase)
        : trailAnchorAsset;

    Matrix baseWorldMatrix{};
    Matrix tipWorldMatrix{};

    if (!Try_ResolveTrailAnchorWorldMatrix(*trailPart, sampleAnchorAsset.base, baseWorldMatrix))
        return false;
    if (!Try_ResolveTrailAnchorWorldMatrix(*trailPart, sampleAnchorAsset.tip, tipWorldMatrix))
        return false;

    outSample.baseWorldPosition = Vec3::Transform(sampleAnchorAsset.base.localPosition, baseWorldMatrix);
    outSample.tipWorldPosition = Vec3::Transform(sampleAnchorAsset.tip.localPosition, tipWorldMatrix);
    outSample.baseWorldRotation = Resolve_NormalizedWorldRotation(baseWorldMatrix);
    outSample.tipWorldRotation = Resolve_NormalizedWorldRotation(tipWorldMatrix);

    return true;
}

bool EffectCom::Try_ResolveSourceHistoryEffectName(
    const wstring& effectName,
    wstring& outEffectName) const
{
    outEffectName.clear();

    if (effectName.empty())
        return false;

    outEffectName = effectName;
    return true;
}

bool EffectCom::Try_BuildSourcePointSample(
    const wstring& partTag,
    SourceHistorySourceBindingMode sourceBindingMode,
    const string& socketName,
    const string& sourceHistoryAnchorName,
    const Vec3& sourceHistoryLocalOffset,
    const TrailAnchorAsset* trailAnchorAsset,
    EffectSourcePointSample& outSample) const
{
    if (sourceBindingMode == SourceHistorySourceBindingMode::Socket)
    {
        Matrix socketWorldMatrix{};
        if (!Try_GetSocketWorldMatrix(partTag, socketName, socketWorldMatrix))
            return false;

        outSample.worldPosition = Vec3::Transform(sourceHistoryLocalOffset, socketWorldMatrix);
        outSample.worldRotation = Resolve_NormalizedWorldRotation(socketWorldMatrix);
        return true;
    }

    if (static_cast<int32>(sourceBindingMode) == 3)
    {
        if (trailAnchorAsset == nullptr)
            return false;

        EffectTrailSample trailSample{};
        if (!Try_BuildTrailSample(partTag, *trailAnchorAsset, nullptr, trailSample))
            return false;

        if (sourceHistoryAnchorName == "Base")
        {
            outSample.worldPosition = trailSample.baseWorldPosition + Vec3::Transform(
                sourceHistoryLocalOffset,
                Matrix::CreateFromQuaternion(trailSample.baseWorldRotation));
            outSample.worldRotation = trailSample.baseWorldRotation;
            return true;
        }

        if (sourceHistoryAnchorName == "Mid")
        {
            outSample.worldPosition =
                (trailSample.baseWorldPosition + trailSample.tipWorldPosition) * 0.5f +
                Vec3::Transform(sourceHistoryLocalOffset, Matrix::CreateFromQuaternion(Quat::Slerp(
                    trailSample.baseWorldRotation,
                    trailSample.tipWorldRotation,
                    0.5f)));
            outSample.worldRotation = Quat::Slerp(trailSample.baseWorldRotation, trailSample.tipWorldRotation, 0.5f);
            return true;
        }

        outSample.worldPosition = trailSample.tipWorldPosition + Vec3::Transform(
            sourceHistoryLocalOffset,
            Matrix::CreateFromQuaternion(trailSample.tipWorldRotation));
        outSample.worldRotation = trailSample.tipWorldRotation;
        return true;
    }

    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    const PartObject* weaponPart = ownerContainer->Get_PartObject(partTag);
    if (weaponPart == nullptr)
        return false;

    Matrix sourceWorldMatrix{};
    if (!Try_ResolveWeaponAnchorWorldMatrix(
        *weaponPart,
        sourceHistoryAnchorName,
        sourceHistoryLocalOffset,
        sourceWorldMatrix))
        return false;

    outSample.worldPosition = Vec3::Transform(sourceHistoryLocalOffset, sourceWorldMatrix);
    outSample.worldRotation = Resolve_NormalizedWorldRotation(sourceWorldMatrix);
    return true;
}

bool EffectCom::Try_ResolveTrailAnchorWorldMatrix(
    const PartObject& trailPart,
    const TrailAnchorPointDesc& anchor,
    Matrix& outWorldMatrix) const
{
    const Shared<ModelCom> weaponModel = Resolve_TrailPartModel(&trailPart);
    if (weaponModel == nullptr || anchor.boneName.empty())
        return false;

    const int32 boneIndex = weaponModel->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = weaponModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix anchorLocalMatrix = weaponModel->Get_ModelType() == ModelType::NonAnim
                                     ? Matrix::Identity
                                     : *boneMatrix;
    const Matrix anchorWorldMatrix = anchorLocalMatrix * trailPart.Get_CombinedWorldMatrix();
    outWorldMatrix = anchorWorldMatrix;
    return true;
}

bool EffectCom::Try_ResolveWeaponAnchorWorldMatrix(
    const PartObject& weaponPart,
    const string& anchorName,
    const Vec3& localOffset,
    Matrix& outWorldMatrix) const
{
    const WeaponSourceHistoryAnchorDesc* anchor = nullptr;
    Shared<ModelCom> weaponModel = nullptr;

    if (const Weapon* weapon = dynamic_cast<const Weapon*>(&weaponPart))
    {
        anchor = weapon->Find_SourceHistoryAnchor(anchorName);
        weaponModel = weapon->Get_Model();
    }
    else if (const Mon_Simon_Weapon* simonWeapon = dynamic_cast<const Mon_Simon_Weapon*>(&weaponPart))
    {
        anchor = simonWeapon->Find_SourceHistoryAnchor(anchorName);
        weaponModel = simonWeapon->Get_Model();
    }
    else if (const Mon_Dualliste_Sword_L* swordL = dynamic_cast<const Mon_Dualliste_Sword_L*>(&weaponPart))
    {
        anchor = swordL->Find_SourceHistoryAnchor(anchorName);
        weaponModel = swordL->Get_Model();
    }
    else if (const Mon_Dualliste_Sword_R* swordR = dynamic_cast<const Mon_Dualliste_Sword_R*>(&weaponPart))
    {
        anchor = swordR->Find_SourceHistoryAnchor(anchorName);
        weaponModel = swordR->Get_Model();
    }
    else if (const Mon_PotatoBag_Weapon* potatoBagWeapon = dynamic_cast<const Mon_PotatoBag_Weapon*>(&weaponPart))
    {
        anchor = potatoBagWeapon->Find_SourceHistoryAnchor(anchorName);
        weaponModel = potatoBagWeapon->Get_Model();
    }

    if (anchor == nullptr || anchor->boneName.empty())
        return false;

    if (weaponModel == nullptr)
        return false;

    const int32 boneIndex = weaponModel->Get_BoneIndex(anchor->boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = weaponModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix sourceLocalMatrix = weaponModel->Get_ModelType() == ModelType::NonAnim
                                     ? Matrix::Identity
                                     : *boneMatrix;
    const Matrix sourceWorldMatrix = sourceLocalMatrix * weaponPart.Get_CombinedWorldMatrix();
    outWorldMatrix = Matrix::CreateTranslation(anchor->localPosition) * sourceWorldMatrix;
    return true;
}

Shared<EffectCom> EffectCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : EffectCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> EffectCom::Clone(void* arg)
{
    auto clone = make_shared<EffectCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : EffectCom");
        return nullptr;
    }

    return clone;
}

void EffectCom::Free()
{
    Stop_AllAttachedEffects();

    __super::Free();
}

NS_END
