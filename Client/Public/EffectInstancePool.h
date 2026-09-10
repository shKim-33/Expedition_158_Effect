#pragma once

#include "Base.h"
#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class GameObject;
class EffectInstance;
NS_END

NS_BEGIN(Client)


// 자동 완료 반환은 fire-and-forget entry에만 허용하고, owner-tracked entry는 consumer의 명시 release를 정본으로 유지

// EffectInstance root 단위 acquire/release/reuse를 관리하는 Client-local pool helper
class EffectInstancePool final : public Base
{
public: //## Types::Public

    // Acquire()에 전달하는 "이 EffectInstance 1개를 어떻게 준비할지" 입력값
    // 여기서 instance 단위는 EffectEmitter 1개가 아니라, 여러 emitter를 소유한 EffectInstance root 1개
    struct AcquireDesc
    {
        wstring effectName{};                                               // effect definition 조회 이름이자 pool 재사용 key
        Shared<const EffectDefinition> definition{};                        // effectName lookup을 건너뛰고 직접 재생. editor preview 갱신용
        uint32 layerLevelIndex{ ETOI(LevelType::Static) };                  // spawn / 제거 시 접근할 level layer index
        wstring layerTag{};                                                 // 재사용 가능한 instance가 없을 때 새로 등록할 layer tag
        Vec3 worldPosition{ Vec3::Zero };                                   // acquire된 EffectInstance root GameObject에 적용할 시작 월드 위치
        Vec3 scale{ Vec3::One };                                            // acquire된 EffectInstance root GameObject에 적용할 시작 스케일
        Quat worldRotation{ Quat::Identity };                               // acquire된 EffectInstance root GameObject에 적용할 시작 월드 회전
        float playbackSpeed{ 1.f };                                         // acquire된 EffectInstance의 emitter 시간 진행 배율
        Weak<IEffectTrailSampleProvider> trailSampleProvider{};             // trail base/tip sample provider. pool 아닌 owner가 전달
        EffectHistorySourceGroupKey trailSampleGroupKey{};                  // shared history 사용 시 trail provider를 구분하는 source group key
        Weak<IEffectSourcePointSampleProvider> sourcePointSampleProvider{}; // source history emitter가 단일 source sample을 얻는 provider
        EffectHistorySourceGroupKey sourcePointGroupKey{};                  // shared history에서 source point provider를 구분하는 key
        Weak<IEffectSourcePointSampleProvider> ribbonSourcePointSampleProvider{};
        bool autoReleaseOnFinished{ true };                                 // true면 Release_Finished()가 완료된 instance를 자동 반환할 수 있음
    };

    // acquire 결과로 caller가 보관하는 외부 handle
    // effectObject의 C++ 타입은 GameObject지만 실제 객체는 EffectInstance라,
    // "GameObject 1개"를 들고 있는 동시에 "EffectInstance 1개 단위"를 지시
    struct AttachedHandle
    {
        Shared<GameObject> effectObject{};   // acquire된 EffectInstance 1개를 GameObject로 지시. 위치 갱신과 release 대상
        wstring effectName{};                // 이 handle을 만들 때 사용한 effect key. 표시용 label이 아니라 재사용 bucket을 구분하는 이름
        bool requiresManualRelease{ false }; // true면 Is_Finished() 자동 반환 대신 owner가 Stop/release를 직접 호출해야 함. 주로 Trail 이펙트용
    };

    // pool 내부 장부를 외부 debug UI나 로그에서 간단히 확인하기 위한 집계값
    struct DebugSummary
    {
        uint32 entryCount{};  // pool 내부 슬롯 전체 개수. 슬롯 1개는 EffectInstance 1개를 보관
        uint32 activeCount{}; // caller에게 빌려준 슬롯 수
        uint32 idleCount{};   // Stop된 상태로 다음 Acquire()를 기다리는 슬롯 수
    };

    struct PrewarmResult
    {
        uint32 existingCount{};
        uint32 createdCount{};
        uint32 targetCount{};
    };

public:
    EffectInstancePool() = default;
    ~EffectInstancePool() override = default;

public: //## Behavior::Pool
    // 기존 instance 재사용 또는 신규 spawn 후 ready 상태로 반환
    HRESULT Acquire(const AcquireDesc& desc, AttachedHandle& outHandle);
    // 이미 acquired 된 instance root를 유지한 채 새 definition으로 preview/runtime 내용을 갱신
    HRESULT Reload_Attached(const AcquireDesc& desc, AttachedHandle& handle);
    // acquired instance를 Stop 후 inactive 상태로 반환
    void Release(const AttachedHandle& handle);
    // 자동 완료 가능한 instance들을 순회하며 pool로 복원
    void Release_Finished();
    void Clear();

public: //## Debug::EditorSession
    // [Effect Editor용] 같은 key의 inactive entry만 level에서 제거해 mutable preview definition 재사용을 방지
    void Discard_Inactive(const wstring& effectName);
    // [MainEditor Reload Data용] 다음 재생이 디스크의 최신 .effect.json을 다시 읽도록 쉬고 있는 모든 effect entry를 폐기
    uint32 Discard_AllInactive();
    // active/idle 여부와 무관하게 runtime effect instance를 레이어에서 제거
    uint32 Discard_All();
    DebugSummary Get_DebugSummary() const;
    string Build_DebugActiveEffectBreakdown(uint32 maxItems = 8) const;

    // 같은 이팩트를 count개 미리 생성해서 idle 상태로 돌려두기
    HRESULT Prewarm(const AcquireDesc& desc, uint32 count);
    // 같은 key의 pool entry를 targetCount까지 createBudget만큼 증설
    HRESULT Prewarm_ToCount(const AcquireDesc& desc, uint32 targetCount, uint32 createBudget, PrewarmResult* outResult = nullptr);
    // 같은 key/definition/layer에 속한 idle entry 수를 집계
    uint32 Count_Entries(const AcquireDesc& desc, const Shared<const EffectDefinition>& definition) const;

private: //## Types::Pool

    // pool 내부 슬롯 1개. 재사용 가능한 EffectInstance 1개와, 그 instance를 다시 빌려줘도 되는지 여부를 기록
    // caller에게는 AttachedHandle만 노출하고, 실제 EffectInstance 소유/재사용 상태는 이 entry가 추적
    struct PooledInstance
    {
        wstring effectName{};                        // 이 instance가 속한 pool bucket key
        Shared<const EffectDefinition> definition{}; // 재생 시 재사용하는 정적 effect 정의
        Shared<EffectInstance> instance{};           // 실제 재사용되는 runtime root
        uint32 layerLevelIndex{};                    // instance가 올라간 layer index
        wstring layerTag{};                          // instance가 실제 소속된 layer tag
        bool inUse{ false };                         // 현재 consumer가 빌려 간 상태인지 여부
        bool autoReleaseOnFinished{ true };          // 완료 시 전역 자동 반환을 허용하는 fire-and-forget entry인지 기록
    };

private: //## Data::Pool
    // 현재 pool이 관리하는 EffectInstance 재사용 엔트리 목록
    vector<PooledInstance> _entries{};

private: //## Helper::Pool
    // key 기준 instance 확보를 담당
    HRESULT Acquire_Instance(
        const AcquireDesc& desc,
        Shared<const EffectDefinition>& outDefinition,
        Shared<EffectInstance>& outInstance);
    // pool miss 시 기존 spawn 경로로 새 instance를 생성
    HRESULT Spawn_NewInstance(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        Shared<EffectInstance>& outInstance);
    // 다음 재생 전에 Reset/position/Play를 적용
    HRESULT Prepare_AcquiredInstance(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        const Shared<EffectInstance>& instance);
    // definition 내용에 따라 handle 반환 정책을 갱신
    void Populate_Handle(
        const AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition,
        const Shared<EffectInstance>& instance,
        AttachedHandle& outHandle) const;
    // 같은 key의 inactive entry를 조회. direct definition acquire는 definition identity까지 일치
    PooledInstance* Find_AvailableEntry(const AcquireDesc& desc, const Shared<const EffectDefinition>& definition);
    PooledInstance* Find_EntryByObject(const Shared<GameObject>& effectObject);

public:
    static Unique<EffectInstancePool> Create();
    void Free() override;
};

NS_END
