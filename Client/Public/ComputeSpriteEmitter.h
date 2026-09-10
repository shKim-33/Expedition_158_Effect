#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ShaderCom;
class Texture;
class VIBufferCom_ComputePointParticle;
NS_END

NS_BEGIN(Client)

// Compute sprite particle 재생과 material/render payload 제출을 담당
class ComputeSpriteEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeSpriteEmitter, ObjectType::EffectEmitter);

public:
    ComputeSpriteEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeSpriteEmitter(const ComputeSpriteEmitter& prototype);
    ~ComputeSpriteEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    // effect root 재사용 시 sprite playback runtime을 초기화
    HRESULT Reset_ForEffectReplay() override;
    // item spawn reveal처럼 effect material alpha/erosion만 런타임에서 덮어씀
    void Set_MaterialRevealOverride(float alphaMultiplier, float alphaErosion) override;
    void Set_MaterialTintOverride(const Vec4& targetTint, float strength) override;
    // sprite playback이 마지막 loop를 끝냈는지 반환
    bool Is_EffectFinished() const override;
    // follower ribbon이 따라갈 대표 particle 위치를 제공
    bool Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>& outPoints, uint32 maxPointCount) const override;
    // sprite spawn offset까지 반영한 Blend 정렬 대표 위치를 반환
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override;
    EffectSortPolicy Get_BlendSortPolicy() const override;
    int32 Get_BlendSortLayer() const override;
    // Required Sort Bias authoring 값을 Blend 정렬 보정값으로 반환
    float Get_BlendSortBias() const override;

public: //## Debug::EditorSession
    // EffectEditor 세션에서 sprite influence outline shader overlay를 켜고 비활성
    static void Set_InfluenceOutlineDebugEnabled(bool enabled);
    static bool Is_InfluenceOutlineDebugEnabled();

protected: //## Hook::TransformSync
    // root/local-space sync 직후 compute center를 즉시 갱신
    void On_EffectTransformSynced() override;

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct PlaybackTick
    {
        float computeDeltaTime{};    // 이번 frame에서 particle simulation에 흘려보낼 시간
        float lifetimeSamplePhase{}; // 이번 spawn request의 Lifetime curve sampling phase
        uint32 spawnRequest{};       // 이번 frame에 활성화할 particle 수
        uint32 spawnSerialBase{};    // 이번 frame 첫 spawn의 replay-local 순번
        bool killActiveParticles{};  // 이번 frame에 active particle을 모두 죽일지 여부
    };

private: //## Static::Runtime
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr auto kEffectSpriteShaderId{ "Shader_EffectSprite" };
    static constexpr auto kEffectDistortionSpriteShaderId{ "Shader_EffectDistortionSprite" };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };
    static constexpr int kOpacitySourceOne{ 3 };
    static constexpr float kInfluenceOutlinePixelWidthScale{ 2.f };
    inline static bool _isInfluenceOutlineDebugEnabled{ false };

private: //## Data::Components
    // EffectSprite material effect shader.
    Shared<ShaderCom> _shader{};
    // compute sprite effect main texture.
    Shared<Texture> _texture{};
    // 선택 noise texture. 없으면 material noise를 비활성화
    Shared<Texture> _noiseTexture{};
    // 선택 mask texture. 없으면 material mask/erosion을 비활성화
    Shared<Texture> _maskTexture{};
    // 선택 flow texture. 없으면 distortion offset을 0으로 처리
    Shared<Texture> _flowTexture{};
    // compute output을 instance draw로 제출하는 검증 버퍼
    Shared<VIBufferCom_ComputePointParticle> _viBuffer{};

private: //## Data::RuntimeDesc
    // clone 생성 시 전달받은 compute sprite emitter 규칙
    ComputeSpriteEmitterDesc _desc{};
    // shader opacity source selector.
    int _opacitySourceIndex{ kOpacitySourceAlpha };
    // code-driven reveal multiplier. Pool 재사용 시 기본값으로 복구
    float _materialRevealAlphaMultiplier = 1.f;
    float _materialRevealAlphaErosion = 0.f;
    Vec4 _materialTintOverrideTarget{ 1.f, 1.f, 1.f, 1.f };
    float _materialTintOverrideStrength = 0.f;
    // compute payload에 마지막으로 반영한 effect root world scale
    Vec3 _runtimeDescWorldScale{ Vec3::One };
    // 최초 sync 때는 scale 값이 같아도 payload를 한 번 갱신
    bool _hasRuntimeDescWorldScale{ false };

private: //## Data::SpawnRuntime
    // burst 목록의 1회 실행 여부를 emitter runtime 동안 유지
    vector<bool> _burstExecuted{};
    // spawn rate/burst 평가에 사용할 emitter-local 누적 시간
    float _emitterElapsedTime{};
    // fractional spawn rate를 프레임 사이에 보존
    float _spawnAccumulator{};
    // Restart Preview마다 0부터 다시 시작하는 spawn 순번
    uint32 _spawnSerialCounter{};
    float _sampledSpawnRate{ 10.f };
    float _sampledSpawnRateScale{ 1.f };

private: //## Data::PlaybackRuntime
    // delay/playing/completed 중 현재 emitter 재생 상태
    PlaybackState _playbackState{ PlaybackState::Delayed };
    // 현재 loop 안에서 delay를 포함해 흐른 시간
    float _loopElapsedTime{};
    // 0부터 시작하는 현재 loop index
    uint32 _loopIndex{};
    // 완료 kill 요청을 한 번만 보내기 위한 latch
    bool _completedKillDispatched{};

private: //## Helper::Setup
    HRESULT Ready_Components();
    // compute sprite main texture를 준비하고 fallback 결과를 기록
    HRESULT Ready_TextureComponent(string& outResolvedTexturePath);
    HRESULT Ready_ShaderComponent();
    // 선택 noise texture를 준비하고 없으면 noise path를 비활성화
    HRESULT Ready_NoiseTextureComponent(const string& resolvedTexturePath);
    // 선택 mask texture를 준비하고 없으면 alpha mask를 비활성화
    HRESULT Ready_MaskTextureComponent(const string& resolvedTexturePath);
    // SpriteDistortion용 flow texture를 준비하고 없으면 neutral flow로 유지
    HRESULT Ready_FlowTextureComponent(const string& resolvedTexturePath);

private: //## Helper::Render
    HRESULT Bind_ShaderResources();
    HRESULT Bind_MaterialResources();
    HRESULT Bind_DistortionResources();
    // material scalar EmitterTime 평가에 사용할 playback phase를 반환
    float Resolve_MaterialScalarEmitterPhase() const;
    bool Is_DistortionFamily() const;
    RenderGroup Resolve_RenderGroup() const;
    const char* Resolve_ShaderId() const;
    uint32 Resolve_ShaderPassIndex() const;
    // authoring opacitySource 문자열을 shader selector 값으로 변환
    int Resolve_OpacitySourceIndex() const;
    uint32 Resolve_BlendPassIndex() const;

private: //## Helper::RuntimeDesc
    // effect root scale을 compute payload 배율로 해석
    Vec3 Resolve_RuntimeWorldScale() const;
    // compute sprite effect용 particle 규칙을 생성
    ComputePointParticleDesc Make_ComputePointParticleDesc() const;
    // root scale 변경을 compute particle payload에 반영
    void Update_RuntimeDescForCurrentScale(bool forceUpdate);
    // emitter transform world position을 compute center에 반영
    void Sync_WorldCenterToComputeBuffer();

private: //## Helper::SpawnRuntime
    // 이번 frame에 새로 활성화할 particle 수를 계산
    uint32 Consume_SpawnRequest(float timeDelta);
    // loop 시작 시 seed 기반 spawn rate 분포를 고정 샘플
    void Resample_LoopSpawnDistributions();
    float Sample_SpawnDistribution(const Vec2& range, const PointParticleRandomSeedRuntimeDesc& seed, uint32 salt) const;

private: //## Helper::PlaybackRuntime
    // emitter timing runtime state를 초기화
    void Reset_PlaybackRuntime();
    // delay/duration/loop 정책을 진행하고 compute tick을 생성
    PlaybackTick Advance_Playback(float timeDelta);
    // loop-local spawn/burst state를 초기화하고 다음 loop로 전환
    void Start_NextLoop();
    // 현재 loop에 적용할 delay를 반환
    float Resolve_CurrentLoopDelay() const;
    // 0 이하 duration으로 인한 무한 루프를 막는 안전 duration을 반환
    float Resolve_Duration() const;
    // loopCount 정책상 다음 loop가 있는지 확인
    bool Has_NextLoop() const;
    // delay/completed kill 정책상 render 제출 가능한지 확인
    bool Can_SubmitRender() const;

public:
    static Shared<ComputeSpriteEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
