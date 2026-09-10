#include "EffectEditorPreviewRuntime.h"

#include "ClientInstance.h"
#include "EffectEditorInstance.h"
#include "EffectEditorPreviewDefinitionBuilder.h"
#include "EffectEmitter.h"
#include "EffectInstance.h"
#include "GameInstance.h"
#include "Level.h"
#include "TrailPreviewController.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kPreviewPoolKey = L"EffectEditorPreview";
    constexpr auto kPreviewTrailStrokePoolKey = L"EffectEditorPreviewTrailStroke";

    class TrailPreviewStrokeSampleProvider final : public IEffectTrailSampleProvider
    {
    public:
        TrailPreviewStrokeSampleProvider(
            const Weak<IEffectTrailSampleProvider>& sourceProvider,
            const Shared<bool>& sampleEnabled)
            : _sourceProvider(sourceProvider)
            , _sampleEnabled(sampleEnabled)
        {
        }

        bool Try_GetTrailSample(EffectTrailSample& outSample) const override
        {
            if (nullptr == _sampleEnabled || !*_sampleEnabled)
                return false;

            const Shared<IEffectTrailSampleProvider> sourceProvider = _sourceProvider.lock();
            if (nullptr == sourceProvider)
                return false;

            return sourceProvider->Try_GetTrailSample(outSample);
        }

    private:
        Weak<IEffectTrailSampleProvider> _sourceProvider{};
        Shared<bool> _sampleEnabled{};
    };

    class TrailTipPreviewSampleProvider final : public IEffectSourcePointSampleProvider
    {
    public:
        explicit TrailTipPreviewSampleProvider(const Weak<IEffectTrailSampleProvider>& trailProvider)
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

    class SourceHistoryPreviewSampleProvider final : public IEffectSourcePointSampleProvider
    {
    public:
        SourceHistoryPreviewSampleProvider(
            const Weak<IEffectSourcePointSampleProvider>& sourceProvider,
            const Weak<TrailPreviewController>& previewController,
            bool useSpriteTrailGate)
            : _sourceProvider(sourceProvider)
            , _previewController(previewController)
            , _useSpriteTrailGate(useSpriteTrailGate)
        {
        }

        bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const override
        {
            if (nullptr == EDITOR ||
                !EDITOR->Is_TrailPreviewVisible())
                return false;

            if (_useSpriteTrailGate)
            {
                float sourceRatio = 0.f;
                if (EDITOR->Get_SourceHistorySpriteTrailPreviewGateMode() == TrailPreviewGateMode::Window)
                {
                    const Shared<TrailPreviewController> previewController = _previewController.lock();
                    if (nullptr == previewController ||
                        !previewController->Try_GetCurrentMotionRatio(sourceRatio))
                        return false;
                }

                if (!EDITOR->Is_SourceHistorySpriteTrailPreviewGateOpen(sourceRatio))
                    return false;
            }

            const Shared<IEffectSourcePointSampleProvider> sourceProvider = _sourceProvider.lock();
            if (nullptr == sourceProvider)
                return false;

            return sourceProvider->Try_GetSourcePointSample(outSample);
        }

    private:
        Weak<IEffectSourcePointSampleProvider> _sourceProvider{};
        Weak<TrailPreviewController> _previewController{};
        bool _useSpriteTrailGate{};
    };

    template <typename TData>
    const TData* Find_ModuleData(const AuthoringEmitter& emitter, AuthoringModuleType type)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != type)
                continue;

            if (!module.enabled)
                continue;

            return get_if<TData>(&module.data);
        }

        return nullptr;
    }

    float Resolve_RequiredDuration(const AuthoringEmitter& emitter)
    {
        const RequiredModuleData* required = Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required);
        if (nullptr == required)
            return 0.f;

        if (required->useDurationRange)
            return max(required->duration, required->durationLow);

        return required->duration;
    }

    const RequiredModuleData* Find_RequiredModuleData(const AuthoringEmitter& emitter)
    {
        return Find_ModuleData<RequiredModuleData>(emitter, AuthoringModuleType::Required);
    }

    float Resolve_TrailPreviewGateDuration(const vector<AuthoringEmitter>& emitters)
    {
        float trailDuration = 0.f;
        float anyDuration = 0.f;

        for (const AuthoringEmitter& emitter : emitters)
        {
            if (!emitter.enabled)
                continue;

            const float duration = max(0.f, Resolve_RequiredDuration(emitter));
            if (duration <= 0.f)
                continue;

            anyDuration = max(anyDuration, duration);
            if (emitter.typeData.kind == AuthoringTypeDataKind::Trail)
                trailDuration = max(trailDuration, duration);
        }

        constexpr float fallbackDuration = 1.f;
        return trailDuration > 0.f ? trailDuration : max(fallbackDuration, anyDuration);
    }

    float Resolve_TrailStrokeDrainDuration(const Shared<const EffectDefinition>& definition)
    {
        if (nullptr == definition)
            return 0.25f;

        float duration = 0.f;
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (!emitter.enabled || emitter.kind != EffectEmitterKind::Trail)
                continue;

            const auto* desc = get_if<ComputeTrailEmitterDesc>(&emitter.concreteDesc);
            if (nullptr == desc)
                continue;

            duration = max(duration, desc->segmentLifetime);
        }

        return duration > 0.f ? duration : 0.25f;
    }

    Weak<IEffectSourcePointSampleProvider> Resolve_PreviewSourcePointProvider(
        const Shared<const EffectDefinition>& definition,
        const Shared<IEffectSourcePointSampleProvider>& ribbonProvider,
        const Shared<IEffectSourcePointSampleProvider>& spriteTrailProvider)
    {
        if (nullptr == definition)
            return {};

        bool needsRibbonProvider = false;
        bool needsSpriteTrailProvider = false;
        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (!emitter.enabled)
                continue;

            if (emitter.kind == EffectEmitterKind::Ribbon)
            {
                const auto* ribbonDesc = get_if<ComputeRibbonEmitterDesc>(&emitter.concreteDesc);
                if (ribbonDesc != nullptr && ribbonDesc->sourceMode == EffectSourceHistoryRibbonSourceMode::SelfRoot)
                    needsRibbonProvider = true;
            }
            else if (emitter.kind == EffectEmitterKind::SourceHistorySpriteTrail)
            {
                const auto* spriteTrailDesc = get_if<ComputeSourceHistorySpriteTrailEmitterDesc>(&emitter.concreteDesc);
                if (spriteTrailDesc != nullptr && spriteTrailDesc->sourceMode == EffectSourceHistoryRibbonSourceMode::SelfRoot)
                    needsSpriteTrailProvider = true;
            }
        }

        if (needsRibbonProvider)
            return ribbonProvider;
        if (needsSpriteTrailProvider)
            return spriteTrailProvider;

        return {};
    }
}

HRESULT EffectEditorPreviewRuntime::Initialize(const wstring& layerTag)
{
    if (layerTag.empty())
        return E_FAIL;

    _layerTag = layerTag;
    return S_OK;
}

void EffectEditorPreviewRuntime::Bind_TrailPreviewSources(
    const Weak<IEffectTrailSampleProvider>& trailSampleProvider,
    const Weak<IEffectSourcePointSampleProvider>& sourcePointSampleProvider,
    const Weak<TrailPreviewController>& trailPreviewController)
{
    _trailPreviewSampleProvider = trailSampleProvider;
    _trailPreviewController = trailPreviewController;
    _ribbonPreviewSampleProvider = make_shared<TrailTipPreviewSampleProvider>(trailSampleProvider);
    _sourceHistoryPreviewSampleProvider =
        make_shared<SourceHistoryPreviewSampleProvider>(sourcePointSampleProvider, trailPreviewController, false);
    _sourceHistorySpriteTrailPreviewSampleProvider =
        make_shared<SourceHistoryPreviewSampleProvider>(sourcePointSampleProvider, trailPreviewController, true);
}

void EffectEditorPreviewRuntime::Update(float timeDelta)
{
    Update_PreviewTrailStrokes(timeDelta);
}

HRESULT EffectEditorPreviewRuntime::Restart_FromAuthoring(const vector<AuthoringEmitter>& emitters, const HistoryBudgetData& historyBudget)
{
    Clear_PreviewTrailStrokes();

    const bool splitTrailPreview =
        EDITOR->Get_TrailPreviewGateMode() == TrailPreviewGateMode::Window;
    const Shared<const EffectDefinition> previewDefinition =
        PreviewDefinition::Build_EffectDefinition(emitters, historyBudget, !splitTrailPreview);
    CHECK_NULL(previewDefinition, E_FAIL);
    const Shared<const EffectDefinition> previewTrailDefinition =
        splitTrailPreview ? PreviewDefinition::Build_TrailEffectDefinition(emitters, historyBudget) : nullptr;

    const EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    CHECK_NULL(effectPool, E_FAIL);

    CHECK_FAILED(Restart_FromDefinition(previewDefinition), E_FAIL);
    _currentAuthoringEmitters = emitters;
    Apply_PreviewEmitterTransformOverrides(_currentAuthoringEmitters);
    _currentPreviewDefinition = previewDefinition;
    _currentPreviewTrailDefinition = previewTrailDefinition;
    _wasTrailPreviewGateOpen = false;
    _lastTrailPreviewSourceRatio = 0.f;
    _hasTrailPreviewSourceRatio = false;
    EDITOR->Set_TrailPreviewGateDuration(Resolve_TrailPreviewGateDuration(emitters));
    EDITOR->Reset_TrailPreviewGate();

    return S_OK;
}

HRESULT EffectEditorPreviewRuntime::Reset_Runtime()
{
    if (nullptr == _currentPreviewDefinition)
        return S_OK;

    Clear_PreviewTrailStrokes();
    _wasTrailPreviewGateOpen = false;
    _lastTrailPreviewSourceRatio = 0.f;
    _hasTrailPreviewSourceRatio = false;
    CHECK_FAILED(Restart_FromDefinition(_currentPreviewDefinition), E_FAIL);
    Apply_PreviewEmitterTransformOverrides(_currentAuthoringEmitters);
    return S_OK;
}

bool EffectEditorPreviewRuntime::Is_Finished() const
{
    const Shared<EffectInstance> previewInstance =
        dynamic_pointer_cast<EffectInstance>(_previewPoolHandle.effectObject);
    return previewInstance != nullptr && previewInstance->Is_Finished();
}

void EffectEditorPreviewRuntime::Apply_PreviewEmitterTransformOverrides(const vector<AuthoringEmitter>& emitters)
{
    if (EDITOR == nullptr)
        return;

    const Shared<EffectInstance> previewInstance =
        dynamic_pointer_cast<EffectInstance>(_previewPoolHandle.effectObject);
    if (previewInstance == nullptr)
        return;

    for (const AuthoringEmitter& emitter : emitters)
    {
        if (!emitter.enabled)
            continue;

        const RequiredModuleData* required = Find_RequiredModuleData(emitter);
        const Vec3 authoredPosition = required != nullptr ? required->emitterOrigin : Vec3::Zero;
        const Vec3 authoredRotation = required != nullptr ? required->emitterRotationDegrees : Vec3::Zero;
        const PreviewEmitterTransformOverride* overrideState =
            EDITOR->Find_PreviewEmitterTransformOverride(emitter.id);
        const bool overrideEnabled =
            EffectEditorInstance::Can_UsePreviewEmitterTransform(emitter) &&
            overrideState != nullptr &&
            overrideState->enabled;
        const Vec3 positionOffset = overrideEnabled ? overrideState->localPositionOffset : Vec3::Zero;
        const Vec3 rotationOffset = overrideEnabled ? overrideState->localRotationOffsetDegrees : Vec3::Zero;

        const Shared<EffectEmitter> previewEmitter = previewInstance->Find_EmitterById(emitter.id);
        if (previewEmitter == nullptr)
            continue;

        previewEmitter->Set_RuntimeLocalTransform(
            authoredPosition + positionOffset,
            authoredRotation + rotationOffset
        );
    }
}

bool EffectEditorPreviewRuntime::Try_GetPreviewEmitterWorldMatrix(uint32 emitterId, Matrix& outWorldMatrix) const
{
    const Shared<EffectInstance> previewInstance =
        dynamic_pointer_cast<EffectInstance>(_previewPoolHandle.effectObject);
    if (previewInstance == nullptr)
        return false;

    const Shared<EffectEmitter> previewEmitter = previewInstance->Find_EmitterById(emitterId);
    if (previewEmitter == nullptr || previewEmitter->Get_Transform() == nullptr)
        return false;

    outWorldMatrix = previewEmitter->Get_Transform()->Get_WorldMatrix();
    return true;
}

void EffectEditorPreviewRuntime::Clear()
{
    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    if (nullptr == effectPool)
        return;

    Clear_PreviewTrailStrokes();
    effectPool->Release(_previewPoolHandle);
    _previewPoolHandle = {};
    _currentAuthoringEmitters.clear();
    effectPool->Discard_Inactive(kPreviewPoolKey);
    effectPool->Discard_Inactive(kPreviewTrailStrokePoolKey);
}

HRESULT EffectEditorPreviewRuntime::Restart_FromDefinition(const Shared<const EffectDefinition>& previewDefinition)
{
    CHECK_NULL(previewDefinition, E_FAIL);

    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    CHECK_NULL(effectPool, E_FAIL);

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = kPreviewPoolKey;
    acquireDesc.definition = previewDefinition;
    acquireDesc.layerLevelIndex = ETOI(LevelType::Static);
    acquireDesc.layerTag = _layerTag;
    acquireDesc.trailSampleProvider = _trailPreviewSampleProvider;
    acquireDesc.trailSampleGroupKey = EffectHistorySourceGroupKey{
        EffectHistorySourceGroupKind::TrailPairHistory,
        "EffectEditorPreview|TrailPair"
    };
    acquireDesc.sourcePointSampleProvider = Resolve_PreviewSourcePointProvider(
        previewDefinition,
        _sourceHistoryPreviewSampleProvider,
        _sourceHistorySpriteTrailPreviewSampleProvider
    );
    if (EDITOR != nullptr && EDITOR->Is_TrailPreviewVisible())
        acquireDesc.ribbonSourcePointSampleProvider = _ribbonPreviewSampleProvider;
    acquireDesc.sourcePointGroupKey = EffectHistorySourceGroupKey{
        EffectHistorySourceGroupKind::SourcePointHistory,
        "EffectEditorPreview|SourcePoint"
    };
    acquireDesc.autoReleaseOnFinished = false;

    if (nullptr != _previewPoolHandle.effectObject)
    {
        if (SUCCEEDED(effectPool->Reload_Attached(acquireDesc, _previewPoolHandle)))
            return S_OK;

        LOG_WARN("[EffectEditor] Preview reload failed. Falling back to acquire path.");
        effectPool->Release(_previewPoolHandle);
        _previewPoolHandle = {};
    }

    CHECK_FAILED(effectPool->Acquire(acquireDesc, _previewPoolHandle), E_FAIL);
    return S_OK;
}

void EffectEditorPreviewRuntime::Update_PreviewTrailStrokes(float timeDelta)
{
    if (EDITOR->Get_TrailPreviewGateMode() != TrailPreviewGateMode::Window ||
        !EDITOR->Is_TrailPreviewVisible() ||
        nullptr == _currentPreviewTrailDefinition)
    {
        Clear_PreviewTrailStrokes();
        _wasTrailPreviewGateOpen = false;
        _hasTrailPreviewSourceRatio = false;
        return;
    }

    if (fabs(EDITOR->Get_TrailPreviewGateEndRatio() - EDITOR->Get_TrailPreviewGateStartRatio()) <= 0.0001f)
    {
        Drain_ActivePreviewTrailStrokes();
        _wasTrailPreviewGateOpen = false;
        _hasTrailPreviewSourceRatio = false;
    }
    else
    {
        float sourceRatio = 0.f;
        const Shared<TrailPreviewController> trailPreviewController = _trailPreviewController.lock();
        const bool hasSourceRatio =
            nullptr != trailPreviewController &&
            trailPreviewController->Try_GetCurrentMotionRatio(sourceRatio);

        if (!hasSourceRatio)
        {
            Drain_ActivePreviewTrailStrokes();
            _wasTrailPreviewGateOpen = false;
            _hasTrailPreviewSourceRatio = false;
        }
        else
        {
            const bool gateOpen = EDITOR->Is_TrailPreviewGateOpen(sourceRatio);
            if (gateOpen && !_wasTrailPreviewGateOpen)
            {
                if (FAILED(Start_PreviewTrailStroke()))
                    LOG_WARN("[EffectEditor] Failed to start trail preview stroke.");
            }
            else if (!gateOpen && _wasTrailPreviewGateOpen)
                Drain_ActivePreviewTrailStrokes();

            _wasTrailPreviewGateOpen = gateOpen;
            _lastTrailPreviewSourceRatio = sourceRatio;
            _hasTrailPreviewSourceRatio = true;
        }
    }

    const float safeTimeDelta = max(0.f, timeDelta);
    for (int32 runtimeIndex = static_cast<int32>(_previewTrailStrokes.size()) - 1; runtimeIndex >= 0; --runtimeIndex)
    {
        PreviewTrailStrokeRuntime& runtime = _previewTrailStrokes[runtimeIndex];
        if (!runtime.isDraining)
            continue;

        runtime.drainElapsed += safeTimeDelta;
        if (runtime.drainElapsed < runtime.drainDuration)
            continue;

        EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
        if (nullptr != effectPool)
            effectPool->Release(runtime.poolHandle);

        _previewTrailStrokes.erase(_previewTrailStrokes.begin() + runtimeIndex);
    }
}

HRESULT EffectEditorPreviewRuntime::Start_PreviewTrailStroke()
{
    CHECK_NULL(_currentPreviewTrailDefinition, E_FAIL);

    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    CHECK_NULL(effectPool, E_FAIL);

    PreviewTrailStrokeRuntime runtime{};
    runtime.sampleEnabled = make_shared<bool>(true);
    runtime.sampleProvider = make_shared<TrailPreviewStrokeSampleProvider>(
        _trailPreviewSampleProvider,
        runtime.sampleEnabled
    );
    CHECK_NULL(runtime.sampleProvider, E_FAIL);
    runtime.drainDuration = Resolve_TrailStrokeDrainDuration(_currentPreviewTrailDefinition);

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = kPreviewTrailStrokePoolKey;
    acquireDesc.definition = _currentPreviewTrailDefinition;
    acquireDesc.layerLevelIndex = ETOI(LevelType::Static);
    acquireDesc.layerTag = _layerTag;
    acquireDesc.trailSampleProvider = runtime.sampleProvider;
    acquireDesc.trailSampleGroupKey = EffectHistorySourceGroupKey{
        EffectHistorySourceGroupKind::TrailPairHistory,
        "EffectEditorPreview|TrailStroke"
    };
    acquireDesc.autoReleaseOnFinished = false;

    EffectTrailSample initialSample{};
    if (runtime.sampleProvider->Try_GetTrailSample(initialSample))
        acquireDesc.worldPosition = initialSample.baseWorldPosition;

    CHECK_FAILED(effectPool->Acquire(acquireDesc, runtime.poolHandle), E_FAIL);
    _previewTrailStrokes.push_back(std::move(runtime));
    return S_OK;
}

void EffectEditorPreviewRuntime::Begin_PreviewTrailStrokeDrain(PreviewTrailStrokeRuntime& runtime)
{
    if (runtime.isDraining)
        return;

    if (nullptr != runtime.sampleEnabled)
        *runtime.sampleEnabled = false;

    runtime.isDraining = true;
    runtime.drainElapsed = 0.f;
}

void EffectEditorPreviewRuntime::Drain_ActivePreviewTrailStrokes()
{
    for (PreviewTrailStrokeRuntime& runtime : _previewTrailStrokes)
        Begin_PreviewTrailStrokeDrain(runtime);
}

void EffectEditorPreviewRuntime::Clear_PreviewTrailStrokes()
{
    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;

    if (nullptr != effectPool)
    {
        for (const PreviewTrailStrokeRuntime& runtime : _previewTrailStrokes)
            effectPool->Release(runtime.poolHandle);
    }

    _previewTrailStrokes.clear();
    _wasTrailPreviewGateOpen = false;
    _lastTrailPreviewSourceRatio = 0.f;
    _hasTrailPreviewSourceRatio = false;
}

NS_END
