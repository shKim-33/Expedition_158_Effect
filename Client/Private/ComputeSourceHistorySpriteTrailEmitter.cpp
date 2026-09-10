#include "ComputeSourceHistorySpriteTrailEmitter.h"

#include "EffectInstance.h"
#include "EffectMaterialScalarModulationRuntime.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"

NS_BEGIN(Client)

namespace
{
    Vec3 Normalize_OrFallback(Vec3 value, const Vec3& fallback)
    {
        if (value.LengthSquared() <= 0.0001f)
            return fallback;
        value.Normalize();
        return value;
    }

    Vec3 Catmull_Rom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float ratio)
    {
        const float t2 = ratio * ratio;
        const float t3 = t2 * ratio;

        return (p1 * 2.f
                + (p2 - p0) * ratio
                + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2
                + (p1 * 3.f - p0 - p2 * 3.f + p3) * t3)
               * 0.5f;
    }

    Vec3 Catmull_RomDerivative(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float ratio)
    {
        const float t2 = ratio * ratio;

        return (p2 - p0
                + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * (2.f * ratio)
                + (p1 * 3.f - p0 - p2 * 3.f + p3) * (3.f * t2))
               * 0.5f;
    }

    float Hash01(uint32 value)
    {
        value ^= value >> 16u;
        value *= 0x7feb352du;
        value ^= value >> 15u;
        value *= 0x846ca68bu;
        value ^= value >> 16u;
        return static_cast<float>(value & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
    }

    uint32 Resolve_SeedSalt(const PointParticleRandomSeedRuntimeDesc& seed, uint32 effectPlaybackSeed)
    {
        uint32 salt = seed.manualSeedEnabled ? seed.seed : 0u;
        if (seed.useInstanceSeed)
            salt += effectPlaybackSeed;
        return salt;
    }

    struct SubUVFrameRange
    {
        uint32 startFrame{};
        uint32 frameCount{ 1u };
        uint32 rangeCount{ 1u };
    };

    SubUVFrameRange Resolve_SubUVFrameRange(uint32 startFrame, uint32 endFrame, uint32 frameCount)
    {
        const uint32 safeFrameCount = max(1u, frameCount);
        const uint32 lastFrame = safeFrameCount - 1u;
        const uint32 clampedStart = min(startFrame, lastFrame);
        const uint32 clampedEnd = min(endFrame, lastFrame);
        const uint32 rangeCount =
            clampedEnd >= clampedStart
            ? clampedEnd - clampedStart + 1u
            : safeFrameCount - clampedStart + clampedEnd + 1u;
        return SubUVFrameRange{ clampedStart, safeFrameCount, max(1u, rangeCount) };
    }

    uint32 Resolve_SubUVFrameInRange(const SubUVFrameRange& range, uint32 frameOffset)
    {
        return (range.startFrame + min(frameOffset, range.rangeCount - 1u)) % range.frameCount;
    }

    uint32 Resolve_SubUVPlaybackOffset(const SubUVFrameRange& range, uint32 frameOffset, uint32 phaseOffset, bool loop)
    {
        const uint32 offset = frameOffset + phaseOffset;
        return loop ? offset % range.rangeCount : min(offset, range.rangeCount - 1u);
    }

    int Resolve_MaterialSourceIndex(const string& source)
    {
        if (source == "Red" || source == "red")
            return 1;

        if (source == "Luminance" || source == "luminance")
            return 2;

        return 0;
    }

    HRESULT Bind_MaterialScalarModulationPayload(
        ShaderCom* shader,
        const EffectMaterialScalarModulationRuntimeDesc& modulation,
        float emitterPhase)
    {
        CHECK_NULL(shader, E_FAIL);

        const EffectMaterialScalarModulationShaderPayload payload =
            Build_MaterialScalarModulationShaderPayload(modulation, emitterPhase);

        CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationParams", &payload.params, sizeof(payload.params)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationMeta", payload.meta.data(), sizeof(payload.meta)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyTimes", payload.keyTimes.data(), sizeof(payload.keyTimes)), E_FAIL);
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyTimesBlock1", payload.keyTimesBlock1.data(), sizeof(payload.keyTimesBlock1)),
            E_FAIL
        );
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyValues", payload.keyValues.data(), sizeof(payload.keyValues)), E_FAIL);
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyValuesBlock1", payload.keyValuesBlock1.data(), sizeof(payload.keyValuesBlock1)),
            E_FAIL
        );
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyArriveTangents", payload.keyArriveTangents.data(), sizeof(payload.keyArriveTangents)),
            E_FAIL
        );
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyArriveTangentsBlock1", payload.keyArriveTangentsBlock1.data(), sizeof(payload.
                keyArriveTangentsBlock1)),
            E_FAIL
        );
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyLeaveTangents", payload.keyLeaveTangents.data(), sizeof(payload.keyLeaveTangents)),
            E_FAIL
        );
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyLeaveTangentsBlock1", payload.keyLeaveTangentsBlock1.data(), sizeof(payload.
                keyLeaveTangentsBlock1)),
            E_FAIL
        );
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyModes", payload.keyModes.data(), sizeof(payload.keyModes)), E_FAIL);
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyModesBlock1", payload.keyModesBlock1.data(), sizeof(payload.keyModesBlock1)),
            E_FAIL
        );
        return S_OK;
    }

    float Read_Vec4Component(const Vec4& valuesBlock0, const Vec4& valuesBlock1, uint32 index)
    {
        const Vec4& values = index < 4u ? valuesBlock0 : valuesBlock1;
        switch (index % 4u)
        {
        case 0u:
            return values.x;
        case 1u:
            return values.y;
        case 2u:
            return values.z;
        case 3u:
        default:
            return values.w;
        }
    }

    wstring Resolve_TexturePathByGuid(const string& textureGuid)
    {
        if (GAME == nullptr || textureGuid.empty())
            return {};

        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (assetMeta == nullptr || assetMeta->type != "Texture")
            return {};

        const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
        return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
    }

    wstring Resolve_TexturePathByPath(const string& texturePath)
    {
        if (texturePath.empty())
            return {};

        fs::path candidatePath = String::ToWString(texturePath);
        if (candidatePath.is_relative() && GAME != nullptr)
            candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

        candidatePath = candidatePath.lexically_normal();
        return fs::exists(candidatePath) ? candidatePath.wstring() : wstring{};
    }

    bool Is_SamePath(const wstring& lhs, const wstring& rhs)
    {
        if (lhs.empty() || rhs.empty())
            return false;

        return fs::path(lhs).lexically_normal() == fs::path(rhs).lexically_normal();
    }

    float RandomRange(float minValue, float maxValue, uint32 seed)
    {
        const float low = min(minValue, maxValue);
        const float high = max(minValue, maxValue);
        if (low == high)
            return low;

        return lerp(low, high, Hash01(seed));
    }

    Vec3 SampleConeDirection(const Vec3& axis, float angleDegrees, uint32 seed)
    {
        const Vec3 normalizedAxis = Normalize_OrFallback(axis, Vec3{ 0.f, 1.f, 0.f });
        const float clampedAngle = clamp(angleDegrees, 0.f, 180.f);
        if (clampedAngle <= 0.0001f)
            return normalizedAxis;

        const float cosMax = cosf(XMConvertToRadians(clampedAngle));
        const float cosTheta = lerp(1.f, cosMax, Hash01(seed + 1703u));
        const float sinTheta = sqrtf(max(0.f, 1.f - cosTheta * cosTheta));
        const float phi = XM_2PI * Hash01(seed + 1721u);
        const Vec3 helper = fabsf(normalizedAxis.y) < 0.999f ? Vec3{ 0.f, 1.f, 0.f } : Vec3{ 1.f, 0.f, 0.f };
        Vec3 tangent = helper.Cross(normalizedAxis);
        tangent.Normalize();
        const Vec3 bitangent = normalizedAxis.Cross(tangent);
        return normalizedAxis * cosTheta + (tangent * cosf(phi) + bitangent * sinf(phi)) * sinTheta;
    }

    wstring Resolve_TexturePath(const string& textureGuid, const string& texturePath, bool& outUsedPathFallback, bool& outGuidPathMismatch)
    {
        outUsedPathFallback = false;
        outGuidPathMismatch = false;

        wstring resolvedPath = Resolve_TexturePathByGuid(textureGuid);
        if (!resolvedPath.empty())
        {
            const wstring pathResolved = Resolve_TexturePathByPath(texturePath);
            outGuidPathMismatch = !pathResolved.empty() && !Is_SamePath(resolvedPath, pathResolved);
            return resolvedPath;
        }

        resolvedPath = Resolve_TexturePathByPath(texturePath);
        if (!resolvedPath.empty())
        {
            outUsedPathFallback = true;
            return resolvedPath;
        }

        return {};
    }
}

IMPLEMENT_REFLECTION(ComputeSourceHistorySpriteTrailEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "ComputeSourceHistorySpriteTrailEmitter";
    info.category = "Effect";
    return true;
}

ComputeSourceHistorySpriteTrailEmitter::ComputeSourceHistorySpriteTrailEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter(device, context)
{
}

ComputeSourceHistorySpriteTrailEmitter::ComputeSourceHistorySpriteTrailEmitter(const ComputeSourceHistorySpriteTrailEmitter& prototype)
    : EffectEmitter(prototype)
    , _desc(prototype._desc)
{
}

ComputeSourceHistorySpriteTrailEmitter::~ComputeSourceHistorySpriteTrailEmitter() = default;

HRESULT ComputeSourceHistorySpriteTrailEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Initialize(void* arg)
{
    if (arg != nullptr)
        _desc = *static_cast<ComputeSourceHistorySpriteTrailEmitterDesc*>(arg);

    _desc.followerLaneCount = max(1u, _desc.followerLaneCount);
    _desc.sampleLifetime = max(0.0001f, _desc.sampleLifetime);
    _desc.sampleSpacing = max(0.001f, _desc.sampleSpacing);
    _desc.curveSubdivision = min(_desc.curveSubdivision, kMaxCurveSubdivision);
    _desc.stampSpacing = max(0.001f, _desc.stampSpacing);
    _desc.stampInterval = max(0.001f, _desc.stampInterval);
    _desc.stampLifetime = max(0.0001f, _desc.stampLifetime);
    _desc.maxStampCount = max(1u, _desc.maxStampCount);
    _desc.cardLength = max(0.001f, _desc.cardLength);
    _desc.cardWidth = max(0.001f, _desc.cardWidth);
    _desc.spawnJitter = max(0.f, _desc.spawnJitter);
    _desc.pathReplay.delayTime = max(0.f, _desc.pathReplay.delayTime);
    _desc.pathReplay.speedScale = max(0.f, _desc.pathReplay.speedScale);
    _desc.pathReplay.drainDuration = max(0.0001f, _desc.pathReplay.drainDuration);
    if (_desc.pathReplay.enabled)
        _desc.pathFollow.enabled = false;

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    Reset_PlaybackRuntime();
    return S_OK;
}

void ComputeSourceHistorySpriteTrailEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();

    if (!_enabled)
        return;

    if (timeDelta <= 0.f)
        return;

    Advance_Playback(timeDelta);
    Update_HistoryAndStamps(timeDelta);
    _materialElapsedTime += timeDelta;
}

void ComputeSourceHistorySpriteTrailEmitter::Late_Update(float)
{
    if (!Can_SubmitRender())
        return;

    GAME->Add_RenderGroup(Resolve_RenderGroup(), dynamic_pointer_cast<GameObject>(shared_from_this()));
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Render()
{
    if (!Can_SubmitRender())
        return S_OK;

    CHECK_FAILED(Update_InstanceBuffer(), E_FAIL);
    if (_drawStampCount == 0u)
        return S_OK;

    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED(_shader->Begin(Resolve_ShaderPassIndex()), E_FAIL);

    ID3D11Buffer* vertexBuffers[] = { _pointVB.Get(), _instanceBuffer.Get() };
    const uint32 strides[] = { sizeof(Vec3), sizeof(StampInstanceVertex) };
    const uint32 offsets[] = { 0u, 0u };
    _context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    _context->DrawInstanced(kPointVertexCount, _drawStampCount, 0, 0);
    return S_OK;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    Reset_LoopScopedRuntime();
    _retiredLoopStrokes.clear();
    _materialElapsedTime = 0.f;
    _stampSerialCounter = 0u;
    return S_OK;
}

bool ComputeSourceHistorySpriteTrailEmitter::Is_EffectFinished() const
{
    if (PlaybackState::Completed != _playbackState)
        return false;

    return !Has_LiveStamps();
}

void ComputeSourceHistorySpriteTrailEmitter::Bind_SourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider)
{
    _desc.sourcePointSampleProvider = provider;
}

bool ComputeSourceHistorySpriteTrailEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (!_stamps.empty())
    {
        outWorldPosition = _stamps.front().center;
        return true;
    }

    for (const HistoryLane& lane : _sourceLanes)
    {
        if (!lane.stamps.empty())
        {
            outWorldPosition = lane.stamps.front().center;
            return true;
        }
    }

    for (const RetiredLoopStroke& stroke : _retiredLoopStrokes)
    {
        if (!stroke.stamps.empty())
        {
            outWorldPosition = stroke.stamps.front().center;
            return true;
        }
    }

    outWorldPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
    return true;
}

EffectSortPolicy ComputeSourceHistorySpriteTrailEmitter::Get_BlendSortPolicy() const
{
    return _desc.sort.sortPolicy;
}

int32 ComputeSourceHistorySpriteTrailEmitter::Get_BlendSortLayer() const
{
    return _desc.sort.sortLayer;
}

float ComputeSourceHistorySpriteTrailEmitter::Get_BlendSortBias() const
{
    return _desc.sort.artistSortBias;
}

void ComputeSourceHistorySpriteTrailEmitter::Reset_PlaybackRuntime()
{
    _playbackState = PlaybackState::Delayed;
    _finishEmissionAfterUpdate = false;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
}

void ComputeSourceHistorySpriteTrailEmitter::Reset_LoopScopedRuntime()
{
    _history.clear();
    _stamps.clear();
    _sourceLanes.clear();
    _sampleAccumulator = 0.f;
    _stampDistanceAccumulator = 0.f;
    _stampTimeAccumulator = 0.f;
    _selfRootSourcePosition = Vec3{};
    _selfRootLastValidTangent = Vec3{ 1.f, 0.f, 0.f };
    _drawStampCount = 0u;
}

void ComputeSourceHistorySpriteTrailEmitter::Advance_Playback(float timeDelta)
{
    _finishEmissionAfterUpdate = false;

    if (PlaybackState::Completed == _playbackState ||
        PlaybackState::Draining == _playbackState)
        return;

    _loopElapsedTime += max(0.f, timeDelta);
    const float delay = Resolve_CurrentLoopDelay();
    if (_loopElapsedTime < delay)
    {
        _playbackState = PlaybackState::Delayed;
        return;
    }

    const float activeElapsedTime = _loopElapsedTime - delay;
    if (activeElapsedTime < Resolve_Duration())
    {
        _playbackState = PlaybackState::Emitting;
        return;
    }

    if (Has_NextLoop())
    {
        Start_NextLoop();
        return;
    }

    _playbackState = PlaybackState::Emitting;
    _finishEmissionAfterUpdate = true;
}

bool ComputeSourceHistorySpriteTrailEmitter::Is_EmittingSourceSamples() const
{
    return PlaybackState::Emitting == _playbackState;
}

void ComputeSourceHistorySpriteTrailEmitter::Update_HistoryAndStamps(float timeDelta)
{
    Update_RetiredLoopStrokes(timeDelta);

    if (_desc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
        Update_ParticleFollowers(timeDelta);
    else
        Update_SelfRoot(timeDelta);

    if (_finishEmissionAfterUpdate)
        _playbackState = PlaybackState::Draining;

    if (PlaybackState::Draining == _playbackState && !Has_LiveStamps())
        _playbackState = PlaybackState::Completed;
}

void ComputeSourceHistorySpriteTrailEmitter::Update_SelfRoot(float timeDelta)
{
    Age_Stamps(_stamps, timeDelta);
    for (HistorySample& sample : _history)
        sample.age += max(0.f, timeDelta);

    if (Is_EmittingSourceSamples())
    {
        Vec3 sourcePosition{};
        if (Try_Resolve_SourcePosition(sourcePosition))
        {
            if (_history.empty())
            {
                Insert_HistorySample(_history, _sampleAccumulator, sourcePosition, _materialElapsedTime);
                _selfRootSourcePosition = sourcePosition;
            }
            else
            {
                const Vec3 previousPosition = _history.front().position;
                const bool movedEnough = (sourcePosition - previousPosition).Length() >= _desc.sampleSpacing;
                _sampleAccumulator += max(0.f, timeDelta);
                if (movedEnough)
                    Insert_HistorySample(_history, _sampleAccumulator, sourcePosition, _materialElapsedTime);
                Spawn_StampsForPath(
                    _stamps,
                    _stampDistanceAccumulator,
                    _stampTimeAccumulator,
                    _selfRootLastValidTangent,
                    _history,
                    previousPosition,
                    sourcePosition,
                    timeDelta
                );
                _selfRootSourcePosition = sourcePosition;
            }
        }
    }

    Update_PathFollowStamps(_stamps, _history, _selfRootLastValidTangent, timeDelta);
    Update_PathReplayStamps(_stamps, _history, _selfRootLastValidTangent, timeDelta);
    Prune_History(_history, Resolve_RequiredHistoryDistance(_stamps));
}

void ComputeSourceHistorySpriteTrailEmitter::Update_ParticleFollowers(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    for (HistoryLane& lane : _sourceLanes)
    {
        lane.activeThisFrame = false;
        lane.missingTime += safeDeltaTime;
        lane.sampleAccumulator += safeDeltaTime;
        Age_Stamps(lane.stamps, safeDeltaTime);
        for (HistorySample& sample : lane.history)
            sample.age += safeDeltaTime;
    }

    if (Is_EmittingSourceSamples())
    {
        const Shared<Engine::EffectInstance> effectOwner = dynamic_pointer_cast<Engine::EffectInstance>(_effectOwner.lock());
        const Shared<EffectEmitter> sourceEmitter =
            effectOwner != nullptr ? effectOwner->Find_EmitterById(_desc.sourceEmitterId) : nullptr;

        vector<EffectFollowerSourcePoint> sourcePoints{};
        if (sourceEmitter != nullptr && _desc.followerLaneCount > 0u)
            sourceEmitter->Collect_FollowerSourcePoints(sourcePoints, _desc.followerLaneCount);

        if (sourceEmitter != nullptr && sourceEmitter->Is_EffectFinished() && sourcePoints.empty())
            _finishEmissionAfterUpdate = true;

        for (const EffectFollowerSourcePoint& sourcePoint : sourcePoints)
        {
            HistoryLane* lane = Find_HistoryLane(sourcePoint.sourceIndex);
            if (lane == nullptr)
            {
                _sourceLanes.push_back(HistoryLane{ .sourceIndex = sourcePoint.sourceIndex });
                lane = &_sourceLanes.back();
            }

            lane->activeThisFrame = true;
            lane->missingTime = 0.f;
            Update_Lane(*lane, sourcePoint.position, safeDeltaTime);
        }
    }

    for (HistoryLane& lane : _sourceLanes)
    {
        Update_PathFollowStamps(lane.stamps, lane.history, lane.lastValidTangent, safeDeltaTime);
        Update_PathReplayStamps(lane.stamps, lane.history, lane.lastValidTangent, safeDeltaTime);
        Prune_History(lane.history, Resolve_RequiredHistoryDistance(lane.stamps));
    }

    erase_if(
        _sourceLanes,
        [](const HistoryLane& lane)
        {
            return !lane.activeThisFrame &&
                   lane.missingTime >= kParticleFollowerLaneRetireGrace &&
                   lane.history.empty() &&
                   lane.stamps.empty();
        }
    );
}

void ComputeSourceHistorySpriteTrailEmitter::Update_Lane(HistoryLane& lane, const Vec3& sourcePosition, float timeDelta)
{
    if (lane.history.empty())
    {
        Insert_HistorySample(lane.history, lane.sampleAccumulator, sourcePosition, _materialElapsedTime);
        lane.currentSourcePosition = sourcePosition;
        return;
    }

    const Vec3 previousPosition = lane.history.front().position;
    const bool movedEnough = (sourcePosition - previousPosition).Length() >= _desc.sampleSpacing;
    if (movedEnough)
        Insert_HistorySample(lane.history, lane.sampleAccumulator, sourcePosition, _materialElapsedTime);

    Spawn_StampsForPath(
        lane.stamps,
        lane.stampDistanceAccumulator,
        lane.stampTimeAccumulator,
        lane.lastValidTangent,
        lane.history,
        previousPosition,
        sourcePosition,
        timeDelta
    );
    lane.currentSourcePosition = sourcePosition;
}

void ComputeSourceHistorySpriteTrailEmitter::Retire_ActiveLoopStrokes()
{
    if (_desc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
    {
        for (const HistoryLane& lane : _sourceLanes)
        {
            if (lane.history.empty() && lane.stamps.empty())
                continue;

            _retiredLoopStrokes.push_back(
                RetiredLoopStroke{
                    .history = lane.history,
                    .stamps = lane.stamps,
                    .lastValidTangent = lane.lastValidTangent
                }
            );
        }
    }
    else if (!_history.empty() || !_stamps.empty())
    {
        _retiredLoopStrokes.push_back(
            RetiredLoopStroke{
                .history = _history,
                .stamps = _stamps,
                .lastValidTangent = _selfRootLastValidTangent
            }
        );
    }

    while (_retiredLoopStrokes.size() > kMaxRetiredLoopStrokeCount)
    {
        _retiredLoopStrokes.erase(_retiredLoopStrokes.begin());
    }
}

void ComputeSourceHistorySpriteTrailEmitter::Update_RetiredLoopStrokes(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    for (RetiredLoopStroke& stroke : _retiredLoopStrokes)
    {
        Age_Stamps(stroke.stamps, safeDeltaTime);
        for (HistorySample& sample : stroke.history)
            sample.age += safeDeltaTime;

        Update_PathFollowStamps(stroke.stamps, stroke.history, stroke.lastValidTangent, safeDeltaTime);
        Update_PathReplayStamps(stroke.stamps, stroke.history, stroke.lastValidTangent, safeDeltaTime);
        Prune_History(stroke.history, Resolve_RequiredHistoryDistance(stroke.stamps));
    }

    erase_if(
        _retiredLoopStrokes,
        [](const RetiredLoopStroke& stroke)
        {
            return stroke.stamps.empty();
        }
    );
}

bool ComputeSourceHistorySpriteTrailEmitter::Has_LiveStamps() const
{
    if (!_stamps.empty())
        return true;

    if (ranges::any_of(
        _sourceLanes,
        [](const HistoryLane& lane)
        {
            return !lane.stamps.empty();
        }
    ))
        return true;

    return ranges::any_of(
        _retiredLoopStrokes,
        [](const RetiredLoopStroke& stroke)
        {
            return !stroke.stamps.empty();
        }
    );
}

void ComputeSourceHistorySpriteTrailEmitter::Age_Stamps(vector<Stamp>& stamps, float timeDelta) const
{
    const float safeDeltaTime = max(0.f, timeDelta);
    for (Stamp& stamp : stamps)
    {
        Update_StampMotion(stamp, safeDeltaTime);
        stamp.age += safeDeltaTime;
    }

    erase_if(
        stamps,
        [](const Stamp& stamp)
        {
            return stamp.age >= stamp.lifetime;
        }
    );
}

void ComputeSourceHistorySpriteTrailEmitter::Update_StampMotion(Stamp& stamp, float timeDelta) const
{
    if (stamp.pathFollowActive || stamp.pathReplayActive)
        return;

    if (!_desc.motion.enabled || timeDelta <= 0.f)
        return;

    const float lifeProgress = clamp(stamp.age / max(0.0001f, stamp.lifetime), 0.f, 1.f);
    const float accelerationScale = Evaluate_VelocityScaleByLifeChannel(
        _desc.motion.accelerationIntegratedVelocityScaleByLife,
        lifeProgress
    );
    const Vec3 acceleration = Evaluate_AccelerationByLife(stamp, lifeProgress) * accelerationScale;
    stamp.accelerationIntegratedVelocity += acceleration * timeDelta;

    if (stamp.drag > 0.f)
    {
        const float dragDecay = expf(-stamp.drag * timeDelta);
        stamp.initialVelocity *= dragDecay;
        stamp.initialRadialVelocity *= dragDecay;
        stamp.velocityCone *= dragDecay;
        stamp.sourceMotionVelocity *= dragDecay;
        stamp.accelerationIntegratedVelocity *= dragDecay;
    }

    stamp.velocity = Evaluate_ScaledVelocityChannels(stamp, lifeProgress);
    stamp.center += stamp.velocity * timeDelta;
}

void ComputeSourceHistorySpriteTrailEmitter::Insert_HistorySample(
    vector<HistorySample>& history,
    float& sampleAccumulator,
    const Vec3& sourcePosition,
    float sampleTime)
{
    float recordedSpeed = 0.f;
    if (!history.empty())
    {
        const float distance = (sourcePosition - history.front().position).Length();
        const float deltaTime = max(0.0001f, sampleAccumulator);
        recordedSpeed = distance / deltaTime;
        if (!std::isfinite(recordedSpeed))
            recordedSpeed = 0.f;
    }

    history.insert(
        history.begin(),
        HistorySample{
            .position = sourcePosition,
            .sampleTime = sampleTime,
            .recordedSpeed = max(0.f, recordedSpeed)
        }
    );
    sampleAccumulator = 0.f;
    Recompute_Distances(history);
}

void ComputeSourceHistorySpriteTrailEmitter::Update_PathFollowStamps(
    vector<Stamp>& stamps,
    const vector<HistorySample>& history,
    const Vec3& fallbackTangent,
    float timeDelta) const
{
    if (!_desc.pathFollow.enabled)
        return;

    const float tailDistance = history.empty() ? 0.f : history.back().distanceFromHead;
    const float safeDeltaTime = max(0.f, timeDelta);
    for (Stamp& stamp : stamps)
    {
        if (!stamp.pathFollowActive)
            continue;

        if (stamp.pathFollowArrived)
        {
            if (_desc.pathFollow.arrivalMode == EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive)
                continue;

            stamp.pathDistanceFromHead =
                _desc.pathFollow.direction == EffectSourceHistorySpriteTrailPathFollowDirection::TowardTail
                ? tailDistance
                : 0.f;

            Vec3 sampledPosition{};
            Vec3 sampledTangent{};
            if (Sample_HistoryAtDistance(
                history,
                stamp.pathDistanceFromHead,
                stamp.tangent.LengthSquared() > 0.0001f ? stamp.tangent : fallbackTangent,
                sampledPosition,
                sampledTangent
            ))
            {
                stamp.center = sampledPosition;
                stamp.tangent = sampledTangent;
            }
            continue;
        }

        float movementDeltaTime = safeDeltaTime;
        if (stamp.pathFollowStartDelay > 0.f)
        {
            stamp.pathDistanceFromHead = Resolve_HistoryDistanceForPosition(history, stamp.center);
            const float consumedDelay = min(stamp.pathFollowStartDelay, movementDeltaTime);
            stamp.pathFollowStartDelay -= consumedDelay;
            movementDeltaTime -= consumedDelay;
        }

        const bool followStarted = movementDeltaTime > 0.f;
        if (followStarted)
        {
            if (_desc.pathFollow.direction == EffectSourceHistorySpriteTrailPathFollowDirection::TowardTail)
            {
                if (tailDistance <= 0.0001f && stamp.pathDistanceFromHead <= 0.0001f)
                {
                    stamp.pathDistanceFromHead = 0.f;
                    stamp.pathFollowArrived = true;
                }
            }
            else if (stamp.pathDistanceFromHead <= 0.0001f)
            {
                stamp.pathDistanceFromHead = 0.f;
                stamp.pathFollowArrived = true;
            }
        }

        if (followStarted && !stamp.pathFollowArrived)
        {
            const float deltaDistance = max(0.f, stamp.pathFollowSpeed) * movementDeltaTime;
            if (_desc.pathFollow.direction == EffectSourceHistorySpriteTrailPathFollowDirection::TowardTail)
                stamp.pathDistanceFromHead += deltaDistance;
            else
                stamp.pathDistanceFromHead -= deltaDistance;

            if (_desc.pathFollow.direction == EffectSourceHistorySpriteTrailPathFollowDirection::TowardTail)
            {
                if (stamp.pathDistanceFromHead >= tailDistance)
                {
                    stamp.pathDistanceFromHead = tailDistance;
                    stamp.pathFollowArrived = true;
                }
            }
            else if (stamp.pathDistanceFromHead <= 0.f)
            {
                stamp.pathDistanceFromHead = 0.f;
                stamp.pathFollowArrived = true;
            }
        }

        if (stamp.pathFollowArrived &&
            _desc.pathFollow.arrivalMode == EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive)
            continue;

        if (!followStarted)
            continue;

        Vec3 sampledPosition{};
        Vec3 sampledTangent{};
        if (Sample_HistoryAtDistance(
            history,
            stamp.pathDistanceFromHead,
            stamp.tangent.LengthSquared() > 0.0001f ? stamp.tangent : fallbackTangent,
            sampledPosition,
            sampledTangent
        ))
        {
            stamp.center = sampledPosition;
            stamp.tangent = sampledTangent;
        }
    }

    erase_if(
        stamps,
        [this](const Stamp& stamp)
        {
            return stamp.pathFollowActive &&
                   stamp.pathFollowArrived &&
                   _desc.pathFollow.arrivalMode == EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive;
        }
    );
}

void ComputeSourceHistorySpriteTrailEmitter::Update_PathReplayStamps(
    vector<Stamp>& stamps,
    const vector<HistorySample>& history,
    const Vec3& fallbackTangent,
    float timeDelta) const
{
    if (!_desc.pathReplay.enabled)
        return;

    const float safeDeltaTime = max(0.f, timeDelta);
    for (Stamp& stamp : stamps)
    {
        if (!stamp.pathReplayActive ||
            stamp.pathReplayArrived ||
            stamp.pathReplayElapsedTime > 0.f)
            continue;

        const float currentDistanceFromHead = Resolve_HistoryDistanceForPosition(history, stamp.center);
        stamp.pathDistanceFromHead = currentDistanceFromHead;
        stamp.pathReplayStartDistance = currentDistanceFromHead;
    }

    float maxReplayStartDistance = 0.f;
    for (const Stamp& stamp : stamps)
    {
        if (stamp.pathReplayActive && !stamp.pathReplayArrived)
            maxReplayStartDistance = max(maxReplayStartDistance, stamp.pathReplayStartDistance);
    }

    for (Stamp& stamp : stamps)
    {
        if (!stamp.pathReplayActive)
            continue;

        if (stamp.pathReplayArrived)
        {
            if (_desc.pathReplay.arrivalMode == EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive)
                continue;

            stamp.pathDistanceFromHead = 0.f;

            Vec3 sampledPosition{};
            Vec3 sampledTangent{};
            if (Sample_HistoryAtDistance(
                history,
                stamp.pathDistanceFromHead,
                stamp.tangent.LengthSquared() > 0.0001f ? stamp.tangent : fallbackTangent,
                sampledPosition,
                sampledTangent
            ))
            {
                stamp.center = sampledPosition;
                stamp.tangent = sampledTangent;
            }
            continue;
        }

        float movementDeltaTime = safeDeltaTime;
        if (stamp.pathReplayDelayRemaining > 0.f)
        {
            const float consumedDelay = min(stamp.pathReplayDelayRemaining, movementDeltaTime);
            stamp.pathReplayDelayRemaining -= consumedDelay;
            movementDeltaTime -= consumedDelay;
        }

        if (movementDeltaTime > 0.f)
            stamp.pathReplayElapsedTime += movementDeltaTime;

        if (stamp.pathReplayDelayRemaining <= 0.f && stamp.pathReplayStartDistance <= 0.0001f)
        {
            stamp.pathDistanceFromHead = 0.f;
            stamp.pathReplayArrived = true;
        }

        if (stamp.pathReplayArrived)
            continue;

        float tailFirstOffset = 0.f;
        if (_desc.pathReplay.startMode == EffectSourceHistorySpriteTrailPathReplayStartMode::TailFirst &&
            maxReplayStartDistance > 0.0001f)
        {
            const float distanceBehindTail = max(0.f, maxReplayStartDistance - stamp.pathReplayStartDistance);
            if (_desc.pathReplay.replayMode == EffectSourceHistorySpriteTrailPathReplayMode::FitDuration)
            {
                const float duration = max(0.0001f, _desc.pathReplay.drainDuration);
                tailFirstOffset = distanceBehindTail / maxReplayStartDistance * duration;
            }
            else
            {
                const float speed = max(
                    0.0001f,
                    Sample_RecordedSpeedAtDistance(history, stamp.pathReplayStartDistance) * max(0.f, _desc.pathReplay.speedScale)
                );
                tailFirstOffset = distanceBehindTail / speed;
            }
        }

        const float effectiveReplayTime = stamp.pathReplayElapsedTime - tailFirstOffset;
        const bool replayStarted = effectiveReplayTime > 0.f;
        if (replayStarted)
        {
            if (_desc.pathReplay.replayMode == EffectSourceHistorySpriteTrailPathReplayMode::FitDuration)
            {
                const float progress = clamp(effectiveReplayTime / max(0.0001f, _desc.pathReplay.drainDuration), 0.f, 1.f);
                const float curvedProgress = clamp(Evaluate_PathReplayDrainCurve(progress), 0.f, 1.f);
                stamp.pathDistanceFromHead = max(0.f, stamp.pathReplayStartDistance * (1.f - curvedProgress));
            }
            else
            {
                const float segmentSpeed = Sample_RecordedSpeedAtDistance(history, stamp.pathDistanceFromHead);
                const float activeDeltaTime = min(movementDeltaTime, effectiveReplayTime);
                const float deltaDistance = max(0.f, segmentSpeed * max(0.f, _desc.pathReplay.speedScale)) * activeDeltaTime;
                stamp.pathDistanceFromHead -= deltaDistance;
            }

            if (stamp.pathDistanceFromHead <= 0.f)
            {
                stamp.pathDistanceFromHead = 0.f;
                stamp.pathReplayArrived = true;
            }
        }

        if (stamp.pathReplayArrived &&
            _desc.pathReplay.arrivalMode == EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive)
            continue;

        if (!replayStarted)
            continue;

        Vec3 sampledPosition{};
        Vec3 sampledTangent{};
        if (Sample_HistoryAtDistance(
            history,
            stamp.pathDistanceFromHead,
            stamp.tangent.LengthSquared() > 0.0001f ? stamp.tangent : fallbackTangent,
            sampledPosition,
            sampledTangent
        ))
        {
            stamp.center = sampledPosition;
            stamp.tangent = sampledTangent;
        }
    }

    erase_if(
        stamps,
        [this](const Stamp& stamp)
        {
            return stamp.pathReplayActive &&
                   stamp.pathReplayArrived &&
                   _desc.pathReplay.arrivalMode == EffectSourceHistorySpriteTrailArrivalMode::KillOnArrive;
        }
    );
}

void ComputeSourceHistorySpriteTrailEmitter::Prune_History(vector<HistorySample>& history, float requiredTailDistance) const
{
    erase_if(
        history,
        [this, requiredTailDistance](const HistorySample& sample)
        {
            if (requiredTailDistance > 0.f && sample.distanceFromHead <= requiredTailDistance)
                return false;

            return sample.age > _desc.sampleLifetime ||
                   (_desc.maxLength > 0.f && sample.distanceFromHead > _desc.maxLength);
        }
    );
    Recompute_Distances(history);
}

void ComputeSourceHistorySpriteTrailEmitter::Recompute_Distances(vector<HistorySample>& history) const
{
    if (history.empty())
        return;

    history.front().distanceFromHead = 0.f;
    for (size_t index = 1; index < history.size(); ++index)
    {
        history[index].distanceFromHead =
            history[index - 1].distanceFromHead +
            (history[index].position - history[index - 1].position).Length();
    }
}

bool ComputeSourceHistorySpriteTrailEmitter::Try_ResolveSmoothedHistoryTangent(
    const vector<HistorySample>& history,
    size_t sampleIndex,
    Vec3& outTangent) const
{
    if (sampleIndex == 0u || sampleIndex + 1u >= history.size())
        return false;

    const Vec3 newerDirection = history[sampleIndex - 1u].position - history[sampleIndex].position;
    const Vec3 olderDirection = history[sampleIndex].position - history[sampleIndex + 1u].position;
    if (newerDirection.LengthSquared() <= 0.0001f || olderDirection.LengthSquared() <= 0.0001f)
        return false;

    const Vec3 averaged =
        Normalize_OrFallback(newerDirection, Vec3{}) +
        Normalize_OrFallback(olderDirection, Vec3{});
    if (averaged.LengthSquared() <= 0.0001f)
        return false;

    outTangent = Normalize_OrFallback(averaged, Vec3{ 1.f, 0.f, 0.f });
    return true;
}

float ComputeSourceHistorySpriteTrailEmitter::Resolve_RequiredHistoryDistance(const vector<Stamp>& stamps) const
{
    if (!_desc.pathFollow.enabled && !_desc.pathReplay.enabled)
        return 0.f;

    float requiredDistance = 0.f;
    for (const Stamp& stamp : stamps)
    {
        if (stamp.pathFollowActive && !stamp.pathFollowArrived)
            requiredDistance = max(requiredDistance, stamp.pathDistanceFromHead);
        if (stamp.pathReplayActive && !stamp.pathReplayArrived)
            requiredDistance = max(requiredDistance, stamp.pathDistanceFromHead);
    }
    return requiredDistance;
}

float ComputeSourceHistorySpriteTrailEmitter::Resolve_HistoryDistanceForPosition(const vector<HistorySample>& history, const Vec3& position) const
{
    if (history.empty())
        return 0.f;

    if (history.size() == 1)
        return history.front().distanceFromHead;

    float bestDistanceFromHead = history.front().distanceFromHead;
    float bestDistanceSq = FLT_MAX;
    for (size_t index = 1; index < history.size(); ++index)
    {
        const HistorySample& headSample = history[index - 1];
        const HistorySample& tailSample = history[index];
        const Vec3 segment = tailSample.position - headSample.position;
        const float segmentLengthSq = segment.LengthSquared();
        float segmentRatio = 0.f;
        if (segmentLengthSq > 0.0001f)
            segmentRatio = clamp((position - headSample.position).Dot(segment) / segmentLengthSq, 0.f, 1.f);

        const Vec3 projected = headSample.position + segment * segmentRatio;
        const float distanceSq = (position - projected).LengthSquared();
        if (distanceSq < bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            bestDistanceFromHead = headSample.distanceFromHead +
                                   (tailSample.distanceFromHead - headSample.distanceFromHead) * segmentRatio;
        }
    }

    return bestDistanceFromHead;
}

bool ComputeSourceHistorySpriteTrailEmitter::Sample_HistoryAtDistance(
    const vector<HistorySample>& history,
    float distanceFromHead,
    const Vec3& fallbackTangent,
    Vec3& outPosition,
    Vec3& outTangent) const
{
    if (history.empty())
        return false;

    if (history.size() == 1)
    {
        outPosition = history.front().position;
        outTangent = Normalize_OrFallback(fallbackTangent, Vec3{ 1.f, 0.f, 0.f });
        return true;
    }

    const auto sample_segment =
        [this, &history, &fallbackTangent, &outPosition, &outTangent](size_t headIndex, float targetDistance, float fallbackRatio)
    {
        const size_t tailIndex = min(headIndex + 1u, history.size() - 1u);
        const HistorySample& p0 = history[headIndex > 0u ? headIndex - 1u : headIndex];
        const HistorySample& p1 = history[headIndex];
        const HistorySample& p2 = history[tailIndex];
        const HistorySample& p3 = history[min(tailIndex + 1u, history.size() - 1u)];
        float clampedRatio = clamp(fallbackRatio, 0.f, 1.f);
        const uint32 stepCount = max(1u, _desc.curveSubdivision);
        if (stepCount > 1u && targetDistance > 0.f)
        {
            const float clampedTargetDistance = max(0.f, targetDistance);
            Vec3 previousPosition = p1.position;
            float previousRatio = 0.f;
            float accumulatedDistance = 0.f;
            for (uint32 step = 1u; step <= stepCount; ++step)
            {
                const float ratio = static_cast<float>(step) / static_cast<float>(stepCount);
                const Vec3 position = Catmull_Rom(p0.position, p1.position, p2.position, p3.position, ratio);
                const float stepDistance = (position - previousPosition).Length();
                if (accumulatedDistance + stepDistance >= clampedTargetDistance)
                {
                    const float localRatio =
                        stepDistance > 0.0001f
                        ? clamp((clampedTargetDistance - accumulatedDistance) / stepDistance, 0.f, 1.f)
                        : 0.f;
                    clampedRatio = lerp(previousRatio, ratio, localRatio);
                    break;
                }

                accumulatedDistance += stepDistance;
                previousPosition = position;
                previousRatio = ratio;
            }
        }
        outPosition = Catmull_Rom(p0.position, p1.position, p2.position, p3.position, clampedRatio);
        const Vec3 rawTangent = Normalize_OrFallback(
            Catmull_RomDerivative(p0.position, p1.position, p2.position, p3.position, clampedRatio) * -1.f,
            fallbackTangent
        );
        outTangent = rawTangent;
        if (_desc.smoothTangent)
        {
            Vec3 headTangent{};
            Vec3 tailTangent{};
            if (Try_ResolveSmoothedHistoryTangent(history, headIndex, headTangent) &&
                Try_ResolveSmoothedHistoryTangent(history, tailIndex, tailTangent))
            {
                const Vec3 blendedTangent = headTangent + (tailTangent - headTangent) * clampedRatio;
                outTangent = Normalize_OrFallback(blendedTangent, rawTangent);
            }
        }
    };

    if (distanceFromHead <= 0.f)
    {
        sample_segment(0u, 0.f, 0.f);
        return true;
    }

    if (distanceFromHead >= history.back().distanceFromHead)
    {
        const float segmentDistance = max(0.0001f, history.back().distanceFromHead - history[history.size() - 2u].distanceFromHead);
        sample_segment(history.size() - 2u, segmentDistance, 1.f);
        return true;
    }

    for (size_t index = 1; index < history.size(); ++index)
    {
        const HistorySample& headSample = history[index - 1];
        const HistorySample& tailSample = history[index];
        if (distanceFromHead > tailSample.distanceFromHead)
            continue;

        const float segmentDistance = max(0.0001f, tailSample.distanceFromHead - headSample.distanceFromHead);
        const float targetDistance = max(0.f, distanceFromHead - headSample.distanceFromHead);
        const float ratio = clamp(targetDistance / segmentDistance, 0.f, 1.f);
        sample_segment(index - 1u, targetDistance, ratio);
        return true;
    }

    outPosition = history.back().position;
    outTangent = Normalize_OrFallback(fallbackTangent, Vec3{ 1.f, 0.f, 0.f });
    return true;
}

float ComputeSourceHistorySpriteTrailEmitter::Sample_RecordedSpeedAtDistance(const vector<HistorySample>& history, float distanceFromHead) const
{
    if (history.empty())
        return 0.f;

    if (history.size() == 1 || distanceFromHead <= 0.f)
        return max(0.f, history.front().recordedSpeed);

    for (size_t index = 1; index < history.size(); ++index)
    {
        const HistorySample& tailSample = history[index];
        if (distanceFromHead <= tailSample.distanceFromHead)
            return max(0.f, tailSample.recordedSpeed);
    }

    return max(0.f, history.back().recordedSpeed);
}

void ComputeSourceHistorySpriteTrailEmitter::Spawn_StampsForPath(
    vector<Stamp>& stamps,
    float& distanceAccumulator,
    float& timeAccumulator,
    Vec3& lastValidTangent,
    const vector<HistorySample>& history,
    const Vec3& previousPosition,
    const Vec3& currentPosition,
    float timeDelta)
{
    const Vec3 movement = currentPosition - previousPosition;
    const float distance = movement.Length();
    const Vec3 tangent = Normalize_OrFallback(movement, lastValidTangent);
    if (tangent.LengthSquared() > 0.0001f)
        lastValidTangent = tangent;
    const bool sourceVelocityValid = distance > 0.0001f && timeDelta > 0.f;
    const Vec3 sourceVelocity = sourceVelocityValid ? movement / max(timeDelta, 0.0001f) : Vec3{};

    const bool canSampleHistoryPath =
        !history.empty() && (history.front().position - currentPosition).LengthSquared() <= 0.0001f;
    const auto sample_spawn_point =
        [this, &history, canSampleHistoryPath, &tangent](float pathDistanceFromHead, const Vec3& fallbackCenter, Vec3& outCenter, Vec3& outTangent)
    {
        outCenter = fallbackCenter;
        outTangent = tangent;
        if (!canSampleHistoryPath)
            return;

        Vec3 sampledPosition{};
        Vec3 sampledTangent{};
        if (Sample_HistoryAtDistance(history, pathDistanceFromHead, tangent, sampledPosition, sampledTangent))
        {
            outCenter = sampledPosition;
            outTangent = sampledTangent;
        }
    };
    const auto make_source_motion_context =
        [&sourceVelocity, sourceVelocityValid](const Vec3& stampTangent)
    {
        SourceMotionVelocityContext context{};
        context.sourceVelocity = sourceVelocity;
        context.sourceVelocityValid = sourceVelocityValid;
        context.tangent = Normalize_OrFallback(stampTangent, Vec3{ 1.f, 0.f, 0.f });
        return context;
    };

    if (_desc.stampSpawnMode == EffectSourceHistorySpriteTrailStampSpawnMode::Time)
    {
        timeAccumulator += max(0.f, timeDelta);
        while (timeAccumulator >= _desc.stampInterval)
        {
            timeAccumulator -= _desc.stampInterval;
            Vec3 center{};
            Vec3 stampTangent{};
            sample_spawn_point(0.f, currentPosition, center, stampTangent);
            Push_Stamp(stamps, center, stampTangent, make_source_motion_context(stampTangent), 0.f, 1.f, 1.f);
        }
        return;
    }

    distanceAccumulator += distance;
    if (distance <= 0.0001f)
        return;

    while (distanceAccumulator >= _desc.stampSpacing)
    {
        const float backDistance = distanceAccumulator - _desc.stampSpacing;
        const float ratio = clamp(1.f - backDistance / distance, 0.f, 1.f);
        Vec3 center{};
        Vec3 stampTangent{};
        sample_spawn_point(
            backDistance,
            Vec3::Lerp(previousPosition, currentPosition, ratio),
            center,
            stampTangent
        );
        Push_Stamp(stamps, center, stampTangent, make_source_motion_context(stampTangent), backDistance, 1.f, 1.f);
        distanceAccumulator -= _desc.stampSpacing;
    }
}

void ComputeSourceHistorySpriteTrailEmitter::Push_Stamp(
    vector<Stamp>& stamps,
    const Vec3& center,
    const Vec3& tangent,
    const SourceMotionVelocityContext& sourceMotionContext,
    float pathDistanceFromHead,
    float widthScale,
    float lengthScale)
{
    const uint32 stampSerial = _stampSerialCounter++;
    Vec3 jitteredCenter = center;
    if (_desc.spawnJitter > 0.f)
    {
        const Vec3 normalizedTangent = Normalize_OrFallback(tangent, Vec3{ 1.f, 0.f, 0.f });
        Vec3 side = normalizedTangent.Cross(Vec3{ 0.f, 1.f, 0.f });
        if (side.LengthSquared() <= 0.0001f)
            side = normalizedTangent.Cross(Vec3{ 0.f, 0.f, 1.f });
        side = Normalize_OrFallback(side, Vec3{ 0.f, 0.f, 1.f });
        const float offset = (Hash01(_effectPlaybackSeed + stampSerial * 977u) * 2.f - 1.f) * _desc.spawnJitter;
        jitteredCenter += side * offset;
    }

    Stamp stamp{};
    stamp.center = jitteredCenter;
    stamp.tangent = Normalize_OrFallback(tangent, Vec3{ 1.f, 0.f, 0.f });
    stamp.lifetime = Sample_Lifetime(stampSerial);
    stamp.serial = stampSerial;
    stamp.startColor = Sample_Color(_desc.initialColor.startColorMin, _desc.initialColor.startColorMax, _desc.initialColor.colorSeed, stampSerial, 37u);
    stamp.startColor.w = Sample_Color(
        Vec4{ _desc.initialColor.startColorMin.w, _desc.initialColor.startColorMin.w, _desc.initialColor.startColorMin.w, _desc.initialColor.startColorMin.w },
        Vec4{ _desc.initialColor.startColorMax.w, _desc.initialColor.startColorMax.w, _desc.initialColor.startColorMax.w, _desc.initialColor.startColorMax.w },
        _desc.initialColor.alphaSeed,
        stampSerial,
        73u
    ).x;
    stamp.endColor = Sample_Color(_desc.colorOverLife.endColorMin, _desc.colorOverLife.endColorMax, _desc.colorOverLife.colorSeed, stampSerial, 109u);
    stamp.endColor.w = Sample_Color(
        Vec4{ _desc.colorOverLife.endColorMin.w, _desc.colorOverLife.endColorMin.w, _desc.colorOverLife.endColorMin.w, _desc.colorOverLife.endColorMin.w },
        Vec4{ _desc.colorOverLife.endColorMax.w, _desc.colorOverLife.endColorMax.w, _desc.colorOverLife.endColorMax.w, _desc.colorOverLife.endColorMax.w },
        _desc.colorOverLife.alphaSeed,
        stampSerial,
        149u
    ).x;
    stamp.coreColorRgb = Sample_ParticleLifeCoreColorRgbUniformModulation(
        _desc.material.coreColorRgbModulation,
        Vec3{
            _desc.material.coreEmissive.coreColor.x,
            _desc.material.coreEmissive.coreColor.y,
            _desc.material.coreEmissive.coreColor.z
        },
        _effectPlaybackSeed,
        stampSerial
    );
    stamp.frameIndex = Evaluate_SubUVFrameIndex(stamp);
    const Vec2 initialSize = Sample_InitialSize(stampSerial);
    stamp.widthScale = max(0.f, widthScale * initialSize.x);
    stamp.lengthScale = max(0.f, lengthScale * initialSize.y);
    stamp.initialRotationRadians = Sample_InitialRotation(stampSerial);
    stamp.initialAngularVelocityRadians = Sample_InitialAngularVelocity(stampSerial);
    stamp.initialTiltDegrees = Sample_InitialTilt(stampSerial);
    stamp.tiltOverLifeDegrees = Sample_TiltOverLife(stampSerial);
    Sample_InitialVelocityChannels(stamp, stampSerial, stamp.center, sourceMotionContext);
    stamp.acceleration = Sample_Acceleration(stampSerial);
    stamp.drag = Sample_Drag(stampSerial);
    stamp.pathFollowActive = _desc.pathFollow.enabled && !_desc.pathReplay.enabled;
    stamp.pathDistanceFromHead = max(0.f, pathDistanceFromHead);
    stamp.pathFollowSpeed = Sample_PathFollowSpeed(stampSerial);
    stamp.pathFollowStartDelay = Sample_PathFollowStartDelay(stampSerial);
    stamp.pathReplayActive = _desc.pathReplay.enabled;
    stamp.pathReplayDelayRemaining = max(0.f, _desc.pathReplay.delayTime);
    stamp.pathReplayStartDistance = stamp.pathDistanceFromHead;

    stamps.insert(
        stamps.begin(),
        stamp
    );

    if (stamps.size() > _desc.maxStampCount)
        stamps.resize(_desc.maxStampCount);
}

ComputeSourceHistorySpriteTrailEmitter::HistoryLane* ComputeSourceHistorySpriteTrailEmitter::Find_HistoryLane(uint32 sourceIndex)
{
    for (HistoryLane& lane : _sourceLanes)
    {
        if (lane.sourceIndex == sourceIndex)
            return &lane;
    }
    return nullptr;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Ready_Components()
{
    if (FAILED(Add_Component(ETOI(LevelType::Static), kShaderId, _shader)))
    {
        LOG_WARN(
            "SourceHistorySpriteTrail ready failed: shader. emitter={}, shader={}",
            _emitterName,
            String::TCharToString(kShaderId)
        );
        return E_FAIL;
    }
    if (FAILED(Ready_Texture()))
    {
        LOG_WARN(
            "SourceHistorySpriteTrail ready failed: texture. emitter={}, mainGuid={}, mainPath={}",
            _emitterName,
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath
        );
        return E_FAIL;
    }
    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    if (FAILED(Ready_DrawBuffers()))
    {
        LOG_WARN(
            "SourceHistorySpriteTrail ready failed: buffers. emitter={}, maxStamp={}",
            _emitterName,
            Compute_MaxStampCount()
        );
        return E_FAIL;
    }
    return S_OK;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Ready_Texture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = Resolve_TexturePath(
        _desc.material.mainTextureGuid,
        _desc.material.mainTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
        resolvedPath = Resolve_TexturePathByPath(kFallbackTexturePath);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Texture_Effect_DefaultTexture", _mainTexture), E_FAIL);
    if (!resolvedPath.empty())
        _mainTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);

    CHECK_NULL(_mainTexture, E_FAIL);
    CHECK_FAILED(Ready_OptionalTexture(_noiseTexture, _desc.material.noiseTextureGuid, _desc.material.noiseTexturePath), E_FAIL);
    CHECK_FAILED(Ready_OptionalTexture(_maskTexture, _desc.material.maskTextureGuid, _desc.material.maskTexturePath), E_FAIL);
    return S_OK;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Ready_OptionalTexture(Shared<Texture>& outTexture, const string& textureGuid, const string& texturePath)
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Resolve_TexturePath(textureGuid, texturePath, usedPathFallback, guidPathMismatch);
    if (resolvedPath.empty())
    {
        outTexture.reset();
        return S_OK;
    }

    outTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    return outTexture != nullptr ? S_OK : E_FAIL;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Ready_DrawBuffers()
{
    const Vec3 vertices[kPointVertexCount] = {
        Vec3{ 0.f, 0.f, 0.f },
    };
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = vertices;
    CHECK_FAILED(_device->CreateBuffer(&vbDesc, &vbData, _pointVB.GetAddressOf()), E_FAIL);

    D3D11_BUFFER_DESC instanceDesc{};
    instanceDesc.ByteWidth = sizeof(StampInstanceVertex) * Compute_MaxStampCount();
    instanceDesc.Usage = D3D11_USAGE_DYNAMIC;
    instanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    instanceDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    CHECK_FAILED(_device->CreateBuffer(&instanceDesc, nullptr, _instanceBuffer.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Update_InstanceBuffer()
{
    vector<StampInstanceVertex> instances{};
    instances.reserve(Compute_MaxStampCount());
    CHECK_FAILED(Append_StampInstances(_stamps, instances), E_FAIL);
    for (const HistoryLane& lane : _sourceLanes)
        CHECK_FAILED(Append_StampInstances(lane.stamps, instances), E_FAIL);
    for (const RetiredLoopStroke& stroke : _retiredLoopStrokes)
        CHECK_FAILED(Append_StampInstances(stroke.stamps, instances), E_FAIL);

    _drawStampCount = min(static_cast<uint32>(instances.size()), Compute_MaxStampCount());
    if (_desc.drawLimit.useMaxDrawCount)
        _drawStampCount = min(_drawStampCount, max(1u, _desc.drawLimit.maxDrawCount));

    if (_drawStampCount == 0u)
        return S_OK;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    CHECK_FAILED(_context->Map(_instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped), E_FAIL);
    memcpy(mapped.pData, instances.data(), sizeof(StampInstanceVertex) * _drawStampCount);
    _context->Unmap(_instanceBuffer.Get(), 0);
    return S_OK;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Append_StampInstances(const vector<Stamp>& stamps, vector<StampInstanceVertex>& outInstances) const
{
    const uint32 maxStampCount = Compute_MaxStampCount();
    for (const Stamp& stamp : stamps)
    {
        if (outInstances.size() >= maxStampCount)
            break;

        const float lifeProgress = clamp(stamp.age / max(0.0001f, stamp.lifetime), 0.f, 1.f);
        const Vec4 color = Evaluate_ColorOverLife(stamp, lifeProgress);

        const float widthScale = stamp.widthScale * Evaluate_WidthScaleByLife(lifeProgress);
        const float lengthScale = stamp.lengthScale * Evaluate_LengthScaleByLife(lifeProgress);
        const float rotationRadians =
            XMConvertToRadians(_desc.rotationOffsetDegrees) +
            stamp.initialRotationRadians +
            Evaluate_RotationOverLife(lifeProgress) +
            stamp.initialAngularVelocityRadians * Evaluate_RotationRateScaleByLife(lifeProgress) * stamp.age;
        const Vec2 spriteTiltDegrees = stamp.initialTiltDegrees + Evaluate_SpriteTiltOverLife(stamp, lifeProgress);
        const uint32 frameIndex =
            _desc.subUVFrameOverLife.playbackMode == SubUVFramePlaybackMode::RandomFrame
            ? stamp.frameIndex
            : Evaluate_SubUVFrameIndex(stamp);

        outInstances.push_back(
            StampInstanceVertex{
                .centerAndLength = Vec4{ stamp.center.x, stamp.center.y, stamp.center.z, _desc.cardLength * max(0.f, lengthScale) },
                .tangentAndWidth = Vec4{ stamp.tangent.x, stamp.tangent.y, stamp.tangent.z, _desc.cardWidth * max(0.f, widthScale) },
                .lifeTime = Vec2{ stamp.lifetime, stamp.age },
                .startColor = color,
                .endColor = color,
                .subUVRect = Resolve_SubUVRect(frameIndex),
                .rotationAndTilt = Vec4{ rotationRadians, spriteTiltDegrees.x, spriteTiltDegrees.y, 0.f },
                .coreColorRgb = Vec4{ stamp.coreColorRgb.x, stamp.coreColorRgb.y, stamp.coreColorRgb.z, 0.f }
            }
        );
    }
    return S_OK;
}

HRESULT ComputeSourceHistorySpriteTrailEmitter::Bind_ShaderResources()
{
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);
    CHECK_FAILED(_mainTexture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    if (_noiseTexture != nullptr)
        CHECK_FAILED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);

    if (_maskTexture != nullptr)
        CHECK_FAILED(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

    const float emitterPhase = Compute_MaterialScalarModulationPhase(_materialElapsedTime, _desc.playback.duration);
    const EffectRequiredMaterialRuntimeDesc material =
        Resolve_EmitterTimeMaterialUniformParameterModulation(_desc.material, emitterPhase, _effectPlaybackSeed);

    const Vec4 effectSpriteParams = Vec4(
        material.intensity,
        material.opacityPower,
        material.noiseStrength,
        _noiseTexture != nullptr ? 1.f : 0.f
    );
    const Vec4 effectSpriteAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        _maskTexture != nullptr ? 1.f : 0.f,
        clamp(material.alphaMultiplier, 0.f, 1.f)
    );
    const Vec4 effectSpriteMainUVParams = Vec4(material.mainUVScale.x, material.mainUVScale.y, material.mainUVScrollSpeed.x, material.mainUVScrollSpeed.y);
    const Vec4 effectSpriteNoiseUVParams = Vec4(material.noiseUVScale.x, material.noiseUVScale.y, material.noiseUVScrollSpeed.x, material.noiseUVScrollSpeed.y);
    const Vec4 effectSpriteMaskUVParams = Vec4(material.maskUVScale.x, material.maskUVScale.y, material.maskUVScrollSpeed.x, material.maskUVScrollSpeed.y);
    const Vec4 effectSpriteUVOffsetParams = Vec4(material.mainUVOffset.x, material.mainUVOffset.y, material.noiseUVOffset.x, material.noiseUVOffset.y);
    const Vec4 effectSpriteMaskUVOffsetParams = Vec4(material.maskUVOffset.x, material.maskUVOffset.y, 0.f, 0.f);
    const Vec4 effectSpriteUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
        0.f
    );
    const Vec4 effectSpriteUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.vPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.vPolicy))
    );
    const Vec4 effectSpriteMaskUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.vPolicy)),
        0.f,
        0.f
    );
    const Vec4 effectSpriteUVRotationParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVRotation)),
        static_cast<float>(static_cast<uint32>(material.noiseUVRotation)),
        static_cast<float>(static_cast<uint32>(material.maskUVRotation)),
        0.f
    );
    const Vec4 effectSpriteSourceParams = Vec4(
        static_cast<float>(Resolve_MaterialSourceIndex(material.noiseSource)),
        static_cast<float>(Resolve_MaterialSourceIndex(material.maskSource)),
        material.noiseInvert ? 1.f : 0.f,
        material.maskInvert ? 1.f : 0.f
    );
    const EffectMaterialAdditiveContributionData& additive = material.additive;
    const EffectMaterialCoreEmissiveData& coreEmissive = material.coreEmissive;
    const Vec4 effectSpriteAdditiveParams = Vec4(
        static_cast<float>(static_cast<uint32>(additive.colorSource)),
        static_cast<float>(static_cast<uint32>(additive.amountSource)),
        static_cast<float>(static_cast<uint32>(additive.coveragePolicy)),
        additive.intensityScale
    );
    const Vec4 effectSpriteAdditiveFlags = Vec4(additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
    const Vec4 effectSpriteCoreEmissiveParams = Vec4(
        coreEmissive.enabled ? 1.f : 0.f,
        coreEmissive.corePower,
        coreEmissive.coreIntensity,
        coreEmissive.outerPower
    );
    const Vec4 effectSpriteCoreEmissiveColor = Vec4(
        coreEmissive.coreColor.x,
        coreEmissive.coreColor.y,
        coreEmissive.coreColor.z,
        coreEmissive.outerIntensity
    );
    const Vec4 sourceHistorySpriteTrailParams = Vec4(
        static_cast<float>(static_cast<uint32>(_desc.spriteRender.screenAlignment)),
        0.f,
        _desc.flipU ? 1.f : 0.f,
        _desc.flipV ? 1.f : 0.f
    );
    const int textureAxis = static_cast<int>(_desc.spriteRender.spriteTextureAxis);
    const int directionalAlignmentMode = static_cast<int>(_desc.spriteRender.directionalAlignmentMode);
    const float spriteRollOffsetRadians = XMConvertToRadians(_desc.spriteRender.spriteRollOffsetDegrees);

    CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteParams", &effectSpriteParams, sizeof(effectSpriteParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteAlphaParams", &effectSpriteAlphaParams, sizeof(effectSpriteAlphaParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteMainUVParams", &effectSpriteMainUVParams, sizeof(effectSpriteMainUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteNoiseUVParams", &effectSpriteNoiseUVParams, sizeof(effectSpriteNoiseUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteMaskUVParams", &effectSpriteMaskUVParams, sizeof(effectSpriteMaskUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteUVOffsetParams", &effectSpriteUVOffsetParams, sizeof(effectSpriteUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteMaskUVOffsetParams", &effectSpriteMaskUVOffsetParams, sizeof(effectSpriteMaskUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteUVModeParams", &effectSpriteUVModeParams, sizeof(effectSpriteUVModeParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteUVAxisPolicyParams", &effectSpriteUVAxisPolicyParams, sizeof(effectSpriteUVAxisPolicyParams)), E_FAIL);
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectSpriteMaskUVAxisPolicyParams", &effectSpriteMaskUVAxisPolicyParams, sizeof(effectSpriteMaskUVAxisPolicyParams)),
        E_FAIL
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteUVRotationParams", &effectSpriteUVRotationParams, sizeof(effectSpriteUVRotationParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteSourceParams", &effectSpriteSourceParams, sizeof(effectSpriteSourceParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteMaterialTime", &_materialElapsedTime, sizeof(_materialElapsedTime)), E_FAIL);
    CHECK_FAILED(Bind_MaterialScalarModulationPayload(_shader.get(), material.scalarModulation, emitterPhase), E_FAIL);
    CHECK_FAILED(
        Bind_CoreColorRgbModulationShaderPayload(
            _shader.get(),
            material.coreColorRgbModulation,
            emitterPhase,
            _effectPlaybackSeed
        ),
        E_FAIL
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteAdditiveParams", &effectSpriteAdditiveParams, sizeof(effectSpriteAdditiveParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteAdditiveFlags", &effectSpriteAdditiveFlags, sizeof(effectSpriteAdditiveFlags)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteCoreEmissiveParams", &effectSpriteCoreEmissiveParams, sizeof(effectSpriteCoreEmissiveParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectSpriteCoreEmissiveColor", &effectSpriteCoreEmissiveColor, sizeof(effectSpriteCoreEmissiveColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_OpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_TextureAxis", &textureAxis, sizeof(textureAxis)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_DirectionalAlignmentMode", &directionalAlignmentMode, sizeof(directionalAlignmentMode)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_SpriteRollOffsetRadians", &spriteRollOffsetRadians, sizeof(spriteRollOffsetRadians)), E_FAIL);
    CHECK_FAILED(
        _shader->Bind_RawValue("g_SourceHistorySpriteTrailParams", &sourceHistorySpriteTrailParams, sizeof(sourceHistorySpriteTrailParams)),
        E_FAIL
    );
    return S_OK;
}

bool ComputeSourceHistorySpriteTrailEmitter::Can_SubmitRender() const
{
    if (_shader == nullptr || _mainTexture == nullptr || _instanceBuffer == nullptr)
        return false;

    return Has_LiveStamps();
}

uint32 ComputeSourceHistorySpriteTrailEmitter::Compute_MaxStampCount() const
{
    return max(1u, _desc.maxStampCount * (max(1u, _desc.followerLaneCount) + kMaxRetiredLoopStrokeCount));
}

float ComputeSourceHistorySpriteTrailEmitter::Resolve_CurrentLoopDelay() const
{
    return _loopIndex == 0u || !_desc.playback.delayFirstLoopOnly ? max(0.f, _desc.playback.delay) : 0.f;
}

float ComputeSourceHistorySpriteTrailEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.playback.duration);
}

bool ComputeSourceHistorySpriteTrailEmitter::Has_NextLoop() const
{
    return _desc.playback.loopCount == 0u || _loopIndex + 1u < _desc.playback.loopCount;
}

void ComputeSourceHistorySpriteTrailEmitter::Start_NextLoop()
{
    Retire_ActiveLoopStrokes();

    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _playbackState = PlaybackState::Delayed;
    _finishEmissionAfterUpdate = false;
    Reset_LoopScopedRuntime();
}

RenderGroup ComputeSourceHistorySpriteTrailEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    return RenderGroup::Blend;
}

uint32 ComputeSourceHistorySpriteTrailEmitter::Resolve_ShaderPassIndex() const
{
    switch (_desc.material.blendMode)
    {
    case EffectMaterialBlendMode::Additive:
        return 1u;
    case EffectMaterialBlendMode::Masked:
        return 2u;
    case EffectMaterialBlendMode::AlphaBlend:
    default:
        return 0u;
    }
}

int ComputeSourceHistorySpriteTrailEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.material.opacitySource == "Red" || _desc.material.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.material.opacitySource == "Luminance" || _desc.material.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    return kOpacitySourceAlpha;
}

bool ComputeSourceHistorySpriteTrailEmitter::Try_Resolve_SourcePosition(Vec3& outPosition) const
{
    const Shared<IEffectSourcePointSampleProvider> sampleProvider = _desc.sourcePointSampleProvider.lock();
    if (sampleProvider != nullptr)
    {
        EffectSourcePointSample sample{};
        if (!sampleProvider->Try_GetSourcePointSample(sample))
            return false;

        outPosition = sample.worldPosition;
        return true;
    }

    outPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
    return true;
}

Vec3 ComputeSourceHistorySpriteTrailEmitter::Resolve_SourcePosition() const
{
    Vec3 sourcePosition{};
    Try_Resolve_SourcePosition(sourcePosition);
    return sourcePosition;
}

float ComputeSourceHistorySpriteTrailEmitter::Sample_Lifetime(uint32 stampSerial) const
{
    if (!_desc.useLifetimeStampLifetime)
        return max(0.0001f, _desc.stampLifetime);

    const float lifeMin = max(0.0001f, min(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    if (_desc.lifetime.lifeTimeCurve.enabled)
    {
        const float spawnPhase = clamp(_loopElapsedTime / Resolve_Duration(), 0.f, 1.f);
        return max(
            0.0001f,
            Evaluate_CompactCurve(
                spawnPhase,
                _desc.lifetime.lifeTimeCurve.curveKeyTimes,
                _desc.lifetime.lifeTimeCurve.curveKeyTimesBlock1,
                _desc.lifetime.lifeTimeCurve.curveKeyValues,
                _desc.lifetime.lifeTimeCurve.curveKeyValuesBlock1,
                _desc.lifetime.lifeTimeCurve.curveKeyArriveTangents,
                _desc.lifetime.lifeTimeCurve.curveKeyArriveTangentsBlock1,
                _desc.lifetime.lifeTimeCurve.curveKeyLeaveTangents,
                _desc.lifetime.lifeTimeCurve.curveKeyLeaveTangentsBlock1,
                _desc.lifetime.lifeTimeCurve.curveKeyModes,
                _desc.lifetime.lifeTimeCurve.curveKeyModesBlock1,
                _desc.lifetime.lifeTimeCurve.curveKeyCount,
                lifeMax
            )
        );
    }

    if (lifeMin == lifeMax)
        return lifeMin;

    const uint32 seed = Resolve_SeedSalt(_desc.lifetimeSeed, _effectPlaybackSeed) + stampSerial * 9781u + 1319u;
    return lerp(lifeMin, lifeMax, Hash01(seed));
}

Vec2 ComputeSourceHistorySpriteTrailEmitter::Sample_InitialSize(uint32 stampSerial) const
{
    if (!_desc.initialSize.enabled)
        return Vec2{ 1.f, 1.f };

    return Vec2{
        max(0.f, Sample_Range(_desc.initialSize.sizeMin.x, _desc.initialSize.sizeMax.x, _desc.initialSize.sizeSeed, stampSerial, 173u)),
        max(0.f, Sample_Range(_desc.initialSize.sizeMin.y, _desc.initialSize.sizeMax.y, _desc.initialSize.sizeSeed, stampSerial, 197u))
    };
}

float ComputeSourceHistorySpriteTrailEmitter::Sample_InitialRotation(uint32 stampSerial) const
{
    if (!_desc.rotation.enabled)
        return 0.f;

    return XMConvertToRadians(
        Sample_Range(
            _desc.rotation.initialRotationDegrees.x,
            _desc.rotation.initialRotationDegrees.y,
            _desc.rotation.initialRotationSeed,
            stampSerial,
            809u
        )
    );
}

float ComputeSourceHistorySpriteTrailEmitter::Sample_InitialAngularVelocity(uint32 stampSerial) const
{
    if (!_desc.rotation.enabled)
        return 0.f;

    return XMConvertToRadians(
        Sample_Range(
            _desc.rotation.initialRotationRateDegrees.x,
            _desc.rotation.initialRotationRateDegrees.y,
            _desc.rotation.initialRotationRateSeed,
            stampSerial,
            907u
        )
    );
}

Vec2 ComputeSourceHistorySpriteTrailEmitter::Sample_InitialTilt(uint32 stampSerial) const
{
    if (!_desc.spriteTilt.initialTiltEnabled)
        return Vec2{};

    return Sample_Vector2Range(
        _desc.spriteTilt.tiltDegreesMin,
        _desc.spriteTilt.tiltDegreesMax,
        _desc.spriteTilt.tiltSeed,
        stampSerial,
        617u
    );
}

Vec2 ComputeSourceHistorySpriteTrailEmitter::Sample_TiltOverLife(uint32 stampSerial) const
{
    if (!_desc.spriteTilt.tiltOverLifeEnabled || _desc.spriteTilt.tiltOverLifeCurveEnabled)
        return Vec2{};

    return Sample_Vector2Range(
        _desc.spriteTilt.tiltOverLifeMin,
        _desc.spriteTilt.tiltOverLifeMax,
        _desc.spriteTilt.tiltOverLifeSeed,
        stampSerial,
        631u
    );
}

void ComputeSourceHistorySpriteTrailEmitter::Sample_InitialVelocityChannels(
    Stamp& stamp,
    uint32 stampSerial,
    const Vec3& spawnCenter,
    const SourceMotionVelocityContext& sourceMotionContext) const
{
    stamp.velocity = Vec3{};
    stamp.initialVelocity = Vec3{};
    stamp.initialRadialVelocity = Vec3{};
    stamp.velocityCone = Vec3{};
    stamp.sourceMotionVelocity = Vec3{};
    stamp.accelerationIntegratedVelocity = Vec3{};

    if (!_desc.motion.enabled)
        return;

    const auto resolve_local_vector = [this](const Vec3& localVector) -> Vec3
    {
        if (_transformCom == nullptr)
            return localVector;

        return _transformCom->Get_WorldRight() * localVector.x +
               _transformCom->Get_WorldUp() * localVector.y +
               _transformCom->Get_WorldForward() * localVector.z;
    };

    const Vec3 tangent = Normalize_OrFallback(sourceMotionContext.tangent, Vec3{ 1.f, 0.f, 0.f });
    if (_desc.motion.initialVelocityEnabled)
    {
        const Vec3 sampledVelocity = Sample_VectorRange(
            _desc.motion.initialVelocityMin,
            _desc.motion.initialVelocityMax,
            _desc.motion.initialVelocitySeed,
            stampSerial,
            503u
        );
        stamp.initialVelocity += _desc.motion.initialVelocityInWorldSpace ? sampledVelocity : resolve_local_vector(sampledVelocity);
    }

    if (_desc.motion.initialRadialVelocityEnabled)
    {
        const Vec3 radialPivotOffset =
            _desc.motion.initialRadialVelocityInWorldSpace
            ? _desc.motion.radialPivot
            : resolve_local_vector(_desc.motion.radialPivot);
        const Vec3 emitterPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
        const Vec3 pivotPosition = emitterPosition + radialPivotOffset;
        Vec3 radialDirection = spawnCenter - pivotPosition;
        if (radialDirection.LengthSquared() < 0.0001f)
        {
            if (_desc.motion.initialRadialVelocityCenterDirectionMode == PointParticleInitialRadialVelocityCenterDirectionMode::PlaneRadial)
            {
                const Vec3 normalizedTangent = Normalize_OrFallback(tangent, Vec3{ 1.f, 0.f, 0.f });
                Vec3 side{};
                if (GAME != nullptr)
                {
                    if (const Vec4* camPosition = GAME->Get_CamPosition())
                    {
                        const Vec3 cameraLook = Normalize_OrFallback(
                            Vec3{ camPosition->x, camPosition->y, camPosition->z } - spawnCenter,
                            Vec3{ 0.f, 0.f, -1.f }
                        );
                        side = cameraLook.Cross(normalizedTangent);
                    }
                }
                if (side.LengthSquared() < 0.0001f)
                    side = normalizedTangent.Cross(Vec3{ 0.f, 1.f, 0.f });
                if (side.LengthSquared() < 0.0001f)
                    side = normalizedTangent.Cross(Vec3{ 0.f, 0.f, 1.f });
                radialDirection = side;
            }
            else
            {
                const uint32 fallbackSeed =
                    Resolve_SeedSalt(_desc.motion.initialRadialVelocitySeed, _effectPlaybackSeed) +
                    stampSerial * 3571u +
                    307u;
                radialDirection = resolve_local_vector(
                    Vec3{
                        Hash01(fallbackSeed) * 2.f - 1.f,
                        0.25f,
                        Hash01(fallbackSeed + 94u) * 2.f - 1.f
                    }
                );
            }
        }
        radialDirection = Normalize_OrFallback(radialDirection, Vec3{ 0.f, 1.f, 0.f });
        const float radialSpeed = Sample_Range(
            _desc.motion.radialSpeed.x,
            _desc.motion.radialSpeed.y,
            _desc.motion.initialRadialVelocitySeed,
            stampSerial,
            509u
        );
        stamp.initialRadialVelocity += radialDirection * radialSpeed;
    }

    if (_desc.motion.velocityConeEnabled)
    {
        const uint32 coneSeed =
            Resolve_SeedSalt(_desc.motion.velocityConeSeed, _effectPlaybackSeed) +
            stampSerial * 3571u +
            1703u;
        const Vec3 sampledDirection = SampleConeDirection(
            _desc.motion.velocityConeAxis,
            _desc.motion.velocityConeAngleDegrees,
            coneSeed
        );
        const Vec3 coneDirection = _desc.motion.velocityConeInWorldSpace ? sampledDirection : resolve_local_vector(sampledDirection);
        const float coneSpeed = Sample_Range(
            _desc.motion.velocityConeSpeed.x,
            _desc.motion.velocityConeSpeed.y,
            _desc.motion.velocityConeSeed,
            stampSerial,
            1747u
        );
        stamp.velocityCone += coneDirection * coneSpeed;
    }

    if (_desc.motion.sourceMotionVelocityEnabled)
    {
        const bool requiresSourceVelocity =
            _desc.motion.sourceMotionVelocityDirectionMode == PointParticleSourceMotionVelocityDirectionMode::InheritSourceVelocity ||
            _desc.motion.sourceMotionVelocityDirectionMode == PointParticleSourceMotionVelocityDirectionMode::SourceVelocityDirection ||
            _desc.motion.sourceMotionVelocityDirectionMode == PointParticleSourceMotionVelocityDirectionMode::SourceVelocityOpposite;
        if (requiresSourceVelocity && !sourceMotionContext.sourceVelocityValid)
            return;

        const float sourceSpeed =
            sourceMotionContext.sourceVelocityValid ? sourceMotionContext.sourceVelocity.Length() : 0.f;
        const Vec3 fallbackForward = _transformCom != nullptr
                                     ? _transformCom->Get_WorldForward()
                                     : Vec3{ 0.f, 0.f, 1.f };
        const Vec3 normalizedSourceVelocity =
            sourceMotionContext.sourceVelocityValid
            ? Normalize_OrFallback(sourceMotionContext.sourceVelocity, fallbackForward)
            : Normalize_OrFallback(fallbackForward, Vec3{ 0.f, 0.f, 1.f });
        const Vec3 normalizedTangent = Normalize_OrFallback(tangent, normalizedSourceVelocity);

        const auto resolve_side_from_tangent = [&normalizedTangent]
        {
            Vec3 side = normalizedTangent.Cross(Vec3{ 0.f, 1.f, 0.f });
            if (side.LengthSquared() <= 0.0001f)
                side = normalizedTangent.Cross(Vec3{ 0.f, 0.f, 1.f });
            if (side.LengthSquared() <= 0.0001f)
                side = Vec3{ 1.f, 0.f, 0.f };
            return Normalize_OrFallback(side, Vec3{ 1.f, 0.f, 0.f });
        };

        const uint32 sourceMotionSeed =
            Resolve_SeedSalt(_desc.motion.sourceMotionVelocitySeed, _effectPlaybackSeed) +
            stampSerial * 3571u +
            2309u;
        Vec3 resolvedDirection{};
        switch (_desc.motion.sourceMotionVelocityDirectionMode)
        {
        case PointParticleSourceMotionVelocityDirectionMode::InheritSourceVelocity:
            break;
        case PointParticleSourceMotionVelocityDirectionMode::SourceVelocityDirection:
            resolvedDirection = normalizedSourceVelocity;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::SourceVelocityOpposite:
            resolvedDirection = -normalizedSourceVelocity;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::TrailTangentOpposite:
            resolvedDirection = -normalizedTangent;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::SideFromTangent:
            resolvedDirection = resolve_side_from_tangent();
            break;
        case PointParticleSourceMotionVelocityDirectionMode::RandomSideFromTangent:
            resolvedDirection = resolve_side_from_tangent();
            if (Hash01(sourceMotionSeed + 37u) < 0.5f)
                resolvedDirection = -resolvedDirection;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::TrailTangent:
        default:
            resolvedDirection = normalizedTangent;
            break;
        }

        if (_desc.motion.sourceMotionVelocityDirectionMode != PointParticleSourceMotionVelocityDirectionMode::InheritSourceVelocity &&
            _desc.motion.sourceMotionVelocitySpreadAngleDegrees > 0.f)
        {
            resolvedDirection = SampleConeDirection(
                resolvedDirection,
                _desc.motion.sourceMotionVelocitySpreadAngleDegrees,
                sourceMotionSeed + 71u
            );
        }

        const float sampledSpeed = Sample_Range(
            _desc.motion.sourceMotionVelocitySpeed.x,
            _desc.motion.sourceMotionVelocitySpeed.y,
            _desc.motion.sourceMotionVelocitySeed,
            stampSerial,
            2357u
        );

        if (_desc.motion.sourceMotionVelocityDirectionMode == PointParticleSourceMotionVelocityDirectionMode::InheritSourceVelocity)
        {
            stamp.sourceMotionVelocity += sourceMotionContext.sourceVelocity * _desc.motion.sourceMotionVelocitySourceSpeedScale;
            stamp.sourceMotionVelocity += normalizedSourceVelocity * sampledSpeed;
        }
        else
        {
            const float resolvedSpeed =
                sampledSpeed + sourceSpeed * _desc.motion.sourceMotionVelocitySourceSpeedScale;
            stamp.sourceMotionVelocity += resolvedDirection * resolvedSpeed;
        }
    }
}

Vec3 ComputeSourceHistorySpriteTrailEmitter::Sample_Acceleration(uint32 stampSerial) const
{
    if (!_desc.motion.enabled)
        return Vec3{};

    const Vec3 sampledAcceleration = Sample_VectorRange(
        _desc.motion.accelerationMin,
        _desc.motion.accelerationMax,
        _desc.motion.accelerationSeed,
        stampSerial,
        701u
    );
    if (_desc.motion.accelerationInWorldSpace || _transformCom == nullptr)
        return sampledAcceleration;

    return _transformCom->Get_WorldRight() * sampledAcceleration.x +
           _transformCom->Get_WorldUp() * sampledAcceleration.y +
           _transformCom->Get_WorldForward() * sampledAcceleration.z;
}

float ComputeSourceHistorySpriteTrailEmitter::Sample_Drag(uint32 stampSerial) const
{
    if (!_desc.motion.enabled)
        return 0.f;

    return max(0.f, Sample_Range(_desc.motion.drag.x, _desc.motion.drag.y, _desc.motion.dragSeed, stampSerial, 607u));
}

float ComputeSourceHistorySpriteTrailEmitter::Sample_PathFollowSpeed(uint32 stampSerial) const
{
    if (!_desc.pathFollow.enabled)
        return 0.f;

    return max(0.f, Sample_Range(_desc.pathFollow.speed.x, _desc.pathFollow.speed.y, _desc.pathFollow.speedSeed, stampSerial, 857u));
}

float ComputeSourceHistorySpriteTrailEmitter::Sample_PathFollowStartDelay(uint32 stampSerial) const
{
    if (!_desc.pathFollow.enabled)
        return 0.f;

    return max(0.f, Sample_Range(_desc.pathFollow.startDelay.x, _desc.pathFollow.startDelay.y, _desc.pathFollow.startDelaySeed, stampSerial, 881u));
}

Vec4 ComputeSourceHistorySpriteTrailEmitter::Sample_Color(
    const Vec4& minColor,
    const Vec4& maxColor,
    const PointParticleRandomSeedRuntimeDesc& seed,
    uint32 stampSerial,
    uint32 salt) const
{
    const uint32 baseSeed = Resolve_SeedSalt(seed, _effectPlaybackSeed) + stampSerial * 3571u + salt;
    return Vec4{
        lerp(minColor.x, maxColor.x, Hash01(baseSeed + 11u)),
        lerp(minColor.y, maxColor.y, Hash01(baseSeed + 23u)),
        lerp(minColor.z, maxColor.z, Hash01(baseSeed + 47u)),
        lerp(minColor.w, maxColor.w, Hash01(baseSeed + 71u))
    };
}

float ComputeSourceHistorySpriteTrailEmitter::Sample_Range(
    float minValue,
    float maxValue,
    const PointParticleRandomSeedRuntimeDesc& seed,
    uint32 stampSerial,
    uint32 salt) const
{
    const uint32 baseSeed = Resolve_SeedSalt(seed, _effectPlaybackSeed) + stampSerial * 3571u + salt;
    return RandomRange(minValue, maxValue, baseSeed);
}

Vec2 ComputeSourceHistorySpriteTrailEmitter::Sample_Vector2Range(
    const Vec2& minValue,
    const Vec2& maxValue,
    const PointParticleRandomSeedRuntimeDesc& seed,
    uint32 stampSerial,
    uint32 salt) const
{
    const uint32 baseSeed = Resolve_SeedSalt(seed, _effectPlaybackSeed) + stampSerial * 3571u + salt;
    return Vec2{
        RandomRange(minValue.x, maxValue.x, baseSeed + 11u),
        RandomRange(minValue.y, maxValue.y, baseSeed + 23u)
    };
}

Vec3 ComputeSourceHistorySpriteTrailEmitter::Sample_VectorRange(
    const Vec3& minValue,
    const Vec3& maxValue,
    const PointParticleRandomSeedRuntimeDesc& seed,
    uint32 stampSerial,
    uint32 salt) const
{
    const uint32 baseSeed = Resolve_SeedSalt(seed, _effectPlaybackSeed) + stampSerial * 3571u + salt;
    return Vec3{
        RandomRange(minValue.x, maxValue.x, baseSeed + 11u),
        RandomRange(minValue.y, maxValue.y, baseSeed + 23u),
        RandomRange(minValue.z, maxValue.z, baseSeed + 47u)
    };
}

Vec4 ComputeSourceHistorySpriteTrailEmitter::Evaluate_ColorOverLife(const Stamp& stamp, float lifeProgress) const
{
    Vec4 color{
        lerp(stamp.startColor.x, stamp.endColor.x, lifeProgress),
        lerp(stamp.startColor.y, stamp.endColor.y, lifeProgress),
        lerp(stamp.startColor.z, stamp.endColor.z, lifeProgress),
        lerp(stamp.startColor.w, stamp.endColor.w, lifeProgress)
    };

    const PointParticleColorOverLifeCurveDesc& curve = _desc.colorOverLife.curve;
    if (curve.colorCurveEnabled)
    {
        const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, curve.colorCurveKeyCount));
        color.x = Evaluate_CompactCurve(
            lifeProgress,
            curve.colorCurveTimes,
            curve.colorCurveTimesBlock1,
            curve.colorCurveValuesR,
            curve.colorCurveValuesRBlock1,
            curve.colorCurveArriveR,
            curve.colorCurveArriveRBlock1,
            curve.colorCurveLeaveR,
            curve.colorCurveLeaveRBlock1,
            curve.colorCurveModes,
            curve.colorCurveModesBlock1,
            keyCount,
            color.x
        );
        color.y = Evaluate_CompactCurve(
            lifeProgress,
            curve.colorCurveTimes,
            curve.colorCurveTimesBlock1,
            curve.colorCurveValuesG,
            curve.colorCurveValuesGBlock1,
            curve.colorCurveArriveG,
            curve.colorCurveArriveGBlock1,
            curve.colorCurveLeaveG,
            curve.colorCurveLeaveGBlock1,
            curve.colorCurveModes,
            curve.colorCurveModesBlock1,
            keyCount,
            color.y
        );
        color.z = Evaluate_CompactCurve(
            lifeProgress,
            curve.colorCurveTimes,
            curve.colorCurveTimesBlock1,
            curve.colorCurveValuesB,
            curve.colorCurveValuesBBlock1,
            curve.colorCurveArriveB,
            curve.colorCurveArriveBBlock1,
            curve.colorCurveLeaveB,
            curve.colorCurveLeaveBBlock1,
            curve.colorCurveModes,
            curve.colorCurveModesBlock1,
            keyCount,
            color.z
        );
    }

    if (curve.alphaCurveEnabled)
    {
        const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, curve.alphaCurveKeyCount));
        color.w = Evaluate_CompactCurve(
            lifeProgress,
            curve.alphaCurveTimes,
            curve.alphaCurveTimesBlock1,
            curve.alphaCurveValues,
            curve.alphaCurveValuesBlock1,
            curve.alphaCurveArrive,
            curve.alphaCurveArriveBlock1,
            curve.alphaCurveLeave,
            curve.alphaCurveLeaveBlock1,
            curve.alphaCurveModes,
            curve.alphaCurveModesBlock1,
            keyCount,
            color.w
        );
    }

    color.w = clamp(color.w, 0.f, 1.f);
    return color;
}

float ComputeSourceHistorySpriteTrailEmitter::Evaluate_WidthScaleByLife(float lifeProgress) const
{
    if (!_desc.sizeByLife.enabled)
        return 1.f;

    const float fallbackScale = lerp(_desc.sizeByLife.multiplyXStart, _desc.sizeByLife.multiplyXEnd, lifeProgress);
    const float curveScale = Evaluate_CompactCurve(
        lifeProgress,
        _desc.sizeByLife.curveKeyTimes,
        _desc.sizeByLife.curveKeyTimesBlock1,
        _desc.sizeByLife.curveKeyValuesX,
        _desc.sizeByLife.curveKeyValuesXBlock1,
        _desc.sizeByLife.curveKeyArriveTangentsX,
        _desc.sizeByLife.curveKeyArriveTangentsXBlock1,
        _desc.sizeByLife.curveKeyLeaveTangentsX,
        _desc.sizeByLife.curveKeyLeaveTangentsXBlock1,
        _desc.sizeByLife.curveKeyModes,
        _desc.sizeByLife.curveKeyModesBlock1,
        _desc.sizeByLife.curveKeyCount,
        fallbackScale
    );
    return _desc.sizeByLife.multiplyX ? max(0.f, curveScale) : 1.f;
}

float ComputeSourceHistorySpriteTrailEmitter::Evaluate_LengthScaleByLife(float lifeProgress) const
{
    if (!_desc.sizeByLife.enabled)
        return 1.f;

    const float fallbackScale = lerp(_desc.sizeByLife.multiplyYStart, _desc.sizeByLife.multiplyYEnd, lifeProgress);
    const float curveScale = Evaluate_CompactCurve(
        lifeProgress,
        _desc.sizeByLife.curveKeyTimes,
        _desc.sizeByLife.curveKeyTimesBlock1,
        _desc.sizeByLife.curveKeyValuesY,
        _desc.sizeByLife.curveKeyValuesYBlock1,
        _desc.sizeByLife.curveKeyArriveTangentsY,
        _desc.sizeByLife.curveKeyArriveTangentsYBlock1,
        _desc.sizeByLife.curveKeyLeaveTangentsY,
        _desc.sizeByLife.curveKeyLeaveTangentsYBlock1,
        _desc.sizeByLife.curveKeyModes,
        _desc.sizeByLife.curveKeyModesBlock1,
        _desc.sizeByLife.curveKeyCount,
        fallbackScale
    );
    return _desc.sizeByLife.multiplyY ? max(0.f, curveScale) : 1.f;
}

float ComputeSourceHistorySpriteTrailEmitter::Evaluate_RotationOverLife(float lifeProgress) const
{
    if (!_desc.rotation.enabled)
        return 0.f;

    const float fallbackRotation = lerp(_desc.rotation.rotationOverLifeDegrees.x, _desc.rotation.rotationOverLifeDegrees.y, lifeProgress);
    const float rotationDegrees = Evaluate_CompactCurve(
        lifeProgress,
        _desc.rotation.rotationOverLifeCurveTimes,
        _desc.rotation.rotationOverLifeCurveTimesBlock1,
        _desc.rotation.rotationOverLifeCurveValues,
        _desc.rotation.rotationOverLifeCurveValuesBlock1,
        _desc.rotation.rotationOverLifeCurveArriveTangents,
        _desc.rotation.rotationOverLifeCurveArriveTangentsBlock1,
        _desc.rotation.rotationOverLifeCurveLeaveTangents,
        _desc.rotation.rotationOverLifeCurveLeaveTangentsBlock1,
        _desc.rotation.rotationOverLifeCurveModes,
        _desc.rotation.rotationOverLifeCurveModesBlock1,
        _desc.rotation.rotationOverLifeCurveKeyCount,
        fallbackRotation
    );
    return XMConvertToRadians(rotationDegrees);
}

float ComputeSourceHistorySpriteTrailEmitter::Evaluate_RotationRateScaleByLife(float lifeProgress) const
{
    if (!_desc.rotation.enabled)
        return 1.f;

    const float fallbackScale = lerp(_desc.rotation.rotationRateScaleByLife.x, _desc.rotation.rotationRateScaleByLife.y, lifeProgress);
    return Evaluate_CompactCurve(
        lifeProgress,
        _desc.rotation.rotationRateScaleByLifeCurveTimes,
        _desc.rotation.rotationRateScaleByLifeCurveTimesBlock1,
        _desc.rotation.rotationRateScaleByLifeCurveValues,
        _desc.rotation.rotationRateScaleByLifeCurveValuesBlock1,
        _desc.rotation.rotationRateScaleByLifeCurveArriveTangents,
        _desc.rotation.rotationRateScaleByLifeCurveArriveTangentsBlock1,
        _desc.rotation.rotationRateScaleByLifeCurveLeaveTangents,
        _desc.rotation.rotationRateScaleByLifeCurveLeaveTangentsBlock1,
        _desc.rotation.rotationRateScaleByLifeCurveModes,
        _desc.rotation.rotationRateScaleByLifeCurveModesBlock1,
        _desc.rotation.rotationRateScaleByLifeCurveKeyCount,
        fallbackScale
    );
}

Vec2 ComputeSourceHistorySpriteTrailEmitter::Evaluate_SpriteTiltOverLife(const Stamp& stamp, float lifeProgress) const
{
    if (!_desc.spriteTilt.tiltOverLifeEnabled)
        return Vec2{};
    if (!_desc.spriteTilt.tiltOverLifeCurveEnabled)
        return stamp.tiltOverLifeDegrees;

    const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, _desc.spriteTilt.tiltOverLifeCurveKeyCount));
    const Vec2 fallbackTilt{
        lerp(_desc.spriteTilt.tiltOverLifeMin.x, _desc.spriteTilt.tiltOverLifeMax.x, lifeProgress),
        lerp(_desc.spriteTilt.tiltOverLifeMin.y, _desc.spriteTilt.tiltOverLifeMax.y, lifeProgress)
    };
    return Vec2{
        Evaluate_CompactCurve(
            lifeProgress,
            _desc.spriteTilt.tiltOverLifeCurveTimes,
            _desc.spriteTilt.tiltOverLifeCurveTimesBlock1,
            _desc.spriteTilt.tiltOverLifeCurveValuesX,
            _desc.spriteTilt.tiltOverLifeCurveValuesXBlock1,
            _desc.spriteTilt.tiltOverLifeCurveArriveTangentsX,
            _desc.spriteTilt.tiltOverLifeCurveArriveTangentsXBlock1,
            _desc.spriteTilt.tiltOverLifeCurveLeaveTangentsX,
            _desc.spriteTilt.tiltOverLifeCurveLeaveTangentsXBlock1,
            _desc.spriteTilt.tiltOverLifeCurveModes,
            _desc.spriteTilt.tiltOverLifeCurveModesBlock1,
            keyCount,
            fallbackTilt.x
        ),
        Evaluate_CompactCurve(
            lifeProgress,
            _desc.spriteTilt.tiltOverLifeCurveTimes,
            _desc.spriteTilt.tiltOverLifeCurveTimesBlock1,
            _desc.spriteTilt.tiltOverLifeCurveValuesY,
            _desc.spriteTilt.tiltOverLifeCurveValuesYBlock1,
            _desc.spriteTilt.tiltOverLifeCurveArriveTangentsY,
            _desc.spriteTilt.tiltOverLifeCurveArriveTangentsYBlock1,
            _desc.spriteTilt.tiltOverLifeCurveLeaveTangentsY,
            _desc.spriteTilt.tiltOverLifeCurveLeaveTangentsYBlock1,
            _desc.spriteTilt.tiltOverLifeCurveModes,
            _desc.spriteTilt.tiltOverLifeCurveModesBlock1,
            keyCount,
            fallbackTilt.y
        )
    };
}

float ComputeSourceHistorySpriteTrailEmitter::Evaluate_VelocityScaleByLifeChannel(
    const PointParticleFloatCurveRuntimeDesc& curve,
    float lifeProgress) const
{
    if (!_desc.motion.enabled)
        return 1.f;
    if (!curve.enabled)
        return 1.f;

    const float curveScale = Evaluate_CompactCurve(
        lifeProgress,
        curve.curveKeyTimes,
        curve.curveKeyTimesBlock1,
        curve.curveKeyValues,
        curve.curveKeyValuesBlock1,
        curve.curveKeyArriveTangents,
        curve.curveKeyArriveTangentsBlock1,
        curve.curveKeyLeaveTangents,
        curve.curveKeyLeaveTangentsBlock1,
        curve.curveKeyModes,
        curve.curveKeyModesBlock1,
        curve.curveKeyCount,
        1.f
    );
    return max(0.f, curveScale);
}

Vec3 ComputeSourceHistorySpriteTrailEmitter::Evaluate_ScaledVelocityChannels(const Stamp& stamp, float lifeProgress) const
{
    const float initialVelocityScale = Evaluate_VelocityScaleByLifeChannel(
        _desc.motion.initialVelocityScaleByLife,
        lifeProgress
    );
    const float initialRadialVelocityScale = Evaluate_VelocityScaleByLifeChannel(
        _desc.motion.initialRadialVelocityScaleByLife,
        lifeProgress
    );
    const float velocityConeScale = Evaluate_VelocityScaleByLifeChannel(
        _desc.motion.velocityConeScaleByLife,
        lifeProgress
    );
    const float sourceMotionVelocityScale = Evaluate_VelocityScaleByLifeChannel(
        _desc.motion.sourceMotionVelocityScaleByLife,
        lifeProgress
    );
    const float accelerationIntegratedVelocityScale = Evaluate_VelocityScaleByLifeChannel(
        _desc.motion.accelerationIntegratedVelocityScaleByLife,
        lifeProgress
    );

    return
        stamp.initialVelocity * initialVelocityScale +
        stamp.initialRadialVelocity * initialRadialVelocityScale +
        stamp.velocityCone * velocityConeScale +
        stamp.sourceMotionVelocity * sourceMotionVelocityScale +
        stamp.accelerationIntegratedVelocity * accelerationIntegratedVelocityScale;
}

Vec3 ComputeSourceHistorySpriteTrailEmitter::Evaluate_AccelerationByLife(const Stamp& stamp, float lifeProgress) const
{
    if (!_desc.motion.enabled || !_desc.motion.accelerationCurveEnabled)
        return stamp.acceleration;

    const float activeLoopTime = max(0.f, _loopElapsedTime - Resolve_CurrentLoopDelay());
    const float curveProgress =
        _desc.motion.accelerationTimeBasis == PointParticleAccelerationTimeBasis::EmitterNormalizedTime
        ? clamp(activeLoopTime / Resolve_Duration(), 0.f, 1.f)
        : lifeProgress;
    const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, _desc.motion.accelerationCurveKeyCount));
    const Vec3 acceleration{
        Evaluate_CompactCurve(
            curveProgress,
            _desc.motion.accelerationCurveTimes,
            _desc.motion.accelerationCurveTimesBlock1,
            _desc.motion.accelerationCurveValuesX,
            _desc.motion.accelerationCurveValuesXBlock1,
            _desc.motion.accelerationCurveArriveTangentsX,
            _desc.motion.accelerationCurveArriveTangentsXBlock1,
            _desc.motion.accelerationCurveLeaveTangentsX,
            _desc.motion.accelerationCurveLeaveTangentsXBlock1,
            _desc.motion.accelerationCurveModes,
            _desc.motion.accelerationCurveModesBlock1,
            keyCount,
            stamp.acceleration.x
        ),
        Evaluate_CompactCurve(
            curveProgress,
            _desc.motion.accelerationCurveTimes,
            _desc.motion.accelerationCurveTimesBlock1,
            _desc.motion.accelerationCurveValuesY,
            _desc.motion.accelerationCurveValuesYBlock1,
            _desc.motion.accelerationCurveArriveTangentsY,
            _desc.motion.accelerationCurveArriveTangentsYBlock1,
            _desc.motion.accelerationCurveLeaveTangentsY,
            _desc.motion.accelerationCurveLeaveTangentsYBlock1,
            _desc.motion.accelerationCurveModes,
            _desc.motion.accelerationCurveModesBlock1,
            keyCount,
            stamp.acceleration.y
        ),
        Evaluate_CompactCurve(
            curveProgress,
            _desc.motion.accelerationCurveTimes,
            _desc.motion.accelerationCurveTimesBlock1,
            _desc.motion.accelerationCurveValuesZ,
            _desc.motion.accelerationCurveValuesZBlock1,
            _desc.motion.accelerationCurveArriveTangentsZ,
            _desc.motion.accelerationCurveArriveTangentsZBlock1,
            _desc.motion.accelerationCurveLeaveTangentsZ,
            _desc.motion.accelerationCurveLeaveTangentsZBlock1,
            _desc.motion.accelerationCurveModes,
            _desc.motion.accelerationCurveModesBlock1,
            keyCount,
            stamp.acceleration.z
        )
    };

    if (_desc.motion.accelerationInWorldSpace || _transformCom == nullptr)
        return acceleration;

    return _transformCom->Get_WorldRight() * acceleration.x +
           _transformCom->Get_WorldUp() * acceleration.y +
           _transformCom->Get_WorldForward() * acceleration.z;
}

float ComputeSourceHistorySpriteTrailEmitter::Evaluate_PathReplayDrainCurve(float progress) const
{
    if (!_desc.pathReplay.drainCurve.enabled)
        return progress;

    return Evaluate_CompactCurve(
        clamp(progress, 0.f, 1.f),
        _desc.pathReplay.drainCurve.curveKeyTimes,
        _desc.pathReplay.drainCurve.curveKeyTimesBlock1,
        _desc.pathReplay.drainCurve.curveKeyValues,
        _desc.pathReplay.drainCurve.curveKeyValuesBlock1,
        _desc.pathReplay.drainCurve.curveKeyCount,
        progress
    );
}

uint32 ComputeSourceHistorySpriteTrailEmitter::Evaluate_SubUVFrameIndex(const Stamp& stamp) const
{
    const uint32 rows = max(1u, _desc.material.subUVRows);
    const uint32 cols = max(1u, _desc.material.subUVCols);
    const uint32 frameCount = max(1u, rows * cols);
    if (!_desc.subUVFrameOverLife.enabled)
        return 0u;

    const SubUVFrameRange frameRange = Resolve_SubUVFrameRange(
        _desc.subUVFrameOverLife.startFrame,
        _desc.subUVFrameOverLife.endFrame,
        frameCount
    );
    const float stampLifeProgress = clamp(stamp.age / max(0.0001f, stamp.lifetime), 0.f, 1.f);
    switch (_desc.subUVFrameOverLife.playbackMode)
    {
    case SubUVFramePlaybackMode::LifeProgress:
    {
        const uint32 frameOffset =
            min(static_cast<uint32>(floorf(stampLifeProgress * static_cast<float>(frameRange.rangeCount))), frameRange.rangeCount - 1u);
        return Resolve_SubUVFrameInRange(frameRange, frameOffset);
    }
    case SubUVFramePlaybackMode::FramesPerSecond:
    {
        const uint32 frameOffset = static_cast<uint32>(floorf(stamp.age * max(0.f, _desc.subUVFrameOverLife.framesPerSecond)));
        const uint32 phaseOffset =
            _desc.subUVFrameOverLife.randomStartPhase
            ? Sample_RandomSubUVFrameIndex(stamp.serial, 0u, frameRange.rangeCount, frameRange.rangeCount)
            : 0u;
        return Resolve_SubUVFrameInRange(
            frameRange,
            Resolve_SubUVPlaybackOffset(frameRange, frameOffset, phaseOffset, _desc.subUVFrameOverLife.loop)
        );
    }
    case SubUVFramePlaybackMode::RandomFrame:
        return Sample_RandomSubUVFrameIndex(stamp.serial, frameRange.startFrame, frameRange.frameCount, frameRange.rangeCount);
    case SubUVFramePlaybackMode::FixedFrame:
    default:
        return frameRange.startFrame;
    }
}

uint32 ComputeSourceHistorySpriteTrailEmitter::Sample_RandomSubUVFrameIndex(uint32 stampSerial, uint32 startFrame, uint32 frameCount, uint32 rangeCount) const
{
    const uint32 seed =
        Resolve_SeedSalt(_desc.subUVRandomFrameSeed, _effectPlaybackSeed) +
        stampSerial * 7919u +
        1201u;
    const uint32 safeRangeCount = max(1u, rangeCount);
    const uint32 frameOffset = min(static_cast<uint32>(floorf(Hash01(seed) * static_cast<float>(safeRangeCount))), safeRangeCount - 1u);
    return (startFrame + frameOffset) % max(1u, frameCount);
}

Vec4 ComputeSourceHistorySpriteTrailEmitter::Resolve_SubUVRect(uint32 frameIndex) const
{
    const uint32 rows = max(1u, _desc.material.subUVRows);
    const uint32 cols = max(1u, _desc.material.subUVCols);
    const uint32 clampedFrameIndex = min(frameIndex, rows * cols - 1u);
    const uint32 row = clampedFrameIndex / cols;
    const uint32 col = clampedFrameIndex % cols;
    const float invCols = 1.f / static_cast<float>(cols);
    const float invRows = 1.f / static_cast<float>(rows);
    const float u0 = static_cast<float>(col) * invCols;
    const float v0 = static_cast<float>(row) * invRows;
    return Vec4{ u0, v0, u0 + invCols, v0 + invRows };
}

float ComputeSourceHistorySpriteTrailEmitter::Evaluate_CompactCurve(
    float lifeProgress,
    const Vec4& times,
    const Vec4& timesBlock1,
    const Vec4& values,
    const Vec4& valuesBlock1,
    uint32 keyCount,
    float fallbackValue) const
{
    const uint32 clampedKeyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= Read_Vec4Component(times, timesBlock1, 0u))
        return Read_Vec4Component(values, valuesBlock1, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = Read_Vec4Component(times, timesBlock1, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = Read_Vec4Component(times, timesBlock1, index - 1u);
        const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
        return lerp(
            Read_Vec4Component(values, valuesBlock1, index - 1u),
            Read_Vec4Component(values, valuesBlock1, index),
            ratio
        );
    }

    return lifeProgress > Read_Vec4Component(times, timesBlock1, clampedKeyCount - 1u)
           ? Read_Vec4Component(values, valuesBlock1, clampedKeyCount - 1u)
           : fallbackValue;
}

float ComputeSourceHistorySpriteTrailEmitter::Evaluate_CompactCurve(
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
    float fallbackValue) const
{
    const uint32 clampedKeyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= Read_Vec4Component(times, timesBlock1, 0u))
        return Read_Vec4Component(values, valuesBlock1, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = Read_Vec4Component(times, timesBlock1, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = Read_Vec4Component(times, timesBlock1, index - 1u);
        const float leftValue = Read_Vec4Component(values, valuesBlock1, index - 1u);
        const float rightValue = Read_Vec4Component(values, valuesBlock1, index);
        const float mode = Read_Vec4Component(modes, modesBlock1, index - 1u);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = clamp((lifeProgress - leftTime) / width, 0.f, 1.f);

        if (mode < 0.5f)
            return leftValue;

        if (mode >= 1.5f)
        {
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            const float leftLeave = Read_Vec4Component(leaveTangents, leaveTangentsBlock1, index - 1u) * width;
            const float rightArrive = Read_Vec4Component(arriveTangents, arriveTangentsBlock1, index) * width;
            return
                (2.f * t3 - 3.f * t2 + 1.f) * leftValue +
                (t3 - 2.f * t2 + ratio) * leftLeave +
                (-2.f * t3 + 3.f * t2) * rightValue +
                (t3 - t2) * rightArrive;
        }

        return lerp(leftValue, rightValue, ratio);
    }

    return lifeProgress > Read_Vec4Component(times, timesBlock1, clampedKeyCount - 1u)
           ? Read_Vec4Component(values, valuesBlock1, clampedKeyCount - 1u)
           : fallbackValue;
}

Shared<ComputeSourceHistorySpriteTrailEmitter> ComputeSourceHistorySpriteTrailEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeSourceHistorySpriteTrailEmitter>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeSourceHistorySpriteTrailEmitter");
        MSG_BOX("Failed to Create : ComputeSourceHistorySpriteTrailEmitter");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> ComputeSourceHistorySpriteTrailEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeSourceHistorySpriteTrailEmitter>(*this);
    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeSourceHistorySpriteTrailEmitter");
        MSG_BOX("Failed to Clone : ComputeSourceHistorySpriteTrailEmitter");
        return nullptr;
    }
    return instance;
}

void ComputeSourceHistorySpriteTrailEmitter::Free()
{
}

NS_END
