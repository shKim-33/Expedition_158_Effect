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

class ComputeSourceHistoryRibbonEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeSourceHistoryRibbonEmitter, ObjectType::EffectEmitter);

public:
    ComputeSourceHistoryRibbonEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeSourceHistoryRibbonEmitter(const ComputeSourceHistoryRibbonEmitter& prototype);
    ~ComputeSourceHistoryRibbonEmitter() override;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    HRESULT Reset_ForEffectReplay() override;
    bool Is_EffectFinished() const override;
    void Bind_SourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider) override;
    void Bind_RibbonSourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider) override;
    bool Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const override;
    EffectSortPolicy Get_BlendSortPolicy() const override;
    int32 Get_BlendSortLayer() const override;
    float Get_BlendSortBias() const override;

private: //## Types::PlaybackRuntime
    enum class PlaybackState : uint8
    {
        Delayed,
        Emitting,
        Draining,
        Completed,
    };

    struct HistorySample
    {
        Vec3 position{};
        float age{};
        float distance{};
        uint32 serial{};
    };

    struct RenderSample
    {
        HistorySample sample{};
        float distanceFromHead{};
    };

    struct SegmentPayload
    {
        Vec4 previousPosition{};
        Vec4 currentPosition{};
        Vec4 nextPosition{};
        Vec4 nextNextPosition{};
        Vec4 currentSampleParams{};
        Vec4 nextSampleParams{};
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 subUVRect{ 0.f, 0.f, 1.f, 1.f };
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

    struct HistoryLane
    {
        uint32 sourceIndex{};
        Vec3 currentSourcePosition{};
        Vec3 previousSourcePosition{};
        vector<HistorySample> history{};
        float sampleIntervalAccumulator{};
        float collapseLength{ -1.f };
        float age{};
        float missingTime{};
        Vec3 headOffset{};
        bool activeThisFrame{};
        bool hasPreviousSourcePosition{};
    };

    struct RetiredLoopStroke
    {
        vector<HistorySample> history{};
        float widthMultiplier{ 1.f };
    };

    struct RibbonComputeParams
    {
        uint32 segmentCount{};
        uint32 renderAxis{};
        float tilingDistance{};
        float visibleLength{ 1.f };
        Vec4 axisFallback{};
        Vec4 fadeParams{};
    };

    struct RibbonInstanceVertex
    {
        Vec4 previousPosition{};
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

    struct DrawIndexedInstancedIndirectArgs
    {
        uint32 indexCountPerInstance{};
        uint32 instanceCount{};
        uint32 startIndexLocation{};
        int32 baseVertexLocation{};
        uint32 startInstanceLocation{};
    };

private: //## Static::Runtime
    static constexpr auto kRibbonShaderId{ L"Shader_EffectRibbon" };
    static constexpr auto kRibbonDistortionShaderId{ L"Shader_EffectRibbonDistortion" };
    static constexpr auto kRibbonComputeShaderId{ L"ComputeShader_RibbonStrip" };
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr auto kNeutralFlowTexturePath{ "Effects/Textures/Shared/DefaultFlowMap.dds" };
    static constexpr uint32 kThreadCountX{ 64 };
    static constexpr uint32 kMaxCurveSubdivision{ 32 };
    static constexpr uint32 kMaxRetiredLoopStrokeCount{ 4 };
    static constexpr float kSmoothSampleSideWeight{ 0.25f };
    static constexpr float kParticleFollowerLaneRetireGrace{ 0.08f };
    static constexpr float kMinRenderableRibbonLength{ 0.0001f };

private: //## Data::PlaybackRuntime
    ComputeRibbonEmitterDesc _desc{};
    PlaybackState _playbackState{ PlaybackState::Delayed };
    bool _finishEmissionAfterHistoryUpdate{};
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    vector<HistorySample> _history{};
    float _sampleIntervalAccumulator{};
    float _selfRootCollapseLength{ -1.f };
    Vec3 _selfRootSourcePosition{};
    Vec3 _selfRootPreviousSourcePosition{};
    Vec3 _selfRootHeadOffset{};
    Weak<IEffectSourcePointSampleProvider> _ribbonSourcePointSampleProvider{};
    bool _hasSelfRootPreviousSourcePosition{};
    vector<HistoryLane> _sourceLanes{};
    vector<RetiredLoopStroke> _retiredLoopStrokes{};
    float _materialElapsedTime{};
    float _visibleRibbonLength{ 1.f };
    uint32 _drawSegmentCount{};
    uint32 _randomSubUVFrameIndex{};
    uint32 _sampleSerialCounter{};

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
    void Reset_PlaybackRuntime();
    void Advance_Playback(float timeDelta);
    bool Is_UpdatingHistory() const;
    bool Is_EmittingSourceSamples() const;
    void Update_History(float timeDelta);
    void Update_SelfRootHistory(float timeDelta);
    void Update_ParticleFollowerHistory(float timeDelta);
    void Insert_HistorySample(vector<HistorySample>& history, float& sampleIntervalAccumulator, const Vec3& sourcePosition);
    void Prune_History(vector<HistorySample>& history);
    void Build_RenderSamples(vector<RenderSample>& outSamples) const;
    void Build_RenderSamples(
        const vector<HistorySample>& history,
        vector<RenderSample>& outSamples,
        const Vec3* currentSourcePosition = nullptr,
        const float* visibleLengthLimit = nullptr) const;
    HRESULT Append_SegmentPayloads(
        const vector<RenderSample>& renderSamples,
        vector<SegmentPayload>& outPayloads,
        float widthMultiplier = 1.f) const;
    bool Has_RenderableHistory() const;
    bool Has_RenderableHistory(const vector<HistorySample>& history, const Vec3* currentSourcePosition = nullptr) const;
    const Vec3* Resolve_LaneCurrentHeadForRender(const HistoryLane& lane) const;
    HistoryLane* Find_HistoryLane(uint32 sourceIndex);
    void Clear_ParticleFollowerHistory();
    void Retire_ActiveLoopStrokes();
    void Retire_HistoryStroke(vector<HistorySample> history, const Vec3* currentSourcePosition, float widthMultiplier);
    void Update_RetiredLoopStrokes(float timeDelta);
    void Refresh_VisibleRibbonLengthFromRetired();

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
    EffectRibbonRenderAxis Resolve_RibbonRenderAxis() const;
    Vec4 Resolve_RibbonFallbackAxis() const;
    Vec3 Resolve_SourceBasisAxis() const;
    bool Can_SubmitRender() const;
    uint32 Compute_MaxLaneRenderSegmentCount() const;
    uint32 Compute_MaxRenderSegmentCount() const;

    void Wrap_PlaybackLoop();
    void Start_NextLoop();
    float Resolve_CurrentLoopDelay() const;
    float Resolve_Duration() const;
    bool Has_NextLoop() const;
    bool Is_DistortionFamily() const;
    RenderGroup Resolve_RenderGroup() const;
    const wchar_t* Resolve_ShaderId() const;
    uint32 Resolve_ShaderPassIndex() const;
    int Resolve_OpacitySourceIndex() const;
    Vec3 Sample_HeadOffset(uint32 seed) const;
    Vec3 Resolve_HeadOffsetWorld(const Vec3& localOffset, const Quat* sourceRotation = nullptr) const;
    Vec3 Apply_HeadOffset(const Vec3& sourcePosition, const Vec3& localOffset, const Quat* sourceRotation = nullptr) const;
    bool Try_Resolve_SourcePosition(Vec3& outPosition) const;
    Vec3 Resolve_SourcePosition() const;
    Vec4 Evaluate_ColorOverLife(float lifeProgress) const;
    float Evaluate_WidthScaleByLife(float lifeProgress) const;
    uint32 Evaluate_SubUVFrameIndex() const;
    Vec4 Resolve_SubUVRect(uint32 frameIndex) const;
    void Resample_SampleLifetime();
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
    static Shared<ComputeSourceHistoryRibbonEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
