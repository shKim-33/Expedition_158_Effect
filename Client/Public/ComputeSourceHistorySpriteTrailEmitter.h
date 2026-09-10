#pragma once
#include "Client_Defines.h"
#include "EffectEmitter.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(Client)

class ComputeSourceHistorySpriteTrailEmitter final : public EffectEmitter
{
    GENERATED_GAMEOBJECT(ComputeSourceHistorySpriteTrailEmitter, ObjectType::EffectEmitter);

public:
    ComputeSourceHistorySpriteTrailEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeSourceHistorySpriteTrailEmitter(const ComputeSourceHistorySpriteTrailEmitter& prototype);
    ~ComputeSourceHistorySpriteTrailEmitter() override;

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
        float distanceFromHead{};
        float sampleTime{};
        float recordedSpeed{};
    };

    struct Stamp
    {
        Vec3 center{};
        Vec3 tangent{ 1.f, 0.f, 0.f };
        float age{};
        float lifetime{ 0.18f };
        uint32 serial{};
        uint32 frameIndex{};
        Vec4 startColor{ 1.f, 1.f, 1.f, 1.f };
        Vec4 endColor{ 1.f, 1.f, 1.f, 1.f };
        Vec3 coreColorRgb{ 1.f, 0.85f, 0.45f };
        float widthScale{ 1.f };
        float lengthScale{ 1.f };
        float initialRotationRadians{};
        float initialAngularVelocityRadians{};
        Vec2 initialTiltDegrees{};
        Vec2 tiltOverLifeDegrees{};
        Vec3 velocity{};
        Vec3 initialVelocity{};
        Vec3 initialRadialVelocity{};
        Vec3 velocityCone{};
        Vec3 sourceMotionVelocity{};
        Vec3 accelerationIntegratedVelocity{};
        Vec3 acceleration{};
        float drag{};
        bool pathFollowActive{};
        bool pathFollowArrived{};
        float pathDistanceFromHead{};
        float pathFollowSpeed{ 1.f };
        float pathFollowStartDelay{};
        bool pathReplayActive{};
        bool pathReplayArrived{};
        float pathReplayDelayRemaining{};
        float pathReplayStartDistance{};
        float pathReplayElapsedTime{};
    };

    struct SourceMotionVelocityContext
    {
        Vec3 sourceVelocity{};
        Vec3 tangent{ 1.f, 0.f, 0.f };
        bool sourceVelocityValid{};
    };

    struct HistoryLane
    {
        uint32 sourceIndex{};
        Vec3 currentSourcePosition{};
        Vec3 lastValidTangent{ 1.f, 0.f, 0.f };
        vector<HistorySample> history{};
        vector<Stamp> stamps{};
        float sampleAccumulator{};
        float stampDistanceAccumulator{};
        float stampTimeAccumulator{};
        float missingTime{};
        bool activeThisFrame{};
    };

    struct RetiredLoopStroke
    {
        vector<HistorySample> history{};
        vector<Stamp> stamps{};
        Vec3 lastValidTangent{ 1.f, 0.f, 0.f };
    };

    struct StampInstanceVertex
    {
        Vec4 centerAndLength{};
        Vec4 tangentAndWidth{};
        Vec2 lifeTime{};
        Vec4 startColor{};
        Vec4 endColor{};
        Vec4 subUVRect{ 0.f, 0.f, 1.f, 1.f };
        Vec4 rotationAndTilt{};
        Vec4 coreColorRgb{ 1.f, 0.85f, 0.45f, 0.f };
    };

private: //## Static::Runtime
    static constexpr auto kShaderId{ L"Shader_EffectSourceHistorySpriteTrail" };
    static constexpr auto kFallbackTexturePath{ "Effects/Textures/Shared/DefaultTexture.dds" };
    static constexpr uint32 kPointVertexCount{ 1 };
    static constexpr uint32 kMaxCurveSubdivision{ 32 };
    static constexpr uint32 kMaxRetiredLoopStrokeCount{ 4 };
    static constexpr float kParticleFollowerLaneRetireGrace{ 0.08f };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr int kOpacitySourceRed{ 1 };
    static constexpr int kOpacitySourceLuminance{ 2 };

private: //## Data::PlaybackRuntime
    ComputeSourceHistorySpriteTrailEmitterDesc _desc{};
    PlaybackState _playbackState{ PlaybackState::Delayed };
    bool _finishEmissionAfterUpdate{};
    float _loopElapsedTime{};
    uint32 _loopIndex{};
    float _materialElapsedTime{};
    Vec3 _selfRootSourcePosition{};
    Vec3 _selfRootLastValidTangent{ 1.f, 0.f, 0.f };
    vector<HistorySample> _history{};
    vector<Stamp> _stamps{};
    float _sampleAccumulator{};
    float _stampDistanceAccumulator{};
    float _stampTimeAccumulator{};
    vector<HistoryLane> _sourceLanes{};
    vector<RetiredLoopStroke> _retiredLoopStrokes{};
    uint32 _drawStampCount{};
    uint32 _stampSerialCounter{};
    int _opacitySourceIndex{ kOpacitySourceAlpha };

private: //## Data::Components
    Shared<ShaderCom> _shader{};
    Shared<Texture> _mainTexture{};
    Shared<Texture> _noiseTexture{};
    Shared<Texture> _maskTexture{};
    ComPtr<Buffer> _pointVB{};
    ComPtr<Buffer> _instanceBuffer{};

private: //## Helper::PlaybackRuntime
    void Reset_PlaybackRuntime();
    void Reset_LoopScopedRuntime();
    void Advance_Playback(float timeDelta);
    bool Is_EmittingSourceSamples() const;
    void Update_HistoryAndStamps(float timeDelta);
    void Update_SelfRoot(float timeDelta);
    void Update_ParticleFollowers(float timeDelta);
    void Update_Lane(HistoryLane& lane, const Vec3& sourcePosition, float timeDelta);
    void Retire_ActiveLoopStrokes();
    void Update_RetiredLoopStrokes(float timeDelta);
    bool Has_LiveStamps() const;
    void Age_Stamps(vector<Stamp>& stamps, float timeDelta) const;
    void Update_StampMotion(Stamp& stamp, float timeDelta) const;
    void Insert_HistorySample(vector<HistorySample>& history, float& sampleAccumulator, const Vec3& sourcePosition, float sampleTime);
    void Update_PathFollowStamps(vector<Stamp>& stamps, const vector<HistorySample>& history, const Vec3& fallbackTangent, float timeDelta) const;
    void Update_PathReplayStamps(vector<Stamp>& stamps, const vector<HistorySample>& history, const Vec3& fallbackTangent, float timeDelta) const;
    void Prune_History(vector<HistorySample>& history, float requiredTailDistance = 0.f) const;
    void Recompute_Distances(vector<HistorySample>& history) const;
    bool Try_ResolveSmoothedHistoryTangent(const vector<HistorySample>& history, size_t sampleIndex, Vec3& outTangent) const;
    float Resolve_RequiredHistoryDistance(const vector<Stamp>& stamps) const;
    float Resolve_HistoryDistanceForPosition(const vector<HistorySample>& history, const Vec3& position) const;
    bool Sample_HistoryAtDistance(
        const vector<HistorySample>& history,
        float distanceFromHead,
        const Vec3& fallbackTangent,
        Vec3& outPosition,
        Vec3& outTangent) const;
    float Sample_RecordedSpeedAtDistance(const vector<HistorySample>& history, float distanceFromHead) const;
    void Spawn_StampsForPath(
        vector<Stamp>& stamps,
        float& distanceAccumulator,
        float& timeAccumulator,
        Vec3& lastValidTangent,
        const vector<HistorySample>& history,
        const Vec3& previousPosition,
        const Vec3& currentPosition,
        float timeDelta);
    void Push_Stamp(
        vector<Stamp>& stamps,
        const Vec3& center,
        const Vec3& tangent,
        const SourceMotionVelocityContext& sourceMotionContext,
        float pathDistanceFromHead,
        float widthScale,
        float lengthScale);
    HistoryLane* Find_HistoryLane(uint32 sourceIndex);

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_Texture();
    HRESULT Ready_OptionalTexture(Shared<Texture>& outTexture, const string& textureGuid, const string& texturePath);
    HRESULT Ready_DrawBuffers();

private: //## Helper::Render
    HRESULT Update_InstanceBuffer();
    HRESULT Append_StampInstances(const vector<Stamp>& stamps, vector<StampInstanceVertex>& outInstances) const;
    HRESULT Bind_ShaderResources();
    bool Can_SubmitRender() const;
    uint32 Compute_MaxStampCount() const;

    float Resolve_CurrentLoopDelay() const;
    float Resolve_Duration() const;
    bool Has_NextLoop() const;
    void Start_NextLoop();
    RenderGroup Resolve_RenderGroup() const;
    uint32 Resolve_ShaderPassIndex() const;
    int Resolve_OpacitySourceIndex() const;
    bool Try_Resolve_SourcePosition(Vec3& outPosition) const;
    Vec3 Resolve_SourcePosition() const;
    float Sample_Lifetime(uint32 stampSerial) const;
    Vec2 Sample_InitialSize(uint32 stampSerial) const;
    float Sample_InitialRotation(uint32 stampSerial) const;
    float Sample_InitialAngularVelocity(uint32 stampSerial) const;
    Vec2 Sample_InitialTilt(uint32 stampSerial) const;
    Vec2 Sample_TiltOverLife(uint32 stampSerial) const;
    void Sample_InitialVelocityChannels(
        Stamp& stamp,
        uint32 stampSerial,
        const Vec3& spawnCenter,
        const SourceMotionVelocityContext& sourceMotionContext) const;
    Vec3 Sample_Acceleration(uint32 stampSerial) const;
    float Sample_Drag(uint32 stampSerial) const;
    float Sample_PathFollowSpeed(uint32 stampSerial) const;
    float Sample_PathFollowStartDelay(uint32 stampSerial) const;
    Vec4 Sample_Color(const Vec4& minColor, const Vec4& maxColor, const PointParticleRandomSeedRuntimeDesc& seed, uint32 stampSerial, uint32 salt) const;
    float Sample_Range(float minValue, float maxValue, const PointParticleRandomSeedRuntimeDesc& seed, uint32 stampSerial, uint32 salt) const;
    Vec2 Sample_Vector2Range(const Vec2& minValue, const Vec2& maxValue, const PointParticleRandomSeedRuntimeDesc& seed, uint32 stampSerial, uint32 salt) const;
    Vec3 Sample_VectorRange(const Vec3& minValue, const Vec3& maxValue, const PointParticleRandomSeedRuntimeDesc& seed, uint32 stampSerial, uint32 salt) const;
    Vec4 Evaluate_ColorOverLife(const Stamp& stamp, float lifeProgress) const;
    float Evaluate_WidthScaleByLife(float lifeProgress) const;
    float Evaluate_LengthScaleByLife(float lifeProgress) const;
    float Evaluate_RotationOverLife(float lifeProgress) const;
    float Evaluate_RotationRateScaleByLife(float lifeProgress) const;
    Vec2 Evaluate_SpriteTiltOverLife(const Stamp& stamp, float lifeProgress) const;
    float Evaluate_VelocityScaleByLifeChannel(const PointParticleFloatCurveRuntimeDesc& curve, float lifeProgress) const;
    Vec3 Evaluate_ScaledVelocityChannels(const Stamp& stamp, float lifeProgress) const;
    Vec3 Evaluate_AccelerationByLife(const Stamp& stamp, float lifeProgress) const;
    float Evaluate_PathReplayDrainCurve(float progress) const;
    uint32 Evaluate_SubUVFrameIndex(const Stamp& stamp) const;
    uint32 Sample_RandomSubUVFrameIndex(uint32 stampSerial, uint32 startFrame, uint32 frameCount, uint32 rangeCount) const;
    Vec4 Resolve_SubUVRect(uint32 frameIndex) const;
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
    static Shared<ComputeSourceHistorySpriteTrailEmitter> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
