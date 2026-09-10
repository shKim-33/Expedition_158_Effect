#pragma once
#include "EffectEmitter.h"
#include "EffectEmitterFactory.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL EffectInstance final : public GameObject
{
    GENERATED_GAMEOBJECT(EffectInstance, ObjectType::Effect)

public:
    EffectInstance(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectInstance(const EffectInstance& prototype);
    ~EffectInstance() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public: //## Behavior::Lifecycle
    HRESULT Play();
    void Stop();
    HRESULT Reset();
    // 기존 root object를 유지한 채 새 definition으로 child emitter 구성을 다시 생성
    HRESULT Reload_Definition(const Shared<const EffectDefinition>& definition);
    // 현재 update/render fan-out 대상인지 반환
    bool Is_Playing() const;
    // one-shot 완료 후 pool 반환 후보인지 반환
    bool Is_Finished() const;

public: //## Behavior::PlaybackOptions
    void Set_Loop(bool loop) { _loop = loop; }
    bool Is_Loop() const { return _loop; }
    // 튜토리얼 freeze는 Stop이 아니라 update만 정지
    void Set_PlaybackPaused(bool paused) { _isPlaybackPaused = paused; }
    bool Is_PlaybackPaused() const { return _isPlaybackPaused; }
    void Set_PlaybackSpeed(float speed);
    float Get_PlaybackSpeed() const { return _playbackSpeed; }
    void Set_MaterialRevealOverride(float alphaMultiplier, float alphaErosion);
    void Set_MaterialTintOverride(const vector<uint32>& emitterIds, const Vec4& targetTint, float strength);

public: //## Behavior::ProviderBinding
    // 현재 acquire context가 공급한 trail sample provider를 child emitter에 다시 전파
    void Bind_TrailSampleProvider(
        const Weak<IEffectTrailSampleProvider>& provider,
        const EffectHistorySourceGroupKey& sourceGroupKey = {});
    // 현재 acquire context가 공급한 source point sample provider를 child emitter에 다시 전파
    void Bind_SourcePointSampleProvider(
        const Weak<IEffectSourcePointSampleProvider>& provider,
        const EffectHistorySourceGroupKey& sourceGroupKey = {});
    void Bind_RibbonSourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider);

public: //## Accessors::ChildEmitter
    // 같은 effect root 안의 child emitter를 follower source id로 조회
    Shared<EffectEmitter> Find_EmitterById(uint32 emitterId) const;

private: //## Types::Lifecycle
    enum class EffectPlaybackState : uint8
    {
        Stopped,  // 재생 전이거나 명시 정지된 상태
        Playing,  // child emitter update/render fan-out을 수행하는 상태
        Finished, // 모든 자동 완료형 child emitter가 완료된 상태
    };

private: //## Data::Definition
    // root clone 시 전달받는 runtime desc
    EffectInstanceDesc _desc{};
    // root가 재생할 정적 effect 정의
    Shared<const EffectDefinition> _definition{};

private: //## Data::Lifecycle
    // pool 재사용 판단에 쓰는 root 재생 상태
    EffectPlaybackState _playbackState{ EffectPlaybackState::Stopped };
    // 에디터 배치 이펙트가 끝난 뒤 같은 definition으로 다시 재생할지 결정
    bool _loop{ false };
    // 현재 프레임을 화면에 남긴 채 emitter 시간 진행만 방지
    bool _isPlaybackPaused{ false };
    // emitter 시간 진행 배율. 0이면 transform follow는 유지하고 visual time만 정지
    float _playbackSpeed{ 1.f };
    // effect root 재생 1회 단위 random seed
    uint32 _effectPlaybackSeed{ 0 };

private: //## Data::Children
    // root가 직접 소유하는 child emitter 목록
    vector<Shared<EffectEmitter>> _emitters{};

private: //## Data::SharedHistory
    uint64 _sharedHistoryFrameSerial{};
    EffectHistorySourceGroupKey _trailSourceGroupKey{};
    EffectHistorySourceGroupKey _sourcePointGroupKey{};
    Shared<IEffectTrailSampleProvider> _sharedTrailSampleProvider{};
    Shared<IEffectSourcePointSampleProvider> _sharedSourcePointSampleProvider{};
    Weak<IEffectSourcePointSampleProvider> _ribbonSourcePointSampleProvider{};

private: //## Helper::Children
    // definition emitters를 concrete runtime child로 생성
    HRESULT Ready_Emitters();
    // 기존 child가 새 definition을 받을 수 있으면 재생성 없이 갱신
    bool Try_ReloadEmitters(const Shared<const EffectDefinition>& definition);
    // root visibility/transform을 child emitter에 fan-out
    void Sync_Emitters();
    // child emitter 완료 상태를 종합해 root 완료 상태를 갱신
    void Evaluate_Finished();
    // Loop가 켜진 배치 이펙트를 완료 직후 다시 재생
    void Restart_LoopIfNeeded();
    // root가 소유한 child 목록을 정리
    void Clear_Emitters();
    bool Is_SharedSourceHistoryEnabled() const;
    void Advance_SharedHistoryFrame();
    void Clear_SharedHistoryCache();

public:
    static Shared<EffectInstance> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
