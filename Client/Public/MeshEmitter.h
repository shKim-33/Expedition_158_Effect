#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

// Mesh particle runtime을 CPU에서 시뮬레이션하고 instanced mesh effect를 제출
class MeshEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(MeshEmitter, ObjectType::EffectEmitter);

public:
    MeshEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MeshEmitter(const MeshEmitter& prototype);
    ~MeshEmitter() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    // effect root 재사용 시 mesh playback runtime과 particle state를 초기화
    HRESULT Reset_ForEffectReplay() override;
    // 현재 instance buffer 재사용이 가능한 mesh desc인지 확인
    bool Can_ReloadDefinition(const EffectEmitterDefinition& emitterDefinition) const override;
    // 호환되는 mesh desc를 기존 runtime 인스턴스에 다시 적용
    HRESULT Reload_Definition(const EffectEmitterDefinition& emitterDefinition) override;
    // 마지막 loop 완료와 active particle 소진 여부를 반환
    bool Is_EffectFinished() const override;
    bool Collect_FollowerSourcePoints(vector<EffectFollowerSourcePoint>& outPoints, uint32 maxPointCount) const override;
    // follower ribbon이 따라갈 active mesh particle 위치를 제공
    // active mesh particle의 카메라 기준 대표 표면점을 Blend 정렬 위치로 반환
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override;
    EffectSortPolicy Get_BlendSortPolicy() const override;
    int32 Get_BlendSortLayer() const override;
    // Required Sort Bias authoring 값을 Blend 정렬 보정값으로 반환
    float Get_BlendSortBias() const override;

protected: //## Hook::TransformSync
    // root/local-space sync 직후 active instance world matrix를 다시 계산
    void On_EffectTransformSynced() override;

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct MeshInstanceVertex
    {
        Matrix world{};                              // instanced mesh draw가 읽는 particle world matrix
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };            // life 평가까지 반영된 최종 particle color
        Vec2 lifeTime{};                             // x: particle 수명, y: 현재 age
        Vec2 padding{};                              // instance layout 16-byte 정렬 유지용 padding
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f }; // Core Color RGB Uniform+ParticleLife 생성 시점 샘플
    };

    struct MeshParticleState
    {
        bool active{ false };                          // 현재 slot이 살아 있는 particle인지 여부
        float age{};                                   // spawn 이후 누적된 particle age
        float lifeMax{ 1.f };                          // 이 particle에 배정된 최대 lifetime
        Vec3 position{};                               // local-space면 emitter 기준, world-space면 world 기준 현재 위치
        Vec3 velocity{};                               // direction align 등이 참조하는 최종 frame velocity cache
        Vec3 initialVelocity{};                        // InitialVelocity channel의 drag 누적 속도
        Vec3 initialRadialVelocity{};                  // InitialRadialVelocity channel의 drag 누적 속도
        Vec3 velocityCone{};                           // VelocityCone channel의 drag 누적 속도
        Vec3 sourceMotionVelocity{};                   // SourceMotionVelocity channel의 drag 누적 속도
        Vec3 accelerationIntegratedVelocity{};         // AccelerationIntegratedVelocity channel의 누적 속도
        Vec3 acceleration{};                           // particle별 초기 acceleration sample 값
        float drag{};                                  // 매 frame velocity 감쇠에 사용할 drag 계수
        Vec3 baseScale{ 1.f, 1.f, 1.f };               // life curve 적용 전 초기 mesh scale
        Vec3 spawnOffset{};                            // OrbitOverLife가 회전시킬 emitter-local spawn offset
        Vec3 spawnPosition{};                          // 기존 motion 누적량을 분리하기 위한 spawn 기준 위치
        Quat spawnTransformRotation{ Quat::Identity }; // world-space particle이 spawn될 때의 effect root 회전
        Quat spawnOrientation{ Quat::Identity };       // radial orientation module에서 만든 spawn 기준 mesh 회전
        bool spawnOrientationEnabled{ false };         // spawn 기준 radial mesh 회전이 실제 적용 가능한지 여부
        Vec3 rotationRadians{};                        // 현재 particle 회전값
        Quat rotationQuat{ Quat::Identity };           // world-space angular velocity 경로가 누적하는 현재 회전
        Vec3 angularVelocityRadians{};                 // 현재 particle 회전 속도
        float directionAlignRandomDelay{ 0.f };        // MeshDirectionAlignOverLife 정렬 시작 normalized life 지연
        float directionAlignWeightScale{ 1.f };        // MeshDirectionAlignOverLife 최종 weight 배율
        Vec4 startColor{ 1.f, 1.f, 1.f, 1.f };         // InitialColor에서 정한 particle 시작 색상
        Vec4 endColor{ 1.f, 1.f, 1.f, 1.f };           // ColorOverLife endpoint 보간에 사용할 particle 끝 색상
        Vec3 coreColorRgb{ 1.f, 0.85f, 0.45f };        // Core Color RGB Uniform+ParticleLife 생성 시점 샘플
        uint32 seed{};                                 // particle별 deterministic random sample seed
    };

    struct PlaneRadialSample
    {
        bool enabled{ false }; // PlaneRadialLocation이 실제 sample frame을 만들었는지 여부
        Vec3 localOffset{};    // 기존 location module과 더할 emitter-local offset
        Vec3 radialDirection{ 1.f, 0.f, 0.f };
        Vec3 tangentCW{ 0.f, -1.f, 0.f };
        Vec3 tangentCCW{ 0.f, 1.f, 0.f };
        Vec3 planeNormal{ 0.f, 0.f, 1.f };
        float sampleAngleRadians{};
    };

    struct SphereRadialSample
    {
        bool enabled{ false }; // SphereLocation이 실제 sphere sample frame을 만들었는지 여부
        Vec3 localOffset{};    // 기존 location module과 더할 emitter-local offset
        Vec3 radialDirection{ 0.f, 0.f, 1.f };
    };

    struct CylinderRadialSample
    {
        bool enabled{ false }; // CylinderLocation이 실제 sample frame을 만들었는지 여부
        Vec3 localOffset{};    // 기존 location module과 더할 emitter-local offset
        Vec3 radialDirection{ 1.f, 0.f, 0.f };
        Vec3 tangentCW{ 0.f, -1.f, 0.f };
        Vec3 tangentCCW{ 0.f, 1.f, 0.f };
        Vec3 cylinderAxis{ 0.f, 0.f, 1.f };
    };

    struct SourceMotionVelocityContext
    {
        Vec3 sourceVelocity{};
        Vec3 tangent{ 0.f, 0.f, 1.f };
        bool sourceVelocityValid{ false };
    };

private: //## Static::Runtime
    // mesh effect draw shader id
    static constexpr auto kEffectMeshShaderId{ "Shader_EffectMesh" };
    // mesh distortion field output 전용 shader id
    static constexpr auto kEffectMeshDistortionShaderId{ "Shader_EffectMeshDistortion" };
    // mesh glass surface 전용 shader id
    static constexpr auto kEffectMeshGlassShaderId{ "Shader_EffectMeshGlass" };
    // main texture 미지정 시 사용할 fallback texture
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    // main texture alpha를 opacity로 사용
    static constexpr int kOpacitySourceAlpha{ 0 };
    // main texture red를 opacity로 사용
    static constexpr int kOpacitySourceRed{ 1 };
    // main texture luminance를 opacity로 사용
    static constexpr int kOpacitySourceLuminance{ 2 };

private: //## Data::Components
    // instanced draw에 사용할 non-anim model component
    Shared<ModelCom> _model{};
    // mesh effect material shader component
    Shared<ShaderCom> _shader{};
    // 현재 material family에 맞춰 선택된 shader prototype tag
    wstring _loadedShaderPrototypeTag{};
    // mesh effect main texture
    Shared<Texture> _texture{};
    // 선택 noise texture. 없으면 material noise를 비활성화
    Shared<Texture> _noiseTexture{};
    // 선택 mask texture. 없으면 mask alpha 소비를 비활성화
    Shared<Texture> _maskTexture{};
    // 선택 distortion flow texture. 없으면 distortion offset을 0으로 처리
    Shared<Texture> _flowTexture{};
    // MeshInstanceVertex payload를 GPU에 제출하는 dynamic instance buffer
    ComPtr<Buffer> _instanceBuffer{};

private: //## Data::RuntimeDesc
    // clone 생성 시 전달받은 mesh emitter runtime 규칙
    MeshEmitterDesc _desc{};
    // shader opacity source selector
    int _opacitySourceIndex{ kOpacitySourceAlpha };
    // model/shader/buffer가 모두 준비돼 render 가능한지 여부
    bool _isReady{ false };
    // sphere radial mesh axis fallback warning을 emitter당 한 번만 기록
    mutable bool _sphereRadialMeshOrientationFallbackLogged{ false };
    // mesh axis fallback warning을 emitter당 한 번만 기록
    mutable bool _planeRadialMeshOrientationFallbackLogged{ false };
    // cylinder radial mesh axis fallback warning을 emitter당 한 번만 기록
    mutable bool _cylinderRadialMeshOrientationFallbackLogged{ false };
    // MeshDirectionAlignOverLife fallback warning을 emitter당 한 번만 기록
    mutable bool _meshDirectionAlignFallbackLogged{ false };

private: //## Data::ParticleRuntime
    // active/inactive mesh particle slot 배열
    vector<MeshParticleState> _particles{};
    // draw 직전 GPU instance buffer에 복사할 CPU payload
    vector<MeshInstanceVertex> _instancePayload{};
    // 이번 frame에 실제로 그릴 active instance 수
    uint32 _activeInstanceCount{};
    // ring-style slot 재사용을 위한 다음 spawn 시작 index
    uint32 _nextSpawnCursor{};
    // 현재 instance buffer와 particle 배열 capacity
    uint32 _instanceCapacity{};
    Vec3 _sourceMotionVelocity{};
    Vec3 _sourceMotionTangent{ 0.f, 0.f, 1.f };
    Vec3 _lastSourceMotionCenter{};
    bool _sourceMotionVelocityValid{ false };
    bool _hasLastSourceMotionCenter{ false };

private: //## Data::SpawnRuntime
    // burst 목록의 1회 실행 여부를 emitter runtime 동안 유지
    vector<bool> _burstExecuted{};
    // spawn/material time 평가에 사용할 emitter-local 누적 시간
    float _emitterElapsedTime{};
    // fractional spawn rate를 프레임 사이에 보존
    float _spawnAccumulator{};
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

private: //## Helper::DefinitionReload
    void Log_DescPayload(const char* context) const;
    // initialize/reload 단계별 준비 시간을 로그로 기록
    void Log_InitializeTiming(
        const char* context,
        double shaderMs,
        double resolveModelMs,
        double modelMs,
        double materialMs,
        double bufferMs,
        double totalMs) const;
    // 기존 component 재사용이 가능한 reload desc인지 확인
    bool Is_CompatibleReloadDesc(const MeshEmitterDesc& desc) const;
    // effect material override texture가 하나라도 지정됐는지 확인
    bool Has_MaterialOverride(const MeshEmitterDesc& desc) const;
    // 호환되는 reload desc를 현재 runtime 리소스에 다시 적용
    HRESULT Apply_DescForReload(const MeshEmitterDesc& desc);

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_Shader();
    // spawn capacity에 맞는 dynamic instance buffer를 준비
    HRESULT Ready_InstanceBuffer();
    HRESULT Apply_EffectMaterialOverride();
    HRESULT Ready_EffectMaterialTextures();
    HRESULT Ready_OptionalTexture(const string& textureGuid, const string& texturePath, Shared<Texture>& outTexture);
    // modelGuid/modelPath에서 실제 model resource 경로를 resolve
    wstring Resolve_ModelPath() const;
    // effect material texture guid/path를 실제 resource 경로로 resolve
    wstring Resolve_MaterialTexturePath(const string& textureGuid, const string& texturePath) const;

private: //## Helper::Render
    HRESULT Bind_EffectMaterialResources();
    HRESULT Bind_ModelMaterialResourcesForMesh(uint32 meshIndex, uint32 materialIndex);
    HRESULT Bind_DistortionResources();
    bool Is_DistortionFamily() const;
    bool Is_MeshGlassFamily() const;
    // particle state를 instanced draw world matrix로 변환
    Matrix Build_InstanceMatrix(const MeshParticleState& particle) const;
    Vec3 Resolve_ParticleWorldPosition(const MeshParticleState& particle) const;
    RenderGroup Resolve_RenderGroup() const;
    uint32 Resolve_ShaderPassIndex() const;
    // authoring opacitySource 문자열을 shader selector 값으로 변환
    int Resolve_OpacitySourceIndex() const;

private: //## Helper::PlaybackRuntime
    // emitter timing/burst/playback runtime state를 초기화
    void Reset_PlaybackRuntime();
    // delay/duration/loop 정책을 진행하고 particle simulation을 갱신
    void Advance_Playback(float timeDelta);
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

private: //## Helper::ParticleRuntime
    // 완료 판정에 사용할 실제 active particle 수를 반환
    uint32 Count_ActiveParticles() const;
    // 이번 frame에 새로 활성화할 particle 수를 계산
    uint32 Consume_SpawnRequest(float timeDelta);
    void Resample_LoopSpawnDistributions();
    float Sample_SpawnDistribution(const Vec2& range, const PointParticleRandomSeedRuntimeDesc& seed, uint32 salt) const;
    // spawn request만큼 particle slot을 활성화
    void Spawn_Particles(uint32 spawnRequest);
    // 단일 particle slot에 초기 위치/속도/색/회전을 채움
    void Activate_Particle(MeshParticleState& particle, uint32 seed, uint32 particleIndex, uint32 placementIndex, uint32 placementCount);
    // active particle의 age, motion, color/scale 기반 상태를 진행
    void Update_Particles(float timeDelta);
    // active particle을 MeshInstanceVertex payload로 다시 작성
    void Update_InstanceBuffer();

private: //## Helper::SamplingEvaluation
    // location module에서 초기 local offset을 sample
    Vec3 Sample_InitialOffset(
        uint32 seed,
        uint32 particleIndex,
        uint32 placementIndex,
        uint32 placementCount,
        PlaneRadialSample& outPlaneRadialSample,
        SphereRadialSample& outSphereRadialSample,
        CylinderRadialSample& outCylinderRadialSample) const;
    // SphereLocation sample frame을 mesh radial orientation용 회전으로 변환
    Quat Build_SphereRadialMeshOrientation(const SphereRadialSample& sample, bool& outEnabled) const;
    // PlaneRadialLocation sample frame을 mesh radial orientation용 회전으로 변환
    Quat Build_PlaneRadialMeshOrientation(const PlaneRadialSample& sample, bool& outEnabled) const;
    // CylinderLocation sample frame을 mesh radial orientation용 회전으로 변환
    Quat Build_CylinderMeshOrientation(const CylinderRadialSample& sample, bool& outEnabled) const;
    float Evaluate_DirectionAlignWeight(float lifeProgress, const MeshParticleState& particle) const;
    Quat Build_MeshDirectionAlignTargetOrientation(const MeshParticleState& particle, const Vec3& visualPosition, bool& outEnabled) const;
    // velocity channel별 초기 속도를 sample
    void Sample_InitialVelocityChannels(
        MeshParticleState& particle,
        const Vec3& spawnPosition,
        const PlaneRadialSample& planeRadialSample,
        const SourceMotionVelocityContext& sourceMotionContext,
        uint32 seed) const;
    void Update_SourceMotionVelocity(float timeDelta);
    // OrbitOverLife payload를 lifeProgress 기준 위치 offset으로 평가
    Vec3 Evaluate_OrbitOffset(const MeshParticleState& particle, float lifeProgress) const;
    Vec3 Sample_Acceleration(uint32 seed) const;
    float Sample_Drag(uint32 seed) const;
    // min/max color 범위에서 particle color를 sample
    Vec4 Sample_Color(
        const Vec4& minColor,
        const Vec4& maxColor,
        const PointParticleRandomSeedRuntimeDesc& colorSeed,
        const PointParticleRandomSeedRuntimeDesc& alphaSeed,
        uint32 seed) const;
    Vec3 Sample_InitialScale(uint32 seed) const;
    Vec3 Sample_InitialRotation(uint32 seed) const;
    Vec3 Sample_InitialAngularVelocity(uint32 seed) const;
    float Evaluate_VelocityScaleByLifeChannel(const PointParticleFloatCurveRuntimeDesc& curve, float lifeProgress) const;
    // 현재 channel state와 VelocityOverLife channel scale을 합쳐 최종 이동 속도를 계산
    Vec3 Evaluate_ScaledVelocityChannels(const MeshParticleState& particle, float lifeProgress) const;
    // acceleration curve가 있으면 선택된 time basis 기준 절대 가속을 평가
    Vec3 Evaluate_AccelerationByLife(const MeshParticleState& particle, float lifeProgress) const;
    Vec3 Evaluate_ScaleByLife(float lifeProgress) const;
    Vec3 Evaluate_RotationByLife(float lifeProgress) const;
    Vec3 Evaluate_AngularVelocityScaleByLife(float lifeProgress) const;
    // particle start/end color와 curve를 합쳐 현재 색을 계산
    Vec4 Evaluate_ColorOverLife(const MeshParticleState& particle, float lifeProgress) const;
    // compact Vec4 curve payload를 life 진행도에 맞춰 평가
    float Evaluate_CompactCurve(
        float lifeProgress,
        const Vec4& times,
        const Vec4& timesBlock1,
        const Vec4& values,
        const Vec4& valuesBlock1,
        uint32 keyCount,
        float fallbackValue) const;
    float Evaluate_CompactCurve(
        float lifeProgress,
        const Vec4& times,
        const Vec4& timesBlock1,
        const Vec4& values,
        const Vec4& valuesBlock1,
        const Vec4& arriveTangents,
        const Vec4& arriveTangentsBlock1,
        const Vec4& leaveTangents,
        const Vec4& leaveTangentsBlock1,
        const Vec4& modes,
        const Vec4& modesBlock1,
        uint32 keyCount,
        float fallbackValue) const;
    // mesh vector3 curve runtime desc를 life 진행도에 맞춰 평가
    Vec3 Evaluate_MeshVector3Curve(const MeshVector3CurveRuntimeDesc& desc, float lifeProgress, const Vec3& fallbackValue) const;
    // local/world space 정책에 따라 벡터 기준 좌표계를 변환
    Vec3 Resolve_LocalVector(const Vec3& localVector) const;
    Vec3 Resolve_WorldVectorToSimulation(const Vec3& worldVector) const;
    Vec3 Resolve_LocalPoint(const Vec3& localPoint) const;
    Vec3 Resolve_WorldPointToSimulation(const Vec3& worldPoint) const;
    // deterministic seed 기반 0..1 난수를 반환
    float Random01(uint32 seed) const;
    // deterministic seed 기반 범위 난수를 반환
    float RandomRange(float minValue, float maxValue, uint32 seed) const;

public:
    static Shared<MeshEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
