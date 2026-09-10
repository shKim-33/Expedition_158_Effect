#pragma once
#include "GameObject.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)

// Effect root 아래에서 실제 simulation/render를 수행하는 하위 emitter runtime 공통 부모
class ENGINE_DLL EffectEmitter abstract : public GameObject
{
public: //## Types::RuntimeDesc
    // effect root가 child emitter 생성 직후 주입하는 공통 runtime 상태
    struct EffectEmitterRuntimeDesc
    {
        Shared<GameObject> effectOwner{};           // 이 emitter를 소유하는 effect root
        uint32 emitterId{};                         // definition에서 내려온 emitter id
        string emitterName{};                       // definition에서 내려온 emitter 이름
        Vec3 localPosition{ 0.f, 0.f, 0.f };        // root 기준 local 위치
        Vec3 localRotationDegrees{ 0.f, 0.f, 0.f }; // root 기준 local 회전
        bool useLocalSpace{ false };                // true면 root transform을 추종
        bool enabled{ true };                       // root가 child를 활성 emitter로 볼지 여부
        uint32 effectPlaybackSeed{ 0 };             // effect root 재생 1회 단위 random seed
    };

public:
    EffectEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectEmitter(const EffectEmitter& prototype);
    ~EffectEmitter() override = default;

public: //## Behavior::RootBinding
    // root가 생성 직후 공통 runtime 상태를 주입
    HRESULT Attach_ToEffect(const EffectEmitterRuntimeDesc& runtimeDesc);
    // root와 local/world 정책을 기준으로 emitter transform을 동기화
    void Sync_FromEffectOwner();
    // root replay seed를 child emitter에 갱신
    void Set_EffectPlaybackSeed(uint32 effectPlaybackSeed);
    // editor/runtime preview가 definition 재생성 없이 emitter local transform만 임시 갱신할 때 사용
    void Set_RuntimeLocalTransform(const Vec3& localPosition, const Vec3& localRotationDegrees);

public: //## Behavior::Lifecycle
    // effect root가 재사용 전에 emitter runtime 상태를 초기화할 때 호출
    virtual HRESULT Reset_ForEffectReplay();
    // 기존 child를 유지한 채 새 definition을 적용할 수 있는지 판단
    virtual bool Can_ReloadDefinition(const EffectEmitterDefinition& emitterDefinition) const;
    // 기존 child resource를 가능한 한 유지하고 새 definition payload를 적용
    virtual HRESULT Reload_Definition(const EffectEmitterDefinition& emitterDefinition);
    // effect root가 자연 완료 여부를 종합할 때 사용하는 완료 상태
    virtual bool Is_EffectFinished() const;

public: //## Behavior::ProviderBinding
    // trail sample provider를 acquire 시점 context로 다시 바인딩
    virtual void Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>& provider);
    // source history sample provider를 acquire 시점 context로 다시 바인딩
    virtual void Bind_SourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider);
    // Ribbon SelfRoot emitters consume this independently from source-history sprite trails.
    virtual void Bind_RibbonSourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider);
    // follower ribbon이 같은 effect 내부 source particle 위치를 요청할 때 쓰는 좁은 query
    virtual bool Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>& outPoints, uint32 maxPointCount) const;

public: //## Behavior::MaterialRuntime
    // attached effect 등장 연출에서 material alpha/erosion만 일시적으로 덮어씀
    virtual void Set_MaterialRevealOverride(float alphaMultiplier, float alphaErosion);
    virtual void Set_MaterialTintOverride(const Vec4& targetTint, float strength);

public: //## Behavior::Sort
    // Blend 정렬 시 emitter를 대표할 월드 위치를 반환
    virtual bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const;
    // Blend 정렬 시 sort layer 안에서 적용할 정렬 정책을 반환
    virtual EffectSortPolicy Get_BlendSortPolicy() const;
    // Blend 정렬 시 우선 적용할 제작자 layer를 반환
    virtual int32 Get_BlendSortLayer() const;
    // Blend 정렬 key에 적용할 제작자용 앞뒤 보정값을 반환
    virtual float Get_BlendSortBias() const;

public: //## Accessors::Runtime
    // root fan-out 시 enabled gate로 사용
    bool Is_EmitterEnabled() const { return _enabled; }
    uint32 Get_EmitterId() const { return _emitterId; }
    const string& Get_EmitterName() const { return _emitterName; }

protected: //## Data::Runtime
    // 이 emitter를 소유하는 effect root
    Weak<GameObject> _effectOwner{};
    // definition 기반 emitter id
    uint32 _emitterId{};
    // definition 기반 emitter 이름
    string _emitterName{};
    // root 기준 local 위치
    Vec3 _localPosition{ 0.f, 0.f, 0.f };
    // root 기준 local 회전
    Vec3 _localRotationDegrees{ 0.f, 0.f, 0.f };
    // true면 root transform을 추종
    bool _useLocalSpace{ false };
    // root 수준 enabled 플래그
    bool _enabled{ true };
    // effect root 재생 1회 단위 random seed
    uint32 _effectPlaybackSeed{ 0 };

protected: //## Hook::TransformSync
    // concrete emitter가 root sync 직후 내부 payload를 갱신할 때 override
    virtual void On_EffectTransformSynced() {}

public:
    Shared<GameObject> Clone(void* arg) override = 0;
};

NS_END
