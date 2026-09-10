#include "EffectInstance.h"

namespace
{
uint32 Generate_EffectPlaybackSeed()
{
    static uint32 state{ 0x9E3779B9u };
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state != 0u ? state : 0xA341316Cu;
}

class SharedTrailSampleProvider final : public IEffectTrailSampleProvider
{
public:
    SharedTrailSampleProvider(
        Weak<IEffectTrailSampleProvider> sourceProvider,
        const uint64* frameSerial,
        EffectHistorySourceGroupKey sourceGroupKey)
        : _sourceProvider(move(sourceProvider))
        , _frameSerial(frameSerial)
        , _sourceGroupKey(move(sourceGroupKey))
    {
    }

    bool Try_GetTrailSample(EffectTrailSample& outSample) const override
    {
        const uint64 currentFrameSerial = _frameSerial != nullptr ? *_frameSerial : 0u;
        if (!_hasCachedFrame || _cachedFrameSerial != currentFrameSerial)
        {
            _cachedFrameSerial = currentFrameSerial;
            _hasCachedFrame = true;
            _stats.sourceRequestCount++;

            const Shared<IEffectTrailSampleProvider> sourceProvider = _sourceProvider.lock();
            _cachedResult = sourceProvider != nullptr && sourceProvider->Try_GetTrailSample(_cachedSample);
        }
        else
        {
            _stats.cacheHitCount++;
            _stats.savedRequestCount++;
        }

        if (!_cachedResult)
            return false;

        outSample = _cachedSample;
        return true;
    }

private:
    Weak<IEffectTrailSampleProvider> _sourceProvider{};
    const uint64* _frameSerial{};
    EffectHistorySourceGroupKey _sourceGroupKey{};
    mutable EffectHistorySourceGroupStats _stats{ EffectHistorySourceGroupKind::TrailPairHistory };
    mutable uint64 _cachedFrameSerial{};
    mutable bool _hasCachedFrame{};
    mutable bool _cachedResult{};
    mutable EffectTrailSample _cachedSample{};
};

class SharedSourcePointSampleProvider final : public IEffectSourcePointSampleProvider
{
public:
    SharedSourcePointSampleProvider(
        Weak<IEffectSourcePointSampleProvider> sourceProvider,
        const uint64* frameSerial,
        EffectHistorySourceGroupKey sourceGroupKey)
        : _sourceProvider(move(sourceProvider))
        , _frameSerial(frameSerial)
        , _sourceGroupKey(move(sourceGroupKey))
    {
    }

    bool Try_GetSourcePointSample(EffectSourcePointSample& outSample) const override
    {
        const uint64 currentFrameSerial = _frameSerial != nullptr ? *_frameSerial : 0u;
        if (!_hasCachedFrame || _cachedFrameSerial != currentFrameSerial)
        {
            _cachedFrameSerial = currentFrameSerial;
            _hasCachedFrame = true;
            _stats.sourceRequestCount++;

            const Shared<IEffectSourcePointSampleProvider> sourceProvider = _sourceProvider.lock();
            _cachedResult = sourceProvider != nullptr && sourceProvider->Try_GetSourcePointSample(_cachedSample);
        }
        else
        {
            _stats.cacheHitCount++;
            _stats.savedRequestCount++;
        }

        if (!_cachedResult)
            return false;

        outSample = _cachedSample;
        return true;
    }

private:
    Weak<IEffectSourcePointSampleProvider> _sourceProvider{};
    const uint64* _frameSerial{};
    EffectHistorySourceGroupKey _sourceGroupKey{};
    mutable EffectHistorySourceGroupStats _stats{ EffectHistorySourceGroupKind::SourcePointHistory };
    mutable uint64 _cachedFrameSerial{};
    mutable bool _hasCachedFrame{};
    mutable bool _cachedResult{};
    mutable EffectSourcePointSample _cachedSample{};
};
}

IMPLEMENT_REFLECTION(EffectInstance)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "EffectInstance";
    info.category = "Effect";
    PROPERTY_BOOL("Loop", _loop);
    return true;
}

EffectInstance::EffectInstance(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

EffectInstance::EffectInstance(const EffectInstance& prototype)
    : GameObject{ prototype }
{
}

EffectInstance::~EffectInstance()
{
    Free();
}

HRESULT EffectInstance::Initialize_Prototype()
{
    return S_OK;
}

HRESULT EffectInstance::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<EffectInstanceDesc*>(arg);

    _definition = _desc.definition;
    _effectPlaybackSeed = Generate_EffectPlaybackSeed();

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Emitters(), E_FAIL);

    return S_OK;
}

void EffectInstance::BeginPlay()
{
    __super::BeginPlay();

    for (const auto& emitter : _emitters)
    {
        if (emitter)
            (void)emitter->Try_BeginPlay();
    }

    (void)Play();
}

void EffectInstance::Priority_Update(float timeDelta)
{
    if (!Is_Playing())
        return;

    Sync_Emitters();

    if (_isPlaybackPaused)
        return; // pause 중에도 transform/visible 동기화는 유지하고 emitter tick만 막는다.

    const float effectTimeDelta = timeDelta * _playbackSpeed;
    for (const auto& emitter : _emitters)
    {
        if (emitter && emitter->Is_EmitterEnabled())
            emitter->Priority_Update(effectTimeDelta);
    }
}

void EffectInstance::Update(float timeDelta)
{
    if (!Is_Playing())
        return;

    Sync_Emitters();

    if (_isPlaybackPaused)
        return; // 트레일/파티클 history와 material elapsed를 현재 프레임에 고정한다.

    Advance_SharedHistoryFrame();

    const float effectTimeDelta = timeDelta * _playbackSpeed;
    for (const auto& emitter : _emitters)
    {
        if (emitter && emitter->Is_EmitterEnabled())
            emitter->Update(effectTimeDelta);
    }
}

void EffectInstance::Late_Update(float timeDelta)
{
    if (!Is_Playing())
    {
        Restart_LoopIfNeeded();
        return;
    }

    Sync_Emitters();

    if (_isPlaybackPaused)
    {
        for (const auto& emitter : _emitters)
        {
            if (emitter && emitter->Is_EmitterEnabled())
                emitter->Late_Update(0.f); // render 등록은 유지하고 시간 진행만 막는다.
        }

        return;
    }

    const float effectTimeDelta = timeDelta * _playbackSpeed;
    for (const auto& emitter : _emitters)
    {
        if (emitter && emitter->Is_EmitterEnabled())
            emitter->Late_Update(effectTimeDelta);
    }

    Evaluate_Finished();
    Restart_LoopIfNeeded();
}

HRESULT EffectInstance::Render()
{
    return S_OK;
}

HRESULT EffectInstance::Play()
{
    _playbackState = EffectPlaybackState::Playing;
    _isPlaybackPaused = false; // pool 재사용 시 이전 튜토리얼 pause가 남지 않게 초기화한다.
    Set_Visible(true);
    Sync_Emitters();

    return S_OK;
}

void EffectInstance::Stop()
{
    Bind_TrailSampleProvider({});
    Bind_SourcePointSampleProvider({});
    Bind_RibbonSourcePointSampleProvider({});
    Clear_SharedHistoryCache();
    _isPlaybackPaused = false; // Stop은 완전 정지이므로 pause 상태도 같이 해제한다.
    _playbackState = EffectPlaybackState::Stopped;
    Set_Visible(false);
}

HRESULT EffectInstance::Reset()
{
    _effectPlaybackSeed = Generate_EffectPlaybackSeed();
    _playbackState = EffectPlaybackState::Stopped;
    _isPlaybackPaused = false; // Reset 이후 새 재생은 pause되지 않은 상태로 시작한다.
    Set_Visible(false);
    Set_MaterialRevealOverride(1.f, 0.f);
    Advance_SharedHistoryFrame();
    Sync_Emitters();

    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (nullptr == emitter)
            continue;

        emitter->Set_EffectPlaybackSeed(_effectPlaybackSeed);
        CHECK_FAILED(emitter->Reset_ForEffectReplay(), E_FAIL);
    }

    return S_OK;
}

HRESULT EffectInstance::Reload_Definition(const Shared<const EffectDefinition>& definition)
{
    CHECK_NULL(definition, E_FAIL);

    Stop();
    _definition = definition;
    _desc.definition = definition;

    if (!Try_ReloadEmitters(definition))
        CHECK_FAILED(Ready_Emitters(), E_FAIL);

    return S_OK;
}

bool EffectInstance::Is_Playing() const
{
    return EffectPlaybackState::Playing == _playbackState;
}

bool EffectInstance::Is_Finished() const
{
    return EffectPlaybackState::Finished == _playbackState;
}

void EffectInstance::Set_PlaybackSpeed(float speed)
{
    _playbackSpeed = speed < 0.f ? 0.f : speed > 10.f ? 10.f : speed;
}

void EffectInstance::Set_MaterialRevealOverride(float alphaMultiplier, float alphaErosion)
{
    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (emitter == nullptr)
            continue;

        emitter->Set_MaterialRevealOverride(alphaMultiplier, alphaErosion);
    }
}

void EffectInstance::Set_MaterialTintOverride(const vector<uint32>& emitterIds, const Vec4& targetTint, float strength)
{
    const float clampedStrength = clamp(strength, 0.f, 1.f);

    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (emitter == nullptr)
            continue;

        if (!emitterIds.empty() &&
            ranges::find(emitterIds, emitter->Get_EmitterId()) == emitterIds.end())
            continue;

        emitter->Set_MaterialTintOverride(targetTint, clampedStrength);
    }
}

void EffectInstance::Bind_TrailSampleProvider(
    const Weak<IEffectTrailSampleProvider>& provider,
    const EffectHistorySourceGroupKey& sourceGroupKey)
{
    Weak<IEffectTrailSampleProvider> boundProvider = provider;
    _trailSourceGroupKey = sourceGroupKey;
    _sharedTrailSampleProvider.reset();

    if (Is_SharedSourceHistoryEnabled() &&
        sourceGroupKey.kind == EffectHistorySourceGroupKind::TrailPairHistory &&
        provider.lock() != nullptr)
    {
        _sharedTrailSampleProvider = make_shared<SharedTrailSampleProvider>(
            provider,
            &_sharedHistoryFrameSerial,
            sourceGroupKey
        );
        boundProvider = _sharedTrailSampleProvider;
    }

    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (nullptr == emitter)
            continue;

        emitter->Bind_TrailSampleProvider(boundProvider);
    }
}

void EffectInstance::Bind_SourcePointSampleProvider(
    const Weak<IEffectSourcePointSampleProvider>& provider,
    const EffectHistorySourceGroupKey& sourceGroupKey)
{
    Weak<IEffectSourcePointSampleProvider> boundProvider = provider;
    _sourcePointGroupKey = sourceGroupKey;
    _sharedSourcePointSampleProvider.reset();

    if (Is_SharedSourceHistoryEnabled() &&
        sourceGroupKey.kind == EffectHistorySourceGroupKind::SourcePointHistory &&
        provider.lock() != nullptr)
    {
        _sharedSourcePointSampleProvider = make_shared<SharedSourcePointSampleProvider>(
            provider,
            &_sharedHistoryFrameSerial,
            sourceGroupKey
        );
        boundProvider = _sharedSourcePointSampleProvider;
    }

    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (nullptr == emitter)
            continue;

        emitter->Bind_SourcePointSampleProvider(boundProvider);
    }
}

void EffectInstance::Bind_RibbonSourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider)
{
    _ribbonSourcePointSampleProvider = provider;

    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (nullptr == emitter)
            continue;

        emitter->Bind_RibbonSourcePointSampleProvider(provider);
    }
}

Shared<EffectEmitter> EffectInstance::Find_EmitterById(uint32 emitterId) const
{
    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (emitter != nullptr && emitter->Get_EmitterId() == emitterId)
            return emitter;
    }

    return nullptr;
}

bool EffectInstance::Is_SharedSourceHistoryEnabled() const
{
    return _definition != nullptr && _definition->historyBudget.sharedSourceHistory;
}

void EffectInstance::Advance_SharedHistoryFrame()
{
    ++_sharedHistoryFrameSerial;
    if (_sharedHistoryFrameSerial == 0u)
        _sharedHistoryFrameSerial = 1u;
}

void EffectInstance::Clear_SharedHistoryCache()
{
    ++_sharedHistoryFrameSerial;
    _trailSourceGroupKey = {};
    _sourcePointGroupKey = {};
    _sharedTrailSampleProvider.reset();
    _sharedSourcePointSampleProvider.reset();
}

HRESULT EffectInstance::Ready_Emitters()
{
    Clear_Emitters();

    if (nullptr == _definition)
        return E_FAIL;

    const Shared<GameObject> rootOwner = GetSharedPtr<GameObject>();

    for (const EffectEmitterDefinition& emitterDefinition : _definition->emitters)
    {
        if (!emitterDefinition.enabled)
            continue;

        Shared<EffectEmitter> emitter = EffectEmitterFactory::Create_EffectEmitter(emitterDefinition);
        CHECK_NULL(emitter, E_FAIL);

        EffectEmitter::EffectEmitterRuntimeDesc runtimeDesc{};
        runtimeDesc.effectOwner = rootOwner;
        runtimeDesc.emitterId = emitterDefinition.id;
        runtimeDesc.emitterName = emitterDefinition.name;
        runtimeDesc.localPosition = emitterDefinition.localPosition;
        runtimeDesc.localRotationDegrees = emitterDefinition.localRotationDegrees;
        runtimeDesc.useLocalSpace = emitterDefinition.useLocalSpace;
        runtimeDesc.enabled = emitterDefinition.enabled;
        runtimeDesc.effectPlaybackSeed = _effectPlaybackSeed;

        CHECK_FAILED(emitter->Attach_ToEffect(runtimeDesc), E_FAIL);
        _emitters.push_back(emitter);
    }

    return S_OK;
}

bool EffectInstance::Try_ReloadEmitters(const Shared<const EffectDefinition>& definition)
{
    if (nullptr == definition)
        return false;

    vector<const EffectEmitterDefinition*> enabledDefinitions{};
    enabledDefinitions.reserve(definition->emitters.size());
    for (const EffectEmitterDefinition& emitterDefinition : definition->emitters)
    {
        if (emitterDefinition.enabled)
            enabledDefinitions.push_back(&emitterDefinition);
    }

    if (enabledDefinitions.size() != _emitters.size())
        return false;

    for (size_t index = 0; index < enabledDefinitions.size(); ++index)
    {
        const Shared<EffectEmitter>& emitter = _emitters[index];
        const EffectEmitterDefinition& emitterDefinition = *enabledDefinitions[index];
        if (nullptr == emitter)
            return false;
        if (emitter->Get_EmitterId() != emitterDefinition.id)
            return false;
        if (!emitter->Can_ReloadDefinition(emitterDefinition))
            return false;
    }

    for (size_t index = 0; index < enabledDefinitions.size(); ++index)
    {
        if (FAILED(_emitters[index]->Reload_Definition(*enabledDefinitions[index])))
            return false;
    }

    return true;
}

void EffectInstance::Sync_Emitters()
{
    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (nullptr == emitter)
            continue;

        emitter->Set_Visible(Is_Visible());
        emitter->Sync_FromEffectOwner();
    }
}

void EffectInstance::Evaluate_Finished()
{
    bool hasEnabledEmitter = false;

    for (const Shared<EffectEmitter>& emitter : _emitters)
    {
        if (nullptr == emitter || !emitter->Is_EmitterEnabled())
            continue;

        hasEnabledEmitter = true;

        if (!emitter->Is_EffectFinished())
            return;
    }

    if (hasEnabledEmitter)
    {
        _playbackState = EffectPlaybackState::Finished;
        Set_Visible(false);
    }
}

void EffectInstance::Restart_LoopIfNeeded()
{
    if (!_loop || !Is_Finished())
        return;

    if (SUCCEEDED(Reset()))
        (void)Play();
}

void EffectInstance::Clear_Emitters()
{
    for (Shared<EffectEmitter>& emitter : _emitters)
    {
        if (emitter != nullptr)
            emitter->Free();
    }

    _emitters.clear();
}

Shared<EffectInstance> EffectInstance::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectInstance>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : EffectInstance");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> EffectInstance::Clone(void* arg)
{
    auto instance = make_shared<EffectInstance>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : EffectInstance");
        MSG_BOX("Failed to Clone : EffectInstance");
        return nullptr;
    }

    return instance;
}

void EffectInstance::Free()
{
    Clear_Emitters();

    __super::Free();
}
