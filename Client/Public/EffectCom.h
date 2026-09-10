#pragma once

#include "ANS_Trail.h"
#include "Component.h"
#include "EffectInstancePool.h"
#include "PartObject.h"
#include "PreviewRimFeature.h"
#include "SourcePointSampleProvider_ByPart.h"
#include "SwitchDissolveFeature.h"
#include "TrailAnchor_Asset.h"

NS_BEGIN(Engine)
class GameObject;
class EffectInstance;
class IEffectTrailSampleProvider;
class IEffectSourcePointSampleProvider;
class ShaderCom;
NS_END

NS_BEGIN(Client)

class Weapon;

// Owner gameplay object의 effect 재생, attach, trail/source-history binding을 관리
class EffectCom final : public Component
{
    GENERATED_COMPONENT(EffectCom);

public: //## Types::Public
    enum class EffectPlayTarget : uint8
    {
        OwnerRoot = 0,  // 플레이할 Owner의 Transform 기준
        PartSocket = 1, // partTag + socketName
        PartRoot = 2,   // socket 없는 part root 기준
    };

    struct EffectTransformInheritance
    {
        bool inheritPosition = true;
        bool inheritRotation = true;
        bool inheritScale = false;
    };

    struct EffectPlayDesc
    {
        uint32 levelIndex = ETOI(LevelType::Static);
        wstring effectName{};
        EffectPlayTarget target = EffectPlayTarget::OwnerRoot;

        wstring partTag{};
        string socketName{};
        wstring layerTag = L"Layer_Effect";

        Vec3 localOffset = Vec3::Zero;
        Vec3 localRotation = Vec3::Zero;
        Vec3 scale = Vec3::One;
        float playbackSpeed = 1.f;
        EffectTransformInheritance transformInheritance{};
        bool localOffsetZTowardCamera = false;

        bool followTarget = false;          // AN은 false, ANS/attach 는 true로 사용
        bool requiresManualRelease = false; // owner가 살아 있는 동안 붙어 있어야 하는 effect는 자동 완료 release를 방지
        Weak<IEffectTrailSampleProvider> trailSampleProvider{};
        Weak<IEffectSourcePointSampleProvider> sourcePointSampleProvider{};
    };

    struct InspectorEffectSettings
    {
        bool autoPlay = false;
        wstring effectName = L"";
        EffectPlayTarget target = EffectPlayTarget::OwnerRoot;
        wstring partTag = L"";
        string socketName = "";
        wstring layerTag = L"Layer_Effect";
        Vec3 localOffset = Vec3::Zero;
        Vec3 localRotation = Vec3::Zero;
        Vec3 scale = Vec3::One;
        bool followTarget = false;
        bool requiresManualRelease = false;
    };

    struct AttachedEffectPauseState
    {
        Shared<EffectInstance> effect{}; // EffectInstance root를 직접 pause/restore
        bool wasPaused = false;          // 튜토리얼 시작 전 pause 상태로 되돌리기 위해 저장
    };

    struct AttachedEffectDebugSummary
    {
        uint32 totalCount = 0;
        uint32 activeCount = 0;
        uint32 trailCount = 0;
        uint32 trailDrainingCount = 0;
        uint32 sourceHistoryCount = 0;
        uint32 sourceHistoryDrainingCount = 0;
    };

    struct TrailOffsetRuntimeDesc
    {
        float phase = 0.f;
        ANS_Trail::TipExtendCurve tipExtendCurve{};
        ANS_Trail::LocalOffsetCurve baseLocalOffsetCurve{};
        ANS_Trail::LocalOffsetCurve tipLocalOffsetCurve{};
    };

    struct TrailAnchorRuntimeDesc
    {
        ANS_Trail::TrailAnchorSource source = ANS_Trail::TrailAnchorSource::ModelDefault;
        string baseBoneName{};
        Vec3 baseLocalOffset{ Vec3::Zero };
        string tipBoneName{};
        Vec3 tipLocalOffset{ Vec3::Zero };
    };

public:
    EffectCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~EffectCom() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Update(float timeDelta);
    void Tick_AttachedEffectInstances(float timeDelta);

public: //## Behavior::DrawFeatures
    // World에서 Active 중인 플레이어 바꿀 때
    void Start_SwitchDissolve(float durationSec, const string& noiseName);
    // 몬스터 죽을 때 사라지게
    void Start_VanishDissolve(float durationSec, const string& noiseName, bool vanishComplete);
    // UI Preview에서 역광 효과 내게
    void Set_PreviewRim(const Vec3& color, float intensity, float power, float hairIntensity);
    void Clear_PreviewRim();

    HRESULT Bind_DrawFeatures(ShaderCom* shader, uint32 supportFeatureMask);

    // Render 쪽에서는 owner만 세팅
    static HRESULT Bind_DrawFeatures(const Shared<GameObject>& owner, ShaderCom* shader, uint32 supportFeatureMask);

public: //## Behavior::AttachmentState
    // 현재 이 컴포넌트가 소유한 attached effect를 모두 pool로 반환
    void Stop_AllAttachedEffects();
    // AN/ANS에서 context.owner가 part일 수도 있어서 owner chain 타고 EffectCom를 찾는 공용 진입점
    static Shared<EffectCom> Find_FromNotifyOwner(GameObject* notifyOwner);
    static Shared<EffectCom> Find_FromNotifyOwner(GameObject* notifyOwner, EffectPlayTarget target);

    // socket follow와 자동 release 후보 정리를 함께 수행
    void Update_AttachedEffects(float timeDelta);
    void Capture_AttachedEffectPauseStates(vector<AttachedEffectPauseState>& outStates);
    void Restore_AttachedEffectPauseStates(vector<AttachedEffectPauseState>& states);
    AttachedEffectDebugSummary Get_AttachedEffectDebugSummary() const;

    InspectorEffectSettings Get_InspectorEffectSettings() const;
    void Set_InspectorEffectSettings(const InspectorEffectSettings& settings);
    void Set_InspectorEffectLocalOffset(const Vec3& localOffset);
    void Set_AttachedEffectMaterialReveal(float alphaMultiplier, float alphaErosion);
    void Set_AttachedEffectMaterialTintOverride(const vector<uint32>& emitterIds, const Vec4& targetTint, float strength);
    HRESULT Play_InspectorEffect();

public: //## Behavior::Playback
    HRESULT Play_Effect(const EffectPlayDesc& desc);
    HRESULT Play_Effect(const EffectPlayDesc& desc, EffectInstancePool::AttachedHandle* outHandle);

    // ANS 종료될 때, 같은 key의 attached effect를 pool로 반환하게
    void Stop_Effect(
        const wstring& effectName,
        EffectPlayTarget target,
        const wstring& partTag = L"",
        const string& socketName = "");
    void Stop_Effect(const EffectInstancePool::AttachedHandle& handle);

public: //## Behavior::PartAttachment
    HRESULT Play_AttachedEffect(
        uint32 levelIndex,
        const wstring& effectName,
        const wstring& partTag,
        const string& socketName,
        const wstring& layerTag,
        const Vec3& localOffset = Vec3::Zero,
        const Vec3& localRotation = Vec3::Zero,
        const Vec3& scale = Vec3::One,
        const Weak<IEffectTrailSampleProvider>& trailSampleProvider = {});

    HRESULT Play_AttachedPartEffect(
        uint32 levelIndex,
        const wstring& effectName,
        const wstring& partTag,
        const string& socketName,
        const wstring& layerTag,
        const Vec3& localOffset = Vec3::Zero,
        const Vec3& localRotation = Vec3::Zero,
        const Vec3& scale = Vec3::One);

    HRESULT Play_AttachedPartEffect(
        uint32 levelIndex,
        const wstring& effectName,
        const wstring& partTag,
        const wstring& layerTag,
        const Vec3& localOffset = Vec3::Zero,
        const Vec3& localRotation = Vec3::Zero,
        const Vec3& scale = Vec3::One);

public: //## Behavior::TrailAndSourceHistory
    HRESULT Play_TrailEffect(
        const wstring& effectName,
        const wstring& partTag,
        const wstring& layerTag,
        uint64 trailNotifyToken = 0,
        const TrailOffsetRuntimeDesc& offsetDesc = TrailOffsetRuntimeDesc{},
        const TrailAnchorRuntimeDesc& anchorDesc = TrailAnchorRuntimeDesc{});

    void Update_TrailEffectOffset(
        const wstring& effectName,
        const wstring& partTag,
        uint64 trailNotifyToken,
        const TrailOffsetRuntimeDesc& offsetDesc);

    void Stop_TrailEffect(
        const wstring& effectName,
        const wstring& partTag,
        uint64 trailNotifyToken = 0);

    bool Try_GetTrailSampleForPart(
        const wstring& partTag,
        const TrailAnchorAsset& trailAnchorAsset,
        uint64 trailNotifyToken,
        EffectTrailSample& outSample) const;

    bool Try_GetActiveTrailEffect(
        const wstring& partTag,
        wstring& outEffectName,
        wstring& outLayerTag,
        TrailOffsetRuntimeDesc& outOffsetDesc) const;

    HRESULT Play_SourceHistoryEffect(
        const wstring& effectName,
        const wstring& partTag,
        const wstring& layerTag,
        SourceHistorySourceBindingMode sourceBindingMode = SourceHistorySourceBindingMode::WeaponAnchor,
        const string& socketName = "",
        const string& sourceHistoryAnchorName = "Source",
        const Vec3& sourceHistoryLocalOffset = Vec3::Zero);

    void Stop_SourceHistoryEffect(
        const wstring& effectName,
        const wstring& partTag,
        SourceHistorySourceBindingMode sourceBindingMode = SourceHistorySourceBindingMode::WeaponAnchor,
        const string& socketName = "",
        const string& sourceHistoryAnchorName = "Source");

    bool Try_GetSourcePointSampleForPart(
        const wstring& partTag,
        SourceHistorySourceBindingMode sourceBindingMode,
        const string& socketName,
        const string& sourceHistoryAnchorName,
        const Vec3& sourceHistoryLocalOffset,
        EffectSourcePointSample& outSample) const;

private: //## Types::AttachedEffect
    enum class AttachedEffectRole : uint8
    {
        Normal,
        Trail,
        SourceHistoryPoint,
    };

    struct AttachedEffect
    {
        EffectInstancePool::AttachedHandle poolHandle{};

        wstring effectName{};
        EffectPlayTarget target = EffectPlayTarget::OwnerRoot;
        wstring partTag{};

        string socketName{};
        wstring layerTag{};

        Vec3 localOffset{ Vec3::Zero };
        Vec3 localRotation{ Vec3::Zero };
        Vec3 scale{ Vec3::One };
        EffectTransformInheritance transformInheritance{};
        bool localOffsetZTowardCamera = false;

        bool followTarget = false;                                // true면 Update에서 owner/socket 위치를 계속 추종
        AttachedEffectRole role{ AttachedEffectRole::Normal };
        TrailAnchorAsset trailAnchorAsset{};
        Shared<IEffectTrailSampleProvider> trailSampleProvider{}; // 각 trail instance가 자기 sample source를 유지
        Shared<IEffectSourcePointSampleProvider> ribbonSourcePointSampleProvider{};
        uint64 trailNotifyToken = 0;
        TrailOffsetRuntimeDesc trailOffset{};
        bool trailEchoEligible = true;
        Shared<bool> trailSampleEnabled{};
        bool isTrailDraining = false;
        float trailDrainElapsed = 0.f;
        float trailDrainDuration = 0.25f;
        SourceHistorySourceBindingMode sourceHistorySourceBindingMode{
            SourceHistorySourceBindingMode::WeaponAnchor };
        string sourceHistoryAnchorName{ "Source" };
        Vec3 sourceHistoryLocalOffset{ Vec3::Zero };
        Shared<IEffectSourcePointSampleProvider> sourcePointSampleProvider{};
        Shared<bool> sourcePointSampleEnabled{};
        bool isSourceHistoryDraining = false;
        float sourceHistoryDrainElapsed = 0.f;
        float sourceHistoryDrainDuration = 0.5f;
    };

    struct RecentTrailEffect
    {
        bool valid = false;
        wstring partTag{};
        wstring effectName{};
        wstring layerTag{};
        TrailOffsetRuntimeDesc offsetDesc{};
        bool echoEligible = true;
        float ageSec = 0.f;
    };

private: //## Data::DrawFeatures
    // 디졸브
    SwitchDissolveRuntime _switchDissolve{};
    // SkillTree preview 외곽광
    PreviewRimRuntime _previewRim{};

private: //## Data::AttachedEffects
    // 소켓에 붙일 이팩트
    vector<AttachedEffect> _attachedEffects{};

private: //## Data::Inspector
    bool _vanishComplete = false;

    RecentTrailEffect _recentTrailEffect{};

    bool _inspectorAutoPlayEffect = false;
    wstring _inspectorEffectName = L"";
    int _inspectorEffectTarget = static_cast<int>(EffectPlayTarget::OwnerRoot);
    wstring _inspectorPartTag = L"";
    string _inspectorSocketName = "";
    wstring _inspectorLayerTag = L"Layer_Effect";
    Vec3 _inspectorLocalOffset = Vec3::Zero;
    Vec3 _inspectorLocalRotation = Vec3::Zero;
    Vec3 _inspectorScale = Vec3::One;
    bool _inspectorFollowTarget = false;
    bool _inspectorRequiresManualRelease = false;
    bool _inspectorEffectPlayed = false;
    float _attachedEffectRevealAlphaMultiplier = 1.f;
    float _attachedEffectRevealAlphaErosion = 0.f;

private: //## Helper::AttachedEffects
    void Play_InspectorEffectIfEnabled();
    // 명시 반환이 필요한 attached effect를 pool로 복원
    void Release_AttachedEffect(AttachedEffect& attached);
    // sample 공급을 끊고 segmentLifetime 동안 자연 fade
    void Begin_TrailDrain(AttachedEffect& attached);
    // sample 공급을 끊고 단일 source history가 자연 fade되게 유지
    void Begin_SourceHistoryDrain(AttachedEffect& attached);
    // auto/manual release 정책을 판결정
    bool Should_ReleaseAttachedEffect(const AttachedEffect& attached) const;
    // 반환이 끝난 attached entry를 목록에서 제거
    void Prune_ReleasedAttachedEffects();
    void Apply_AttachedEffectMaterialReveal(const Shared<GameObject>& effectObject) const;

    bool Try_GetOwnerRootWorldMatrix(Matrix& outWorldMatrix) const;

    bool Try_GetSocketWorldMatrix(
        const wstring& partTag,
        const string& socketName,
        Matrix& outWorldMatrix) const;

    bool Try_GetPartRootWorldMatrix(
        const wstring& partTag,
        Matrix& outWorldMatrix) const;

    bool Try_ResolveEffectWorldTransform(
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
        Vec3& outWorldScale) const;

    bool Try_ResolveTrailEffectName(
        const wstring& effectNameOverride,
        const wstring& partTag,
        wstring& outEffectName) const;

    bool Try_BuildTrailSample(
        const wstring& partTag,
        const TrailAnchorAsset& trailAnchorAsset,
        const TrailOffsetRuntimeDesc* offsetDesc,
        EffectTrailSample& outSample) const;

    bool Try_ResolveTrailAnchorWorldMatrix(
        const PartObject& trailPart,
        const TrailAnchorPointDesc& anchor,
        Matrix& outWorldMatrix) const;

    bool Try_ResolveSourceHistoryEffectName(
        const wstring& effectName,
        wstring& outEffectName) const;

    bool Try_BuildSourcePointSample(
        const wstring& partTag,
        SourceHistorySourceBindingMode sourceBindingMode,
        const string& socketName,
        const string& sourceHistoryAnchorName,
        const Vec3& sourceHistoryLocalOffset,
        const TrailAnchorAsset* trailAnchorAsset,
        EffectSourcePointSample& outSample) const;

    bool Try_ResolveWeaponAnchorWorldMatrix(
        const PartObject& weaponPart,
        const string& anchorName,
        const Vec3& localOffset,
        Matrix& outWorldMatrix) const;

public:
    static Shared<EffectCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
