#include "ComputeGeneratedBeamEmitter.h"

#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "EffectMaterialScalarModulationRuntime.h"
#include "GameInstance.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "Texture.h"

namespace Client::EffectBeamRuntime
{
static int Resolve_MaterialSourceIndex(const string& source)
{
    if (source == "Red" || source == "red")
        return 1;

    if (source == "Luminance" || source == "luminance")
        return 2;

    return 0;
}

static uint32 Resolve_SeedSalt(const PointParticleRandomSeedRuntimeDesc& seed, uint32 effectPlaybackSeed)
{
    uint32 salt = seed.manualSeedEnabled ? seed.seed : 0u;
    if (seed.useInstanceSeed)
        salt += effectPlaybackSeed;
    return salt;
}

static uint32 MixSeed(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return seed;
}

static float Hash01(uint32 seed)
{
    return static_cast<float>(MixSeed(seed) & 0x00FFFFFFu) / 16777215.f;
}

struct SubUVFrameRange
{
    uint32 startFrame{};
    uint32 frameCount{ 1u };
    uint32 rangeCount{ 1u };
};

static SubUVFrameRange Resolve_SubUVFrameRange(uint32 startFrame, uint32 endFrame, uint32 frameCount)
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

static uint32 Resolve_SubUVFrameInRange(const SubUVFrameRange& range, uint32 frameOffset)
{
    return (range.startFrame + min(frameOffset, range.rangeCount - 1u)) % range.frameCount;
}

static uint32 Resolve_SubUVPlaybackOffset(const SubUVFrameRange& range, uint32 frameOffset, uint32 phaseOffset, bool loop)
{
    const uint32 offset = frameOffset + phaseOffset;
    return loop ? offset % range.rangeCount : min(offset, range.rangeCount - 1u);
}

static void Resolve_PerpendicularAxes(const Vec3& tangent, Vec3& outRight, Vec3& outUp)
{
    const Vec3 upAxis = fabsf(tangent.y) < 0.99f ? Vec3{ 0.f, 1.f, 0.f } : Vec3{ 1.f, 0.f, 0.f };
    outRight = tangent.Cross(upAxis);
    if (outRight.LengthSquared() <= 0.0001f)
        outRight = tangent.Cross(Vec3{ 0.f, 0.f, 1.f });

    if (outRight.LengthSquared() <= 0.0001f)
    {
        outRight = Vec3::Zero;
        outUp = Vec3::Zero;
        return;
    }

    outRight.Normalize();
    outUp = outRight.Cross(tangent);
    if (outUp.LengthSquared() <= 0.0001f)
    {
        outRight = Vec3::Zero;
        outUp = Vec3::Zero;
        return;
    }
    outUp.Normalize();
}

static float Read_Vec4Component(const Vec4& values, uint32 index)
{
    switch (index)
    {
    case 0:
        return values.x;
    case 1:
        return values.y;
    case 2:
        return values.z;
    case 3:
        return values.w;
    default:
        return values.x;
    }
}

static float Read_Vec4Component(const Vec4& valuesBlock0, const Vec4& valuesBlock1, uint32 index)
{
    if (index < 4u)
        return Read_Vec4Component(valuesBlock0, index);

    return Read_Vec4Component(valuesBlock1, index - 4u);
}

static float Evaluate_FloatCurve(const PointParticleFloatCurveRuntimeDesc& curve, float phase, float fallbackValue)
{
    if (!curve.enabled)
        return fallbackValue;

    const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, curve.curveKeyCount));
    const float clampedPhase = clamp(phase, 0.f, 1.f);
    if (keyCount == 1u || clampedPhase <= Read_Vec4Component(curve.curveKeyTimes, curve.curveKeyTimesBlock1, 0u))
        return Read_Vec4Component(curve.curveKeyValues, curve.curveKeyValuesBlock1, 0u);

    for (uint32 index = 1u; index < keyCount; ++index)
    {
        const float rightTime = Read_Vec4Component(curve.curveKeyTimes, curve.curveKeyTimesBlock1, index);
        if (clampedPhase > rightTime)
            continue;

        const float leftTime = Read_Vec4Component(curve.curveKeyTimes, curve.curveKeyTimesBlock1, index - 1u);
        const float leftValue = Read_Vec4Component(curve.curveKeyValues, curve.curveKeyValuesBlock1, index - 1u);
        const float rightValue = Read_Vec4Component(curve.curveKeyValues, curve.curveKeyValuesBlock1, index);
        const float mode = Read_Vec4Component(curve.curveKeyModes, curve.curveKeyModesBlock1, index - 1u);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = clamp((clampedPhase - leftTime) / width, 0.f, 1.f);

        if (mode < 0.5f)
            return leftValue;

        if (mode >= 1.5f)
        {
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            const float leftLeave = Read_Vec4Component(curve.curveKeyLeaveTangents, curve.curveKeyLeaveTangentsBlock1, index - 1u) * width;
            const float rightArrive = Read_Vec4Component(curve.curveKeyArriveTangents, curve.curveKeyArriveTangentsBlock1, index) * width;
            return
                (2.f * t3 - 3.f * t2 + 1.f) * leftValue +
                (t3 - 2.f * t2 + ratio) * leftLeave +
                (-2.f * t3 + 3.f * t2) * rightValue +
                (t3 - t2) * rightArrive;
        }

        return lerp(leftValue, rightValue, ratio);
    }

    return clampedPhase > Read_Vec4Component(curve.curveKeyTimes, curve.curveKeyTimesBlock1, keyCount - 1u)
           ? Read_Vec4Component(curve.curveKeyValues, curve.curveKeyValuesBlock1, keyCount - 1u)
           : fallbackValue;
}

static Vec4 Average_Color(const Vec4& minColor, const Vec4& maxColor)
{
    return Vec4{
        (minColor.x + maxColor.x) * 0.5f,
        (minColor.y + maxColor.y) * 0.5f,
        (minColor.z + maxColor.z) * 0.5f,
        (minColor.w + maxColor.w) * 0.5f
    };
}

static HRESULT Bind_MaterialScalarModulationPayload(
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
        shader->Bind_RawValue(
            "g_EffectMaterialScalarModulationKeyArriveTangentsBlock1",
            payload.keyArriveTangentsBlock1.data(),
            sizeof(payload.keyArriveTangentsBlock1)
        ),
        E_FAIL
    );
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyLeaveTangents", payload.keyLeaveTangents.data(), sizeof(payload.keyLeaveTangents)),
        E_FAIL
    );
    CHECK_FAILED(
        shader->Bind_RawValue(
            "g_EffectMaterialScalarModulationKeyLeaveTangentsBlock1",
            payload.keyLeaveTangentsBlock1.data(),
            sizeof(payload.keyLeaveTangentsBlock1)
        ),
        E_FAIL
    );
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyModes", payload.keyModes.data(), sizeof(payload.keyModes)), E_FAIL);
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyModesBlock1", payload.keyModesBlock1.data(), sizeof(payload.keyModesBlock1)),
        E_FAIL
    );

    return S_OK;
}

static wstring Resolve_TexturePathByGuid(const string& textureGuid)
{
    if (GAME == nullptr || textureGuid.empty())
        return {};

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
    if (assetMeta == nullptr || assetMeta->type != "Texture")
        return {};

    const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
    return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
}

static wstring Resolve_TexturePathByPath(const string& texturePath)
{
    if (texturePath.empty())
        return {};

    fs::path candidatePath = String::ToWString(texturePath);
    if (candidatePath.is_relative() && GAME != nullptr)
        candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

    candidatePath = candidatePath.lexically_normal();
    return fs::exists(candidatePath) ? candidatePath.wstring() : wstring{};
}

static bool Is_TextureSamePath(const wstring& lhs, const wstring& rhs)
{
    if (lhs.empty() || rhs.empty())
        return false;

    return fs::path(lhs).lexically_normal() == fs::path(rhs).lexically_normal();
}

static wstring Resolve_TexturePath(
    const string& textureGuid,
    const string& texturePath,
    bool& outUsedPathFallback,
    bool& outGuidPathMismatch)
{
    outUsedPathFallback = false;
    outGuidPathMismatch = false;

    wstring resolvedPath = Resolve_TexturePathByGuid(textureGuid);
    if (!resolvedPath.empty())
    {
        const wstring pathResolved = Resolve_TexturePathByPath(texturePath);
        outGuidPathMismatch = !pathResolved.empty() && !Is_TextureSamePath(resolvedPath, pathResolved);
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

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(ComputeGeneratedBeamEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "ComputeGeneratedBeamEmitter";
    info.category = "Effect";

    return true;
}

ComputeGeneratedBeamEmitter::ComputeGeneratedBeamEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

ComputeGeneratedBeamEmitter::ComputeGeneratedBeamEmitter(const ComputeGeneratedBeamEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

ComputeGeneratedBeamEmitter::~ComputeGeneratedBeamEmitter()
{
    Free();
}

HRESULT ComputeGeneratedBeamEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Initialize(void* arg)
{
    if (arg != nullptr)
        _desc = *static_cast<ComputeBeamEmitterDesc*>(arg);

    _desc.playback.duration = max(0.0001f, _desc.playback.duration);
    _desc.playback.delay = max(0.f, _desc.playback.delay);
    _desc.lifetime.lifeTime.x = max(0.0001f, _desc.lifetime.lifeTime.x);
    _desc.lifetime.lifeTime.y = max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y);
    _desc.length = max(0.f, _desc.length);
    _desc.segmentCount = max(1u, _desc.segmentCount);
    _desc.noiseAmplitude = max(0.f, _desc.noiseAmplitude);
    _desc.stripCount = max(1u, _desc.stripCount);
    _desc.endSpreadRadius = max(0.f, _desc.endSpreadRadius);
    _desc.lengthVariance = max(0.f, _desc.lengthVariance);
    _desc.branchChance = clamp(_desc.branchChance, 0.f, 1.f);
    _desc.branchSegmentCount = max(1u, _desc.branchSegmentCount);
    _desc.branchLength = max(0.f, _desc.branchLength);
    _desc.branchLengthVariance = max(0.f, _desc.branchLengthVariance);
    _desc.branchStartMin = clamp(_desc.branchStartMin, 0.f, 1.f);
    _desc.branchStartMax = clamp(_desc.branchStartMax, 0.f, 1.f);
    if (_desc.branchStartMin > _desc.branchStartMax)
        swap(_desc.branchStartMin, _desc.branchStartMax);
    _desc.branchSpreadRadius = max(0.f, _desc.branchSpreadRadius);
    _desc.branchEndSpreadRadius = max(0.f, _desc.branchEndSpreadRadius);
    _desc.branchOutwardAmount = max(0.f, _desc.branchOutwardAmount);
    _desc.branchCurveAmount = max(0.f, _desc.branchCurveAmount);
    _desc.branchDownLength = max(0.f, _desc.branchDownLength);
    _desc.branchEntangleRadius = max(0.f, _desc.branchEntangleRadius);
    _desc.branchEntangleAdvance = max(0.f, _desc.branchEntangleAdvance);
    _desc.branchCrackLength = max(0.f, _desc.branchCrackLength);
    _desc.branchCrackSpreadRadius = max(0.f, _desc.branchCrackSpreadRadius);
    _desc.branchWidthScale = max(0.f, _desc.branchWidthScale);
    _desc.baseWidth = max(0.001f, _desc.baseWidth);
    _desc.tilingDistance = max(0.f, _desc.tilingDistance);
    _desc.material.subUVRows = max(1u, _desc.material.subUVRows);
    _desc.material.subUVCols = max(1u, _desc.material.subUVCols);
    _desc.subUVFrameOverLife.frameCurveKeyCount =
        max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, _desc.subUVFrameOverLife.frameCurveKeyCount));

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    Reset_PlaybackRuntime();
    return S_OK;
}

void ComputeGeneratedBeamEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();
    Advance_Playback(timeDelta);
    _materialElapsedTime += max(0.f, timeDelta);

    if (PlaybackState::Playing == _playbackState &&
        Resolve_ActiveElapsedTime(Resolve_Duration()) < Resolve_VisualLife())
        Rebuild_PathSamples();
    else
    {
        _pathEntries.clear();
        _visibleBeamLength = 1.f;
    }

    CHECK_FAILED_THROTTLED(Dispatch_Compute(), 60);
}

void ComputeGeneratedBeamEmitter::Late_Update(float)
{
    if (Can_SubmitRender())
        GAME->Add_RenderGroup(Resolve_RenderGroup(), GetSharedPtr<GameObject>());
}

HRESULT ComputeGeneratedBeamEmitter::Render()
{
    CHECK_FAILED_THROTTLED(Bind_ShaderResources(), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Begin(Resolve_ShaderPassIndex()), 60, E_FAIL);

    constexpr uint32 stride0 = sizeof(Vec3);
    constexpr uint32 offset0 = 0;
    constexpr uint32 stride1 = sizeof(BeamInstanceVertex);
    constexpr uint32 offset1 = 0;
    Buffer* vertexBuffers[]{ _pointVB.Get(), _instanceBuffer.Get() };
    const uint32 strides[]{ stride0, stride1 };
    const uint32 offsets[]{ offset0, offset1 };

    _context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
    _context->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    _context->DrawIndexedInstancedIndirect(_indirectArgsBuffer.Get(), 0);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    _pathEntries.clear();
    _materialElapsedTime = 0.f;
    _visibleBeamLength = 1.f;
    _drawSegmentCount = 0u;

    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    if (nullptr != _computeOutput && nullptr != _computeArgsOutput)
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);

    return S_OK;
}

bool ComputeGeneratedBeamEmitter::Is_EffectFinished() const
{
    return _playbackState == PlaybackState::Completed;
}

bool ComputeGeneratedBeamEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    for (const PathEntry& pathEntry : _pathEntries)
    {
        const vector<PathSample>& pathSamples = pathEntry.samples;
        if (pathSamples.size() >= 2u)
        {
            outWorldPosition = (pathSamples.front().position + pathSamples.back().position) * 0.5f;
            return true;
        }
    }

    if (_transformCom == nullptr)
        return false;

    outWorldPosition = _transformCom->Get_WorldPosition();
    return true;
}

EffectSortPolicy ComputeGeneratedBeamEmitter::Get_BlendSortPolicy() const
{
    return _desc.sort.sortPolicy;
}

int32 ComputeGeneratedBeamEmitter::Get_BlendSortLayer() const
{
    return _desc.sort.sortLayer;
}

float ComputeGeneratedBeamEmitter::Get_BlendSortBias() const
{
    return _desc.sort.artistSortBias;
}

void ComputeGeneratedBeamEmitter::Reset_PlaybackRuntime()
{
    _playbackState = PlaybackState::Delayed;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _drawSegmentCount = 0u;
    _visibleBeamLength = 1.f;
    Resample_VisualLife();
    Resample_RandomSubUVFrameIndex();
}

void ComputeGeneratedBeamEmitter::Advance_Playback(float timeDelta)
{
    if (PlaybackState::Completed == _playbackState)
        return;

    const float safeDeltaTime = max(0.f, timeDelta);
    _loopElapsedTime += safeDeltaTime;

    const float delay = Resolve_CurrentLoopDelay();
    if (_loopElapsedTime < delay)
    {
        _playbackState = PlaybackState::Delayed;
        return;
    }

    _playbackState = PlaybackState::Playing;
    const float activeElapsedTime = _loopElapsedTime - delay;
    if (activeElapsedTime < Resolve_Duration())
        return;

    if (Has_NextLoop())
    {
        Start_NextLoop();
        return;
    }

    _playbackState = PlaybackState::Completed;
}

void ComputeGeneratedBeamEmitter::Rebuild_PathSamples()
{
    _pathEntries.clear();
    _visibleBeamLength = 1.f;

    const Vec3 localStart = _desc.localStart;
    const uint32 sampleCount = _desc.segmentCount + 1u;
    const float activeElapsedTime = clamp(Resolve_ActiveElapsedTime(Resolve_Duration()), 0.f, Resolve_VisualLife());

    float maxVisibleLength = 0.f;
    for (uint32 stripIndex = 0u; stripIndex < _desc.stripCount; ++stripIndex)
    {
        const Vec3 localEnd = Resolve_LocalEnd(stripIndex);
        const Vec3 localDelta = localEnd - localStart;
        const float localLength = localDelta.Length();
        if (localLength <= 0.0001f)
            continue;

        const Vec3 tangent = localDelta / localLength;
        PathEntry parentPath{};
        parentPath.stripIndex = stripIndex;
        parentPath.widthScale = 1.f;
        parentPath.samples.reserve(sampleCount);
        Vec3 previousWorldPosition{};
        for (uint32 index = 0u; index < sampleCount; ++index)
        {
            const float ratio = sampleCount > 1u ? static_cast<float>(index) / static_cast<float>(sampleCount - 1u) : 0.f;
            Vec3 localPoint = Vec3::Lerp(localStart, localEnd, ratio);
            if (index > 0u && index + 1u < sampleCount && _desc.noiseAmplitude > 0.f)
                localPoint += Evaluate_NoiseOffset(stripIndex, index, tangent);

            const Vec3 worldPosition = Transform_LocalPoint(localPoint);
            PathSample sample{};
            sample.position = worldPosition;
            sample.age = activeElapsedTime;
            if (!parentPath.samples.empty())
                sample.distance = parentPath.samples.back().distance + (worldPosition - previousWorldPosition).Length();
            parentPath.samples.push_back(sample);
            previousWorldPosition = worldPosition;
        }

        if (parentPath.samples.size() >= 2u)
        {
            maxVisibleLength = max(maxVisibleLength, parentPath.samples.back().distance);
            _pathEntries.push_back(parentPath);
            Append_BranchPathEntries(parentPath, stripIndex, activeElapsedTime);
        }
    }

    if (maxVisibleLength > 0.f)
        _visibleBeamLength = max(0.0001f, maxVisibleLength);
}

Vec3 ComputeGeneratedBeamEmitter::Resolve_LocalEnd(uint32 stripIndex) const
{
    Vec3 baseLocalEnd = _desc.localEnd;
    if (_desc.endpointMode == EffectBeamEndpointMode::DirectionLength)
    {
        const float safeLength = max(0.f, _desc.length);
        const float directionLength = _desc.localDirection.Length();
        if (directionLength <= 0.0001f || safeLength <= 0.f)
            return _desc.localStart;

        baseLocalEnd = _desc.localStart + _desc.localDirection / directionLength * safeLength;
    }

    const Vec3 baseDelta = baseLocalEnd - _desc.localStart;
    const float baseLength = baseDelta.Length();
    if (baseLength <= 0.0001f || (stripIndex == 0u && _desc.lengthVariance <= 0.f && _desc.endSpreadRadius <= 0.f))
        return baseLocalEnd;

    const Vec3 tangent = baseDelta / baseLength;
    const uint32 baseSeed = _desc.seed + (_loopIndex + 1u) * 9781u + stripIndex * 15485863u;
    const float lengthOffset = stripIndex > 0u && _desc.lengthVariance > 0.f
                               ? (EffectBeamRuntime::Hash01(baseSeed + 11u) * 2.f - 1.f) * _desc.lengthVariance
                               : 0.f;
    const float resolvedLength = max(0.f, baseLength + lengthOffset);
    Vec3 localEnd = _desc.localStart + tangent * resolvedLength;

    if (stripIndex > 0u && _desc.endSpreadRadius > 0.f)
    {
        Vec3 right{};
        Vec3 up{};
        EffectBeamRuntime::Resolve_PerpendicularAxes(tangent, right, up);
        if (right.LengthSquared() > 0.0001f && up.LengthSquared() > 0.0001f)
        {
            const float angle = EffectBeamRuntime::Hash01(baseSeed + 23u) * XM_2PI;
            const float radius = sqrtf(EffectBeamRuntime::Hash01(baseSeed + 29u)) * _desc.endSpreadRadius;
            localEnd += right * cosf(angle) * radius + up * sinf(angle) * radius;
        }
    }

    return localEnd;
}

Vec3 ComputeGeneratedBeamEmitter::Transform_LocalPoint(const Vec3& localPoint) const
{
    if (_transformCom == nullptr)
        return localPoint;

    return Vec3::Transform(localPoint, _transformCom->Get_WorldMatrix());
}

Vec3 ComputeGeneratedBeamEmitter::Evaluate_NoiseOffset(uint32 stripIndex, uint32 sampleIndex, const Vec3& tangent) const
{
    if (_desc.noiseAmplitude <= 0.f)
        return Vec3::Zero;

    Vec3 right{};
    Vec3 up{};
    EffectBeamRuntime::Resolve_PerpendicularAxes(tangent, right, up);
    if (right.LengthSquared() <= 0.0001f || up.LengthSquared() <= 0.0001f)
        return Vec3::Zero;

    const uint32 baseSeed = _desc.seed + (_loopIndex + 1u) * 9781u + stripIndex * 15485863u + sampleIndex * 6271u;
    const float lateralX = EffectBeamRuntime::Hash01(baseSeed) * 2.f - 1.f;
    const float lateralY = EffectBeamRuntime::Hash01(baseSeed + 1u) * 2.f - 1.f;
    return (right * lateralX + up * lateralY) * _desc.noiseAmplitude;
}

void ComputeGeneratedBeamEmitter::Append_BranchPathEntries(const PathEntry& parentPath, uint32 stripIndex, float activeElapsedTime)
{
    if (!_desc.branchEnabled || _desc.branchCount == 0u || _desc.branchChance <= 0.f ||
        _desc.branchSegmentCount == 0u || parentPath.samples.size() < 2u)
        return;

    const uint32 maxStartIndex = static_cast<uint32>(parentPath.samples.size() - 2u);
    const uint32 maxSampleIndex = static_cast<uint32>(parentPath.samples.size() - 1u);
    const float startMin = clamp(_desc.branchStartMin, 0.f, 1.f);
    const float startMax = clamp(_desc.branchStartMax, startMin, 1.f);
    const uint32 baseSeed = _desc.seed + _desc.branchSeedOffset + (_loopIndex + 1u) * 9781u + stripIndex * 15485863u;
    const Vec3 parentEnd = parentPath.samples.back().position;
    Vec3 parentTangent = parentEnd - parentPath.samples.front().position;
    if (parentTangent.LengthSquared() <= 0.0001f)
        return;
    parentTangent.Normalize();

    Vec3 targetRight{};
    Vec3 targetUp{};
    EffectBeamRuntime::Resolve_PerpendicularAxes(parentTangent, targetRight, targetUp);
    if (targetRight.LengthSquared() <= 0.0001f || targetUp.LengthSquared() <= 0.0001f)
        return;

    for (uint32 branchIndex = 0u; branchIndex < _desc.branchCount; ++branchIndex)
    {
        const uint32 branchSeed = baseSeed + branchIndex * 32452843u;
        if (EffectBeamRuntime::Hash01(branchSeed + 3u) > _desc.branchChance)
            continue;

        const float startRatio = startMin + (startMax - startMin) * EffectBeamRuntime::Hash01(branchSeed + 7u);
        uint32 startIndex = static_cast<uint32>(roundf(startRatio * static_cast<float>(maxStartIndex)));
        startIndex = min(startIndex, maxStartIndex);

        const PathSample& startSample = parentPath.samples[startIndex];
        const PathSample& nextSample = parentPath.samples[startIndex + 1u];
        Vec3 tangent = nextSample.position - startSample.position;
        if (tangent.LengthSquared() <= 0.0001f && startIndex > 0u)
            tangent = startSample.position - parentPath.samples[startIndex - 1u].position;
        if (tangent.LengthSquared() <= 0.0001f)
            continue;
        tangent.Normalize();

        Vec3 right{};
        Vec3 up{};
        EffectBeamRuntime::Resolve_PerpendicularAxes(tangent, right, up);
        if (right.LengthSquared() <= 0.0001f || up.LengthSquared() <= 0.0001f)
            continue;

        const float directionAngle = EffectBeamRuntime::Hash01(branchSeed + 17u) * XM_2PI;
        const Vec3 lateralAxis = right * cosf(directionAngle) + up * sinf(directionAngle);

        Vec3 branchEnd{};
        Vec3 controlPoint{};
        switch (_desc.branchPreset)
        {
        case EffectBeamBranchPreset::DownStrike:
        {
            Vec3 downAxis = _transformCom != nullptr ? -_transformCom->Get_WorldUp() : Vec3{ 0.f, -1.f, 0.f };
            if (downAxis.LengthSquared() <= 0.0001f)
                downAxis = Vec3{ 0.f, -1.f, 0.f };
            downAxis.Normalize();

            Vec3 downRight{};
            Vec3 downUp{};
            EffectBeamRuntime::Resolve_PerpendicularAxes(downAxis, downRight, downUp);
            const float spreadAngle = EffectBeamRuntime::Hash01(branchSeed + 19u) * XM_2PI;
            const float spreadRadius = sqrtf(EffectBeamRuntime::Hash01(branchSeed + 23u)) * _desc.branchEndSpreadRadius;
            const Vec3 spreadOffset = (downRight * cosf(spreadAngle) + downUp * sinf(spreadAngle)) * spreadRadius;
            branchEnd = startSample.position + downAxis * _desc.branchDownLength + spreadOffset;
            controlPoint = Vec3::Lerp(startSample.position, branchEnd, 0.5f) + lateralAxis * _desc.branchCurveAmount;
            break;
        }
        case EffectBeamBranchPreset::Entangle:
        {
            const float signedAdvance = EffectBeamRuntime::Hash01(branchSeed + 19u) < 0.5f
                                        ? -_desc.branchEntangleAdvance
                                        : _desc.branchEntangleAdvance;
            const float targetRatio = clamp(startRatio + signedAdvance, 0.f, 1.f);
            uint32 targetIndex = static_cast<uint32>(roundf(targetRatio * static_cast<float>(maxSampleIndex)));
            targetIndex = min(targetIndex, maxSampleIndex);

            const float targetAngle = EffectBeamRuntime::Hash01(branchSeed + 23u) * XM_2PI;
            const Vec3 targetOffset = (right * cosf(targetAngle) + up * sinf(targetAngle)) * _desc.branchEntangleRadius;
            branchEnd = parentPath.samples[targetIndex].position + targetOffset;

            const float controlAngle = targetAngle + XM_PIDIV2;
            const Vec3 controlOffset =
                (right * cosf(controlAngle) + up * sinf(controlAngle)) * _desc.branchEntangleRadius * (0.5f + _desc.branchCurveAmount);
            controlPoint = Vec3::Lerp(startSample.position, branchEnd, 0.5f) + controlOffset;
            break;
        }
        case EffectBeamBranchPreset::ShortCrack:
        {
            const float crackLength =
                _desc.branchCrackLength + sqrtf(EffectBeamRuntime::Hash01(branchSeed + 23u)) * _desc.branchCrackSpreadRadius;
            branchEnd = startSample.position + lateralAxis * crackLength;
            controlPoint = Vec3::Lerp(startSample.position, branchEnd, 0.5f);
            break;
        }
        case EffectBeamBranchPreset::EndGuided:
        default:
        {
            const float spreadAngle = EffectBeamRuntime::Hash01(branchSeed + 19u) * XM_2PI;
            const float spreadRadius = sqrtf(EffectBeamRuntime::Hash01(branchSeed + 23u)) * _desc.branchEndSpreadRadius;
            const Vec3 spreadOffset = (targetRight * cosf(spreadAngle) + targetUp * sinf(spreadAngle)) * spreadRadius;
            branchEnd = parentEnd + spreadOffset;

            const float curveRatio = clamp(0.25f + _desc.branchCurveAmount * 0.5f, 0.05f, 0.95f);
            controlPoint = Vec3::Lerp(startSample.position, branchEnd, curveRatio) + lateralAxis * _desc.branchOutwardAmount;
            break;
        }
        }

        if ((branchEnd - startSample.position).LengthSquared() <= 0.0001f)
            continue;

        PathEntry branchPath{};
        branchPath.stripIndex = stripIndex;
        branchPath.widthScale = _desc.branchWidthScale;
        branchPath.samples.reserve(_desc.branchSegmentCount + 1u);

        Vec3 previousWorldPosition{};
        for (uint32 sampleIndex = 0u; sampleIndex <= _desc.branchSegmentCount; ++sampleIndex)
        {
            const float ratio = static_cast<float>(sampleIndex) / static_cast<float>(_desc.branchSegmentCount);
            const float inverseRatio = 1.f - ratio;
            Vec3 worldPosition =
                startSample.position * (inverseRatio * inverseRatio) +
                controlPoint * (2.f * inverseRatio * ratio) +
                branchEnd * (ratio * ratio);
            if (sampleIndex > 0u && sampleIndex < _desc.branchSegmentCount && _desc.noiseAmplitude > 0.f)
            {
                const float lateralX = EffectBeamRuntime::Hash01(branchSeed + sampleIndex * 6271u + 29u) * 2.f - 1.f;
                const float lateralY = EffectBeamRuntime::Hash01(branchSeed + sampleIndex * 6271u + 31u) * 2.f - 1.f;
                worldPosition += (right * lateralX + up * lateralY) * _desc.noiseAmplitude;
            }

            PathSample sample{};
            sample.position = worldPosition;
            sample.age = activeElapsedTime;
            if (!branchPath.samples.empty())
                sample.distance = branchPath.samples.back().distance + (worldPosition - previousWorldPosition).Length();
            branchPath.samples.push_back(sample);
            previousWorldPosition = worldPosition;
        }

        if (branchPath.samples.size() >= 2u)
            _pathEntries.push_back(branchPath);
    }
}

HRESULT ComputeGeneratedBeamEmitter::Append_SegmentPayloads(vector<SegmentPayload>& outPayloads)
{
    if (_pathEntries.empty())
        return S_OK;

    const uint32 maxSegmentCount = Compute_MaxRenderSegmentCount();
    if (outPayloads.size() >= maxSegmentCount)
        return S_OK;

    const float visualLife = Resolve_VisualLife();
    const float activeElapsedTime = clamp(Resolve_ActiveElapsedTime(Resolve_Duration()), 0.f, visualLife);
    const float lifeProgress = clamp(activeElapsedTime / visualLife, 0.f, 1.f);
    const Vec3 beamEnvelope = Evaluate_BeamEnvelopeOverLife(lifeProgress);
    const float startRatio = clamp(beamEnvelope.x, 0.f, 1.f);
    const float endRatio = clamp(beamEnvelope.y, 0.f, 1.f);
    const float visibleStartRatio = min(startRatio, endRatio);
    const float visibleEndRatio = max(startRatio, endRatio);
    const float visibleRatioLength = visibleEndRatio - visibleStartRatio;
    if (visibleRatioLength <= 0.0001f)
        return S_OK;

    const float baseWidth = max(0.001f, _desc.baseWidth * Evaluate_WidthScaleByLife(lifeProgress) * beamEnvelope.z);
    const Vec4 color = Evaluate_ColorOverLife(lifeProgress);
    float maxVisibleLength = 0.f;
    for (const PathEntry& pathEntry : _pathEntries)
    {
        if (pathEntry.samples.size() >= 2u)
            maxVisibleLength = max(maxVisibleLength, pathEntry.samples.back().distance * visibleRatioLength);
    }
    _visibleBeamLength = max(0.0001f, maxVisibleLength);

    for (const PathEntry& pathEntry : _pathEntries)
    {
        const vector<PathSample>& pathSamples = pathEntry.samples;
        if (pathSamples.size() < 2u || outPayloads.size() >= maxSegmentCount)
            continue;

        const float pathLength = pathSamples.back().distance;
        const float visibleStartDistance = pathLength * visibleStartRatio;
        const float visibleEndDistance = pathLength * visibleEndRatio;
        const float visiblePathLength = visibleEndDistance - visibleStartDistance;
        if (visiblePathLength <= 0.0001f)
            continue;

        const Vec4 subUVRect = Resolve_SubUVRect(Evaluate_SubUVFrameIndex(pathEntry.stripIndex));
        const float uvDistanceScale = _desc.tilingDistance > 0.f
                                      ? 1.f
                                      : _visibleBeamLength / max(visiblePathLength, 0.0001f);
        const uint32 segmentCount = min(
            static_cast<uint32>(pathSamples.size() - 1u),
            maxSegmentCount - static_cast<uint32>(outPayloads.size())
        );

        for (uint32 index = 0u; index < segmentCount; ++index)
        {
            const PathSample& previous = index > 0u ? pathSamples[index - 1u] : pathSamples[index];
            const PathSample& current = pathSamples[index];
            const PathSample& next = pathSamples[index + 1u];
            if (next.distance <= visibleStartDistance)
                continue;
            if (current.distance >= visibleEndDistance)
                break;

            PathSample clippedCurrent = current;
            bool clippedSegmentStart = false;
            if (current.distance < visibleStartDistance)
            {
                clippedCurrent = Interpolate_PathSample(current, next, visibleStartDistance);
                clippedSegmentStart = true;
            }

            PathSample clippedNext = next;
            bool clippedSegmentEnd = false;
            if (next.distance > visibleEndDistance)
            {
                clippedNext = Interpolate_PathSample(current, next, visibleEndDistance);
                clippedSegmentEnd = true;
            }

            const PathSample& previousSource = clippedSegmentStart ? clippedCurrent : previous;
            const PathSample& nextNextSource = index + 2u < pathSamples.size() ? pathSamples[index + 2u] : clippedNext;
            const PathSample& nextNext = clippedSegmentEnd ? clippedNext : nextNextSource;

            SegmentPayload payload{};
            payload.previousPosition =
                Vec4(previousSource.position.x, previousSource.position.y, previousSource.position.z, !clippedSegmentStart && index > 0u ? 1.f : 0.f);
            payload.currentPosition = Vec4(clippedCurrent.position.x, clippedCurrent.position.y, clippedCurrent.position.z, 1.f);
            payload.nextPosition = Vec4(clippedNext.position.x, clippedNext.position.y, clippedNext.position.z, 1.f);
            payload.nextNextPosition = Vec4(
                nextNext.position.x,
                nextNext.position.y,
                nextNext.position.z,
                !clippedSegmentEnd && index + 2u < pathSamples.size() ? 1.f : 0.f
            );
            payload.currentSampleParams =
                Vec4(clippedCurrent.age, visualLife, (clippedCurrent.distance - visibleStartDistance) * uvDistanceScale, baseWidth * pathEntry.widthScale);
            payload.nextSampleParams =
                Vec4(clippedNext.age, visualLife, (clippedNext.distance - visibleStartDistance) * uvDistanceScale, baseWidth * pathEntry.widthScale);
            payload.startColor = color;
            payload.endColor = color;
            payload.subUVRect = subUVRect;
            const Vec3 coreColorRgb = Sample_ParticleLifeCoreColorRgbUniformModulation(
                _desc.material.coreColorRgbModulation,
                Vec3{
                    _desc.material.coreEmissive.coreColor.x,
                    _desc.material.coreEmissive.coreColor.y,
                    _desc.material.coreEmissive.coreColor.z
                },
                _effectPlaybackSeed,
                pathEntry.stripIndex * 4099u + index
            );
            payload.coreColorRgb = Vec4(coreColorRgb.x, coreColorRgb.y, coreColorRgb.z, 0.f);
            outPayloads.push_back(payload);

            if (clippedSegmentEnd)
                break;
        }
    }

    return S_OK;
}

bool ComputeGeneratedBeamEmitter::Has_RenderablePath() const
{
    if (_visibleBeamLength <= 0.0001f)
        return false;

    for (const PathEntry& pathEntry : _pathEntries)
    {
        if (pathEntry.samples.size() >= 2u)
            return true;
    }

    return false;
}

HRESULT ComputeGeneratedBeamEmitter::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), Resolve_ShaderId(), _shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), kBeamComputeShaderId, _computeShader), E_FAIL);
    CHECK_FAILED(Ready_Texture(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTexture(), E_FAIL);
    CHECK_FAILED(Ready_MaskTexture(), E_FAIL);
    CHECK_FAILED(Ready_FlowTexture(), E_FAIL);
    CHECK_FAILED(Ready_DrawBuffers(), E_FAIL);
    CHECK_FAILED(Ready_ComputeBuffers(), E_FAIL);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Ready_Texture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = EffectBeamRuntime::Resolve_TexturePath(
        _desc.material.mainTextureGuid,
        _desc.material.mainTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
        resolvedPath = EffectBeamRuntime::Resolve_TexturePathByPath(kFallbackTexturePath);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Texture_Effect_DefaultTexture", _mainTexture), E_FAIL);
    if (!resolvedPath.empty())
        _mainTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);

    CHECK_NULL(_mainTexture, E_FAIL);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Ready_NoiseTexture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = EffectBeamRuntime::Resolve_TexturePath(
        _desc.material.noiseTextureGuid,
        _desc.material.noiseTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (!resolvedPath.empty())
        _noiseTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Ready_MaskTexture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = EffectBeamRuntime::Resolve_TexturePath(
        _desc.material.maskTextureGuid,
        _desc.material.maskTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (!resolvedPath.empty())
        _maskTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Ready_FlowTexture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = EffectBeamRuntime::Resolve_TexturePath(
        _desc.material.flowTextureGuid,
        _desc.material.flowTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
        resolvedPath = EffectBeamRuntime::Resolve_TexturePathByPath(kNeutralFlowTexturePath);
    if (!resolvedPath.empty())
        _flowTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Ready_DrawBuffers()
{
    const Vec3 point{};
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(Vec3);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = &point;
    CHECK_FAILED(_device->CreateBuffer(&vbDesc, &vbData, _pointVB.GetAddressOf()), E_FAIL);

    const uint16 index = 0;
    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = sizeof(uint16);
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = &index;
    CHECK_FAILED(_device->CreateBuffer(&ibDesc, &ibData, _indexBuffer.GetAddressOf()), E_FAIL);

    D3D11_BUFFER_DESC instanceDesc{};
    instanceDesc.ByteWidth = sizeof(BeamInstanceVertex) * Compute_MaxRenderSegmentCount();
    instanceDesc.Usage = D3D11_USAGE_DEFAULT;
    instanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    CHECK_FAILED(_device->CreateBuffer(&instanceDesc, nullptr, _instanceBuffer.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Ready_ComputeBuffers()
{
    const uint32 maxSegmentCount = Compute_MaxRenderSegmentCount();
    _sampleInput = ComputeStructuredBuffer::Create(_device, _context, sizeof(SegmentPayload), maxSegmentCount);
    CHECK_NULL(_sampleInput, E_FAIL);
    _computeOutput = ComputeStructuredBuffer::Create(_device, _context, sizeof(BeamInstanceVertex), maxSegmentCount);
    CHECK_NULL(_computeOutput, E_FAIL);
    _computeArgsOutput = ComputeStructuredBuffer::Create(_device, _context, sizeof(DrawIndexedInstancedIndirectArgs), 1);
    CHECK_NULL(_computeArgsOutput, E_FAIL);

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(BeamComputeParams);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CHECK_FAILED(_device->CreateBuffer(&cbDesc, nullptr, _computeConstantBuffer.GetAddressOf()), E_FAIL);

    D3D11_BUFFER_DESC argsDesc{};
    argsDesc.ByteWidth = sizeof(DrawIndexedInstancedIndirectArgs);
    argsDesc.Usage = D3D11_USAGE_DEFAULT;
    argsDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    argsDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    CHECK_FAILED(_device->CreateBuffer(&argsDesc, nullptr, _indirectArgsBuffer.GetAddressOf()), E_FAIL);
    return Reset_IndirectArgs();
}

HRESULT ComputeGeneratedBeamEmitter::Dispatch_Compute()
{
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);

    if (!Has_RenderablePath())
    {
        _drawSegmentCount = 0u;
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Update_ComputeInput(), E_FAIL);
    if (_drawSegmentCount == 0u)
    {
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Update_ComputeConstants(), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_SRV(0, _sampleInput->Get_SRV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()), E_FAIL);
    CHECK_FAILED(_computeShader->Dispatch((_drawSegmentCount + kThreadCountX - 1u) / kThreadCountX, 1, 1), E_FAIL);
    CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Update_ComputeInput()
{
    vector<SegmentPayload> payloads{};
    payloads.reserve(Compute_MaxRenderSegmentCount());
    CHECK_FAILED(Append_SegmentPayloads(payloads), E_FAIL);

    if (payloads.empty())
    {
        _drawSegmentCount = 0u;
        _visibleBeamLength = 1.f;
        return S_OK;
    }

    _drawSegmentCount = static_cast<uint32>(payloads.size());
    CHECK_FAILED(_sampleInput->Update_Data(payloads.data(), _drawSegmentCount), E_FAIL);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Update_ComputeConstants()
{
    BeamComputeParams params{};
    params.segmentCount = _drawSegmentCount;
    params.renderAxis = static_cast<uint32>(EffectRibbonRenderAxis::CameraUp);
    params.tilingDistance = _desc.tilingDistance;
    params.visibleLength = _visibleBeamLength;
    params.axisFallback = Vec4(1.f, 0.f, 0.f, 0.f);
    params.fadeParams = Vec4(0.f, 0.f, 0.f, 0.f);
    _context->UpdateSubresource(_computeConstantBuffer.Get(), 0, nullptr, &params, 0, 0);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Reset_IndirectArgs()
{
    const DrawIndexedInstancedIndirectArgs args{};
    CHECK_FAILED(_computeArgsOutput->Update_Data(&args, 1), E_FAIL);
    return Copy_ComputeOutput();
}

HRESULT ComputeGeneratedBeamEmitter::Copy_ComputeOutput()
{
    if (nullptr != _instanceBuffer && nullptr != _computeOutput)
        _context->CopyResource(_instanceBuffer.Get(), _computeOutput->Get_Buffer());
    if (nullptr != _indirectArgsBuffer && nullptr != _computeArgsOutput)
        _context->CopyResource(_indirectArgsBuffer.Get(), _computeArgsOutput->Get_Buffer());
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Bind_ShaderResources()
{
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

    if (Is_DistortionFamily())
    {
        const Vec4 effectBeamAxisParams = Vec4(static_cast<float>(static_cast<uint32>(EffectRibbonRenderAxis::CameraUp)), _visibleBeamLength, 0.f, 1.f);
        const Vec4 effectBeamFallbackAxis{ 1.f, 0.f, 0.f, 0.f };

        CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &_desc.material.tint, sizeof(_desc.material.tint)), E_FAIL);
        CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamAxisParams", &effectBeamAxisParams, sizeof(effectBeamAxisParams)), E_FAIL);
        CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamFallbackAxis", &effectBeamFallbackAxis, sizeof(effectBeamFallbackAxis)), E_FAIL);
        CHECK_FAILED(Bind_DistortionResources(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Bind_MaterialResources(), E_FAIL);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Bind_MaterialResources()
{
    const float emitterPhase = Compute_MaterialScalarModulationPhase(_materialElapsedTime, _desc.playback.duration);
    const EffectRequiredMaterialRuntimeDesc material =
        Resolve_EmitterTimeMaterialUniformParameterModulation(_desc.material, emitterPhase, _effectPlaybackSeed);

    CHECK_FAILED(_mainTexture->Bind_ShaderResourceView(_shader.get(), "g_MainTexture", 0), E_FAIL);

    if (nullptr != _noiseTexture)
        CHECK_FAILED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);

    if (nullptr != _maskTexture)
        CHECK_FAILED(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

    CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);

    const Vec4 effectBeamParams = Vec4(material.intensity, max(material.opacityPower, 0.0001f), material.noiseStrength, nullptr != _noiseTexture ? 1.f : 0.f);
    const Vec4 effectBeamAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        nullptr != _maskTexture ? 1.f : 0.f,
        clamp(material.alphaMultiplier, 0.f, 1.f)
    );
    const Vec4 effectBeamMainUVParams = Vec4(material.mainUVScale.x, material.mainUVScale.y, material.mainUVScrollSpeed.x, material.mainUVScrollSpeed.y);
    const Vec4 effectBeamNoiseUVParams = Vec4(material.noiseUVScale.x, material.noiseUVScale.y, material.noiseUVScrollSpeed.x, material.noiseUVScrollSpeed.y);
    const Vec4 effectBeamMaskUVParams = Vec4(material.maskUVScale.x, material.maskUVScale.y, material.maskUVScrollSpeed.x, material.maskUVScrollSpeed.y);
    const Vec4 effectBeamUVOffsetParams = Vec4(material.mainUVOffset.x, material.mainUVOffset.y, material.noiseUVOffset.x, material.noiseUVOffset.y);
    const Vec4 effectBeamMaskUVOffsetParams = Vec4(material.maskUVOffset.x, material.maskUVOffset.y, 0.f, 0.f);
    const Vec4 effectBeamUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
        0.f
    );
    const Vec4 effectBeamUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.vPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.vPolicy))
    );
    const Vec4 effectBeamMaskUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.vPolicy)),
        0.f,
        0.f
    );
    const Vec4 effectBeamUVRotationParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVRotation)),
        static_cast<float>(static_cast<uint32>(material.noiseUVRotation)),
        static_cast<float>(static_cast<uint32>(material.maskUVRotation)),
        0.f
    );
    const Vec4 effectBeamSourceParams = Vec4(
        static_cast<float>(EffectBeamRuntime::Resolve_MaterialSourceIndex(material.noiseSource)),
        static_cast<float>(EffectBeamRuntime::Resolve_MaterialSourceIndex(material.maskSource)),
        material.noiseInvert ? 1.f : 0.f,
        material.maskInvert ? 1.f : 0.f
    );
    const Vec4 effectBeamAdditiveParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.additive.colorSource)),
        static_cast<float>(static_cast<uint32>(material.additive.amountSource)),
        static_cast<float>(static_cast<uint32>(material.additive.coveragePolicy)),
        material.additive.intensityScale
    );
    const Vec4 effectBeamAdditiveFlags = Vec4(material.additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
    const Vec4 effectBeamCoreEmissiveParams = Vec4(
        material.coreEmissive.enabled ? 1.f : 0.f,
        material.coreEmissive.corePower,
        material.coreEmissive.coreIntensity,
        material.coreEmissive.outerPower
    );
    const Vec4 effectBeamCoreEmissiveColor = Vec4(
        material.coreEmissive.coreColor.x,
        material.coreEmissive.coreColor.y,
        material.coreEmissive.coreColor.z,
        material.coreEmissive.outerIntensity
    );
    const Vec4 effectBeamAxisParams = Vec4(static_cast<float>(static_cast<uint32>(EffectRibbonRenderAxis::CameraUp)), _visibleBeamLength, 0.f, 1.f);
    const Vec4 effectBeamFallbackAxis{ 1.f, 0.f, 0.f, 0.f };
    const int opacitySourceIndex = Resolve_OpacitySourceIndex();

    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamParams", &effectBeamParams, sizeof(effectBeamParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamAlphaParams", &effectBeamAlphaParams, sizeof(effectBeamAlphaParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamMainUVParams", &effectBeamMainUVParams, sizeof(effectBeamMainUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamNoiseUVParams", &effectBeamNoiseUVParams, sizeof(effectBeamNoiseUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamMaskUVParams", &effectBeamMaskUVParams, sizeof(effectBeamMaskUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamUVOffsetParams", &effectBeamUVOffsetParams, sizeof(effectBeamUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamMaskUVOffsetParams", &effectBeamMaskUVOffsetParams, sizeof(effectBeamMaskUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamUVModeParams", &effectBeamUVModeParams, sizeof(effectBeamUVModeParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamUVAxisPolicyParams", &effectBeamUVAxisPolicyParams, sizeof(effectBeamUVAxisPolicyParams)), E_FAIL);
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectBeamMaskUVAxisPolicyParams", &effectBeamMaskUVAxisPolicyParams, sizeof(effectBeamMaskUVAxisPolicyParams)),
        E_FAIL
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamUVRotationParams", &effectBeamUVRotationParams, sizeof(effectBeamUVRotationParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamSourceParams", &effectBeamSourceParams, sizeof(effectBeamSourceParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamAdditiveParams", &effectBeamAdditiveParams, sizeof(effectBeamAdditiveParams)), E_FAIL);
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectBeamAdditiveEmissiveColor", &material.additive.emissiveColor, sizeof(material.additive.emissiveColor)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectBeamAdditiveConstantColor", &material.additive.constantColor, sizeof(material.additive.constantColor)),
        E_FAIL
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamAdditiveFlags", &effectBeamAdditiveFlags, sizeof(effectBeamAdditiveFlags)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamCoreEmissiveParams", &effectBeamCoreEmissiveParams, sizeof(effectBeamCoreEmissiveParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamCoreEmissiveColor", &effectBeamCoreEmissiveColor, sizeof(effectBeamCoreEmissiveColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamAxisParams", &effectBeamAxisParams, sizeof(effectBeamAxisParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamFallbackAxis", &effectBeamFallbackAxis, sizeof(effectBeamFallbackAxis)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamMaterialTime", &_materialElapsedTime, sizeof(_materialElapsedTime)), E_FAIL);
    CHECK_FAILED(EffectBeamRuntime::Bind_MaterialScalarModulationPayload(_shader.get(), _desc.material.scalarModulation, emitterPhase), E_FAIL);
    CHECK_FAILED(Bind_CoreColorRgbModulationShaderPayload(_shader.get(), _desc.material.coreColorRgbModulation, emitterPhase, _effectPlaybackSeed), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_OpacitySource", &opacitySourceIndex, sizeof(opacitySourceIndex)), E_FAIL);
    return S_OK;
}

HRESULT ComputeGeneratedBeamEmitter::Bind_DistortionResources()
{
    const float emitterPhase = Compute_MaterialScalarModulationPhase(_materialElapsedTime, _desc.playback.duration);
    const EffectRequiredMaterialRuntimeDesc material =
        Resolve_EmitterTimeMaterialUniformParameterModulation(_desc.material, emitterPhase, _effectPlaybackSeed);

    CHECK_FAILED(_mainTexture->Bind_ShaderResourceView(_shader.get(), "g_MainTexture", 0), E_FAIL);

    if (nullptr != _maskTexture)
        CHECK_FAILED(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

    if (nullptr != _flowTexture)
        CHECK_FAILED(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_FlowTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_FlowTexture", nullptr), E_FAIL);

    D3D11_VIEWPORT viewport{};
    uint32 viewportCount = 1u;
    _context->RSGetViewports(&viewportCount, &viewport);

    const Vec4 distortionParams(
        material.refractionIntensity,
        max(0.f, material.refractionPresence),
        nullptr != _flowTexture ? 1.f : 0.f,
        _materialElapsedTime
    );
    const Vec4 effectBeamAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        nullptr != _maskTexture ? 1.f : 0.f,
        max(material.opacityPower, 0.0001f)
    );
    const Vec4 effectBeamMainUVParams = Vec4(
        material.mainUVScale.x,
        material.mainUVScale.y,
        material.mainUVScrollSpeed.x,
        material.mainUVScrollSpeed.y
    );
    const Vec4 effectBeamMaskUVParams = Vec4(
        material.maskUVScale.x,
        material.maskUVScale.y,
        material.maskUVScrollSpeed.x,
        material.maskUVScrollSpeed.y
    );
    const Vec4 effectBeamUVOffsetParams = Vec4(
        material.mainUVOffset.x,
        material.mainUVOffset.y,
        material.maskUVOffset.x,
        material.maskUVOffset.y
    );
    const Vec4 effectBeamUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.vPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.vPolicy))
    );
    const Vec4 effectBeamUVRotationParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVRotation)),
        static_cast<float>(static_cast<uint32>(material.maskUVRotation)),
        0.f,
        0.f
    );
    const Vec4 effectBeamSourceParams = Vec4(
        static_cast<float>(Resolve_OpacitySourceIndex()),
        static_cast<float>(EffectBeamRuntime::Resolve_MaterialSourceIndex(material.maskSource)),
        material.maskInvert ? 1.f : 0.f,
        0.f
    );
    const Vec4 distortionScreenSize(max(1.f, viewport.Width), max(1.f, viewport.Height), 0.f, 0.f);
    const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(material.flowUVTilingMode));
    const float flowUVRotation = static_cast<float>(static_cast<uint32>(material.flowUVRotation));
    const Vec2 flowUVPolicyParams(
        static_cast<float>(static_cast<uint32>(material.flowUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.flowUVPolicy.vPolicy))
    );

    CHECK_FAILED(_shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamAlphaParams", &effectBeamAlphaParams, sizeof(effectBeamAlphaParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamMainUVParams", &effectBeamMainUVParams, sizeof(effectBeamMainUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamMaskUVParams", &effectBeamMaskUVParams, sizeof(effectBeamMaskUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamUVOffsetParams", &effectBeamUVOffsetParams, sizeof(effectBeamUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamUVAxisPolicyParams", &effectBeamUVAxisPolicyParams, sizeof(effectBeamUVAxisPolicyParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamUVRotationParams", &effectBeamUVRotationParams, sizeof(effectBeamUVRotationParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectBeamSourceParams", &effectBeamSourceParams, sizeof(effectBeamSourceParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_DistortionScreenSize", &distortionScreenSize, sizeof(distortionScreenSize)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVPolicyParams", &flowUVPolicyParams, sizeof(flowUVPolicyParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVScale", &material.flowUVScale, sizeof(material.flowUVScale)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVOffset", &material.flowUVOffset, sizeof(material.flowUVOffset)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVScrollSpeed", &material.flowUVScrollSpeed, sizeof(material.flowUVScrollSpeed)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVRotation", &flowUVRotation, sizeof(flowUVRotation)), E_FAIL);
    CHECK_FAILED(EffectBeamRuntime::Bind_MaterialScalarModulationPayload(_shader.get(), material.scalarModulation, emitterPhase), E_FAIL);
    return S_OK;
}

bool ComputeGeneratedBeamEmitter::Can_SubmitRender() const
{
    return _drawSegmentCount > 0u && Is_Visible() && nullptr != _shader &&
           nullptr != _pointVB && nullptr != _indexBuffer && nullptr != _instanceBuffer && nullptr != _indirectArgsBuffer;
}

uint32 ComputeGeneratedBeamEmitter::Compute_MaxRenderSegmentCount() const
{
    const uint32 parentSegmentCount = max(1u, _desc.segmentCount) * max(1u, _desc.stripCount);
    if (!_desc.branchEnabled || _desc.branchCount == 0u)
        return parentSegmentCount;

    return parentSegmentCount + max(1u, _desc.stripCount) * _desc.branchCount * max(1u, _desc.branchSegmentCount);
}

void ComputeGeneratedBeamEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _playbackState = PlaybackState::Delayed;
    _pathEntries.clear();
    _visibleBeamLength = 1.f;
    _drawSegmentCount = 0u;
    Resample_VisualLife();
    Resample_RandomSubUVFrameIndex();
}

void ComputeGeneratedBeamEmitter::Resample_VisualLife()
{
    if (!_desc.useLifetimeVisualLife)
    {
        _sampledVisualLife = Resolve_Duration();
        return;
    }

    const float lifeMin = max(0.0001f, min(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    if (_desc.lifetime.lifeTimeCurve.enabled)
    {
        float sequencePhase = 0.f;
        if (_desc.playback.loopCount > 0u)
        {
            const uint32 lastLoopIndex = max(1u, _desc.playback.loopCount) - 1u;
            sequencePhase = lastLoopIndex == 0u
                            ? 0.f
                            : static_cast<float>(min(_loopIndex, lastLoopIndex)) / static_cast<float>(lastLoopIndex);
        }
        else
            sequencePhase = static_cast<float>(_loopIndex % 2u);

        _sampledVisualLife = max(0.0001f, EffectBeamRuntime::Evaluate_FloatCurve(_desc.lifetime.lifeTimeCurve, sequencePhase, lifeMax));
        return;
    }

    if (lifeMin == lifeMax)
    {
        _sampledVisualLife = lifeMin;
        return;
    }

    const uint32 seed =
        (_loopIndex + 1u) * 9781u +
        EffectBeamRuntime::Resolve_SeedSalt(_desc.lifetimeSeed, _effectPlaybackSeed) +
        _desc.seed * 6271u +
        43u;
    _sampledVisualLife = lerp(lifeMin, lifeMax, EffectBeamRuntime::Hash01(seed));
}

float ComputeGeneratedBeamEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.playback.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.playback.delay);
}

float ComputeGeneratedBeamEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.playback.duration);
}

float ComputeGeneratedBeamEmitter::Resolve_VisualLife() const
{
    if (!_desc.useLifetimeVisualLife)
        return Resolve_Duration();

    return max(0.0001f, _sampledVisualLife);
}

float ComputeGeneratedBeamEmitter::Resolve_ActiveElapsedTime(float duration) const
{
    return clamp(_loopElapsedTime - Resolve_CurrentLoopDelay(), 0.f, duration);
}

bool ComputeGeneratedBeamEmitter::Has_NextLoop() const
{
    return 0u == _desc.playback.loopCount || _loopIndex + 1u < _desc.playback.loopCount;
}

bool ComputeGeneratedBeamEmitter::Is_DistortionFamily() const
{
    return _desc.material.materialFamily == EffectMaterialFamily::SpriteDistortion;
}

RenderGroup ComputeGeneratedBeamEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    return Is_DistortionFamily() ? RenderGroup::Distortion : RenderGroup::Blend;
}

const wchar_t* ComputeGeneratedBeamEmitter::Resolve_ShaderId() const
{
    return Is_DistortionFamily() ? kBeamDistortionShaderId : kBeamShaderId;
}

uint32 ComputeGeneratedBeamEmitter::Resolve_ShaderPassIndex() const
{
    if (Is_DistortionFamily())
        return 0u;

    return _desc.material.blendMode == EffectMaterialBlendMode::Additive ? 1u : 0u;
}

int ComputeGeneratedBeamEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.material.opacitySource == "Red" || _desc.material.opacitySource == "red")
        return 1;

    if (_desc.material.opacitySource == "Luminance" || _desc.material.opacitySource == "luminance")
        return 2;

    return 0;
}

Vec4 ComputeGeneratedBeamEmitter::Evaluate_ColorOverLife(float lifeProgress) const
{
    const Vec4 startColor = EffectBeamRuntime::Average_Color(_desc.initialColor.startColorMin, _desc.initialColor.startColorMax);
    const Vec4 endColor = EffectBeamRuntime::Average_Color(_desc.colorOverLife.endColorMin, _desc.colorOverLife.endColorMax);
    Vec4 color{
        lerp(startColor.x, endColor.x, lifeProgress),
        lerp(startColor.y, endColor.y, lifeProgress),
        lerp(startColor.z, endColor.z, lifeProgress),
        lerp(startColor.w, endColor.w, lifeProgress)
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

float ComputeGeneratedBeamEmitter::Evaluate_WidthScaleByLife(float lifeProgress) const
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

Vec3 ComputeGeneratedBeamEmitter::Evaluate_BeamEnvelopeOverLife(float lifeProgress) const
{
    if (!_desc.beamEnvelopeOverLife.enabled)
        return Vec3{ 0.f, 1.f, 1.f };

    const auto evaluateFloatCurve = [this](const PointParticleFloatCurveRuntimeDesc& curve, float fallbackValue, float lifeProgress)
    {
        if (!curve.enabled)
            return fallbackValue;

        return Evaluate_CompactCurve(
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
            fallbackValue
        );
    };

    const PointParticleFloatCurveRuntimeDesc& startCurve = _desc.beamEnvelopeOverLife.startRatioOverLife;
    const float startRatio = evaluateFloatCurve(startCurve, _desc.beamEnvelopeOverLife.startRatioFallback, lifeProgress);

    const PointParticleFloatCurveRuntimeDesc& endCurve = _desc.beamEnvelopeOverLife.endRatioOverLife;
    const float endRatio = evaluateFloatCurve(endCurve, _desc.beamEnvelopeOverLife.endRatioFallback, lifeProgress);

    const PointParticleFloatCurveRuntimeDesc& widthCurve = _desc.beamEnvelopeOverLife.widthScaleOverLife;
    const float widthScale = evaluateFloatCurve(widthCurve, _desc.beamEnvelopeOverLife.widthScaleFallback, lifeProgress);

    return Vec3{ clamp(startRatio, 0.f, 1.f), clamp(endRatio, 0.f, 1.f), max(0.f, widthScale) };
}

ComputeGeneratedBeamEmitter::PathSample ComputeGeneratedBeamEmitter::Interpolate_PathSample(
    const PathSample& left,
    const PathSample& right,
    float targetDistance)
{
    const float segmentDistance = max(0.0001f, right.distance - left.distance);
    const float ratio = clamp((targetDistance - left.distance) / segmentDistance, 0.f, 1.f);

    PathSample sample{};
    sample.position = Vec3::Lerp(left.position, right.position, ratio);
    sample.distance = targetDistance;
    sample.age = lerp(left.age, right.age, ratio);
    return sample;
}

uint32 ComputeGeneratedBeamEmitter::Evaluate_SubUVFrameIndex(uint32 stripIndex) const
{
    const uint32 rows = max(1u, _desc.material.subUVRows);
    const uint32 cols = max(1u, _desc.material.subUVCols);
    const uint32 frameCount = max(1u, rows * cols);
    if (!_desc.subUVFrameOverLife.enabled)
        return 0u;

    const EffectBeamRuntime::SubUVFrameRange frameRange = EffectBeamRuntime::Resolve_SubUVFrameRange(
        _desc.subUVFrameOverLife.startFrame,
        _desc.subUVFrameOverLife.endFrame,
        frameCount
    );
    const float activeElapsedTime = max(0.f, _loopElapsedTime - Resolve_CurrentLoopDelay());
    switch (_desc.subUVFrameOverLife.playbackMode)
    {
    case SubUVFramePlaybackMode::LifeProgress:
    {
        const float lifeProgress = clamp(activeElapsedTime / Resolve_VisualLife(), 0.f, 1.f);
        const uint32 frameOffset = min(
            static_cast<uint32>(floorf(lifeProgress * static_cast<float>(frameRange.rangeCount))),
            frameRange.rangeCount - 1u
        );
        return EffectBeamRuntime::Resolve_SubUVFrameInRange(frameRange, frameOffset);
    }
    case SubUVFramePlaybackMode::FramesPerSecond:
    {
        const uint32 frameOffset = static_cast<uint32>(floorf(activeElapsedTime * max(0.f, _desc.subUVFrameOverLife.framesPerSecond)));
        const uint32 phaseOffset =
            _desc.subUVFrameOverLife.randomStartPhase
            ? Resolve_FpsSubUVFramePhaseOffset(stripIndex, frameRange.rangeCount)
            : 0u;
        return EffectBeamRuntime::Resolve_SubUVFrameInRange(
            frameRange,
            EffectBeamRuntime::Resolve_SubUVPlaybackOffset(frameRange, frameOffset, phaseOffset, _desc.subUVFrameOverLife.loop)
        );
    }
    case SubUVFramePlaybackMode::RandomFrame:
        return !_desc.usePerStripSubUVVariation || stripIndex == 0u
               ? min(_randomSubUVFrameIndex, frameRange.frameCount - 1u)
               : Resolve_RandomSubUVFrameIndex(stripIndex);
    case SubUVFramePlaybackMode::FixedFrame:
    default:
        return frameRange.startFrame;
    }
}

Vec4 ComputeGeneratedBeamEmitter::Resolve_SubUVRect(uint32 frameIndex) const
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

uint32 ComputeGeneratedBeamEmitter::Resolve_RandomSubUVFrameIndex(uint32 stripIndex) const
{
    const uint32 rows = max(1u, _desc.material.subUVRows);
    const uint32 cols = max(1u, _desc.material.subUVCols);
    const uint32 frameCount = max(1u, rows * cols);
    const EffectBeamRuntime::SubUVFrameRange frameRange = EffectBeamRuntime::Resolve_SubUVFrameRange(
        _desc.subUVFrameOverLife.startFrame,
        _desc.subUVFrameOverLife.endFrame,
        frameCount
    );
    const uint32 stripSeedSalt = stripIndex * 15641u;
    const uint32 seed =
        (_loopIndex + 1u) * 9781u +
        EffectBeamRuntime::Resolve_SeedSalt(_desc.subUVRandomFrameSeed, _effectPlaybackSeed) +
        stripSeedSalt +
        1201u;
    const uint32 frameOffset =
        min(static_cast<uint32>(floorf(EffectBeamRuntime::Hash01(seed) * static_cast<float>(frameRange.rangeCount))), frameRange.rangeCount - 1u);
    return EffectBeamRuntime::Resolve_SubUVFrameInRange(frameRange, frameOffset);
}

uint32 ComputeGeneratedBeamEmitter::Resolve_FpsSubUVFramePhaseOffset(uint32 stripIndex, uint32 rangeCount) const
{
    if (!_desc.subUVFrameOverLife.randomStartPhase ||
        _desc.stripCount <= 1u ||
        stripIndex == 0u ||
        rangeCount <= 1u)
        return 0u;

    const uint32 seed =
        (_loopIndex + 1u) * 9781u +
        EffectBeamRuntime::Resolve_SeedSalt(_desc.subUVRandomFrameSeed, _effectPlaybackSeed) +
        stripIndex * 15641u +
        3919u;
    return min(static_cast<uint32>(floorf(EffectBeamRuntime::Hash01(seed) * static_cast<float>(rangeCount))), rangeCount - 1u);
}

void ComputeGeneratedBeamEmitter::Resample_RandomSubUVFrameIndex()
{
    _randomSubUVFrameIndex = Resolve_RandomSubUVFrameIndex(0u);
}

float ComputeGeneratedBeamEmitter::Evaluate_CompactCurve(
    float lifeProgress,
    const Vec4& times,
    const Vec4& timesBlock1,
    const Vec4& values,
    const Vec4& valuesBlock1,
    uint32 keyCount,
    float fallbackValue) const
{
    const uint32 clampedKeyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= EffectBeamRuntime::Read_Vec4Component(times, timesBlock1, 0u))
        return EffectBeamRuntime::Read_Vec4Component(values, valuesBlock1, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = EffectBeamRuntime::Read_Vec4Component(times, timesBlock1, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = EffectBeamRuntime::Read_Vec4Component(times, timesBlock1, index - 1u);
        const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
        return lerp(
            EffectBeamRuntime::Read_Vec4Component(values, valuesBlock1, index - 1u),
            EffectBeamRuntime::Read_Vec4Component(values, valuesBlock1, index),
            ratio
        );
    }

    return lifeProgress > EffectBeamRuntime::Read_Vec4Component(times, timesBlock1, clampedKeyCount - 1u)
           ? EffectBeamRuntime::Read_Vec4Component(values, valuesBlock1, clampedKeyCount - 1u)
           : fallbackValue;
}

float ComputeGeneratedBeamEmitter::Evaluate_CompactCurve(
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
    if (clampedKeyCount == 1u || lifeProgress <= EffectBeamRuntime::Read_Vec4Component(times, timesBlock1, 0u))
        return EffectBeamRuntime::Read_Vec4Component(values, valuesBlock1, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = EffectBeamRuntime::Read_Vec4Component(times, timesBlock1, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = EffectBeamRuntime::Read_Vec4Component(times, timesBlock1, index - 1u);
        const float leftValue = EffectBeamRuntime::Read_Vec4Component(values, valuesBlock1, index - 1u);
        const float rightValue = EffectBeamRuntime::Read_Vec4Component(values, valuesBlock1, index);
        const float mode = EffectBeamRuntime::Read_Vec4Component(modes, modesBlock1, index - 1u);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = clamp((lifeProgress - leftTime) / width, 0.f, 1.f);

        if (mode < 0.5f)
            return leftValue;

        if (mode >= 1.5f)
        {
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            const float leftLeave = EffectBeamRuntime::Read_Vec4Component(leaveTangents, leaveTangentsBlock1, index - 1u) * width;
            const float rightArrive = EffectBeamRuntime::Read_Vec4Component(arriveTangents, arriveTangentsBlock1, index) * width;
            return
                (2.f * t3 - 3.f * t2 + 1.f) * leftValue +
                (t3 - 2.f * t2 + ratio) * leftLeave +
                (-2.f * t3 + 3.f * t2) * rightValue +
                (t3 - t2) * rightArrive;
        }

        return lerp(leftValue, rightValue, ratio);
    }

    return lifeProgress > EffectBeamRuntime::Read_Vec4Component(times, timesBlock1, clampedKeyCount - 1u)
           ? EffectBeamRuntime::Read_Vec4Component(values, valuesBlock1, clampedKeyCount - 1u)
           : fallbackValue;
}

Shared<ComputeGeneratedBeamEmitter> ComputeGeneratedBeamEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeGeneratedBeamEmitter>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeGeneratedBeamEmitter");
        MSG_BOX("Failed to Create : ComputeGeneratedBeamEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputeGeneratedBeamEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeGeneratedBeamEmitter>(*this);
    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeGeneratedBeamEmitter");
        MSG_BOX("Failed to Clone : ComputeGeneratedBeamEmitter");
        return nullptr;
    }

    return instance;
}

void ComputeGeneratedBeamEmitter::Free()
{
    __super::Free();
}

NS_END
