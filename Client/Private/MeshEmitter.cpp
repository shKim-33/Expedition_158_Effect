#include "MeshEmitter.h"

#include "Camera.h"
#include "EffectMaterialScalarModulationRuntime.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "TransformCom.h"

NS_BEGIN(Client)

namespace MeshEmitterDetail
{
constexpr double kMeshEmitterInitializeInfoThresholdMs = 8.0;
constexpr double kMeshEmitterReloadReuseInfoThresholdMs = 4.0;
constexpr float kPlaybackCompletionEpsilon = 1e-4f;
constexpr float kPlaneRadialEpsilon = 1e-4f;
constexpr float kAngularDeltaEpsilon = 1e-6f;
constexpr float kMeshDirectionAlignEpsilon = 1e-4f;

float Read_Vec4Component(const Vec4& values, uint32 index)
{
    switch (index)
    {
    case 0: return values.x;
    case 1: return values.y;
    case 2: return values.z;
    case 3: return values.w;
    default: return values.x;
    }
}

float Read_Vec4Component(const Vec4& valuesBlock0, const Vec4& valuesBlock1, uint32 index)
{
    if (index < 4u)
        return Read_Vec4Component(valuesBlock0, index);

    return Read_Vec4Component(valuesBlock1, index - 4u);
}

float Evaluate_FloatCurve(const PointParticleFloatCurveRuntimeDesc& curve, float phase, float fallbackValue)
{
    if (!curve.enabled)
        return fallbackValue;

    const uint32 keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, curve.curveKeyCount));
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

bool Has_NonZeroVector(const Vec3& value)
{
    return fabsf(value.x) > 0.0001f || fabsf(value.y) > 0.0001f || fabsf(value.z) > 0.0001f;
}

uint32 Resolve_SeedSalt(const PointParticleRandomSeedRuntimeDesc& seed, uint32 effectPlaybackSeed)
{
    uint32 salt = seed.manualSeedEnabled ? seed.seed : 0u;
    if (seed.useInstanceSeed)
        salt += effectPlaybackSeed;
    return salt;
}

float Hash01(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}

Vec3 Resolve_WorldScale(const Shared<TransformCom>& transform)
{
    if (transform == nullptr)
        return Vec3::One;

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;
    Matrix worldMatrix = transform->Get_WorldMatrix();
    if (!worldMatrix.Decompose(scale, rotation, translation))
        return Vec3::One;

    return Vec3{
        max(0.0001f, fabsf(scale.x)),
        max(0.0001f, fabsf(scale.y)),
        max(0.0001f, fabsf(scale.z))
    };
}

Vec3 Resolve_RuntimeWorldScale(const Weak<GameObject>& effectOwner, const Shared<TransformCom>& emitterTransform)
{
    const Shared<GameObject> owner = effectOwner.lock();
    const Shared<TransformCom> ownerTransform = owner != nullptr ? owner->Get_Transform() : nullptr;
    if (ownerTransform != nullptr)
        return Resolve_WorldScale(ownerTransform);

    return Resolve_WorldScale(emitterTransform);
}

Vec3 Scale_Vector3(const Vec3& value, const Vec3& scale)
{
    return Vec3{ value.x * scale.x, value.y * scale.y, value.z * scale.z };
}

Vec2 Scale_Vector2(const Vec2& value, const Vec2& scale)
{
    return Vec2{ value.x * scale.x, value.y * scale.y };
}

Vec2 Resolve_PlaneScale(PointParticlePlaneRadialLocationPlane plane, const Vec3& scale)
{
    switch (plane)
    {
    case PointParticlePlaneRadialLocationPlane::XZ:
        return Vec2{ scale.x, scale.z };
    case PointParticlePlaneRadialLocationPlane::YZ:
        return Vec2{ scale.y, scale.z };
    case PointParticlePlaneRadialLocationPlane::CameraFacing:
    case PointParticlePlaneRadialLocationPlane::XY:
    default:
        return Vec2{ scale.x, scale.y };
    }
}

Vec2 Resolve_CylinderScale(PointParticleCylinderLocationAxis axis, const Vec3& scale)
{
    switch (axis)
    {
    case PointParticleCylinderLocationAxis::LocalX:
        return Vec2{ max(scale.y, scale.z), scale.x };
    case PointParticleCylinderLocationAxis::LocalY:
        return Vec2{ max(scale.x, scale.z), scale.y };
    case PointParticleCylinderLocationAxis::LocalLook:
    default:
        return Vec2{ max(scale.x, scale.y), scale.z };
    }
}

float Resolve_EvenByParticleIndexAngleRatio(uint32 particleIndex, uint32 maxActiveCount, float angleMinDegrees, float angleMaxDegrees)
{
    const uint32 clampedCount = max(1u, maxActiveCount);
    if (clampedCount <= 1u)
        return 0.f;

    const float angleSpanDegrees = fabsf(angleMaxDegrees - angleMinDegrees);
    const bool isFullCircle = fabsf(angleSpanDegrees - 360.f) <= kPlaneRadialEpsilon;
    if (isFullCircle)
        return static_cast<float>(particleIndex) / static_cast<float>(clampedCount);

    return static_cast<float>(particleIndex) / static_cast<float>(clampedCount - 1u);
}

Vec3 Resolve_EvenByParticleIndexSphereDirection(uint32 placementIndex, uint32 placementCount)
{
    const uint32 clampedCount = max(1u, placementCount);
    if (clampedCount <= 1u)
        return Vec3{ 0.f, 0.f, 1.f };

    const uint32 clampedIndex = placementIndex % clampedCount;
    const float ratio = (static_cast<float>(clampedIndex) + 0.5f) / static_cast<float>(clampedCount);
    const float z = 1.f - 2.f * ratio;
    const float radiusOnPlane = sqrtf(max(0.f, 1.f - z * z));
    const float goldenAngle = XM_PI * (3.f - sqrtf(5.f));
    const float angle = static_cast<float>(clampedIndex) * goldenAngle;

    return Vec3{
        cosf(angle) * radiusOnPlane,
        sinf(angle) * radiusOnPlane,
        z
    };
}

Vec3 Normalize_OrFallback(const Vec3& value, const Vec3& fallback)
{
    if (value.LengthSquared() <= kPlaneRadialEpsilon * kPlaneRadialEpsilon)
        return fallback;

    Vec3 result = value;
    result.Normalize();
    return result;
}

float Apply_MeshDirectionAlignBlendMode(float weight, EffectMeshDirectionAlignBlendMode blendMode)
{
    const float clampedWeight = clamp(weight, 0.f, 1.f);

    switch (blendMode)
    {
    case EffectMeshDirectionAlignBlendMode::EaseIn:
        return clampedWeight * clampedWeight;
    case EffectMeshDirectionAlignBlendMode::EaseOut:
        return 1.f - (1.f - clampedWeight) * (1.f - clampedWeight);
    case EffectMeshDirectionAlignBlendMode::EaseInOut:
        return clampedWeight * clampedWeight * (3.f - 2.f * clampedWeight);
    case EffectMeshDirectionAlignBlendMode::Linear:
    default:
        return clampedWeight;
    }
}

Vec3 Sample_ConeDirection(const Vec3& axis, float angleDegrees, uint32 seed)
{
    const Vec3 normalizedAxis = Normalize_OrFallback(axis, Vec3{ 0.f, 0.f, 1.f });
    const float clampedAngleDegrees = clamp(angleDegrees, 0.f, 180.f);
    if (clampedAngleDegrees <= 0.0001f)
        return normalizedAxis;

    const float cosMax = cosf(XMConvertToRadians(clampedAngleDegrees));
    const float cosTheta = lerp(1.f, cosMax, Hash01(seed + 1703u));
    const float sinTheta = sqrtf(max(0.f, 1.f - cosTheta * cosTheta));
    const float phi = XM_2PI * Hash01(seed + 1721u);
    const Vec3 helper = fabsf(normalizedAxis.y) < 0.999f ? Vec3{ 0.f, 1.f, 0.f } : Vec3{ 1.f, 0.f, 0.f };
    Vec3 tangent = helper.Cross(normalizedAxis);
    tangent.Normalize();
    const Vec3 bitangent = normalizedAxis.Cross(tangent);

    return normalizedAxis * cosTheta + (tangent * cosf(phi) + bitangent * sinf(phi)) * sinTheta;
}

Vec3 Resolve_WorldAxisToEmitterSamplingAxis(const Vec3& worldAxis, const Shared<TransformCom>& emitterTransform)
{
    if (nullptr == emitterTransform)
        return Normalize_OrFallback(worldAxis, Vec3{ 1.f, 0.f, 0.f });

    const Matrix inverseWorld = emitterTransform->Get_WorldMatrix().Invert();
    return Normalize_OrFallback(Vec3::TransformNormal(worldAxis, inverseWorld), Vec3{ 1.f, 0.f, 0.f });
}

void Resolve_PlaneRadialBasis(
    PointParticlePlaneRadialLocationPlane plane,
    const Shared<TransformCom>& emitterTransform,
    Vec3& axisU,
    Vec3& axisV,
    Vec3& planeNormal)
{
    axisU = Vec3{ 1.f, 0.f, 0.f };
    axisV = Vec3{ 0.f, 1.f, 0.f };
    planeNormal = Vec3{ 0.f, 0.f, 1.f };

    if (plane == PointParticlePlaneRadialLocationPlane::XZ)
    {
        axisV = Vec3{ 0.f, 0.f, 1.f };
        planeNormal = Vec3{ 0.f, 1.f, 0.f };
    }
    else if (plane == PointParticlePlaneRadialLocationPlane::YZ)
    {
        axisU = Vec3{ 0.f, 1.f, 0.f };
        axisV = Vec3{ 0.f, 0.f, 1.f };
        planeNormal = Vec3{ 1.f, 0.f, 0.f };
    }
    else if (plane == PointParticlePlaneRadialLocationPlane::CameraFacing)
    {
        const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
        const Shared<TransformCom> cameraTransform = activeCamera != nullptr ? activeCamera->Get_Transform() : nullptr;
        if (cameraTransform == nullptr)
        {
            static bool warnedCameraFacingFallback = false;
            if (!warnedCameraFacingFallback)
            {
                LOG_WARN("MeshEmitter PlaneRadialLocation CameraFacing fallback: active camera is unavailable.");
                warnedCameraFacingFallback = true;
            }
            return;
        }

        axisU = Resolve_WorldAxisToEmitterSamplingAxis(cameraTransform->Get_WorldRight(), emitterTransform);
        axisV = Resolve_WorldAxisToEmitterSamplingAxis(cameraTransform->Get_WorldUp(), emitterTransform);
        planeNormal = Resolve_WorldAxisToEmitterSamplingAxis(cameraTransform->Get_WorldForward(), emitterTransform);
    }
}

void Resolve_CylinderLocationBasis(
    PointParticleCylinderLocationAxis axis,
    Vec3& axisW,
    Vec3& radialU,
    Vec3& radialV)
{
    axisW = Vec3{ 0.f, 0.f, 1.f };
    radialU = Vec3{ 1.f, 0.f, 0.f };
    radialV = Vec3{ 0.f, 1.f, 0.f };

    if (axis == PointParticleCylinderLocationAxis::LocalX)
    {
        axisW = Vec3{ 1.f, 0.f, 0.f };
        radialU = Vec3{ 0.f, 1.f, 0.f };
        radialV = Vec3{ 0.f, 0.f, 1.f };
    }
    else if (axis == PointParticleCylinderLocationAxis::LocalY)
    {
        axisW = Vec3{ 0.f, 1.f, 0.f };
        radialU = Vec3{ 1.f, 0.f, 0.f };
        radialV = Vec3{ 0.f, 0.f, 1.f };
    }
}

Vec3 Resolve_PlaneRadialOrientationAxis(PointParticlePlaneRadialOrientationAxis axis)
{
    switch (axis)
    {
    case PointParticlePlaneRadialOrientationAxis::NegativeX:
        return Vec3{ -1.f, 0.f, 0.f };
    case PointParticlePlaneRadialOrientationAxis::PositiveY:
        return Vec3{ 0.f, 1.f, 0.f };
    case PointParticlePlaneRadialOrientationAxis::NegativeY:
        return Vec3{ 0.f, -1.f, 0.f };
    case PointParticlePlaneRadialOrientationAxis::PositiveZ:
        return Vec3{ 0.f, 0.f, 1.f };
    case PointParticlePlaneRadialOrientationAxis::NegativeZ:
        return Vec3{ 0.f, 0.f, -1.f };
    case PointParticlePlaneRadialOrientationAxis::PositiveX:
    default:
        return Vec3{ 1.f, 0.f, 0.f };
    }
}

Matrix Build_FrameMatrix(const Vec3& right, const Vec3& up, const Vec3& forward)
{
    Matrix matrix = Matrix::Identity;
    matrix._11 = right.x;
    matrix._12 = right.y;
    matrix._13 = right.z;
    matrix._21 = up.x;
    matrix._22 = up.y;
    matrix._23 = up.z;
    matrix._31 = forward.x;
    matrix._32 = forward.y;
    matrix._33 = forward.z;
    return matrix;
}

double Elapsed_Milliseconds(const std::chrono::steady_clock::time_point& begin, const std::chrono::steady_clock::time_point& end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

Matrix Resolve_MeshPlaneAlignmentMatrix(EffectScreenAlignment alignment)
{
    switch (alignment)
    {
    case EffectScreenAlignment::WorldPlaneXZ:
        return Matrix::CreateRotationX(-XM_PIDIV2);

    case EffectScreenAlignment::WorldPlaneXY:
    default:
        return Matrix::Identity;
    }
}

static int Resolve_MaterialSourceIndex(const string& source)
{
    constexpr int sourceAlpha = 0;
    constexpr int sourceRed = 1;
    constexpr int sourceLuminance = 2;

    if (source == "Red" || source == "red")
        return sourceRed;

    if (source == "Luminance" || source == "luminance")
        return sourceLuminance;

    return sourceAlpha;
}
}

IMPLEMENT_REFLECTION(MeshEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "MeshEmitter";
    info.category = "EffectEmitter";
    return true;
}

MeshEmitter::MeshEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

MeshEmitter::MeshEmitter(const MeshEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

HRESULT MeshEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT MeshEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<MeshEmitterDesc*>(arg);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    Reset_PlaybackRuntime();

    if (FAILED(Ready_Components()))
    {
        LOG_WARN(
            "MeshEmitter disabled because model resources are not ready. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        _isReady = false;
        return S_OK;
    }

    _isReady = true;
    const uint32 instanceCapacity = max(1u, _desc.spawn.instanceCount);
    _particles.assign(instanceCapacity, MeshParticleState{});
    _instancePayload.resize(instanceCapacity);
    Update_InstanceBuffer();
    return S_OK;
}

void MeshEmitter::Update(float timeDelta)
{
    if (!_isReady)
        return;

    Sync_FromEffectOwner();
    Advance_Playback(timeDelta);
}

void MeshEmitter::Late_Update(float)
{
    if (!Can_SubmitRender())
        return;

    GAME->Add_RenderGroup(Resolve_RenderGroup(), GetSharedPtr<GameObject>());
}

HRESULT MeshEmitter::Render()
{
    if (!Can_SubmitRender())
        return S_OK;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter render failed: camera binding failed. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }

    const Matrix identity = Matrix::Identity;
    if (FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &identity)))
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter render failed: pre-transform binding failed. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }

    if (Is_DistortionFamily())
        CHECK_FAILED_THROTTLED(Bind_DistortionResources(), 60, E_FAIL);
    else
        CHECK_FAILED_THROTTLED(Bind_EffectMaterialResources(), 60, E_FAIL);

    const uint32 passIndex = Resolve_ShaderPassIndex();
    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);

        if (!Is_DistortionFamily())
        {
            if (FAILED(Bind_ModelMaterialResourcesForMesh(meshIndex, materialIndex)))
            {
                LOG_WARN_THROTTLED(
                    60,
                    "MeshEmitter render failed: model material binding failed. emitter='{}', meshIndex={}, materialIndex={}, materialFamily='{}', shader='{}', modelGuid='{}', modelPath='{}'",
                    _emitterName,
                    meshIndex,
                    materialIndex,
                    string(magic_enum::enum_name(_desc.material.materialFamily)),
                    String::ToString(_loadedShaderPrototypeTag),
                    _desc.modelGuid,
                    _desc.modelPath
                );
                return E_FAIL;
            }
        }

        if (FAILED(_shader->Begin(passIndex)))
        {
            LOG_WARN_THROTTLED(
                60,
                "MeshEmitter draw failed: shader pass begin failed. emitter='{}', meshIndex={}, materialIndex={}, passIndex={}",
                _emitterName,
                meshIndex,
                materialIndex,
                passIndex
            );
            return E_FAIL;
        }

        if (FAILED(_model->RenderInstanced(meshIndex, _instanceBuffer.Get(), sizeof(MeshInstanceVertex), _activeInstanceCount)))
        {
            LOG_WARN_THROTTLED(
                60,
                "MeshEmitter draw failed: instanced mesh render failed. emitter='{}', meshIndex={}, materialIndex={}, passIndex={}",
                _emitterName,
                meshIndex,
                materialIndex,
                passIndex
            );
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT MeshEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    for (MeshParticleState& particle : _particles)
        particle = {};
    Update_InstanceBuffer();
    return S_OK;
}

bool MeshEmitter::Can_ReloadDefinition(const EffectEmitterDefinition& emitterDefinition) const
{
    if (emitterDefinition.kind != EffectEmitterKind::Mesh)
        return false;

    const MeshEmitterDesc* desc = get_if<MeshEmitterDesc>(&emitterDefinition.concreteDesc);
    return desc != nullptr && Is_CompatibleReloadDesc(*desc);
}

HRESULT MeshEmitter::Reload_Definition(const EffectEmitterDefinition& emitterDefinition)
{
    if (!Can_ReloadDefinition(emitterDefinition))
        return E_FAIL;

    const MeshEmitterDesc* desc = get_if<MeshEmitterDesc>(&emitterDefinition.concreteDesc);
    CHECK_NULL(desc, E_FAIL);

    _emitterId = emitterDefinition.id;
    _emitterName = emitterDefinition.name;
    _localPosition = emitterDefinition.localPosition;
    _localRotationDegrees = emitterDefinition.localRotationDegrees;
    _useLocalSpace = emitterDefinition.useLocalSpace;
    _enabled = emitterDefinition.enabled;

    CHECK_FAILED(Apply_DescForReload(*desc), E_FAIL);
    Sync_FromEffectOwner();

    return S_OK;
}

bool MeshEmitter::Is_EffectFinished() const
{
    return !_isReady || (PlaybackState::Completed == _playbackState && Count_ActiveParticles() == 0u);
}

bool MeshEmitter::Collect_FollowerSourcePoints(
    vector<EffectFollowerSourcePoint>& outPoints,
    uint32 maxPointCount) const
{
    if (!_isReady || maxPointCount == 0u)
        return false;

    const size_t initialCount = outPoints.size();
    for (uint32 index = 0u; index < static_cast<uint32>(_particles.size()) && outPoints.size() - initialCount < maxPointCount; ++index)
    {
        const MeshParticleState& particle = _particles[index];
        if (!particle.active || particle.age >= particle.lifeMax)
            continue;

        outPoints.push_back(
            EffectFollowerSourcePoint{
                .sourceIndex = index,
                .position = Resolve_ParticleWorldPosition(particle)
            }
        );
    }

    return outPoints.size() > initialCount;
}

bool MeshEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (_transformCom == nullptr)
        return false;

    bool hasActiveParticle = false;
    Vec3 accumulatedPosition{};
    uint32 activeParticleCount = 0u;

    for (const MeshParticleState& particle : _particles)
    {
        if (!particle.active)
            continue;

        accumulatedPosition += Resolve_ParticleWorldPosition(particle);
        ++activeParticleCount;
        hasActiveParticle = true;
    }

    if (hasActiveParticle && activeParticleCount > 0u)
    {
        outWorldPosition = accumulatedPosition / static_cast<float>(activeParticleCount);
        return true;
    }

    outWorldPosition = _transformCom->Get_WorldPosition();
    return true;
}

EffectSortPolicy MeshEmitter::Get_BlendSortPolicy() const
{
    return _desc.sort.sortPolicy;
}

int32 MeshEmitter::Get_BlendSortLayer() const
{
    return _desc.sort.sortLayer;
}

float MeshEmitter::Get_BlendSortBias() const
{
    return _desc.sort.artistSortBias;
}

void MeshEmitter::On_EffectTransformSynced()
{
    if (_activeInstanceCount > 0u)
        Update_InstanceBuffer();
}

void MeshEmitter::Log_DescPayload(const char* context) const
{
    LOG_DEBUG(
        "MeshEmitter desc payload. context={}, emitter='{}', spawnCount={}, spawnRate={}, bursts={}, life=({}, {}), meshAlignment={}, meshInitialScale={}, meshScaleByLife={}, motion={}, acceleration={}, drag=({}, {}), velocityScale=({}, {}), meshRotation={}, meshRotationMin=({}, {}, {}), meshRotationMax=({}, {}, {}), colorCurve={}, alphaCurve={}",
        context,
        _emitterName,
        _desc.spawn.instanceCount,
        _desc.spawn.particleSpawn.spawnRate,
        static_cast<uint32>(_desc.spawn.particleSpawn.bursts.size()),
        _desc.lifetime.lifeTime.x,
        _desc.lifetime.lifeTime.y,
        static_cast<uint32>(_desc.meshTransform.alignment),
        _desc.meshTransform.initialScaleEnabled,
        _desc.meshTransform.scaleByLife.enabled,
        _desc.motion.enabled,
        MeshEmitterDetail::Has_NonZeroVector(_desc.motion.accelerationMin) ||
        MeshEmitterDetail::Has_NonZeroVector(_desc.motion.accelerationMax),
        _desc.motion.drag.x,
        _desc.motion.drag.y,
        _desc.motion.velocityScaleByLife.x,
        _desc.motion.velocityScaleByLife.y,
        _desc.meshTransform.rotationEnabled,
        _desc.meshTransform.initialRotationDegreesMin.x,
        _desc.meshTransform.initialRotationDegreesMin.y,
        _desc.meshTransform.initialRotationDegreesMin.z,
        _desc.meshTransform.initialRotationDegreesMax.x,
        _desc.meshTransform.initialRotationDegreesMax.y,
        _desc.meshTransform.initialRotationDegreesMax.z,
        _desc.colorOverLife.curve.colorCurveEnabled,
        _desc.colorOverLife.curve.alphaCurveEnabled
    );
}

void MeshEmitter::Log_InitializeTiming(
    const char* context,
    double shaderMs,
    double resolveModelMs,
    double modelMs,
    double materialMs,
    double bufferMs,
    double totalMs) const
{
    const bool isReloadReuse = std::strcmp(context, "reload_reuse") == 0;
    const double infoThresholdMs = isReloadReuse
                                   ? MeshEmitterDetail::kMeshEmitterReloadReuseInfoThresholdMs
                                   : MeshEmitterDetail::kMeshEmitterInitializeInfoThresholdMs;

    if (totalMs >= infoThresholdMs)
    {
    }
    else
    {
    }
}

bool MeshEmitter::Is_CompatibleReloadDesc(const MeshEmitterDesc& desc) const
{
    if (!_isReady || _model == nullptr || _shader == nullptr)
        return false;
    if (desc.modelGuid != _desc.modelGuid || desc.modelPath != _desc.modelPath)
        return false;
    if (desc.useModelMaterials != _desc.useModelMaterials)
        return false;
    if (Has_MaterialOverride(_desc) && !Has_MaterialOverride(desc))
        return false;
    if (desc.meshTransform.alignment != _desc.meshTransform.alignment)
        return false;

    return true;
}

bool MeshEmitter::Has_MaterialOverride(const MeshEmitterDesc& desc) const
{
    return
        !desc.material.mainTextureGuid.empty() ||
        !desc.material.mainTexturePath.empty() ||
        !desc.material.noiseTextureGuid.empty() ||
        !desc.material.noiseTexturePath.empty() ||
        !desc.material.maskTextureGuid.empty() ||
        !desc.material.maskTexturePath.empty();
}

HRESULT MeshEmitter::Apply_DescForReload(const MeshEmitterDesc& desc)
{
    const auto totalBegin = std::chrono::steady_clock::now();
    _desc = desc;
    Reset_PlaybackRuntime();

    const auto shaderBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Ready_Shader(), E_FAIL);
    const auto shaderEnd = std::chrono::steady_clock::now();

    const auto materialBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Apply_EffectMaterialOverride(), E_FAIL);
    const auto materialEnd = std::chrono::steady_clock::now();

    const auto bufferBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);
    const auto bufferEnd = std::chrono::steady_clock::now();

    const uint32 instanceCapacity = max(1u, _desc.spawn.instanceCount);
    if (_particles.size() != instanceCapacity)
        _particles.assign(instanceCapacity, MeshParticleState{});
    else
    {
        for (MeshParticleState& particle : _particles)
            particle = {};
    }

    _instancePayload.resize(instanceCapacity);
    _isReady = _model != nullptr && _shader != nullptr && _instanceBuffer != nullptr;
    Update_InstanceBuffer();

    const auto totalEnd = std::chrono::steady_clock::now();
    Log_InitializeTiming(
        "reload_reuse",
        MeshEmitterDetail::Elapsed_Milliseconds(shaderBegin, shaderEnd),
        0.0,
        0.0,
        MeshEmitterDetail::Elapsed_Milliseconds(materialBegin, materialEnd),
        MeshEmitterDetail::Elapsed_Milliseconds(bufferBegin, bufferEnd),
        MeshEmitterDetail::Elapsed_Milliseconds(totalBegin, totalEnd)
    );
    return _isReady ? S_OK : E_FAIL;
}

HRESULT MeshEmitter::Ready_Components()
{
    const auto totalBegin = std::chrono::steady_clock::now();

    const auto shaderBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Ready_Shader(), E_FAIL);
    const auto shaderEnd = std::chrono::steady_clock::now();

    const auto resolveModelBegin = std::chrono::steady_clock::now();
    const wstring modelPath = Resolve_ModelPath();
    const auto resolveModelEnd = std::chrono::steady_clock::now();
    if (modelPath.empty())
    {
        LOG_WARN(
            "MeshEmitter model missing: model path could not be resolved. emitter='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }

    const auto modelBegin = std::chrono::steady_clock::now();
    _model = ModelCom::Create(
        _device,
        _context,
        ModelType::NonAnim,
        String::ToString(modelPath).c_str(),
        Matrix::CreateScale(0.01f)
    );
    if (!_model)
    {
        LOG_WARN(
            "MeshEmitter model missing: model load failed. emitter='{}', resolvedPath='{}', modelGuid='{}', modelPath='{}'",
            _emitterName,
            String::ToString(modelPath),
            _desc.modelGuid,
            _desc.modelPath
        );
        return E_FAIL;
    }
    const auto modelEnd = std::chrono::steady_clock::now();

    const auto materialBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Apply_EffectMaterialOverride(), E_FAIL);
    const auto materialEnd = std::chrono::steady_clock::now();

    const auto bufferBegin = std::chrono::steady_clock::now();
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);
    const auto bufferEnd = std::chrono::steady_clock::now();
    const auto totalEnd = std::chrono::steady_clock::now();

    Log_InitializeTiming(
        "initialize",
        MeshEmitterDetail::Elapsed_Milliseconds(shaderBegin, shaderEnd),
        MeshEmitterDetail::Elapsed_Milliseconds(resolveModelBegin, resolveModelEnd),
        MeshEmitterDetail::Elapsed_Milliseconds(modelBegin, modelEnd),
        MeshEmitterDetail::Elapsed_Milliseconds(materialBegin, materialEnd),
        MeshEmitterDetail::Elapsed_Milliseconds(bufferBegin, bufferEnd),
        MeshEmitterDetail::Elapsed_Milliseconds(totalBegin, totalEnd)
    );
    return S_OK;
}

HRESULT MeshEmitter::Ready_Shader()
{
    const char* shaderTag = kEffectMeshShaderId;
    if (Is_DistortionFamily())
        shaderTag = kEffectMeshDistortionShaderId;
    else if (Is_MeshGlassFamily())
        shaderTag = kEffectMeshGlassShaderId;
    const wstring shaderId = String::ToWString(shaderTag);
    if (nullptr != _shader && _loadedShaderPrototypeTag == shaderId)
        return S_OK;

    if (const Shared<ShaderCom> existingShader = Get_Component<ShaderCom>(shaderId))
    {
        _shader = existingShader;
        _loadedShaderPrototypeTag = shaderId;
        return S_OK;
    }

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), shaderId, _shader), E_FAIL);

    if (nullptr == _shader)
    {
        LOG_WARN(
            "MeshEmitter shader missing: effect mesh shader prototype is missing. emitter='{}', shaderId='{}'",
            _emitterName,
            String::ToString(shaderId)
        );
        return E_FAIL;
    }

    _loadedShaderPrototypeTag = shaderId;
    return S_OK;
}

HRESULT MeshEmitter::Apply_EffectMaterialOverride()
{
    return Ready_EffectMaterialTextures();
}

HRESULT MeshEmitter::Ready_EffectMaterialTextures()
{
    if (Is_DistortionFamily())
    {
        _texture.reset();
        _noiseTexture.reset();
        _maskTexture.reset();
        CHECK_FAILED(Ready_OptionalTexture(_desc.material.flowTextureGuid, _desc.material.flowTexturePath, _flowTexture), E_FAIL);
        _opacitySourceIndex = Resolve_OpacitySourceIndex();
        return S_OK;
    }

    _flowTexture.reset();

    wstring mainTexturePath = Resolve_MaterialTexturePath(_desc.material.mainTextureGuid, _desc.material.mainTexturePath);
    if (mainTexturePath.empty())
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter assigned effect material has no resolvable main texture. emitter='{}', materialMainGuid='{}', materialMainPath='{}'. fallback='{}'",
            _emitterName,
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath,
            kFallbackTexturePath
        );
        mainTexturePath = Resolve_MaterialTexturePath({}, kFallbackTexturePath);
    }

    if (mainTexturePath.empty())
        return E_FAIL;

    _texture = Texture::Create(_device, _context, mainTexturePath.c_str(), 1);
    CHECK_NULL(_texture, E_FAIL);

    CHECK_FAILED(Ready_OptionalTexture(_desc.material.noiseTextureGuid, _desc.material.noiseTexturePath, _noiseTexture), E_FAIL);
    CHECK_FAILED(Ready_OptionalTexture(_desc.material.maskTextureGuid, _desc.material.maskTexturePath, _maskTexture), E_FAIL);
    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    return S_OK;
}

HRESULT MeshEmitter::Ready_OptionalTexture(const string& textureGuid, const string& texturePath, Shared<Texture>& outTexture)
{
    outTexture.reset();
    if (textureGuid.empty() && texturePath.empty())
        return S_OK;

    const wstring resolvedPath = Resolve_MaterialTexturePath(textureGuid, texturePath);
    if (resolvedPath.empty())
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter optional effect material texture missing. emitter='{}', textureGuid='{}', texturePath='{}'",
            _emitterName,
            textureGuid,
            texturePath
        );
        return S_OK;
    }

    outTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (outTexture == nullptr)
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter optional effect material texture create failed. emitter='{}', textureGuid='{}', texturePath='{}', resolvedPath='{}'",
            _emitterName,
            textureGuid,
            texturePath,
            String::ToString(resolvedPath)
        );
    }
    return S_OK;
}

HRESULT MeshEmitter::Bind_EffectMaterialResources()
{
    const float emitterPhase = Compute_MaterialScalarModulationPhase(_emitterElapsedTime, _desc.playback.duration);
    const EffectRequiredMaterialRuntimeDesc material =
        Resolve_EmitterTimeMaterialParameterModulation(_desc.material, emitterPhase, _effectPlaybackSeed);

    CHECK_NULL(_texture, E_FAIL);

    CHECK_FAILED_THROTTLED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), 60, E_FAIL);

    if (_noiseTexture != nullptr)
        CHECK_FAILED_THROTTLED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), 60, E_FAIL);
    else
        CHECK_FAILED_THROTTLED(_shader->Bind_SRV("g_NoiseTexture", nullptr), 60, E_FAIL);

    if (_maskTexture != nullptr)
        CHECK_FAILED_THROTTLED(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), 60, E_FAIL);
    else
        CHECK_FAILED_THROTTLED(_shader->Bind_SRV("g_MaskTexture", nullptr), 60, E_FAIL);

    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), 60, E_FAIL);

    const Vec4 effectMeshParams = Vec4(
        material.intensity,
        material.opacityPower,
        material.noiseStrength,
        _noiseTexture != nullptr ? 1.f : 0.f
    );
    const Vec4 effectMeshAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        _maskTexture != nullptr ? 1.f : 0.f,
        clamp(material.alphaMultiplier, 0.f, 1.f)
    );
    const Vec4 effectMeshMainUVParams = Vec4(material.mainUVScale.x, material.mainUVScale.y, material.mainUVScrollSpeed.x, material.mainUVScrollSpeed.y);
    const Vec4 effectMeshNoiseUVParams = Vec4(material.noiseUVScale.x, material.noiseUVScale.y, material.noiseUVScrollSpeed.x, material.noiseUVScrollSpeed.y);
    const Vec4 effectMeshMaskUVParams = Vec4(material.maskUVScale.x, material.maskUVScale.y, material.maskUVScrollSpeed.x, material.maskUVScrollSpeed.y);
    const Vec4 effectMeshUVOffsetParams = Vec4(material.mainUVOffset.x, material.mainUVOffset.y, material.noiseUVOffset.x, material.noiseUVOffset.y);
    const Vec4 effectMeshMaskUVOffsetParams = Vec4(material.maskUVOffset.x, material.maskUVOffset.y, 0.f, 0.f);
    const Vec4 effectMeshUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
        0.f
    );
    const Vec4 effectMeshUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.vPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.vPolicy))
    );
    const Vec4 effectMeshMaskUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.vPolicy)),
        0.f,
        0.f
    );
    const Vec4 effectMeshUVRotationParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVRotation)),
        static_cast<float>(static_cast<uint32>(material.noiseUVRotation)),
        static_cast<float>(static_cast<uint32>(material.maskUVRotation)),
        0.f
    );
    const Vec4 effectMeshSourceParams = Vec4(
        static_cast<float>(MeshEmitterDetail::Resolve_MaterialSourceIndex(material.noiseSource)),
        static_cast<float>(MeshEmitterDetail::Resolve_MaterialSourceIndex(material.maskSource)),
        material.noiseInvert ? 1.f : 0.f,
        material.maskInvert ? 1.f : 0.f
    );
    const EffectMaterialAdditiveContributionData& additive = material.additive;
    const EffectMaterialCoreEmissiveData& coreEmissive = material.coreEmissive;
    const Vec4 effectMeshAdditiveParams = Vec4(
        static_cast<float>(static_cast<uint32>(additive.colorSource)),
        static_cast<float>(static_cast<uint32>(additive.amountSource)),
        static_cast<float>(static_cast<uint32>(additive.coveragePolicy)),
        additive.intensityScale
    );
    const Vec4 effectMeshAdditiveFlags = Vec4(additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
    const Vec4 effectMeshCoreEmissiveParams = Vec4(
        coreEmissive.enabled ? 1.f : 0.f,
        coreEmissive.corePower,
        coreEmissive.coreIntensity,
        coreEmissive.outerPower
    );
    const Vec4 effectMeshCoreEmissiveColor = Vec4(
        coreEmissive.coreColor.x,
        coreEmissive.coreColor.y,
        coreEmissive.coreColor.z,
        coreEmissive.outerIntensity
    );

    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshParams", &effectMeshParams, sizeof(effectMeshParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshAlphaParams", &effectMeshAlphaParams, sizeof(effectMeshAlphaParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshMainUVParams", &effectMeshMainUVParams, sizeof(effectMeshMainUVParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshNoiseUVParams", &effectMeshNoiseUVParams, sizeof(effectMeshNoiseUVParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshMaskUVParams", &effectMeshMaskUVParams, sizeof(effectMeshMaskUVParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshUVOffsetParams", &effectMeshUVOffsetParams, sizeof(effectMeshUVOffsetParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshMaskUVOffsetParams", &effectMeshMaskUVOffsetParams, sizeof(effectMeshMaskUVOffsetParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshUVModeParams", &effectMeshUVModeParams, sizeof(effectMeshUVModeParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshUVAxisPolicyParams", &effectMeshUVAxisPolicyParams, sizeof(effectMeshUVAxisPolicyParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshMaskUVAxisPolicyParams", &effectMeshMaskUVAxisPolicyParams, sizeof(effectMeshMaskUVAxisPolicyParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshUVRotationParams", &effectMeshUVRotationParams, sizeof(effectMeshUVRotationParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshSourceParams", &effectMeshSourceParams, sizeof(effectMeshSourceParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshMaterialTime", &_emitterElapsedTime, sizeof(_emitterElapsedTime)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshAdditiveParams", &effectMeshAdditiveParams, sizeof(effectMeshAdditiveParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshAdditiveFlags", &effectMeshAdditiveFlags, sizeof(effectMeshAdditiveFlags)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshCoreEmissiveParams", &effectMeshCoreEmissiveParams, sizeof(effectMeshCoreEmissiveParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshCoreEmissiveColor", &effectMeshCoreEmissiveColor, sizeof(effectMeshCoreEmissiveColor)), 60, E_FAIL);
    if (Is_MeshGlassFamily())
    {
        const Vec4 glassSurfaceParams{
            max(0.f, material.glassAlpha),
            max(0.0001f, material.glassAlphaPower),
            max(0.f, material.glassNormalStrength),
            clamp(material.glassMainInfluence, 0.f, 1.f)
        };
        const Vec4 glassRimParams{
            max(0.f, material.glassRimIntensity),
            max(0.0001f, material.glassRimPower),
            clamp(material.glassNoiseBreakup, 0.f, 1.f),
            clamp(material.glassMaskStrength, 0.f, 1.f)
        };
        const Vec4 glassLightDirection{
            material.glassLightDirection.x,
            material.glassLightDirection.y,
            material.glassLightDirection.z,
            max(0.f, material.glassLightIntensity)
        };
        const Vec4 glassSpecularParams{
            max(0.0001f, material.glassSpecularPower),
            max(0.0001f, material.glassSpecularSoftness),
            0.f,
            0.f
        };

        CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshGlassSurfaceParams", &glassSurfaceParams, sizeof(glassSurfaceParams)), 60, E_FAIL);
        CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshGlassRimColor", &material.glassRimColor, sizeof(material.glassRimColor)), 60, E_FAIL);
        CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshGlassRimParams", &glassRimParams, sizeof(glassRimParams)), 60, E_FAIL);
        CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshGlassLightDirection", &glassLightDirection, sizeof(glassLightDirection)), 60, E_FAIL);
        CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshGlassLightColor", &material.glassLightColor, sizeof(material.glassLightColor)), 60, E_FAIL);
        CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectMeshGlassSpecularParams", &glassSpecularParams, sizeof(glassSpecularParams)), 60, E_FAIL);
    }
    if (!Is_MeshGlassFamily())
    {
        CHECK_FAILED_THROTTLED(Bind_MaterialScalarModulationShaderPayloadToShader(_shader.get(), material.scalarModulation, emitterPhase), 60, E_FAIL);
        CHECK_FAILED_THROTTLED(
            Bind_CoreColorRgbModulationShaderPayload(_shader.get(), material.coreColorRgbModulation, emitterPhase, _effectPlaybackSeed),
            60,
            E_FAIL
        );
    }
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_OpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)), 60, E_FAIL);
    return S_OK;
}

HRESULT MeshEmitter::Bind_ModelMaterialResourcesForMesh(uint32 meshIndex, uint32 materialIndex)
{
    CHECK_NULL(_texture, E_FAIL);

    const auto logBindFailure = [&](const char* bindName) -> HRESULT
        {
            LOG_WARN_THROTTLED(
                60,
                "MeshEmitter model material bind failed: bind='{}', emitter='{}', meshIndex={}, materialIndex={}, useModelMaterials={}, materialFamily='{}', shader='{}', modelGuid='{}', modelPath='{}'",
                bindName,
                _emitterName,
                meshIndex,
                materialIndex,
                _desc.useModelMaterials,
                string(magic_enum::enum_name(_desc.material.materialFamily)),
                String::ToString(_loadedShaderPrototypeTag),
                _desc.modelGuid,
                _desc.modelPath
            );
            return E_FAIL;
        };

    Vec4 modelMaterialFlags = Vec4::Zero;
    const bool supportsModelOrm = !Is_MeshGlassFamily();
    if (!_desc.useModelMaterials || _model == nullptr)
    {
        if (FAILED(_shader->Bind_SRV("g_ModelNormalTexture", nullptr)))
            return logBindFailure("g_ModelNormalTexture=null");
        if (FAILED(_shader->Bind_SRV("g_ModelEmissiveTexture", nullptr)))
            return logBindFailure("g_ModelEmissiveTexture=null");
        if (supportsModelOrm && FAILED(_shader->Bind_SRV("g_ModelOrmTexture", nullptr)))
            return logBindFailure("g_ModelOrmTexture=null");
        if (FAILED(_shader->Bind_RawValue("g_EffectMeshModelMaterialFlags", &modelMaterialFlags, sizeof(modelMaterialFlags))))
            return logBindFailure("g_EffectMeshModelMaterialFlags");
        return S_OK;
    }

    if (SUCCEEDED(_model->Bind_Material(_shader.get(), "g_Texture", meshIndex, MaterialTextureSlot::BaseColor)))
        modelMaterialFlags.x = 1.f;
    else
    {
        LOG_WARN_THROTTLED(
            60,
            "MeshEmitter model material BaseColor missing; using effect material main texture fallback. emitter='{}', meshIndex={}, materialIndex={}, modelGuid='{}', modelPath='{}'",
            _emitterName,
            meshIndex,
            materialIndex,
            _desc.modelGuid,
            _desc.modelPath
        );
        if (FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0)))
            return logBindFailure("g_Texture=fallbackEffectMaterial");
    }

    if (SUCCEEDED(_model->Bind_Material(_shader.get(), "g_ModelNormalTexture", meshIndex, MaterialTextureSlot::Normal)))
        modelMaterialFlags.y = 1.f;
    else
    {
        if (FAILED(_shader->Bind_SRV("g_ModelNormalTexture", nullptr)))
            return logBindFailure("g_ModelNormalTexture=null");
    }

    if (SUCCEEDED(_model->Bind_Material(_shader.get(), "g_ModelEmissiveTexture", meshIndex, MaterialTextureSlot::Emissive)))
        modelMaterialFlags.z = 1.f;
    else
    {
        if (FAILED(_shader->Bind_SRV("g_ModelEmissiveTexture", nullptr)))
            return logBindFailure("g_ModelEmissiveTexture=null");
    }

    if (supportsModelOrm)
    {
        if (SUCCEEDED(_model->Bind_Material(_shader.get(), "g_ModelOrmTexture", meshIndex, MaterialTextureSlot::ORM)))
            modelMaterialFlags.w = 1.f;
        else
        {
            if (FAILED(_shader->Bind_SRV("g_ModelOrmTexture", nullptr)))
                return logBindFailure("g_ModelOrmTexture=null");
        }
    }

    if (FAILED(_shader->Bind_RawValue("g_EffectMeshModelMaterialFlags", &modelMaterialFlags, sizeof(modelMaterialFlags))))
        return logBindFailure("g_EffectMeshModelMaterialFlags");
    return S_OK;
}

HRESULT MeshEmitter::Bind_DistortionResources()
{
    const float emitterPhase = Compute_MaterialScalarModulationPhase(_emitterElapsedTime, _desc.playback.duration);
    const EffectRequiredMaterialRuntimeDesc material =
        Resolve_EmitterTimeMaterialParameterModulation(_desc.material, emitterPhase, _effectPlaybackSeed);

    if (nullptr != _flowTexture)
        CHECK_FAILED_THROTTLED(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_FlowTexture", 0), 60, E_FAIL);
    else
        CHECK_FAILED_THROTTLED(_shader->Bind_SRV("g_FlowTexture", nullptr), 60, E_FAIL);

    D3D11_VIEWPORT viewport{};
    uint32 viewportCount = 1u;
    _context->RSGetViewports(&viewportCount, &viewport);

    const Vec4 distortionParams{
        material.refractionIntensity,
        max(0.f, material.refractionPresence),
        nullptr != _flowTexture ? 1.f : 0.f,
        _emitterElapsedTime
    };
    const Vec4 distortionScreenSize{ max(1.f, viewport.Width), max(1.f, viewport.Height), 0.f, 0.f };
    const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(material.flowUVTilingMode));
    const float flowUVRotation = static_cast<float>(static_cast<uint32>(material.flowUVRotation));
    const Vec2 flowUVPolicyParams{
        static_cast<float>(static_cast<uint32>(material.flowUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.flowUVPolicy.vPolicy))
    };

    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_DistortionScreenSize", &distortionScreenSize, sizeof(distortionScreenSize)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVPolicyParams", &flowUVPolicyParams, sizeof(flowUVPolicyParams)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVScale", &material.flowUVScale, sizeof(material.flowUVScale)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVOffset", &material.flowUVOffset, sizeof(material.flowUVOffset)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVScrollSpeed", &material.flowUVScrollSpeed, sizeof(material.flowUVScrollSpeed)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Bind_RawValue("g_FlowUVRotation", &flowUVRotation, sizeof(flowUVRotation)), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(Bind_MaterialScalarModulationShaderPayloadToShader(_shader.get(), material.scalarModulation, emitterPhase), 60, E_FAIL);
    return S_OK;
}

bool MeshEmitter::Is_DistortionFamily() const
{
    return _desc.material.materialFamily == EffectMaterialFamily::SpriteDistortion;
}

bool MeshEmitter::Is_MeshGlassFamily() const
{
    return _desc.material.materialFamily == EffectMaterialFamily::MeshGlass;
}

HRESULT MeshEmitter::Ready_InstanceBuffer()
{
    const uint32 instanceCapacity = max(1u, _desc.spawn.instanceCount);
    if (nullptr != _instanceBuffer && _instanceCapacity == instanceCapacity)
        return S_OK;

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(MeshInstanceVertex) * instanceCapacity;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    _instanceBuffer.Reset();
    _instanceCapacity = 0u;
    if (FAILED(_device->CreateBuffer(&bufferDesc, nullptr, _instanceBuffer.GetAddressOf())))
        return E_FAIL;

    _instanceCapacity = instanceCapacity;
    return S_OK;
}

void MeshEmitter::Update_InstanceBuffer()
{
    if (!_instanceBuffer || !_context || !_transformCom)
        return;

    _activeInstanceCount = 0u;
    const uint32 drawLimit = _desc.drawLimit.useMaxDrawCount
                             ? min(max(1u, _desc.drawLimit.maxDrawCount), static_cast<uint32>(_instancePayload.size()))
                             : static_cast<uint32>(_instancePayload.size());
    vector<const MeshParticleState*> sortedParticles{};
    sortedParticles.reserve(_particles.size());

    for (const MeshParticleState& particle : _particles)
    {
        if (!particle.active)
            continue;

        sortedParticles.push_back(&particle);
    }

    const Vec4* cameraPosition4 = Resolve_RenderGroup() == RenderGroup::Blend && GAME != nullptr
                                  ? GAME->Get_CamPosition()
                                  : nullptr;
    Vec3 localBoundsMin{};
    Vec3 localBoundsMax{};
    const bool hasLocalBounds = _model != nullptr && _model->Try_Get_LocalBounds(localBoundsMin, localBoundsMax);
    const Vec3 localBoundsExtent = hasLocalBounds ? (localBoundsMax - localBoundsMin) * 0.5f : Vec3{};
    const float localBoundsRadius = hasLocalBounds ? localBoundsExtent.Length() : 0.f;

    if (cameraPosition4 != nullptr && hasLocalBounds)
    {
        const Vec3 cameraPosition{ cameraPosition4->x, cameraPosition4->y, cameraPosition4->z };
        ranges::stable_sort(
            sortedParticles,
            [&](const MeshParticleState* lhs, const MeshParticleState* rhs)
            {
                const auto compute_particle_depth_sq = [&](const MeshParticleState& particle) -> float
                {
                    return (Resolve_ParticleWorldPosition(particle) - cameraPosition).LengthSquared();
                };

                return compute_particle_depth_sq(*lhs) > compute_particle_depth_sq(*rhs);
            }
        );
    }

    for (const MeshParticleState* particle : sortedParticles)
    {
        if (particle == nullptr || _activeInstanceCount >= drawLimit)
            continue;

        const float lifeProgress = clamp(particle->age / max(0.0001f, particle->lifeMax), 0.f, 1.f);
        MeshInstanceVertex& instance = _instancePayload[_activeInstanceCount++];
        instance.world = Build_InstanceMatrix(*particle);
        instance.color = Evaluate_ColorOverLife(*particle, lifeProgress);
        instance.lifeTime = Vec2{ particle->lifeMax, particle->age };
        instance.padding = Vec2{};
        instance.coreColorRgb = Vec4{ particle->coreColorRgb.x, particle->coreColorRgb.y, particle->coreColorRgb.z, 0.f };
    }

    if (_activeInstanceCount == 0u)
        return;

    if (const Shared<GameObject> effectOwner = _effectOwner.lock(); effectOwner && effectOwner->Get_Name() == L"Monster_Weak")
    {
        const Vec3 emitterWorldPosition = _transformCom->Get_WorldPosition();
        const Vec3 firstParticleWorldPosition = !sortedParticles.empty() && sortedParticles.front() != nullptr
                                                ? Resolve_ParticleWorldPosition(*sortedParticles.front())
                                                : emitterWorldPosition;
        LOG_DEBUG_THROTTLED(
            60,
            "[MonsterWeakMeshEmitter] emitter={} localSpace={} active={} root=({:.3f},{:.3f},{:.3f}) first=({:.3f},{:.3f},{:.3f})",
            _emitterId,
            _useLocalSpace,
            _activeInstanceCount,
            emitterWorldPosition.x,
            emitterWorldPosition.y,
            emitterWorldPosition.z,
            firstParticleWorldPosition.x,
            firstParticleWorldPosition.y,
            firstParticleWorldPosition.z
        );
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource{};
    if (FAILED(_context->Map(_instanceBuffer.Get(), 0u, D3D11_MAP_WRITE_DISCARD, 0u, &mappedResource)))
        return;

    memcpy(mappedResource.pData, _instancePayload.data(), sizeof(MeshInstanceVertex) * _activeInstanceCount);
    _context->Unmap(_instanceBuffer.Get(), 0u);
}

void MeshEmitter::Reset_PlaybackRuntime()
{
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;
    _activeInstanceCount = 0u;
    _nextSpawnCursor = 0u;
    _sourceMotionVelocity = Vec3{};
    _sourceMotionTangent = Vec3{ 0.f, 0.f, 1.f };
    _lastSourceMotionCenter = Vec3{};
    _sourceMotionVelocityValid = false;
    _hasLastSourceMotionCenter = false;
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
    Resample_LoopSpawnDistributions();
}

void MeshEmitter::Advance_Playback(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    Update_SourceMotionVelocity(safeDeltaTime);
    float simulationDeltaTime = 0.f;
    uint32 spawnRequest = 0u;

    if (PlaybackState::Completed == _playbackState)
    {
        if (_desc.playback.killOnCompleted && !_completedKillDispatched)
        {
            for (MeshParticleState& particle : _particles)
                particle.active = false;
            _completedKillDispatched = true;
        }
        else if (!_desc.playback.killOnCompleted)
            simulationDeltaTime = safeDeltaTime;

        Update_Particles(simulationDeltaTime);
        Update_InstanceBuffer();
        return;
    }

    float remainingTime = safeDeltaTime;
    while (remainingTime > 0.f && PlaybackState::Completed != _playbackState)
    {
        const float delay = Resolve_CurrentLoopDelay();
        if (_loopElapsedTime < delay)
        {
            _playbackState = PlaybackState::Delayed;
            const float delayStep = min(remainingTime, delay - _loopElapsedTime);
            _loopElapsedTime += delayStep;
            remainingTime -= delayStep;

            if (_loopElapsedTime < delay || remainingTime <= 0.f)
                break;
        }

        _playbackState = PlaybackState::Playing;

        const float duration = Resolve_Duration();
        const float currentPlaybackTime = max(0.f, _loopElapsedTime - delay);
        const float remainingPlaybackTime = max(0.f, duration - currentPlaybackTime);
        const float playbackStep = min(remainingTime, remainingPlaybackTime);

        if (playbackStep > 0.f)
        {
            simulationDeltaTime += playbackStep;
            spawnRequest += Consume_SpawnRequest(playbackStep);
            _loopElapsedTime += playbackStep;
            remainingTime -= playbackStep;
        }

        const float loopPlaybackTime = max(0.f, _loopElapsedTime - delay);
        const float playbackTimeToEnd = duration - loopPlaybackTime;
        if (playbackTimeToEnd > MeshEmitterDetail::kPlaybackCompletionEpsilon)
            break;

        if (playbackTimeToEnd > 0.f)
        {
            const float completionStep = min(remainingTime, playbackTimeToEnd);
            if (completionStep > 0.f)
            {
                simulationDeltaTime += completionStep;
                spawnRequest += Consume_SpawnRequest(completionStep);
                remainingTime -= completionStep;
            }
        }

        _loopElapsedTime = delay + duration;

        if (Has_NextLoop())
        {
            Start_NextLoop();
            continue;
        }

        _playbackState = PlaybackState::Completed;
        if (_desc.playback.killOnCompleted && !_completedKillDispatched)
        {
            for (MeshParticleState& particle : _particles)
                particle.active = false;
            _completedKillDispatched = true;
        }
        break;
    }

    if (PlaybackState::Completed == _playbackState && !_desc.playback.killOnCompleted)
        simulationDeltaTime += remainingTime;

    Spawn_Particles(spawnRequest);
    Update_Particles(simulationDeltaTime);
    Update_InstanceBuffer();
}

void MeshEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
    Resample_LoopSpawnDistributions();
}

float MeshEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.playback.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.playback.delay);
}

float MeshEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.playback.duration);
}

bool MeshEmitter::Has_NextLoop() const
{
    return _desc.playback.loopCount == 0u || _loopIndex + 1u < _desc.playback.loopCount;
}

bool MeshEmitter::Can_SubmitRender() const
{
    return _isReady && _model && _shader && _instanceBuffer && _activeInstanceCount > 0u &&
           PlaybackState::Delayed != _playbackState;
}

uint32 MeshEmitter::Count_ActiveParticles() const
{
    uint32 activeParticleCount = 0u;
    for (const MeshParticleState& particle : _particles)
    {
        if (particle.active)
            ++activeParticleCount;
    }

    return activeParticleCount;
}

uint32 MeshEmitter::Consume_SpawnRequest(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    const float previousTime = _emitterElapsedTime;
    _emitterElapsedTime += safeDeltaTime;

    uint32 spawnRequest = 0u;

    if (_desc.spawn.particleSpawn.processSpawnRate)
    {
        const float loopPhase = clamp(_emitterElapsedTime / Resolve_Duration(), 0.f, 1.f);
        const float spawnRateValue = MeshEmitterDetail::Evaluate_FloatCurve(
            _desc.spawn.particleSpawn.spawnRateCurve,
            loopPhase,
            _sampledSpawnRate
        );
        const float spawnRateScale = MeshEmitterDetail::Evaluate_FloatCurve(
            _desc.spawn.particleSpawn.spawnRateScaleCurve,
            loopPhase,
            _sampledSpawnRateScale
        );
        const float spawnRate = max(0.f, spawnRateValue) * max(0.f, spawnRateScale);
        _spawnAccumulator += spawnRate * safeDeltaTime;

        const float wholeSpawn = floorf(_spawnAccumulator + 0.0001f);
        if (wholeSpawn > 0.f)
        {
            spawnRequest += static_cast<uint32>(wholeSpawn);
            _spawnAccumulator -= wholeSpawn;
        }
    }

    if (_desc.spawn.particleSpawn.processBurstList)
    {
        if (_burstExecuted.size() != _desc.spawn.particleSpawn.bursts.size())
            _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);

        for (size_t index = 0; index < _desc.spawn.particleSpawn.bursts.size(); ++index)
        {
            if (_burstExecuted[index])
                continue;

            const PointParticleBurstDesc& burst = _desc.spawn.particleSpawn.bursts[index];
            if (previousTime > burst.time || _emitterElapsedTime < burst.time)
                continue;

            const float sampledBurstScale = Sample_SpawnDistribution(
                _desc.spawn.particleSpawn.burstScaleRange,
                _desc.spawn.particleSpawn.burstScaleSeed,
                static_cast<uint32>(0xB851u + index * 131u)
            );
            const float burstPhase = clamp(burst.time / Resolve_Duration(), 0.f, 1.f);
            const float burstScale = MeshEmitterDetail::Evaluate_FloatCurve(
                _desc.spawn.particleSpawn.burstScaleCurve,
                burstPhase,
                sampledBurstScale
            );
            const float scaledCount = static_cast<float>(burst.count) * max(0.f, burstScale);
            spawnRequest += static_cast<uint32>(floorf(scaledCount));
            _burstExecuted[index] = true;
        }
    }

    return min(spawnRequest, max(1u, _desc.spawn.particleSpawn.maxActiveCount));
}

float MeshEmitter::Sample_SpawnDistribution(
    const Vec2& range,
    const PointParticleRandomSeedRuntimeDesc& seed,
    uint32 salt) const
{
    const float minValue = min(range.x, range.y);
    const float maxValue = max(range.x, range.y);
    if (minValue == maxValue)
        return minValue;

    const uint32 baseSeed = (_loopIndex + 1u) * 9781u + salt + MeshEmitterDetail::Resolve_SeedSalt(seed, _effectPlaybackSeed);
    return lerp(minValue, maxValue, MeshEmitterDetail::Hash01(baseSeed));
}

void MeshEmitter::Resample_LoopSpawnDistributions()
{
    _sampledSpawnRate = Sample_SpawnDistribution(
        _desc.spawn.particleSpawn.spawnRateRange,
        _desc.spawn.particleSpawn.spawnRateSeed,
        0x31B7u
    );
    _sampledSpawnRateScale = Sample_SpawnDistribution(
        _desc.spawn.particleSpawn.spawnRateScaleRange,
        _desc.spawn.particleSpawn.spawnRateScaleSeed,
        0x45D9u
    );
}

void MeshEmitter::Spawn_Particles(uint32 spawnRequest)
{
    if (spawnRequest == 0u || _particles.empty())
        return;

    const uint32 maxActiveCount = min(static_cast<uint32>(_particles.size()), max(1u, _desc.spawn.particleSpawn.maxActiveCount));
    uint32 activeCount = 0u;
    for (const MeshParticleState& particle : _particles)
    {
        if (particle.active)
            ++activeCount;
    }

    uint32 remainingRequest = min(spawnRequest, maxActiveCount > activeCount ? maxActiveCount - activeCount : 0u);
    const uint32 placementCount = max(1u, remainingRequest);
    uint32 placementIndex = 0u;
    while (remainingRequest > 0u)
    {
        bool activated = false;
        for (uint32 probe = 0; probe < _particles.size(); ++probe)
        {
            const uint32 index = (_nextSpawnCursor + probe) % static_cast<uint32>(_particles.size());
            MeshParticleState& particle = _particles[index];
            if (particle.active)
                continue;

            const uint32 emitterSeed = _emitterId * 104729u;
            Activate_Particle(
                particle,
                index * 9781u + _nextSpawnCursor * 6271u + _loopIndex * 7919u + emitterSeed + 1u,
                index,
                placementIndex,
                placementCount
            );
            _nextSpawnCursor = (index + 1u) % static_cast<uint32>(_particles.size());
            ++placementIndex;
            --remainingRequest;
            activated = true;
            break;
        }

        if (!activated)
            break;
    }
}

void MeshEmitter::Activate_Particle(MeshParticleState& particle, uint32 seed, uint32 particleIndex, uint32 placementIndex, uint32 placementCount)
{
    const float lifeMin = max(0.0001f, min(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    PlaneRadialSample planeRadialSample{};
    SphereRadialSample sphereRadialSample{};
    CylinderRadialSample cylinderRadialSample{};
    const Vec3 spawnOffset = Sample_InitialOffset(
        seed,
        particleIndex,
        placementIndex,
        placementCount,
        planeRadialSample,
        sphereRadialSample,
        cylinderRadialSample
    );
    Vec3 spawnPosition = spawnOffset;
    if (!_useLocalSpace && _transformCom != nullptr)
        spawnPosition += _transformCom->Get_WorldPosition();

    particle.active = true;
    particle.age = 0.f;
    const float spawnPhase = clamp(_emitterElapsedTime / Resolve_Duration(), 0.f, 1.f);
    const uint32 lifetimeSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.lifetimeSeed, _effectPlaybackSeed);
    particle.lifeMax = _desc.lifetime.lifeTimeCurve.enabled
                       ? max(0.0001f, MeshEmitterDetail::Evaluate_FloatCurve(_desc.lifetime.lifeTimeCurve, spawnPhase, lifeMax))
                       : RandomRange(lifeMin, lifeMax, lifetimeSeed + 11u);
    particle.position = spawnPosition;
    particle.spawnOffset = spawnOffset;
    particle.spawnPosition = spawnPosition;
    // World-space particle은 spawn 시점 root 회전을 고정하고 이후 root 회전을 다시 합성하지 않는다.
    particle.spawnTransformRotation =
        !_useLocalSpace && _transformCom != nullptr
        ? _transformCom->Get_WorldRotationQuaternion()
        : Quat::Identity;
    const SourceMotionVelocityContext sourceMotionContext{
        .sourceVelocity = _sourceMotionVelocity,
        .tangent = _sourceMotionTangent,
        .sourceVelocityValid = _sourceMotionVelocityValid
    };
    Sample_InitialVelocityChannels(particle, spawnPosition, planeRadialSample, sourceMotionContext, seed + 23u);
    particle.acceleration = Sample_Acceleration(seed + 29u);
    particle.drag = Sample_Drag(seed + 31u);
    particle.baseScale = _desc.previewScale * Sample_InitialScale(seed + 37u);
    particle.spawnOrientation = Build_CylinderMeshOrientation(cylinderRadialSample, particle.spawnOrientationEnabled);
    if (!particle.spawnOrientationEnabled)
        particle.spawnOrientation = Build_SphereRadialMeshOrientation(sphereRadialSample, particle.spawnOrientationEnabled);
    if (!particle.spawnOrientationEnabled)
        particle.spawnOrientation = Build_PlaneRadialMeshOrientation(planeRadialSample, particle.spawnOrientationEnabled);
    particle.rotationRadians = Sample_InitialRotation(seed + 41u);
    particle.rotationQuat = Quat::CreateFromYawPitchRoll(
        particle.rotationRadians.y,
        particle.rotationRadians.x,
        particle.rotationRadians.z
    );
    particle.rotationQuat.Normalize();
    particle.angularVelocityRadians = Sample_InitialAngularVelocity(seed + 43u);
    particle.directionAlignRandomDelay = clamp(
        Sample_SpawnDistribution(_desc.meshDirectionAlign.randomDelay, _desc.meshDirectionAlign.randomDelaySeed, seed + 59u),
        0.f,
        1.f
    );
    particle.directionAlignWeightScale = clamp(
        Sample_SpawnDistribution(_desc.meshDirectionAlign.randomWeightScale, _desc.meshDirectionAlign.randomWeightScaleSeed, seed + 61u),
        0.f,
        1.f
    );
    particle.startColor = Sample_Color(
        _desc.initialColor.startColorMin,
        _desc.initialColor.startColorMax,
        _desc.initialColor.colorSeed,
        _desc.initialColor.alphaSeed,
        seed + 47u
    );
    particle.endColor = Sample_Color(
        _desc.colorOverLife.endColorMin,
        _desc.colorOverLife.endColorMax,
        _desc.colorOverLife.colorSeed,
        _desc.colorOverLife.alphaSeed,
        seed + 53u
    );
    particle.seed = seed;
    particle.coreColorRgb = Sample_ParticleLifeCoreColorRgbUniformModulation(
        _desc.material.coreColorRgbModulation,
        Vec3{
            _desc.material.coreEmissive.coreColor.x,
            _desc.material.coreEmissive.coreColor.y,
            _desc.material.coreEmissive.coreColor.z
        },
        _effectPlaybackSeed,
        seed
    );
}

void MeshEmitter::Update_Particles(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    for (MeshParticleState& particle : _particles)
    {
        if (!particle.active)
            continue;

        particle.age += safeDeltaTime;
        if (particle.age >= particle.lifeMax)
        {
            particle.active = false;
            continue;
        }

        const float lifeProgress = clamp(particle.age / max(0.0001f, particle.lifeMax), 0.f, 1.f);
        const float accelerationScale = Evaluate_VelocityScaleByLifeChannel(
            _desc.motion.accelerationIntegratedVelocityScaleByLife,
            lifeProgress
        );
        const Vec3 acceleration = Evaluate_AccelerationByLife(particle, lifeProgress) * accelerationScale;
        particle.accelerationIntegratedVelocity += acceleration * safeDeltaTime;

        if (particle.drag > 0.f)
        {
            const float dragDecay = expf(-particle.drag * safeDeltaTime);
            particle.initialVelocity *= dragDecay;
            particle.initialRadialVelocity *= dragDecay;
            particle.velocityCone *= dragDecay;
            particle.sourceMotionVelocity *= dragDecay;
            particle.accelerationIntegratedVelocity *= dragDecay;
        }

        particle.velocity = Evaluate_ScaledVelocityChannels(particle, lifeProgress);
        particle.position += particle.velocity * safeDeltaTime;

        const Vec3 angularVelocityScale = Evaluate_AngularVelocityScaleByLife(lifeProgress);
        const Vec3 scaledAngularVelocity{
            particle.angularVelocityRadians.x * angularVelocityScale.x,
            particle.angularVelocityRadians.y * angularVelocityScale.y,
            particle.angularVelocityRadians.z * angularVelocityScale.z
        };

        if (_desc.meshTransform.initialAngularVelocityInWorldSpace)
        {
            Vec3 angularDelta = scaledAngularVelocity * safeDeltaTime;
            if (_useLocalSpace)
                angularDelta = Resolve_WorldVectorToSimulation(angularDelta);

            const float angularDeltaLength = angularDelta.Length();
            if (angularDeltaLength > MeshEmitterDetail::kAngularDeltaEpsilon)
            {
                const Vec3 axis = angularDelta / angularDeltaLength;
                Quat delta = Quat::CreateFromAxisAngle(axis, angularDeltaLength);
                delta.Normalize();
                particle.rotationQuat = particle.rotationQuat * delta;
                particle.rotationQuat.Normalize();
            }
        }
        else
            particle.rotationRadians += scaledAngularVelocity * safeDeltaTime;
    }
}

wstring MeshEmitter::Resolve_ModelPath() const
{
    if (nullptr != GAME && !_desc.modelGuid.empty())
    {
        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(_desc.modelGuid);
        if (nullptr != assetMeta && assetMeta->type == "Model")
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(_desc.modelGuid);
            if (!resolvedPath.empty() && fs::exists(resolvedPath))
                return fs::path(resolvedPath).lexically_normal().wstring();
        }
    }

    if (_desc.modelPath.empty())
        return {};

    fs::path modelPath = String::ToWString(_desc.modelPath);
    if (modelPath.is_relative() && GAME != nullptr)
        modelPath = fs::path(GAME->Get_AssetRoot()) / modelPath;

    modelPath = modelPath.lexically_normal();
    return fs::exists(modelPath) ? modelPath.wstring() : wstring{};
}

wstring MeshEmitter::Resolve_MaterialTexturePath(const string& textureGuid, const string& texturePath) const
{
    if (nullptr != GAME && !textureGuid.empty())
    {
        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (nullptr != assetMeta && assetMeta->type == "Texture")
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
            if (!resolvedPath.empty() && fs::exists(resolvedPath))
                return fs::path(resolvedPath).lexically_normal().wstring();
        }
    }

    if (texturePath.empty())
        return {};

    fs::path resolvedPath = String::ToWString(texturePath);
    if (resolvedPath.is_relative() && GAME != nullptr)
        resolvedPath = fs::path(GAME->Get_AssetRoot()) / resolvedPath;

    resolvedPath = resolvedPath.lexically_normal();
    return fs::exists(resolvedPath) ? resolvedPath.wstring() : wstring{};
}

Matrix MeshEmitter::Build_InstanceMatrix(const MeshParticleState& particle) const
{
    const float lifeProgress = clamp(particle.age / max(0.0001f, particle.lifeMax), 0.f, 1.f);
    const Vec3 sizeByLife = Evaluate_ScaleByLife(lifeProgress);
    const Vec3 scale{
        max(0.0001f, particle.baseScale.x * sizeByLife.x),
        max(0.0001f, particle.baseScale.y * sizeByLife.y),
        max(0.0001f, particle.baseScale.z * sizeByLife.z)
    };
    const Vec3 rotationByLife = Evaluate_RotationByLife(lifeProgress);
    const Vec3 rotationRadians{
        particle.rotationRadians.x + XMConvertToRadians(rotationByLife.x),
        particle.rotationRadians.y + XMConvertToRadians(rotationByLife.y),
        particle.rotationRadians.z + XMConvertToRadians(rotationByLife.z)
    };

    Quat rotation = _desc.meshTransform.initialAngularVelocityInWorldSpace
                    ? particle.rotationQuat * Quat::CreateFromYawPitchRoll(
                          XMConvertToRadians(rotationByLife.y),
                          XMConvertToRadians(rotationByLife.x),
                          XMConvertToRadians(rotationByLife.z)
                      )
                    : Quat::CreateFromYawPitchRoll(rotationRadians.y, rotationRadians.x, rotationRadians.z);
    rotation.Normalize();

    const Matrix scaleMatrix = Matrix::CreateScale(scale);
    // Radial orientation은 sampling 단계에서 root 회전을 이미 반영하므로 중복 합성을 피한다.
    const Matrix spawnTransformRotationMatrix =
        !_useLocalSpace && _transformCom != nullptr && !particle.spawnOrientationEnabled
        ? Matrix::CreateFromQuaternion(particle.spawnTransformRotation)
        : Matrix::Identity;
    const Vec3 orbitOffset = Evaluate_OrbitOffset(particle, lifeProgress);
    const Vec3 motionOffset = particle.position - particle.spawnPosition;
    Vec3 emitterPosition{};
    if (!_useLocalSpace && _transformCom != nullptr)
        emitterPosition = _transformCom->Get_WorldPosition();
    Vec3 translation = particle.position;
    if (_desc.orbitOverLife.enabled)
        translation = emitterPosition + orbitOffset + motionOffset;
    const Matrix translationMatrix = Matrix::CreateTranslation(translation);

    const float directionAlignWeight = Evaluate_DirectionAlignWeight(lifeProgress, particle);
    if (directionAlignWeight > MeshEmitterDetail::kMeshDirectionAlignEpsilon)
    {
        bool directionAlignEnabled = false;
        const Quat targetOrientation = Build_MeshDirectionAlignTargetOrientation(particle, translation, directionAlignEnabled);
        if (directionAlignEnabled)
        {
            Quat candidateOrientation{};
            if (particle.spawnOrientationEnabled)
                candidateOrientation = particle.spawnOrientation * rotation;
            else
            {
                Quat alignment = Quat::CreateFromRotationMatrix(
                    MeshEmitterDetail::Resolve_MeshPlaneAlignmentMatrix(_desc.meshTransform.alignment)
                );
                alignment.Normalize();
                candidateOrientation = rotation * alignment * particle.spawnTransformRotation;
            }
            candidateOrientation.Normalize();

            Quat finalOrientation = Quat::Slerp(candidateOrientation, targetOrientation, directionAlignWeight);
            finalOrientation.Normalize();

            Matrix localMatrix =
                scaleMatrix *
                Matrix::CreateFromQuaternion(finalOrientation) *
                translationMatrix;

            if (_useLocalSpace && _transformCom != nullptr)
                return localMatrix * _transformCom->Get_WorldMatrix();

            return localMatrix;
        }
    }

    const Matrix rotationMatrix = Matrix::CreateFromQuaternion(rotation);
    Matrix localMatrix{};
    if (particle.spawnOrientationEnabled)
    {
        localMatrix =
            scaleMatrix *
            Matrix::CreateFromQuaternion(particle.spawnOrientation) *
            rotationMatrix *
            translationMatrix;
    }
    else
    {
        localMatrix =
            scaleMatrix *
            rotationMatrix *
            MeshEmitterDetail::Resolve_MeshPlaneAlignmentMatrix(_desc.meshTransform.alignment) *
            spawnTransformRotationMatrix *
            translationMatrix;
    }

    if (_useLocalSpace && _transformCom != nullptr)
        return localMatrix * _transformCom->Get_WorldMatrix();

    return localMatrix;
}

Vec3 MeshEmitter::Resolve_ParticleWorldPosition(const MeshParticleState& particle) const
{
    if (!_useLocalSpace || _transformCom == nullptr)
        return particle.position;

    return Vec3::Transform(particle.position, _transformCom->Get_WorldMatrix());
}

Vec3 MeshEmitter::Sample_InitialOffset(
    uint32 seed,
    uint32 particleIndex,
    uint32 placementIndex,
    uint32 placementCount,
    PlaneRadialSample& outPlaneRadialSample,
    SphereRadialSample& outSphereRadialSample,
    CylinderRadialSample& outCylinderRadialSample) const
{
    Vec3 offset{};
    PointParticleInitialLocationDesc initialLocation = _desc.initialLocation;
    PointParticleSphereLocationDesc sphereLocation = _desc.sphereLocation;
    PointParticlePlaneRadialLocationDesc planeRadialLocation = _desc.planeRadialLocation;
    PointParticleCylinderLocationDesc cylinderLocation = _desc.cylinderLocation;

    if (!_useLocalSpace && _transformCom != nullptr)
    {
        const Vec3 worldScale = MeshEmitterDetail::Resolve_RuntimeWorldScale(_effectOwner, _transformCom);
        const float uniformLocationScale = max(worldScale.x, max(worldScale.y, worldScale.z));
        const Vec2 planeScale = MeshEmitterDetail::Resolve_PlaneScale(planeRadialLocation.plane, worldScale);
        const float uniformPlaneScale = max(planeScale.x, planeScale.y);
        const Vec2 cylinderScale = MeshEmitterDetail::Resolve_CylinderScale(cylinderLocation.axis, worldScale);

        initialLocation.minOffset = MeshEmitterDetail::Scale_Vector3(initialLocation.minOffset, worldScale);
        initialLocation.maxOffset = MeshEmitterDetail::Scale_Vector3(initialLocation.maxOffset, worldScale);
        sphereLocation.offset = MeshEmitterDetail::Scale_Vector3(sphereLocation.offset, worldScale);
        sphereLocation.radius *= uniformLocationScale;
        planeRadialLocation.offset = MeshEmitterDetail::Scale_Vector3(planeRadialLocation.offset, worldScale);
        planeRadialLocation.thickness *= uniformLocationScale;
        planeRadialLocation.uRange = MeshEmitterDetail::Scale_Vector2(planeRadialLocation.uRange, Vec2{ planeScale.x, planeScale.x });
        planeRadialLocation.vRange = MeshEmitterDetail::Scale_Vector2(planeRadialLocation.vRange, Vec2{ planeScale.y, planeScale.y });
        planeRadialLocation.radiusRange = MeshEmitterDetail::Scale_Vector2(
            planeRadialLocation.radiusRange,
            Vec2{ uniformPlaneScale, uniformPlaneScale }
        );
        cylinderLocation.offset = MeshEmitterDetail::Scale_Vector3(cylinderLocation.offset, worldScale);
        cylinderLocation.radiusRange = MeshEmitterDetail::Scale_Vector2(
            cylinderLocation.radiusRange,
            Vec2{ cylinderScale.x, cylinderScale.x }
        );
        cylinderLocation.heightRange = MeshEmitterDetail::Scale_Vector2(
            cylinderLocation.heightRange,
            Vec2{ cylinderScale.y, cylinderScale.y }
        );
    }

    if (initialLocation.enabled)
    {
        const uint32 initialLocationSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.initialLocationSeed, _effectPlaybackSeed);
        offset += Vec3{
            RandomRange(initialLocation.minOffset.x, initialLocation.maxOffset.x, initialLocationSeed + 1u),
            RandomRange(initialLocation.minOffset.y, initialLocation.maxOffset.y, initialLocationSeed + 2u),
            RandomRange(initialLocation.minOffset.z, initialLocation.maxOffset.z, initialLocationSeed + 3u)
        };
    }

    if (sphereLocation.enabled && sphereLocation.radius > 0.f)
    {
        const uint32 sphereSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.sphereLocationSeed, _effectPlaybackSeed);
        Vec3 direction{};
        const bool useEvenSurfacePlacement =
            sphereLocation.mode == PointParticleSphereLocationMode::Surface &&
            sphereLocation.placementMode == PointParticleSphereLocationPlacementMode::EvenByParticleIndex;
        if (useEvenSurfacePlacement)
            direction = MeshEmitterDetail::Resolve_EvenByParticleIndexSphereDirection(placementIndex, placementCount);
        else
        {
            const float z = RandomRange(-1.f, 1.f, sphereSeed + 5u);
            const float angle = XM_2PI * Random01(sphereSeed + 7u);
            const float radiusOnPlane = sqrtf(max(0.f, 1.f - z * z));
            direction = Vec3{
                cosf(angle) * radiusOnPlane,
                sinf(angle) * radiusOnPlane,
                z
            };
        }

        float radius = sphereLocation.radius;
        if (sphereLocation.mode == PointParticleSphereLocationMode::Volume)
            radius *= cbrtf(Random01(sphereSeed + 9u));

        outSphereRadialSample.enabled = true;
        outSphereRadialSample.localOffset = sphereLocation.offset + direction * radius;
        outSphereRadialSample.radialDirection = direction;
        offset += outSphereRadialSample.localOffset;
    }

    if (planeRadialLocation.enabled)
    {
        Vec3 axisU{};
        Vec3 axisV{};
        Vec3 planeNormal{};
        MeshEmitterDetail::Resolve_PlaneRadialBasis(planeRadialLocation.plane, _transformCom, axisU, axisV, planeNormal);

        float u = 0.f;
        float v = 0.f;
        float sampleAngle = 0.f;
        if (planeRadialLocation.shape == PointParticlePlaneRadialLocationShape::Rectangle)
        {
            u = RandomRange(
                planeRadialLocation.uRange.x,
                planeRadialLocation.uRange.y,
                seed + MeshEmitterDetail::Resolve_SeedSalt(planeRadialLocation.uSeed, _effectPlaybackSeed) + 1601u
            );
            v = RandomRange(
                planeRadialLocation.vRange.x,
                planeRadialLocation.vRange.y,
                seed + MeshEmitterDetail::Resolve_SeedSalt(planeRadialLocation.vSeed, _effectPlaybackSeed) + 1613u
            );
            sampleAngle = atan2f(v, u);
        }
        else
        {
            const float radiusMin = min(planeRadialLocation.radiusRange.x, planeRadialLocation.radiusRange.y);
            const float radiusMax = max(planeRadialLocation.radiusRange.x, planeRadialLocation.radiusRange.y);
            const float angleMinDegrees = planeRadialLocation.angleDegreesRange.x;
            const float angleMaxDegrees = planeRadialLocation.angleDegreesRange.y;
            const float angleMin = XMConvertToRadians(angleMinDegrees);
            const float angleMax = XMConvertToRadians(angleMaxDegrees);
            const float radius = RandomRange(
                radiusMin,
                radiusMax,
                seed + MeshEmitterDetail::Resolve_SeedSalt(planeRadialLocation.radiusSeed, _effectPlaybackSeed) + 1601u
            );
            const float angleRatio = planeRadialLocation.placementMode == PointParticlePlaneRadialLocationPlacementMode::EvenByParticleIndex
                                     ? MeshEmitterDetail::Resolve_EvenByParticleIndexAngleRatio(
                                         placementIndex,
                                         placementCount,
                                         angleMinDegrees,
                                         angleMaxDegrees
                                     )
                                     : Random01(seed + MeshEmitterDetail::Resolve_SeedSalt(planeRadialLocation.angleDegreesSeed, _effectPlaybackSeed) + 1613u);
            sampleAngle = lerp(angleMin, angleMax, angleRatio);
            u = cosf(sampleAngle) * radius;
            v = sinf(sampleAngle) * radius;
        }

        const float thickness = max(0.f, planeRadialLocation.thickness);
        const float normalOffset = thickness > 0.0001f ? (Random01(seed + 1621u) - 0.5f) * thickness : 0.f;
        const Vec3 radialVector = axisU * u + axisV * v;
        const Vec3 fallbackRadial = MeshEmitterDetail::Normalize_OrFallback(axisU * cosf(sampleAngle) + axisV * sinf(sampleAngle), axisU);
        outPlaneRadialSample.enabled = true;
        outPlaneRadialSample.localOffset = planeRadialLocation.offset + radialVector + planeNormal * normalOffset;
        outPlaneRadialSample.radialDirection = MeshEmitterDetail::Normalize_OrFallback(radialVector, fallbackRadial);
        outPlaneRadialSample.tangentCW = MeshEmitterDetail::Normalize_OrFallback(axisU * sinf(sampleAngle) - axisV * cosf(sampleAngle), -axisV);
        outPlaneRadialSample.tangentCCW = -outPlaneRadialSample.tangentCW;
        outPlaneRadialSample.planeNormal = planeNormal;
        outPlaneRadialSample.sampleAngleRadians = sampleAngle;
        offset += outPlaneRadialSample.localOffset;
    }

    if (cylinderLocation.enabled)
    {
        Vec3 axisW{};
        Vec3 radialU{};
        Vec3 radialV{};
        MeshEmitterDetail::Resolve_CylinderLocationBasis(cylinderLocation.axis, axisW, radialU, radialV);

        const float radiusMin = max(0.f, min(cylinderLocation.radiusRange.x, cylinderLocation.radiusRange.y));
        const float radiusMax = max(radiusMin, max(cylinderLocation.radiusRange.x, cylinderLocation.radiusRange.y));
        const float heightMin = min(cylinderLocation.heightRange.x, cylinderLocation.heightRange.y);
        const float heightMax = max(cylinderLocation.heightRange.x, cylinderLocation.heightRange.y);
        const float angleMinDegrees = cylinderLocation.angleDegreesRange.x;
        const float angleMaxDegrees = cylinderLocation.angleDegreesRange.y;
        const float angleMin = XMConvertToRadians(angleMinDegrees);
        const float angleMax = XMConvertToRadians(angleMaxDegrees);
        const uint32 radiusSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(cylinderLocation.radiusSeed, _effectPlaybackSeed) + 1801u;
        const uint32 heightSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(cylinderLocation.heightSeed, _effectPlaybackSeed) + 1811u;
        const uint32 angleSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(cylinderLocation.angleDegreesSeed, _effectPlaybackSeed) + 1823u;

        float radius = RandomRange(radiusMin, radiusMax, radiusSeed);
        if (cylinderLocation.mode == PointParticleCylinderLocationMode::Volume)
        {
            const float minSquared = radiusMin * radiusMin;
            const float maxSquared = radiusMax * radiusMax;
            radius = sqrtf(lerp(minSquared, maxSquared, Random01(radiusSeed)));
        }

        const float height = RandomRange(heightMin, heightMax, heightSeed);
        const float angleRatio = cylinderLocation.placementMode == PointParticleCylinderLocationPlacementMode::EvenByParticleIndex
                                 ? MeshEmitterDetail::Resolve_EvenByParticleIndexAngleRatio(
                                     placementIndex,
                                     placementCount,
                                     angleMinDegrees,
                                     angleMaxDegrees
                                 )
                                 : Random01(angleSeed);
        const float angle = lerp(angleMin, angleMax, angleRatio);
        const Vec3 radialVector = (radialU * cosf(angle) + radialV * sinf(angle)) * radius;
        outCylinderRadialSample.enabled = true;
        outCylinderRadialSample.localOffset = cylinderLocation.offset + axisW * height + radialVector;
        outCylinderRadialSample.radialDirection =
            MeshEmitterDetail::Normalize_OrFallback(radialU * cosf(angle) + radialV * sinf(angle), radialU);
        outCylinderRadialSample.tangentCW =
            MeshEmitterDetail::Normalize_OrFallback(radialU * sinf(angle) - radialV * cosf(angle), -radialV);
        outCylinderRadialSample.tangentCCW = -outCylinderRadialSample.tangentCW;
        outCylinderRadialSample.cylinderAxis = axisW;
        offset += outCylinderRadialSample.localOffset;
    }

    return Resolve_LocalVector(offset);
}

Quat MeshEmitter::Build_SphereRadialMeshOrientation(const SphereRadialSample& sample, bool& outEnabled) const
{
    outEnabled = false;

    if (!sample.enabled ||
        !_desc.sphereRadialOrientation.enabled ||
        _desc.sphereRadialOrientation.orientationMode == PointParticleSphereRadialOrientationMode::None)
        return Quat::Identity;

    Vec3 targetForward = sample.radialDirection;
    if (_desc.sphereRadialOrientation.orientationMode == PointParticleSphereRadialOrientationMode::FaceRadialIn)
        targetForward = -targetForward;

    targetForward = MeshEmitterDetail::Normalize_OrFallback(Resolve_LocalVector(targetForward), Vec3{ 0.f, 0.f, 1.f });
    Vec3 preferredUp = Vec3{ 0.f, 1.f, 0.f };
    if (fabsf(targetForward.Dot(preferredUp)) > 1.f - 1e-3f)
        preferredUp = Vec3{ 0.f, 0.f, 1.f };

    Vec3 targetRight = preferredUp.Cross(targetForward);
    targetRight = MeshEmitterDetail::Normalize_OrFallback(targetRight, Vec3{ 1.f, 0.f, 0.f });
    const Vec3 targetUp = MeshEmitterDetail::Normalize_OrFallback(targetForward.Cross(targetRight), Vec3{ 0.f, 1.f, 0.f });

    const Vec3 sourceForward = MeshEmitterDetail::Resolve_PlaneRadialOrientationAxis(_desc.sphereRadialOrientation.meshForwardAxis);
    Vec3 sourceUp = MeshEmitterDetail::Resolve_PlaneRadialOrientationAxis(_desc.sphereRadialOrientation.meshUpAxis);
    if (fabsf(sourceForward.Dot(sourceUp)) > 1.f - 1e-3f)
    {
        if (!_sphereRadialMeshOrientationFallbackLogged)
        {
            LOG_WARN(
                "MeshEmitter SphereRadialOrientation fallback: meshForwardAxis and meshUpAxis are parallel. emitter='{}'",
                _emitterName
            );
            _sphereRadialMeshOrientationFallbackLogged = true;
        }
        return Quat::Identity;
    }

    Vec3 sourceRight = sourceUp.Cross(sourceForward);
    sourceRight.Normalize();
    sourceUp = sourceForward.Cross(sourceRight);
    sourceUp.Normalize();

    const Matrix sourceFrame = MeshEmitterDetail::Build_FrameMatrix(sourceRight, sourceUp, sourceForward);
    const Matrix targetFrame = MeshEmitterDetail::Build_FrameMatrix(targetRight, targetUp, targetForward);
    Quat orientation = Quat::CreateFromRotationMatrix(sourceFrame.Invert() * targetFrame);
    orientation.Normalize();

    if (fabsf(_desc.sphereRadialOrientation.tiltDegrees) > 0.0001f)
    {
        Quat tilt = Quat::CreateFromAxisAngle(targetRight, XMConvertToRadians(_desc.sphereRadialOrientation.tiltDegrees));
        tilt.Normalize();
        orientation = orientation * tilt;
    }

    if (fabsf(_desc.sphereRadialOrientation.rollOffsetDegrees) > 0.0001f)
    {
        Quat roll = Quat::CreateFromAxisAngle(targetForward, XMConvertToRadians(_desc.sphereRadialOrientation.rollOffsetDegrees));
        roll.Normalize();
        orientation = orientation * roll;
    }

    orientation.Normalize();
    outEnabled = true;
    return orientation;
}

Quat MeshEmitter::Build_PlaneRadialMeshOrientation(const PlaneRadialSample& sample, bool& outEnabled) const
{
    outEnabled = false;

    if (!sample.enabled ||
        !_desc.planeRadialOrientation.enabled ||
        _desc.planeRadialOrientation.orientationMode == PointParticlePlaneRadialOrientationMode::None)
        return Quat::Identity;

    Vec3 targetForward{};
    switch (_desc.planeRadialOrientation.orientationMode)
    {
    case PointParticlePlaneRadialOrientationMode::FaceRadialIn:
        targetForward = -sample.radialDirection;
        break;
    case PointParticlePlaneRadialOrientationMode::FaceTangentCW:
        targetForward = sample.tangentCW;
        break;
    case PointParticlePlaneRadialOrientationMode::FaceTangentCCW:
        targetForward = sample.tangentCCW;
        break;
    case PointParticlePlaneRadialOrientationMode::FacePlaneNormal:
        targetForward = sample.planeNormal;
        break;
    case PointParticlePlaneRadialOrientationMode::FaceRadialOut:
    default:
        targetForward = sample.radialDirection;
        break;
    }

    targetForward = MeshEmitterDetail::Normalize_OrFallback(Resolve_LocalVector(targetForward), Vec3{ 0.f, 0.f, 1.f });
    Vec3 targetUp = MeshEmitterDetail::Normalize_OrFallback(Resolve_LocalVector(sample.planeNormal), Vec3{ 0.f, 1.f, 0.f });
    if (fabsf(targetForward.Dot(targetUp)) > 1.f - 1e-3f)
        targetUp = fabsf(targetForward.Dot(Vec3{ 0.f, 1.f, 0.f })) > 1.f - 1e-3f ? Vec3{ 0.f, 0.f, 1.f } : Vec3{ 0.f, 1.f, 0.f };

    Vec3 targetRight = targetUp.Cross(targetForward);
    targetRight = MeshEmitterDetail::Normalize_OrFallback(targetRight, Vec3{ 1.f, 0.f, 0.f });
    targetUp = MeshEmitterDetail::Normalize_OrFallback(targetForward.Cross(targetRight), Vec3{ 0.f, 1.f, 0.f });

    const Vec3 sourceForward = MeshEmitterDetail::Resolve_PlaneRadialOrientationAxis(_desc.planeRadialOrientation.meshForwardAxis);
    Vec3 sourceUp = MeshEmitterDetail::Resolve_PlaneRadialOrientationAxis(_desc.planeRadialOrientation.meshUpAxis);
    if (fabsf(sourceForward.Dot(sourceUp)) > 1.f - 1e-3f)
    {
        if (!_planeRadialMeshOrientationFallbackLogged)
        {
            LOG_WARN(
                "MeshEmitter PlaneRadialOrientation fallback: meshForwardAxis and meshUpAxis are parallel. emitter='{}'",
                _emitterName
            );
            _planeRadialMeshOrientationFallbackLogged = true;
        }
        return Quat::Identity;
    }

    Vec3 sourceRight = sourceUp.Cross(sourceForward);
    sourceRight.Normalize();
    sourceUp = sourceForward.Cross(sourceRight);
    sourceUp.Normalize();

    const Matrix sourceFrame = MeshEmitterDetail::Build_FrameMatrix(sourceRight, sourceUp, sourceForward);
    const Matrix targetFrame = MeshEmitterDetail::Build_FrameMatrix(targetRight, targetUp, targetForward);
    Quat orientation = Quat::CreateFromRotationMatrix(sourceFrame.Invert() * targetFrame);
    orientation.Normalize();

    if (fabsf(_desc.planeRadialOrientation.tiltDegrees) > 0.0001f)
    {
        Quat tilt = Quat::CreateFromAxisAngle(targetRight, XMConvertToRadians(_desc.planeRadialOrientation.tiltDegrees));
        tilt.Normalize();
        orientation = orientation * tilt;
    }

    if (fabsf(_desc.planeRadialOrientation.rollOffsetDegrees) > 0.0001f)
    {
        Quat roll = Quat::CreateFromAxisAngle(targetForward, XMConvertToRadians(_desc.planeRadialOrientation.rollOffsetDegrees));
        roll.Normalize();
        orientation = orientation * roll;
    }

    orientation.Normalize();
    outEnabled = true;
    return orientation;
}

Quat MeshEmitter::Build_CylinderMeshOrientation(const CylinderRadialSample& sample, bool& outEnabled) const
{
    outEnabled = false;

    if (!sample.enabled ||
        !_desc.cylinderOrientation.enabled ||
        _desc.cylinderOrientation.targetKind == PointParticlePlaneRadialOrientationTargetKind::Sprite2D ||
        _desc.cylinderOrientation.orientationMode == PointParticleCylinderOrientationMode::None)
        return Quat::Identity;

    Vec3 targetForward{};
    switch (_desc.cylinderOrientation.orientationMode)
    {
    case PointParticleCylinderOrientationMode::FaceRadialIn:
        targetForward = -sample.radialDirection;
        break;
    case PointParticleCylinderOrientationMode::FaceTangentCW:
        targetForward = sample.tangentCW;
        break;
    case PointParticleCylinderOrientationMode::FaceTangentCCW:
        targetForward = sample.tangentCCW;
        break;
    case PointParticleCylinderOrientationMode::FaceCylinderAxisPositive:
        targetForward = sample.cylinderAxis;
        break;
    case PointParticleCylinderOrientationMode::FaceCylinderAxisNegative:
        targetForward = -sample.cylinderAxis;
        break;
    case PointParticleCylinderOrientationMode::FaceRadialOut:
    default:
        targetForward = sample.radialDirection;
        break;
    }

    targetForward = MeshEmitterDetail::Normalize_OrFallback(Resolve_LocalVector(targetForward), Vec3{ 0.f, 0.f, 1.f });
    Vec3 targetUp = MeshEmitterDetail::Normalize_OrFallback(Resolve_LocalVector(sample.cylinderAxis), Vec3{ 0.f, 1.f, 0.f });
    if (fabsf(targetForward.Dot(targetUp)) > 1.f - 1e-3f)
        targetUp = fabsf(targetForward.Dot(Vec3{ 0.f, 1.f, 0.f })) > 1.f - 1e-3f ? Vec3{ 0.f, 0.f, 1.f } : Vec3{ 0.f, 1.f, 0.f };

    Vec3 targetRight = targetUp.Cross(targetForward);
    targetRight = MeshEmitterDetail::Normalize_OrFallback(targetRight, Vec3{ 1.f, 0.f, 0.f });
    targetUp = MeshEmitterDetail::Normalize_OrFallback(targetForward.Cross(targetRight), Vec3{ 0.f, 1.f, 0.f });

    const Vec3 sourceForward = MeshEmitterDetail::Resolve_PlaneRadialOrientationAxis(_desc.cylinderOrientation.meshForwardAxis);
    Vec3 sourceUp = MeshEmitterDetail::Resolve_PlaneRadialOrientationAxis(_desc.cylinderOrientation.meshUpAxis);
    if (fabsf(sourceForward.Dot(sourceUp)) > 1.f - 1e-3f)
    {
        if (!_cylinderRadialMeshOrientationFallbackLogged)
        {
            LOG_WARN(
                "MeshEmitter CylinderOrientation fallback: meshForwardAxis and meshUpAxis are parallel. emitter='{}'",
                _emitterName
            );
            _cylinderRadialMeshOrientationFallbackLogged = true;
        }
        return Quat::Identity;
    }

    Vec3 sourceRight = sourceUp.Cross(sourceForward);
    sourceRight.Normalize();
    sourceUp = sourceForward.Cross(sourceRight);
    sourceUp.Normalize();

    const Matrix sourceFrame = MeshEmitterDetail::Build_FrameMatrix(sourceRight, sourceUp, sourceForward);
    const Matrix targetFrame = MeshEmitterDetail::Build_FrameMatrix(targetRight, targetUp, targetForward);
    Quat orientation = Quat::CreateFromRotationMatrix(sourceFrame.Invert() * targetFrame);
    orientation.Normalize();

    if (fabsf(_desc.cylinderOrientation.tiltDegrees) > 0.0001f)
    {
        Quat tilt = Quat::CreateFromAxisAngle(targetRight, XMConvertToRadians(_desc.cylinderOrientation.tiltDegrees));
        tilt.Normalize();
        orientation = orientation * tilt;
    }

    if (fabsf(_desc.cylinderOrientation.rollOffsetDegrees) > 0.0001f)
    {
        Quat roll = Quat::CreateFromAxisAngle(targetForward, XMConvertToRadians(_desc.cylinderOrientation.rollOffsetDegrees));
        roll.Normalize();
        orientation = orientation * roll;
    }

    orientation.Normalize();
    outEnabled = true;
    return orientation;
}

float MeshEmitter::Evaluate_DirectionAlignWeight(float lifeProgress, const MeshParticleState& particle) const
{
    if (!_desc.meshDirectionAlign.enabled || particle.directionAlignWeightScale <= MeshEmitterDetail::kMeshDirectionAlignEpsilon)
        return 0.f;

    const float randomDelay = clamp(particle.directionAlignRandomDelay, 0.f, 1.f);
    const float delayedLifeProgress = clamp(
        (lifeProgress - randomDelay) / max(MeshEmitterDetail::kMeshDirectionAlignEpsilon, 1.f - randomDelay),
        0.f,
        1.f
    );
    const float evaluatedWeight = MeshEmitterDetail::Evaluate_FloatCurve(
        _desc.meshDirectionAlign.alignmentProgress,
        delayedLifeProgress,
        0.f
    );
    const float scaledWeight = evaluatedWeight * clamp(particle.directionAlignWeightScale, 0.f, 1.f);
    return MeshEmitterDetail::Apply_MeshDirectionAlignBlendMode(scaledWeight, _desc.meshDirectionAlign.blendMode);
}

Quat MeshEmitter::Build_MeshDirectionAlignTargetOrientation(
    const MeshParticleState&,
    const Vec3& visualPosition,
    bool& outEnabled) const
{
    outEnabled = false;
    if (!_desc.meshDirectionAlign.enabled)
        return Quat::Identity;

    Vec3 targetDirection{};
    if (_desc.meshDirectionAlign.targetMode == EffectMeshDirectionAlignTargetMode::Point)
    {
        const Vec3 targetPoint = _desc.meshDirectionAlign.space == EffectMeshDirectionAlignSpace::Local
                                 ? Resolve_LocalPoint(_desc.meshDirectionAlign.target)
                                 : Resolve_WorldPointToSimulation(_desc.meshDirectionAlign.target);
        targetDirection = targetPoint - visualPosition;
    }
    else
    {
        targetDirection = _desc.meshDirectionAlign.space == EffectMeshDirectionAlignSpace::Local
                          ? Resolve_LocalVector(_desc.meshDirectionAlign.target)
                          : Resolve_WorldVectorToSimulation(_desc.meshDirectionAlign.target);
    }

    if (targetDirection.LengthSquared() <= MeshEmitterDetail::kMeshDirectionAlignEpsilon * MeshEmitterDetail::kMeshDirectionAlignEpsilon)
    {
        if (!_meshDirectionAlignFallbackLogged)
        {
            LOG_WARN(
                "MeshEmitter MeshDirectionAlignOverLife fallback: target direction is degenerate. emitter='{}'",
                _emitterName
            );
            _meshDirectionAlignFallbackLogged = true;
        }
        return Quat::Identity;
    }

    Vec3 targetForward = targetDirection;
    targetForward.Normalize();
    Vec3 targetUp = Resolve_WorldVectorToSimulation(Vec3{ 0.f, 1.f, 0.f });
    targetUp = MeshEmitterDetail::Normalize_OrFallback(targetUp, Vec3{ 0.f, 1.f, 0.f });
    if (fabsf(targetForward.Dot(targetUp)) > 1.f - 1e-3f)
        targetUp = MeshEmitterDetail::Normalize_OrFallback(Resolve_WorldVectorToSimulation(Vec3{ 0.f, 0.f, 1.f }), Vec3{ 0.f, 0.f, 1.f });
    if (fabsf(targetForward.Dot(targetUp)) > 1.f - 1e-3f)
        targetUp = Vec3{ 1.f, 0.f, 0.f };

    Vec3 targetRight = targetUp.Cross(targetForward);
    targetRight = MeshEmitterDetail::Normalize_OrFallback(targetRight, Vec3{ 1.f, 0.f, 0.f });
    targetUp = MeshEmitterDetail::Normalize_OrFallback(targetForward.Cross(targetRight), Vec3{ 0.f, 1.f, 0.f });

    const Vec3 sourceForward = MeshEmitterDetail::Resolve_PlaneRadialOrientationAxis(_desc.meshDirectionAlign.meshForwardAxis);
    Vec3 sourceUp = MeshEmitterDetail::Resolve_PlaneRadialOrientationAxis(_desc.meshDirectionAlign.meshUpAxis);
    if (fabsf(sourceForward.Dot(sourceUp)) > 1.f - 1e-3f)
    {
        if (!_meshDirectionAlignFallbackLogged)
        {
            LOG_WARN(
                "MeshEmitter MeshDirectionAlignOverLife fallback: meshForwardAxis and meshUpAxis are parallel. emitter='{}'",
                _emitterName
            );
            _meshDirectionAlignFallbackLogged = true;
        }
        return Quat::Identity;
    }

    Vec3 sourceRight = sourceUp.Cross(sourceForward);
    sourceRight.Normalize();
    sourceUp = sourceForward.Cross(sourceRight);
    sourceUp.Normalize();

    const Matrix sourceFrame = MeshEmitterDetail::Build_FrameMatrix(sourceRight, sourceUp, sourceForward);
    const Matrix targetFrame = MeshEmitterDetail::Build_FrameMatrix(targetRight, targetUp, targetForward);
    Quat orientation = Quat::CreateFromRotationMatrix(sourceFrame.Invert() * targetFrame);
    orientation.Normalize();
    outEnabled = true;
    return orientation;
}

void MeshEmitter::Sample_InitialVelocityChannels(
    MeshParticleState& particle,
    const Vec3& spawnPosition,
    const PlaneRadialSample& planeRadialSample,
    const SourceMotionVelocityContext& sourceMotionContext,
    uint32 seed) const
{
    particle.velocity = Vec3{};
    particle.initialVelocity = Vec3{};
    particle.initialRadialVelocity = Vec3{};
    particle.velocityCone = Vec3{};
    particle.sourceMotionVelocity = Vec3{};
    particle.accelerationIntegratedVelocity = Vec3{};

    const uint32 initialVelocitySeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.motion.initialVelocitySeed, _effectPlaybackSeed);
    const uint32 initialRadialVelocitySeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.motion.initialRadialVelocitySeed, _effectPlaybackSeed);
    const uint32 velocityConeSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.motion.velocityConeSeed, _effectPlaybackSeed);

    if (_desc.motion.initialVelocityEnabled)
    {
        const Vec3 sampledVelocity{
            RandomRange(_desc.motion.initialVelocityMin.x, _desc.motion.initialVelocityMax.x, initialVelocitySeed + 1u),
            RandomRange(_desc.motion.initialVelocityMin.y, _desc.motion.initialVelocityMax.y, initialVelocitySeed + 2u),
            RandomRange(_desc.motion.initialVelocityMin.z, _desc.motion.initialVelocityMax.z, initialVelocitySeed + 3u)
        };
        particle.initialVelocity += _desc.motion.initialVelocityInWorldSpace
                                    ? Resolve_WorldVectorToSimulation(sampledVelocity)
                                    : Resolve_LocalVector(sampledVelocity);
    }

    if (_desc.motion.initialRadialVelocityEnabled)
    {
        const Vec3 radialPivotOffset = _desc.motion.initialRadialVelocityInWorldSpace
                                       ? Resolve_WorldVectorToSimulation(_desc.motion.radialPivot)
                                       : Resolve_LocalVector(_desc.motion.radialPivot);
        Vec3 emitterPosition{};
        if (!_useLocalSpace && _transformCom != nullptr)
            emitterPosition = _transformCom->Get_WorldPosition();
        const Vec3 pivotPosition = emitterPosition + radialPivotOffset;
        Vec3 radialDirection = spawnPosition - pivotPosition;
        if (radialDirection.LengthSquared() < 0.0001f)
        {
            if (_desc.motion.initialRadialVelocityCenterDirectionMode ==
                PointParticleInitialRadialVelocityCenterDirectionMode::PlaneRadial &&
                planeRadialSample.enabled)
                radialDirection = Resolve_LocalVector(planeRadialSample.radialDirection);
            else
            {
                radialDirection = Resolve_LocalVector(
                    Vec3{
                        Random01(initialRadialVelocitySeed + 307u) * 2.f - 1.f,
                        0.25f,
                        Random01(initialRadialVelocitySeed + 401u) * 2.f - 1.f
                    }
                );
            }
        }
        radialDirection.Normalize();

        const float radialSpeed = RandomRange(
            min(_desc.motion.radialSpeed.x, _desc.motion.radialSpeed.y),
            max(_desc.motion.radialSpeed.x, _desc.motion.radialSpeed.y),
            initialRadialVelocitySeed + 5u
        );
        particle.initialRadialVelocity += radialDirection * radialSpeed;
    }

    if (_desc.motion.velocityConeEnabled)
    {
        const Vec3 axis = MeshEmitterDetail::Normalize_OrFallback(_desc.motion.velocityConeAxis, Vec3{ 0.f, 1.f, 0.f });
        Vec3 sampledDirection = axis;
        const float angleDegrees = clamp(_desc.motion.velocityConeAngleDegrees, 0.f, 180.f);
        if (angleDegrees > 0.0001f)
        {
            const float cosMax = cosf(XMConvertToRadians(angleDegrees));
            const float cosTheta = lerp(1.f, cosMax, Random01(velocityConeSeed + 1703u));
            const float sinTheta = sqrtf(max(0.f, 1.f - cosTheta * cosTheta));
            const float phi = XM_2PI * Random01(velocityConeSeed + 1721u);
            const Vec3 helper = fabsf(axis.y) < 0.999f ? Vec3{ 0.f, 1.f, 0.f } : Vec3{ 1.f, 0.f, 0.f };
            Vec3 tangent = helper.Cross(axis);
            tangent.Normalize();
            const Vec3 bitangent = axis.Cross(tangent);
            sampledDirection = axis * cosTheta + (tangent * cosf(phi) + bitangent * sinf(phi)) * sinTheta;
        }

        const Vec3 coneDirection = _desc.motion.velocityConeInWorldSpace
                                   ? Resolve_WorldVectorToSimulation(sampledDirection)
                                   : Resolve_LocalVector(sampledDirection);
        const float coneSpeed = RandomRange(
            min(_desc.motion.velocityConeSpeed.x, _desc.motion.velocityConeSpeed.y),
            max(_desc.motion.velocityConeSpeed.x, _desc.motion.velocityConeSpeed.y),
            velocityConeSeed + 1747u
        );
        particle.velocityCone += coneDirection * coneSpeed;
    }

    if (_desc.motion.sourceMotionVelocityEnabled)
    {
        const uint32 sourceMotionSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.motion.sourceMotionVelocitySeed, _effectPlaybackSeed);
        const bool requiresSourceVelocity =
            _desc.motion.sourceMotionVelocityDirectionMode == PointParticleSourceMotionVelocityDirectionMode::InheritSourceVelocity ||
            _desc.motion.sourceMotionVelocityDirectionMode == PointParticleSourceMotionVelocityDirectionMode::SourceVelocityDirection ||
            _desc.motion.sourceMotionVelocityDirectionMode == PointParticleSourceMotionVelocityDirectionMode::SourceVelocityOpposite;
        if (requiresSourceVelocity && !sourceMotionContext.sourceVelocityValid)
            return;

        const Vec3 fallbackForward = Resolve_LocalVector(Vec3{ 0.f, 0.f, 1.f });
        const Vec3 sourceDirection = sourceMotionContext.sourceVelocityValid
                                     ? MeshEmitterDetail::Normalize_OrFallback(sourceMotionContext.sourceVelocity, fallbackForward)
                                     : fallbackForward;
        const Vec3 tangent = MeshEmitterDetail::Normalize_OrFallback(sourceMotionContext.tangent, sourceDirection);
        const Vec3 worldUp = Resolve_WorldVectorToSimulation(Vec3{ 0.f, 1.f, 0.f });
        Vec3 side = tangent.Cross(worldUp);
        if (side.LengthSquared() <= MeshEmitterDetail::kPlaneRadialEpsilon * MeshEmitterDetail::kPlaneRadialEpsilon)
            side = tangent.Cross(Resolve_WorldVectorToSimulation(Vec3{ 0.f, 0.f, 1.f }));
        side = MeshEmitterDetail::Normalize_OrFallback(side, Resolve_LocalVector(Vec3{ 1.f, 0.f, 0.f }));

        Vec3 resolvedDirection = tangent;
        switch (_desc.motion.sourceMotionVelocityDirectionMode)
        {
        case PointParticleSourceMotionVelocityDirectionMode::InheritSourceVelocity:
        case PointParticleSourceMotionVelocityDirectionMode::SourceVelocityDirection:
            resolvedDirection = sourceDirection;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::SourceVelocityOpposite:
            resolvedDirection = -sourceDirection;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::TrailTangentOpposite:
            resolvedDirection = -tangent;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::SideFromTangent:
            resolvedDirection = side;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::RandomSideFromTangent:
            resolvedDirection = MeshEmitterDetail::Hash01(sourceMotionSeed + 3119u) < 0.5f ? side : -side;
            break;
        case PointParticleSourceMotionVelocityDirectionMode::TrailTangent:
        default:
            resolvedDirection = tangent;
            break;
        }

        resolvedDirection = MeshEmitterDetail::Normalize_OrFallback(resolvedDirection, fallbackForward);
        if (_desc.motion.sourceMotionVelocityDirectionMode != PointParticleSourceMotionVelocityDirectionMode::InheritSourceVelocity &&
            _desc.motion.sourceMotionVelocitySpreadAngleDegrees > 0.f)
        {
            resolvedDirection = MeshEmitterDetail::Sample_ConeDirection(
                resolvedDirection,
                _desc.motion.sourceMotionVelocitySpreadAngleDegrees,
                sourceMotionSeed + 3251u
            );
        }

        const float sampledSpeed = RandomRange(
            min(_desc.motion.sourceMotionVelocitySpeed.x, _desc.motion.sourceMotionVelocitySpeed.y),
            max(_desc.motion.sourceMotionVelocitySpeed.x, _desc.motion.sourceMotionVelocitySpeed.y),
            sourceMotionSeed + 2357u
        );
        const float sourceSpeed = sourceMotionContext.sourceVelocityValid
                                  ? sourceMotionContext.sourceVelocity.Length()
                                  : 0.f;

        if (_desc.motion.sourceMotionVelocityDirectionMode == PointParticleSourceMotionVelocityDirectionMode::InheritSourceVelocity)
        {
            particle.sourceMotionVelocity += sourceMotionContext.sourceVelocity * _desc.motion.sourceMotionVelocitySourceSpeedScale;
            particle.sourceMotionVelocity += sourceDirection * sampledSpeed;
        }
        else
        {
            const float resolvedSpeed =
                sampledSpeed + sourceSpeed * _desc.motion.sourceMotionVelocitySourceSpeedScale;
            particle.sourceMotionVelocity += resolvedDirection * resolvedSpeed;
        }
    }
}

void MeshEmitter::Update_SourceMotionVelocity(float timeDelta)
{
    Vec3 currentCenter{};
    if (nullptr != _transformCom)
        currentCenter = _transformCom->Get_WorldPosition();

    _sourceMotionVelocity = Vec3{};
    _sourceMotionVelocityValid = false;

    if (_hasLastSourceMotionCenter && timeDelta > 0.f)
    {
        const Vec3 movement = currentCenter - _lastSourceMotionCenter;
        if (movement.LengthSquared() > 0.0001f * 0.0001f)
        {
            const Vec3 worldVelocity = movement / max(timeDelta, 0.0001f);
            _sourceMotionVelocity = Resolve_WorldVectorToSimulation(worldVelocity);
            _sourceMotionTangent = MeshEmitterDetail::Normalize_OrFallback(
                _sourceMotionVelocity,
                Resolve_LocalVector(Vec3{ 0.f, 0.f, 1.f })
            );
            _sourceMotionVelocityValid = true;
        }
    }

    if (!_sourceMotionVelocityValid)
        _sourceMotionTangent = Resolve_LocalVector(Vec3{ 0.f, 0.f, 1.f });

    _lastSourceMotionCenter = currentCenter;
    _hasLastSourceMotionCenter = true;
}

Vec3 MeshEmitter::Sample_Acceleration(uint32 seed) const
{
    const uint32 accelerationSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.motion.accelerationSeed, _effectPlaybackSeed);
    const Vec3 sampledAcceleration{
        RandomRange(_desc.motion.accelerationMin.x, _desc.motion.accelerationMax.x, accelerationSeed + 1u),
        RandomRange(_desc.motion.accelerationMin.y, _desc.motion.accelerationMax.y, accelerationSeed + 2u),
        RandomRange(_desc.motion.accelerationMin.z, _desc.motion.accelerationMax.z, accelerationSeed + 3u)
    };
    return _desc.motion.accelerationInWorldSpace
           ? Resolve_WorldVectorToSimulation(sampledAcceleration)
           : Resolve_LocalVector(sampledAcceleration);
}

float MeshEmitter::Sample_Drag(uint32 seed) const
{
    const float minDrag = min(_desc.motion.drag.x, _desc.motion.drag.y);
    const float maxDrag = max(_desc.motion.drag.x, _desc.motion.drag.y);
    const uint32 dragSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.motion.dragSeed, _effectPlaybackSeed);
    return max(0.f, RandomRange(minDrag, maxDrag, dragSeed));
}

Vec4 MeshEmitter::Sample_Color(
    const Vec4& minColor,
    const Vec4& maxColor,
    const PointParticleRandomSeedRuntimeDesc& colorSeed,
    const PointParticleRandomSeedRuntimeDesc& alphaSeed,
    uint32 seed) const
{
    const uint32 rgbSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(colorSeed, _effectPlaybackSeed);
    const uint32 resolvedAlphaSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(alphaSeed, _effectPlaybackSeed);
    return Vec4{
        RandomRange(min(minColor.x, maxColor.x), max(minColor.x, maxColor.x), rgbSeed + 1u),
        RandomRange(min(minColor.y, maxColor.y), max(minColor.y, maxColor.y), rgbSeed + 2u),
        RandomRange(min(minColor.z, maxColor.z), max(minColor.z, maxColor.z), rgbSeed + 3u),
        RandomRange(min(minColor.w, maxColor.w), max(minColor.w, maxColor.w), resolvedAlphaSeed + 4u)
    };
}

Vec3 MeshEmitter::Sample_InitialScale(uint32 seed) const
{
    if (!_desc.meshTransform.initialScaleEnabled)
        return Vec3{ 1.f, 1.f, 1.f };

    const uint32 initialScaleSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.meshTransform.initialScaleSeed, _effectPlaybackSeed);
    return Vec3{
        max(0.0001f, RandomRange(_desc.meshTransform.initialScaleMin.x, _desc.meshTransform.initialScaleMax.x, initialScaleSeed + 1u)),
        max(0.0001f, RandomRange(_desc.meshTransform.initialScaleMin.y, _desc.meshTransform.initialScaleMax.y, initialScaleSeed + 2u)),
        max(0.0001f, RandomRange(_desc.meshTransform.initialScaleMin.z, _desc.meshTransform.initialScaleMax.z, initialScaleSeed + 3u))
    };
}

Vec3 MeshEmitter::Sample_InitialRotation(uint32 seed) const
{
    if (!_desc.meshTransform.rotationEnabled)
        return Vec3{};

    const uint32 initialRotationSeed = seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.meshTransform.initialRotationSeed, _effectPlaybackSeed);
    return Vec3{
        XMConvertToRadians(
            RandomRange(_desc.meshTransform.initialRotationDegreesMin.x, _desc.meshTransform.initialRotationDegreesMax.x, initialRotationSeed + 1u)
        ),
        XMConvertToRadians(
            RandomRange(_desc.meshTransform.initialRotationDegreesMin.y, _desc.meshTransform.initialRotationDegreesMax.y, initialRotationSeed + 2u)
        ),
        XMConvertToRadians(
            RandomRange(_desc.meshTransform.initialRotationDegreesMin.z, _desc.meshTransform.initialRotationDegreesMax.z, initialRotationSeed + 3u)
        )
    };
}

Vec3 MeshEmitter::Sample_InitialAngularVelocity(uint32 seed) const
{
    if (!_desc.meshTransform.rotationEnabled)
        return Vec3{};

    const uint32 initialAngularVelocitySeed =
        seed + MeshEmitterDetail::Resolve_SeedSalt(_desc.meshTransform.initialAngularVelocitySeed, _effectPlaybackSeed);
    return Vec3{
        XMConvertToRadians(
            RandomRange(
                _desc.meshTransform.initialAngularVelocityDegreesMin.x,
                _desc.meshTransform.initialAngularVelocityDegreesMax.x,
                initialAngularVelocitySeed + 1u
            )
        ),
        XMConvertToRadians(
            RandomRange(
                _desc.meshTransform.initialAngularVelocityDegreesMin.y,
                _desc.meshTransform.initialAngularVelocityDegreesMax.y,
                initialAngularVelocitySeed + 2u
            )
        ),
        XMConvertToRadians(
            RandomRange(
                _desc.meshTransform.initialAngularVelocityDegreesMin.z,
                _desc.meshTransform.initialAngularVelocityDegreesMax.z,
                initialAngularVelocitySeed + 3u
            )
        )
    };
}

float MeshEmitter::Evaluate_VelocityScaleByLifeChannel(const PointParticleFloatCurveRuntimeDesc& curve, float lifeProgress) const
{
    return max(0.f, MeshEmitterDetail::Evaluate_FloatCurve(curve, lifeProgress, 1.f));
}

Vec3 MeshEmitter::Evaluate_ScaledVelocityChannels(const MeshParticleState& particle, float lifeProgress) const
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
        particle.initialVelocity * initialVelocityScale +
        particle.initialRadialVelocity * initialRadialVelocityScale +
        particle.velocityCone * velocityConeScale +
        particle.sourceMotionVelocity * sourceMotionVelocityScale +
        particle.accelerationIntegratedVelocity * accelerationIntegratedVelocityScale;
}

Vec3 MeshEmitter::Evaluate_AccelerationByLife(const MeshParticleState& particle, float lifeProgress) const
{
    if (!_desc.motion.accelerationCurveEnabled)
        return particle.acceleration;

    const float curveProgress =
        _desc.motion.accelerationTimeBasis == PointParticleAccelerationTimeBasis::EmitterNormalizedTime
        ? clamp(_emitterElapsedTime / Resolve_Duration(), 0.f, 1.f)
        : lifeProgress;
    const uint32 keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, _desc.motion.accelerationCurveKeyCount));
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
            particle.acceleration.x
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
            particle.acceleration.y
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
            particle.acceleration.z
        )
    };
    return _desc.motion.accelerationInWorldSpace
           ? Resolve_WorldVectorToSimulation(acceleration)
           : Resolve_LocalVector(acceleration);
}

Vec3 MeshEmitter::Evaluate_OrbitOffset(const MeshParticleState& particle, float lifeProgress) const
{
    if (!_desc.orbitOverLife.enabled)
        return particle.spawnOffset;

    const float angleDegrees = Evaluate_CompactCurve(
        lifeProgress,
        _desc.orbitOverLife.angleCurveTimes,
        _desc.orbitOverLife.angleCurveTimesBlock1,
        _desc.orbitOverLife.angleCurveValues,
        _desc.orbitOverLife.angleCurveValuesBlock1,
        _desc.orbitOverLife.angleCurveArriveTangents,
        _desc.orbitOverLife.angleCurveArriveTangentsBlock1,
        _desc.orbitOverLife.angleCurveLeaveTangents,
        _desc.orbitOverLife.angleCurveLeaveTangentsBlock1,
        _desc.orbitOverLife.angleCurveModes,
        _desc.orbitOverLife.angleCurveModesBlock1,
        _desc.orbitOverLife.angleCurveKeyCount,
        lerp(_desc.orbitOverLife.angleDegreesOverLife.x, _desc.orbitOverLife.angleDegreesOverLife.y, lifeProgress)
    );
    const float radiusScale = Evaluate_CompactCurve(
        lifeProgress,
        _desc.orbitOverLife.radiusScaleCurveTimes,
        _desc.orbitOverLife.radiusScaleCurveTimesBlock1,
        _desc.orbitOverLife.radiusScaleCurveValues,
        _desc.orbitOverLife.radiusScaleCurveValuesBlock1,
        _desc.orbitOverLife.radiusScaleCurveArriveTangents,
        _desc.orbitOverLife.radiusScaleCurveArriveTangentsBlock1,
        _desc.orbitOverLife.radiusScaleCurveLeaveTangents,
        _desc.orbitOverLife.radiusScaleCurveLeaveTangentsBlock1,
        _desc.orbitOverLife.radiusScaleCurveModes,
        _desc.orbitOverLife.radiusScaleCurveModesBlock1,
        _desc.orbitOverLife.radiusScaleCurveKeyCount,
        lerp(_desc.orbitOverLife.radiusScaleOverLife.x, _desc.orbitOverLife.radiusScaleOverLife.y, lifeProgress)
    );

    Vec3 localOffset = particle.spawnOffset;
    if (!_useLocalSpace && _transformCom != nullptr)
        localOffset = Vec3::TransformNormal(particle.spawnOffset, _transformCom->Get_WorldMatrix().Invert());

    Vec2 planeOffset{ localOffset.x, localOffset.y };
    if (_desc.orbitOverLife.plane == EffectOrbitPlane::XZ)
        planeOffset = Vec2{ localOffset.x, localOffset.z };
    else if (_desc.orbitOverLife.plane == EffectOrbitPlane::YZ)
        planeOffset = Vec2{ localOffset.y, localOffset.z };

    planeOffset *= radiusScale;
    const float angleRadians = XMConvertToRadians(angleDegrees);
    const float angleCos = cosf(angleRadians);
    const float angleSin = sinf(angleRadians);
    const Vec2 rotatedOffset{
        planeOffset.x * angleCos - planeOffset.y * angleSin,
        planeOffset.x * angleSin + planeOffset.y * angleCos
    };

    if (_desc.orbitOverLife.plane == EffectOrbitPlane::XZ)
    {
        localOffset.x = rotatedOffset.x;
        localOffset.z = rotatedOffset.y;
    }
    else if (_desc.orbitOverLife.plane == EffectOrbitPlane::YZ)
    {
        localOffset.y = rotatedOffset.x;
        localOffset.z = rotatedOffset.y;
    }
    else
    {
        localOffset.x = rotatedOffset.x;
        localOffset.y = rotatedOffset.y;
    }

    return Resolve_LocalVector(localOffset);
}

Vec3 MeshEmitter::Evaluate_ScaleByLife(float lifeProgress) const
{
    return Evaluate_MeshVector3Curve(_desc.meshTransform.scaleByLife, lifeProgress, Vec3{ 1.f, 1.f, 1.f });
}

Vec3 MeshEmitter::Evaluate_RotationByLife(float lifeProgress) const
{
    return Evaluate_MeshVector3Curve(_desc.meshTransform.rotationByLife, lifeProgress, Vec3{});
}

Vec3 MeshEmitter::Evaluate_AngularVelocityScaleByLife(float lifeProgress) const
{
    return Evaluate_MeshVector3Curve(_desc.meshTransform.angularVelocityScaleByLife, lifeProgress, Vec3{ 1.f, 1.f, 1.f });
}

Vec4 MeshEmitter::Evaluate_ColorOverLife(const MeshParticleState& particle, float lifeProgress) const
{
    Vec4 color = {
        lerp(particle.startColor.x, particle.endColor.x, lifeProgress),
        lerp(particle.startColor.y, particle.endColor.y, lifeProgress),
        lerp(particle.startColor.z, particle.endColor.z, lifeProgress),
        lerp(particle.startColor.w, particle.endColor.w, lifeProgress)
    };

    const PointParticleColorOverLifeCurveDesc& curve = _desc.colorOverLife.curve;
    if (curve.colorCurveEnabled)
    {
        const uint32 keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, curve.colorCurveKeyCount));
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
        const uint32 keyCount = max(1u, min(kEffectDistributionCurveMaxKeys, curve.alphaCurveKeyCount));
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

float MeshEmitter::Evaluate_CompactCurve(
    float lifeProgress,
    const Vec4& times,
    const Vec4& timesBlock1,
    const Vec4& values,
    const Vec4& valuesBlock1,
    uint32 keyCount,
    float fallbackValue) const
{
    const uint32 clampedKeyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= MeshEmitterDetail::Read_Vec4Component(times, timesBlock1, 0u))
        return MeshEmitterDetail::Read_Vec4Component(values, valuesBlock1, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = MeshEmitterDetail::Read_Vec4Component(times, timesBlock1, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = MeshEmitterDetail::Read_Vec4Component(times, timesBlock1, index - 1u);
        const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
        return lerp(
            MeshEmitterDetail::Read_Vec4Component(values, valuesBlock1, index - 1u),
            MeshEmitterDetail::Read_Vec4Component(values, valuesBlock1, index),
            ratio
        );
    }

    return lifeProgress > MeshEmitterDetail::Read_Vec4Component(times, timesBlock1, clampedKeyCount - 1u)
           ? MeshEmitterDetail::Read_Vec4Component(values, valuesBlock1, clampedKeyCount - 1u)
           : fallbackValue;
}

float MeshEmitter::Evaluate_CompactCurve(
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
    const uint32 clampedKeyCount = max(1u, min(kEffectDistributionCurveMaxKeys, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= MeshEmitterDetail::Read_Vec4Component(times, timesBlock1, 0u))
        return MeshEmitterDetail::Read_Vec4Component(values, valuesBlock1, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = MeshEmitterDetail::Read_Vec4Component(times, timesBlock1, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = MeshEmitterDetail::Read_Vec4Component(times, timesBlock1, index - 1u);
        const float leftValue = MeshEmitterDetail::Read_Vec4Component(values, valuesBlock1, index - 1u);
        const float rightValue = MeshEmitterDetail::Read_Vec4Component(values, valuesBlock1, index);
        const float mode = MeshEmitterDetail::Read_Vec4Component(modes, modesBlock1, index - 1u);
        const float width = max(0.0001f, rightTime - leftTime);
        const float ratio = clamp((lifeProgress - leftTime) / width, 0.f, 1.f);

        if (mode < 0.5f)
            return leftValue;

        if (mode >= 1.5f)
        {
            const float t2 = ratio * ratio;
            const float t3 = t2 * ratio;
            const float leftLeave = MeshEmitterDetail::Read_Vec4Component(leaveTangents, leaveTangentsBlock1, index - 1u) * width;
            const float rightArrive = MeshEmitterDetail::Read_Vec4Component(arriveTangents, arriveTangentsBlock1, index) * width;
            return
                (2.f * t3 - 3.f * t2 + 1.f) * leftValue +
                (t3 - 2.f * t2 + ratio) * leftLeave +
                (-2.f * t3 + 3.f * t2) * rightValue +
                (t3 - t2) * rightArrive;
        }

        return lerp(leftValue, rightValue, ratio);
    }

    return lifeProgress > MeshEmitterDetail::Read_Vec4Component(times, timesBlock1, clampedKeyCount - 1u)
           ? MeshEmitterDetail::Read_Vec4Component(values, valuesBlock1, clampedKeyCount - 1u)
           : fallbackValue;
}

Vec3 MeshEmitter::Evaluate_MeshVector3Curve(const MeshVector3CurveRuntimeDesc& desc, float lifeProgress, const Vec3& fallbackValue) const
{
    if (!desc.enabled)
        return fallbackValue;

    return Vec3{
        Evaluate_CompactCurve(
            lifeProgress,
            desc.curveKeyTimes,
            desc.curveKeyTimesBlock1,
            desc.curveKeyValuesX,
            desc.curveKeyValuesXBlock1,
            desc.curveKeyArriveTangentsX,
            desc.curveKeyArriveTangentsXBlock1,
            desc.curveKeyLeaveTangentsX,
            desc.curveKeyLeaveTangentsXBlock1,
            desc.curveKeyModes,
            desc.curveKeyModesBlock1,
            desc.curveKeyCount,
            lerp(desc.start.x, desc.end.x, lifeProgress)
        ),
        Evaluate_CompactCurve(
            lifeProgress,
            desc.curveKeyTimes,
            desc.curveKeyTimesBlock1,
            desc.curveKeyValuesY,
            desc.curveKeyValuesYBlock1,
            desc.curveKeyArriveTangentsY,
            desc.curveKeyArriveTangentsYBlock1,
            desc.curveKeyLeaveTangentsY,
            desc.curveKeyLeaveTangentsYBlock1,
            desc.curveKeyModes,
            desc.curveKeyModesBlock1,
            desc.curveKeyCount,
            lerp(desc.start.y, desc.end.y, lifeProgress)
        ),
        Evaluate_CompactCurve(
            lifeProgress,
            desc.curveKeyTimes,
            desc.curveKeyTimesBlock1,
            desc.curveKeyValuesZ,
            desc.curveKeyValuesZBlock1,
            desc.curveKeyArriveTangentsZ,
            desc.curveKeyArriveTangentsZBlock1,
            desc.curveKeyLeaveTangentsZ,
            desc.curveKeyLeaveTangentsZBlock1,
            desc.curveKeyModes,
            desc.curveKeyModesBlock1,
            desc.curveKeyCount,
            lerp(desc.start.z, desc.end.z, lifeProgress)
        )
    };
}

RenderGroup MeshEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    if (Is_DistortionFamily())
        return RenderGroup::Distortion;
    if (Is_MeshGlassFamily())
        return RenderGroup::Blend;

    return _desc.material.blendMode == EffectMaterialBlendMode::Masked
           ? RenderGroup::EffectMasked
           : RenderGroup::Blend;
}

uint32 MeshEmitter::Resolve_ShaderPassIndex() const
{
    if (Is_DistortionFamily())
        return 0u;
    if (Is_MeshGlassFamily())
        return 0u;

    const bool additive = _desc.material.blendMode == EffectMaterialBlendMode::Additive;
    const bool masked = _desc.material.blendMode == EffectMaterialBlendMode::Masked;
    const bool twoSided = _desc.material.twoSided;
    if (additive)
        return twoSided ? 3u : 1u;
    if (masked)
        return twoSided ? 5u : 4u;
    return twoSided ? 2u : 0u;
}

int MeshEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.material.opacitySource == "Alpha" || _desc.material.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.material.opacitySource == "Red" || _desc.material.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.material.opacitySource == "Luminance" || _desc.material.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    return kOpacitySourceAlpha;
}

Vec3 MeshEmitter::Resolve_LocalVector(const Vec3& localVector) const
{
    if (_useLocalSpace || _transformCom == nullptr)
        return localVector;

    return _transformCom->Get_WorldRight() * localVector.x +
           _transformCom->Get_WorldUp() * localVector.y +
           _transformCom->Get_WorldForward() * localVector.z;
}

Vec3 MeshEmitter::Resolve_WorldVectorToSimulation(const Vec3& worldVector) const
{
    if (!_useLocalSpace || _transformCom == nullptr)
        return worldVector;

    const Matrix inverseWorld = _transformCom->Get_WorldMatrix().Invert();
    return Vec3::TransformNormal(worldVector, inverseWorld);
}

Vec3 MeshEmitter::Resolve_LocalPoint(const Vec3& localPoint) const
{
    if (_useLocalSpace || _transformCom == nullptr)
        return localPoint;

    return Vec3::Transform(localPoint, _transformCom->Get_WorldMatrix());
}

Vec3 MeshEmitter::Resolve_WorldPointToSimulation(const Vec3& worldPoint) const
{
    if (!_useLocalSpace || _transformCom == nullptr)
        return worldPoint;

    const Matrix inverseWorld = _transformCom->Get_WorldMatrix().Invert();
    return Vec3::Transform(worldPoint, inverseWorld);
}

float MeshEmitter::Random01(uint32 seed) const
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}

float MeshEmitter::RandomRange(float minValue, float maxValue, uint32 seed) const
{
    if (minValue > maxValue)
        std::swap(minValue, maxValue);

    return lerp(minValue, maxValue, Random01(seed));
}

Shared<MeshEmitter> MeshEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MeshEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : MeshEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MeshEmitter::Clone(void* arg)
{
    auto instance = make_shared<MeshEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : MeshEmitter");
        MSG_BOX("Failed to Clone : MeshEmitter");
        return nullptr;
    }

    return instance;
}

void MeshEmitter::Free()
{
    _desc = {};
    _model.reset();
    _shader.reset();
    _texture.reset();
    _noiseTexture.reset();
    _maskTexture.reset();
    _instanceBuffer.Reset();
    _particles.clear();
    _instancePayload.clear();
    _burstExecuted.clear();
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _playbackState = PlaybackState::Delayed;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;
    _activeInstanceCount = 0u;
    _nextSpawnCursor = 0u;
    _instanceCapacity = 0u;
    _opacitySourceIndex = kOpacitySourceAlpha;
    _isReady = false;

    __super::Free();
}

NS_END
