#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ComputeShaderCom;
class ComputeStructuredBuffer;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

// Beam desc 기반 path sample을 strip payload로 낮춰 beam을 렌더
class ComputeGeneratedBeamEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeGeneratedBeamEmitter, ObjectType::EffectEmitter);

public:
    ComputeGeneratedBeamEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeGeneratedBeamEmitter(const ComputeGeneratedBeamEmitter& prototype);
    ~ComputeGeneratedBeamEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    // effect replay 시 generated path와 재생 상태를 초기화
    HRESULT Reset_ForEffectReplay() override;
    // playback loop 완료 여부를 반환
    bool Is_EffectFinished() const override;
    // beam 중간점을 blend sort 기준점으로 제공
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override;
    EffectSortPolicy Get_BlendSortPolicy() const override;
    int32 Get_BlendSortLayer() const override;
    float Get_BlendSortBias() const override;

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Playing,
        Completed,
    };

    struct PathSample
    {
        Vec3 position{};  // generated beam path의 world position sample
        float age{};      // presentation life 평가용 sample age
        float distance{}; // beam 시작점 기준 누적 거리
    };

    struct PathEntry
    {
        vector<PathSample> samples{}; // draw payload로 낮출 generated path sample
        uint32 stripIndex{};          // SubUV variation seed에 사용할 parent strip index
        float widthScale{ 1.f };      // parent 폭 대비 path 폭 배율
    };

    struct SegmentPayload
    {
        Vec4 previousPosition{};              // compute strip tangent용 이전 sample
        Vec4 currentPosition{};               // segment 시작 sample
        Vec4 nextPosition{};                  // segment 끝 sample
        Vec4 nextNextPosition{};              // compute strip tangent용 다음다음 sample
        Vec4 currentSampleParams{};           // x age, y lifetime, z distance, w widthScale.
        Vec4 nextSampleParams{};              // x age, y lifetime, z distance, w widthScale.
        Vec4 startColor{};                    // segment 시작 색
        Vec4 endColor{};                      // segment 끝 색
        Vec4 subUVRect{ 0.f, 0.f, 1.f, 1.f }; // 현재 SubUV rect
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

    struct BeamInstanceVertex
    {
        Vec4 previousPosition{}; // ribbon shader와 동일한 instance layout
        Vec4 currentPosition{};
        Vec4 nextPosition{};
        Vec4 nextNextPosition{};
        Vec2 lifeTime{};
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 subUVRect{};
        Vec4 segmentParams{};
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

    struct BeamComputeParams
    {
        uint32 segmentCount{};      // compute로 낮출 segment 수
        uint32 renderAxis{};        // ribbon strip shader의 axis mode
        float tilingDistance{};     // texture V tiling 기준 거리
        float visibleLength{ 1.f }; // 전체 beam visible length
        Vec4 axisFallback{};        // axis fallback 값
        Vec4 fadeParams{};          // x auto life fade, y max length, z tail fade length, w reserved.
    };

    struct DrawIndexedInstancedIndirectArgs
    {
        uint32 indexCountPerInstance{};
        uint32 instanceCount{};
        uint32 startIndexLocation{};
        int32 baseVertexLocation{};
        uint32 startInstanceLocation{};
    };

private: //## Static::Runtime
    // beam 전용 좌우 UV 보정 shader
    static constexpr auto kBeamShaderId{ L"Shader_EffectBeam" };
    // distortion beam 전용 shader
    static constexpr auto kBeamDistortionShaderId{ L"Shader_EffectBeamDistortion" };
    // segment payload를 instance로 낮추는 compute shader
    static constexpr auto kBeamComputeShaderId{ L"ComputeShader_RibbonStrip" };
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr auto kNeutralFlowTexturePath{ "Effects/Textures/Shared/DefaultFlowMap.dds" };
    // compute dispatch thread group 크기
    static constexpr uint32 kThreadCountX{ 64 };

private: //## Data::RuntimeDesc
    // clone 시점에 받은 Beam runtime 설정
    ComputeBeamEmitterDesc _desc{};

private: //## Data::PlaybackRuntime
    PlaybackState _playbackState{ PlaybackState::Delayed };
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    float _materialElapsedTime{};
    float _sampledVisualLife{ 1.f };
    float _visibleBeamLength{ 1.f };
    uint32 _drawSegmentCount{};
    uint32 _randomSubUVFrameIndex{};
    // 이번 replay에서 생성된 parent/branch path 목록
    vector<PathEntry> _pathEntries{};

private: //## Data::Components
    Shared<ShaderCom> _shader{};
    Shared<ComputeShaderCom> _computeShader{};
    Shared<Texture> _mainTexture{};
    Shared<Texture> _noiseTexture{};
    Shared<Texture> _maskTexture{};
    Shared<Texture> _flowTexture{};
    ComPtr<Buffer> _pointVB{};
    ComPtr<Buffer> _indexBuffer{};
    ComPtr<Buffer> _instanceBuffer{};
    ComPtr<Buffer> _computeConstantBuffer{};
    ComPtr<Buffer> _indirectArgsBuffer{};
    Shared<ComputeStructuredBuffer> _sampleInput{};
    Shared<ComputeStructuredBuffer> _computeOutput{};
    Shared<ComputeStructuredBuffer> _computeArgsOutput{};

private: //## Helper::PlaybackRuntime
    // playback 시간/loop 상태를 초기화
    void Reset_PlaybackRuntime();
    // delay/play/completed 상태를 진행
    void Advance_Playback(float timeDelta);
    // desc의 local start/end 또는 direction/length로 world sample을 다시 생성
    void Rebuild_PathSamples();
    // endpoint mode와 strip index에 따라 local end를 계산
    Vec3 Resolve_LocalEnd(uint32 stripIndex) const;
    // emitter local point를 world point로 변환
    Vec3 Transform_LocalPoint(const Vec3& localPoint) const;
    // seed 기반 jitter offset을 생성
    Vec3 Evaluate_NoiseOffset(uint32 stripIndex, uint32 sampleIndex, const Vec3& tangent) const;
    // parent path에서 branch path를 생성
    void Append_BranchPathEntries(const PathEntry& parentPath, uint32 stripIndex, float activeElapsedTime);
    // path sample을 compute input payload로 변환
    HRESULT Append_SegmentPayloads(vector<SegmentPayload>& outPayloads);
    // 렌더 가능한 path segment가 있는지 확인
    bool Has_RenderablePath() const;
    void Start_NextLoop();
    void Resample_VisualLife();
    float Resolve_CurrentLoopDelay() const;
    float Resolve_Duration() const;
    // 한 loop 안에서 Beam이 실제로 보이는 시간을 반환
    float Resolve_VisualLife() const;
    // delay 이후 현재 loop 진행 시간을 duration 안으로 제한
    float Resolve_ActiveElapsedTime(float duration) const;
    bool Has_NextLoop() const;

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_Texture();
    HRESULT Ready_NoiseTexture();
    HRESULT Ready_MaskTexture();
    HRESULT Ready_FlowTexture();
    HRESULT Ready_DrawBuffers();
    HRESULT Ready_ComputeBuffers();

private: //## Helper::Render
    HRESULT Dispatch_Compute();
    HRESULT Update_ComputeInput();
    HRESULT Update_ComputeConstants();
    HRESULT Reset_IndirectArgs();
    HRESULT Copy_ComputeOutput();
    HRESULT Bind_ShaderResources();
    HRESULT Bind_MaterialResources();
    HRESULT Bind_DistortionResources();
    bool Can_SubmitRender() const;
    uint32 Compute_MaxRenderSegmentCount() const;
    bool Is_DistortionFamily() const;
    RenderGroup Resolve_RenderGroup() const;
    const wchar_t* Resolve_ShaderId() const;
    uint32 Resolve_ShaderPassIndex() const;
    int Resolve_OpacitySourceIndex() const;
    Vec4 Evaluate_ColorOverLife(float lifeProgress) const;
    float Evaluate_WidthScaleByLife(float lifeProgress) const;
    Vec3 Evaluate_BeamEnvelopeOverLife(float lifeProgress) const;
    static PathSample Interpolate_PathSample(const PathSample& left, const PathSample& right, float targetDistance);
    uint32 Evaluate_SubUVFrameIndex(uint32 stripIndex) const;
    Vec4 Resolve_SubUVRect(uint32 frameIndex) const;
    uint32 Resolve_RandomSubUVFrameIndex(uint32 stripIndex) const;
    uint32 Resolve_FpsSubUVFramePhaseOffset(uint32 stripIndex, uint32 rangeCount) const;
    void Resample_RandomSubUVFrameIndex();
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

public:
    static Shared<ComputeGeneratedBeamEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
