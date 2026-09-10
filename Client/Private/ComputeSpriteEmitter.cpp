#include "ComputeSpriteEmitter.h"

#include "Battle_Enum.h"
#include "EffectMaterialScalarModulationRuntime.h"
#include "GameInstance.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_ComputePointParticle.h"

IMPLEMENT_REFLECTION(ComputeSpriteEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ComputeSpriteEmitter";
    info.category = "EffectEmitter";

    return true;
}

void ComputeSpriteEmitter::Set_InfluenceOutlineDebugEnabled(bool enabled)
{
    _isInfluenceOutlineDebugEnabled = enabled;
}

bool ComputeSpriteEmitter::Is_InfluenceOutlineDebugEnabled()
{
    return _isInfluenceOutlineDebugEnabled;
}

namespace Client::EffectAssetRuntimeLoad
{
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

static uint32 Resolve_SeedSalt(const PointParticleRandomSeedRuntimeDesc& seed, uint32 effectPlaybackSeed)
{
    uint32 salt = seed.manualSeedEnabled ? seed.seed : 0u;
    if (seed.useInstanceSeed)
        salt += effectPlaybackSeed;
    return salt;
}

static float Hash01(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
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

static Vec3 Resolve_WorldScale(const Shared<TransformCom>& transform)
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

static Vec3 Scale_Vector3(const Vec3& value, const Vec3& scale)
{
    return Vec3{ value.x * scale.x, value.y * scale.y, value.z * scale.z };
}

static Vec2 Scale_Vector2(const Vec2& value, const Vec2& scale)
{
    return Vec2{ value.x * scale.x, value.y * scale.y };
}

static Vec2 Resolve_PlaneScale(PointParticlePlaneRadialLocationPlane plane, const Vec3& scale)
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

static Vec2 Resolve_CylinderScale(PointParticleCylinderLocationAxis axis, const Vec3& scale)
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

static bool Nearly_EqualScale(const Vec3& lhs, const Vec3& rhs)
{
    constexpr float scaleEpsilon = 0.0001f;
    return fabsf(lhs.x - rhs.x) <= scaleEpsilon &&
           fabsf(lhs.y - rhs.y) <= scaleEpsilon &&
           fabsf(lhs.z - rhs.z) <= scaleEpsilon;
}

static HRESULT Bind_MaterialScalarModulationPayload(
    ShaderCom* shader,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float emitterPhase)
{
    CHECK_NULL(shader, E_FAIL);

    const EffectMaterialScalarModulationShaderPayload payload =
        Build_MaterialScalarModulationShaderPayload(modulation, emitterPhase);

    HRESULT hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationParams",
        &payload.params,
        sizeof(payload.params)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationMeta",
        payload.meta.data(),
        sizeof(payload.meta)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyTimes",
        payload.keyTimes.data(),
        sizeof(payload.keyTimes)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyTimesBlock1",
        payload.keyTimesBlock1.data(),
        sizeof(payload.keyTimesBlock1)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyValues",
        payload.keyValues.data(),
        sizeof(payload.keyValues)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyValuesBlock1",
        payload.keyValuesBlock1.data(),
        sizeof(payload.keyValuesBlock1)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyArriveTangents",
        payload.keyArriveTangents.data(),
        sizeof(payload.keyArriveTangents)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyArriveTangentsBlock1",
        payload.keyArriveTangentsBlock1.data(),
        sizeof(payload.keyArriveTangentsBlock1)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyLeaveTangents",
        payload.keyLeaveTangents.data(),
        sizeof(payload.keyLeaveTangents)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyLeaveTangentsBlock1",
        payload.keyLeaveTangentsBlock1.data(),
        sizeof(payload.keyLeaveTangentsBlock1)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyModes",
        payload.keyModes.data(),
        sizeof(payload.keyModes)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    hr = shader->Bind_RawValue(
        "g_EffectMaterialScalarModulationKeyModesBlock1",
        payload.keyModesBlock1.data(),
        sizeof(payload.keyModesBlock1)
    );
    CHECK_FAILED_ONCE(hr, E_FAIL);

    return S_OK;
}

wstring Resolve_TexturePathByGuid(const string& textureGuid)
{
    if (nullptr == GAME || textureGuid.empty())
        return {};

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
    if (nullptr == assetMeta || assetMeta->type != "Texture")
        return {};

    const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
    return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
}

wstring Resolve_TexturePathByPath(const string& texturePath)
{
    if (!texturePath.empty())
    {
        fs::path candidatePath = String::ToWString(texturePath);
        if (candidatePath.is_relative() && GAME != nullptr)
            candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

        candidatePath = candidatePath.lexically_normal();
        if (fs::exists(candidatePath))
            return candidatePath.wstring();
    }

    return {};
}

bool Is_SpriteTextureSamePath(const wstring& lhs, const wstring& rhs)
{
    if (lhs.empty() || rhs.empty())
        return false;

    return fs::path(lhs).lexically_normal() == fs::path(rhs).lexically_normal();
}

wstring Resolve_TexturePath(const string& textureGuid, const string& texturePath, bool& outUsedPathFallback, bool& outGuidPathMismatch)
{
    outUsedPathFallback = false;
    outGuidPathMismatch = false;

    wstring resolvedPath = Resolve_TexturePathByGuid(textureGuid);
    if (!resolvedPath.empty())
    {
        const wstring pathResolved = Resolve_TexturePathByPath(texturePath);
        outGuidPathMismatch = !pathResolved.empty() && !Is_SpriteTextureSamePath(resolvedPath, pathResolved);
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

ComputeSpriteEmitter::ComputeSpriteEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

ComputeSpriteEmitter::ComputeSpriteEmitter(const ComputeSpriteEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

ComputeSpriteEmitter::~ComputeSpriteEmitter()
{
    Free();
}

HRESULT ComputeSpriteEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeSpriteEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputeSpriteEmitterDesc*>(arg);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    Reset_PlaybackRuntime();

    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void ComputeSpriteEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();

    if (nullptr == _viBuffer)
        return;

    if (PlaybackState::Completed == _playbackState && !_viBuffer->Has_ActiveParticles())
        return;

    const PlaybackTick playbackTick = Advance_Playback(timeDelta);
    if (playbackTick.computeDeltaTime <= 0.f &&
        playbackTick.spawnRequest == 0u &&
        !playbackTick.killActiveParticles)
        return;

    _viBuffer->Dispatch_Compute(
        playbackTick.computeDeltaTime,
        playbackTick.spawnRequest,
        playbackTick.spawnSerialBase,
        playbackTick.lifetimeSamplePhase,
        _emitterElapsedTime,
        playbackTick.killActiveParticles
    );
}

void ComputeSpriteEmitter::Late_Update(float timeDelta)
{
    if (Can_SubmitRender())
        GAME->Add_RenderGroup(Resolve_RenderGroup(), GetSharedPtr<GameObject>());
}

HRESULT ComputeSpriteEmitter::Render()
{
    CHECK_FAILED_ONCE(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Begin(Resolve_ShaderPassIndex()), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    Set_MaterialRevealOverride(1.f, 0.f);
    Set_MaterialTintOverride(Vec4(1.f, 1.f, 1.f, 1.f), 0.f);
    CHECK_NULL(_viBuffer, E_FAIL);
    Update_RuntimeDescForCurrentScale(true);
    Sync_WorldCenterToComputeBuffer();
    CHECK_FAILED(_viBuffer->Reset_ForEffectReplay(), E_FAIL);

    return S_OK;
}

void ComputeSpriteEmitter::Set_MaterialRevealOverride(float alphaMultiplier, float alphaErosion)
{
    _materialRevealAlphaMultiplier = clamp(alphaMultiplier, 0.f, 1.f);
    _materialRevealAlphaErosion = clamp(alphaErosion, 0.f, 1.f);
}

void ComputeSpriteEmitter::Set_MaterialTintOverride(const Vec4& targetTint, float strength)
{
    _materialTintOverrideTarget = targetTint;
    _materialTintOverrideStrength = clamp(strength, 0.f, 1.f);
}

bool ComputeSpriteEmitter::Is_EffectFinished() const
{
    return PlaybackState::Completed == _playbackState;
}

bool ComputeSpriteEmitter::Collect_FollowerSourcePoints(
    vector<EffectFollowerSourcePoint>& outPoints,
    uint32 maxPointCount) const
{
    return nullptr != _viBuffer && _viBuffer->Collect_FollowerSourcePoints(outPoints, maxPointCount);
}

bool ComputeSpriteEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (nullptr == _transformCom)
        return false;

    const Vec4* cameraPosition4 = GAME != nullptr ? GAME->Get_CamPosition() : nullptr;
    if (nullptr == cameraPosition4)
    {
        outWorldPosition = _transformCom->Get_WorldPosition();
        return true;
    }

    const Vec3 emitterWorldPosition = _transformCom->Get_WorldPosition();

    Vec3 localOffset{};
    if (_desc.initialLocation.enabled)
        localOffset += (_desc.initialLocation.minOffset + _desc.initialLocation.maxOffset) * 0.5f;

    if (_desc.sphereLocation.enabled)
        localOffset += _desc.sphereLocation.offset;

    if (_desc.cylinderLocation.enabled)
        localOffset += _desc.cylinderLocation.offset;

    const Vec3 worldOffset =
        _transformCom->Get_WorldRight() * localOffset.x +
        _transformCom->Get_WorldUp() * localOffset.y +
        _transformCom->Get_WorldForward() * localOffset.z;

    const Vec3 representativeCenter = emitterWorldPosition + worldOffset;

    outWorldPosition = representativeCenter;
    return true;
}

EffectSortPolicy ComputeSpriteEmitter::Get_BlendSortPolicy() const
{
    return _desc.required.sort.sortPolicy;
}

int32 ComputeSpriteEmitter::Get_BlendSortLayer() const
{
    return _desc.required.sort.sortLayer;
}

float ComputeSpriteEmitter::Get_BlendSortBias() const
{
    return _desc.required.sort.artistSortBias;
}

void ComputeSpriteEmitter::On_EffectTransformSynced()
{
    Update_RuntimeDescForCurrentScale(false);
    Sync_WorldCenterToComputeBuffer();
}

HRESULT ComputeSpriteEmitter::Ready_Components()
{
    string resolvedTexturePath{};
    CHECK_FAILED(Ready_TextureComponent(resolvedTexturePath), E_FAIL);
    CHECK_FAILED(Ready_ShaderComponent(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTextureComponent(resolvedTexturePath), E_FAIL);
    CHECK_FAILED(Ready_MaskTextureComponent(resolvedTexturePath), E_FAIL);
    CHECK_FAILED(Ready_FlowTextureComponent(resolvedTexturePath), E_FAIL);

    ComputePointParticleDesc desc = Make_ComputePointParticleDesc();
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect_ComputePointParticleBuffer"), _viBuffer, &desc), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_TextureComponent(string& outResolvedTexturePath)
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = EffectAssetRuntimeLoad::Resolve_TexturePath(
        _desc.required.material.mainTextureGuid,
        _desc.required.material.mainTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    bool usedFallback = false;
    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeSpriteEmitter main texture GUID/path mismatch. mainTextureGuid='{}', mainTexturePath='{}', guidResolvedPath='{}'.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeSpriteEmitter main texture fell back to path. mainTextureGuid='{}', mainTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            String::ToString(resolvedPath)
        );
    }

    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeSpriteEmitter main texture resolve failed. mainTextureGuid='{}', mainTexturePath='{}'. Falling back to fallbackPath='{}'.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            kFallbackTexturePath
        );

        resolvedPath = EffectAssetRuntimeLoad::Resolve_TexturePathByPath(kFallbackTexturePath);
        usedFallback = true;
    }

    if (resolvedPath.empty())
    {
        LOG_ERROR(
            "ComputeSpriteEmitter failed to resolve fallback texture. mainTextureGuid='{}', mainTexturePath='{}', fallbackPath='{}'.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            kFallbackTexturePath
        );
        return E_FAIL;
    }

    outResolvedTexturePath = String::ToString(resolvedPath);
    _texture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_texture == nullptr)
    {
        LOG_ERROR(
            "ComputeSpriteEmitter texture create failed. mainTextureGuid='{}', mainTexturePath='{}', resolvedPath='{}', fallbackPath='{}', usedFallback={}.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            outResolvedTexturePath,
            kFallbackTexturePath,
            usedFallback
        );
        return E_FAIL;
    }

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_ShaderComponent()
{
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), String::ToWString(Resolve_ShaderId()), _shader), E_FAIL);
    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_NoiseTextureComponent(const string& resolvedTexturePath)
{
    if (_desc.required.material.noiseTextureGuid.empty() && _desc.required.material.noiseTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = EffectAssetRuntimeLoad::Resolve_TexturePath(
        _desc.required.material.noiseTextureGuid,
        _desc.required.material.noiseTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
    {
        _noiseTexture = nullptr;
        LOG_WARN(
            "ComputeSpriteEmitter noise texture resolve failed. noiseTextureGuid='{}', noiseTexturePath='{}'.",
            _desc.required.material.noiseTextureGuid,
            _desc.required.material.noiseTexturePath
        );
        return S_OK;
    }
    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeSpriteEmitter noise texture GUID/path mismatch. noiseTextureGuid='{}', noiseTexturePath='{}', guidResolvedPath='{}'.",
            _desc.required.material.noiseTextureGuid,
            _desc.required.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeSpriteEmitter noise texture fell back to path. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.noiseTextureGuid,
            _desc.required.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }

    const string noiseTexturePath = String::ToString(resolvedPath);
    if (noiseTexturePath == resolvedTexturePath && nullptr != _texture)
    {
        _noiseTexture = _texture;
        return S_OK;
    }

    _noiseTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_noiseTexture == nullptr)
    {
        LOG_WARN(
            "ComputeSpriteEmitter noise texture create failed. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.noiseTextureGuid,
            _desc.required.material.noiseTexturePath,
            noiseTexturePath
        );
    }

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_MaskTextureComponent(const string& resolvedTexturePath)
{
    if (_desc.required.material.maskTextureGuid.empty() && _desc.required.material.maskTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = EffectAssetRuntimeLoad::Resolve_TexturePath(
        _desc.required.material.maskTextureGuid,
        _desc.required.material.maskTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
    {
        _maskTexture = nullptr;
        LOG_WARN(
            "ComputeSpriteEmitter mask texture resolve failed. maskTextureGuid='{}', maskTexturePath='{}'.",
            _desc.required.material.maskTextureGuid,
            _desc.required.material.maskTexturePath
        );
        return S_OK;
    }
    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeSpriteEmitter mask texture GUID/path mismatch. maskTextureGuid='{}', maskTexturePath='{}', guidResolvedPath='{}'.",
            _desc.required.material.maskTextureGuid,
            _desc.required.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeSpriteEmitter mask texture fell back to path. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.maskTextureGuid,
            _desc.required.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }

    const string maskTexturePath = String::ToString(resolvedPath);
    if (maskTexturePath == resolvedTexturePath && nullptr != _texture)
    {
        _maskTexture = _texture;
        return S_OK;
    }

    _maskTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_maskTexture == nullptr)
    {
        LOG_WARN(
            "ComputeSpriteEmitter mask texture create failed. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.maskTextureGuid,
            _desc.required.material.maskTexturePath,
            maskTexturePath
        );
    }

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_FlowTextureComponent(const string& resolvedTexturePath)
{
    if (!Is_DistortionFamily())
        return S_OK;

    if (_desc.required.material.flowTextureGuid.empty() && _desc.required.material.flowTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = EffectAssetRuntimeLoad::Resolve_TexturePath(
        _desc.required.material.flowTextureGuid,
        _desc.required.material.flowTexturePath,
        usedPathFallback,
        guidPathMismatch
    );

    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeSpriteEmitter flow texture resolve failed. flowTextureGuid='{}', flowTexturePath='{}'.",
            _desc.required.material.flowTextureGuid,
            _desc.required.material.flowTexturePath
        );
        return S_OK;
    }

    const string flowTexturePath = String::ToString(resolvedPath);
    if (flowTexturePath == resolvedTexturePath && nullptr != _texture)
    {
        _flowTexture = _texture;
        return S_OK;
    }

    _flowTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (nullptr == _flowTexture)
    {
        LOG_WARN(
            "ComputeSpriteEmitter flow texture create failed. flowTextureGuid='{}', flowTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.flowTextureGuid,
            _desc.required.material.flowTexturePath,
            flowTexturePath
        );
    }

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Bind_ShaderResources()
{
    CHECK_FAILED_ONCE(GAME->Bind_CameraCB(_shader), E_FAIL);
    const int screenAlignmentMode = static_cast<int>(_desc.required.spriteRender.screenAlignment);
    const int directionalAlignmentMode = static_cast<int>(_desc.required.spriteRender.directionalAlignmentMode);
    const int spriteTextureAxis = static_cast<int>(_desc.required.spriteRender.spriteTextureAxis);
    const int useInstanceLookDirectionBasis =
        _desc.cylinderOrientation.enabled &&
        _desc.cylinderOrientation.targetKind != PointParticlePlaneRadialOrientationTargetKind::Mesh3D &&
        _desc.cylinderOrientation.orientationMode != PointParticleCylinderOrientationMode::None
        ? 1
        : 0;
    const float spriteRollOffsetRadians = XMConvertToRadians(_desc.required.spriteRender.spriteRollOffsetDegrees);
    const Vec4 emitterCenter = nullptr != _transformCom
                               ? Vec4{ _transformCom->Get_WorldPosition().x, _transformCom->Get_WorldPosition().y, _transformCom->Get_WorldPosition().z, 1.f }
                               : Vec4{ _desc.center.x, _desc.center.y, _desc.center.z, 1.f };
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_ScreenAlignmentMode", &screenAlignmentMode, sizeof(screenAlignmentMode)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_DirectionalAlignmentMode", &directionalAlignmentMode, sizeof(directionalAlignmentMode)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_SpriteTextureAxis", &spriteTextureAxis, sizeof(spriteTextureAxis)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_UseInstanceLookDirectionBasis", &useInstanceLookDirectionBasis, sizeof(useInstanceLookDirectionBasis)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_SpriteRollOffsetRadians", &spriteRollOffsetRadians, sizeof(spriteRollOffsetRadians)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteEmitterCenter", &emitterCenter, sizeof(emitterCenter)), E_FAIL);
    // Sprite material은 main texture를 lit base color가 아니라 emissive-like color와 opacity source로 해석한다.
    CHECK_FAILED_ONCE(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    CHECK_FAILED_ONCE(Bind_MaterialResources(), E_FAIL);
    if (Is_DistortionFamily())
        CHECK_FAILED_ONCE(Bind_DistortionResources(), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Bind_MaterialResources()
{
    const float emitterPhase = Resolve_MaterialScalarEmitterPhase();
    EffectRequiredMaterialRuntimeDesc material =
        Resolve_EmitterTimeMaterialUniformParameterModulation(_desc.required.material, emitterPhase, _effectPlaybackSeed);
    material.alphaMultiplier *= _materialRevealAlphaMultiplier;
    material.alphaErosion = max(material.alphaErosion, _materialRevealAlphaErosion);
    material.tint = Vec4::Lerp(material.tint, _materialTintOverrideTarget, _materialTintOverrideStrength);
    material.coreEmissive.coreColor = Vec4::Lerp(
        material.coreEmissive.coreColor,
        _materialTintOverrideTarget,
        _materialTintOverrideStrength);

    if (nullptr != _noiseTexture)
        CHECK_FAILED_ONCE(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);
    else
        CHECK_FAILED_ONCE(_shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);

    if (nullptr != _maskTexture)
        CHECK_FAILED_ONCE(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), E_FAIL);
    else
        CHECK_FAILED_ONCE(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);

    const Vec4 effectSpriteParams = Vec4(
        material.intensity,
        material.opacityPower,
        material.noiseStrength,
        nullptr != _noiseTexture ? 1.f : 0.f
    );
    const Vec4 effectSpriteAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        nullptr != _maskTexture ? 1.f : 0.f,
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
        static_cast<float>(EffectAssetRuntimeLoad::Resolve_MaterialSourceIndex(material.noiseSource)),
        static_cast<float>(EffectAssetRuntimeLoad::Resolve_MaterialSourceIndex(material.maskSource)),
        material.noiseInvert ? 1.f : 0.f,
        material.maskInvert ? 1.f : 0.f
    );

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteParams", &effectSpriteParams, sizeof(effectSpriteParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAlphaParams", &effectSpriteAlphaParams, sizeof(effectSpriteAlphaParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteMainUVParams", &effectSpriteMainUVParams, sizeof(effectSpriteMainUVParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteNoiseUVParams", &effectSpriteNoiseUVParams, sizeof(effectSpriteNoiseUVParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteMaskUVParams", &effectSpriteMaskUVParams, sizeof(effectSpriteMaskUVParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteUVOffsetParams", &effectSpriteUVOffsetParams, sizeof(effectSpriteUVOffsetParams)), E_FAIL);
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_EffectSpriteMaskUVOffsetParams", &effectSpriteMaskUVOffsetParams, sizeof(effectSpriteMaskUVOffsetParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteUVModeParams", &effectSpriteUVModeParams, sizeof(effectSpriteUVModeParams)), E_FAIL);
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_EffectSpriteUVAxisPolicyParams", &effectSpriteUVAxisPolicyParams, sizeof(effectSpriteUVAxisPolicyParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue(
            "g_EffectSpriteMaskUVAxisPolicyParams",
            &effectSpriteMaskUVAxisPolicyParams,
            sizeof(effectSpriteMaskUVAxisPolicyParams)
        ),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_EffectSpriteUVRotationParams", &effectSpriteUVRotationParams, sizeof(effectSpriteUVRotationParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteSourceParams", &effectSpriteSourceParams, sizeof(effectSpriteSourceParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteMaterialTime", &_emitterElapsedTime, sizeof(_emitterElapsedTime)), E_FAIL);
    CHECK_FAILED_ONCE(
        EffectAssetRuntimeLoad::Bind_MaterialScalarModulationPayload(_shader.get(), _desc.required.material.scalarModulation, emitterPhase),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        Bind_CoreColorRgbModulationShaderPayload(_shader.get(), _desc.required.material.coreColorRgbModulation, emitterPhase, _effectPlaybackSeed),
        E_FAIL
    );

    const Vec4 effectInfluenceDebugParams = Vec4(
        _isInfluenceOutlineDebugEnabled ? 1.f : 0.f,
        0.5f,
        kInfluenceOutlinePixelWidthScale,
        0.f
    );
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectInfluenceDebugParams", &effectInfluenceDebugParams, sizeof(effectInfluenceDebugParams)), E_FAIL);

    if (!Is_DistortionFamily())
    {
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
        CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAdditiveParams", &effectSpriteAdditiveParams, sizeof(effectSpriteAdditiveParams)), E_FAIL);
        CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)), E_FAIL);
        CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)), E_FAIL);
        CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAdditiveFlags", &effectSpriteAdditiveFlags, sizeof(effectSpriteAdditiveFlags)), E_FAIL);
        CHECK_FAILED_ONCE(
            _shader->Bind_RawValue("g_EffectSpriteCoreEmissiveParams", &effectSpriteCoreEmissiveParams, sizeof(effectSpriteCoreEmissiveParams)),
            E_FAIL
        );
        CHECK_FAILED_ONCE(
            _shader->Bind_RawValue("g_EffectSpriteCoreEmissiveColor", &effectSpriteCoreEmissiveColor, sizeof(effectSpriteCoreEmissiveColor)),
            E_FAIL
        );
    }

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_OpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Bind_DistortionResources()
{
    const float emitterPhase = Resolve_MaterialScalarEmitterPhase();
    const EffectRequiredMaterialRuntimeDesc material =
        Resolve_EmitterTimeMaterialUniformParameterModulation(_desc.required.material, emitterPhase, _effectPlaybackSeed);

    if (nullptr != _flowTexture)
        CHECK_FAILED_ONCE(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_FlowTexture", 0), E_FAIL);
    else
        CHECK_FAILED_ONCE(_shader->Bind_SRV("g_FlowTexture", nullptr), E_FAIL);

    D3D11_VIEWPORT viewport{};
    uint32 viewportCount = 1u;
    _context->RSGetViewports(&viewportCount, &viewport);

    const Vec4 distortionParams(
        material.refractionIntensity,
        max(0.f, material.refractionPresence),
        nullptr != _flowTexture ? 1.f : 0.f,
        _emitterElapsedTime
    );
    const EffectDistortionShapeMode distortionShapeMode =
        material.distortionShapeMode == EffectDistortionShapeMode::AirSheath
        ? EffectDistortionShapeMode::None
        : material.distortionShapeMode;
    const Vec4 distortionShapeParams(
        static_cast<float>(static_cast<uint32>(distortionShapeMode)),
        max(0.f, material.distortionShapeRadius),
        max(0.f, material.distortionShapeThickness),
        max(0.f, material.distortionShapeSoftness)
    );
    const Vec4 distortionScreenSize(max(1.f, viewport.Width), max(1.f, viewport.Height), 0.f, 0.f);
    const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(material.flowUVTilingMode));
    const float flowUVRotation = static_cast<float>(static_cast<uint32>(material.flowUVRotation));
    const Vec2 flowUVPolicyParams(
        static_cast<float>(static_cast<uint32>(material.flowUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.flowUVPolicy.vPolicy))
    );

    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_EffectDistortionShapeParams", &distortionShapeParams, sizeof(distortionShapeParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_DistortionScreenSize", &distortionScreenSize, sizeof(distortionScreenSize)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVPolicyParams", &flowUVPolicyParams, sizeof(flowUVPolicyParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVScale", &material.flowUVScale, sizeof(material.flowUVScale)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVOffset", &material.flowUVOffset, sizeof(material.flowUVOffset)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVScrollSpeed", &material.flowUVScrollSpeed, sizeof(material.flowUVScrollSpeed)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        EffectAssetRuntimeLoad::Bind_MaterialScalarModulationPayload(_shader.get(), material.scalarModulation, emitterPhase),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVRotation", &flowUVRotation, sizeof(flowUVRotation)),
        E_FAIL
    );

    return S_OK;
}

float ComputeSpriteEmitter::Resolve_MaterialScalarEmitterPhase() const
{
    if (PlaybackState::Completed == _playbackState && !_desc.required.playback.killOnCompleted)
        return 1.f;

    return Compute_MaterialScalarModulationPhase(_emitterElapsedTime, _desc.required.playback.duration);
}

bool ComputeSpriteEmitter::Is_DistortionFamily() const
{
    return _desc.required.material.materialFamily == EffectMaterialFamily::SpriteDistortion;
}

RenderGroup ComputeSpriteEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    if (Is_DistortionFamily())
        return RenderGroup::Distortion;

    return _desc.required.material.blendMode == EffectMaterialBlendMode::Masked
           ? RenderGroup::EffectMasked
           : RenderGroup::Blend;
}

const char* ComputeSpriteEmitter::Resolve_ShaderId() const
{
    return Is_DistortionFamily() ? kEffectDistortionSpriteShaderId : kEffectSpriteShaderId;
}

uint32 ComputeSpriteEmitter::Resolve_ShaderPassIndex() const
{
    return Is_DistortionFamily() ? 0u : Resolve_BlendPassIndex();
}

int ComputeSpriteEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.required.material.opacitySource == "Alpha" || _desc.required.material.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.required.material.opacitySource == "Red" || _desc.required.material.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.required.material.opacitySource == "Luminance" || _desc.required.material.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    if (_desc.required.material.opacitySource == "One" || _desc.required.material.opacitySource == "one")
        return kOpacitySourceOne;

    return kOpacitySourceAlpha;
}

uint32 ComputeSpriteEmitter::Resolve_BlendPassIndex() const
{
    if (_desc.required.material.blendMode == EffectMaterialBlendMode::Additive)
        return 1u;
    if (_desc.required.material.blendMode == EffectMaterialBlendMode::Masked)
        return 2u;
    return 0u;
}

Vec3 ComputeSpriteEmitter::Resolve_RuntimeWorldScale() const
{
    const Shared<GameObject> effectOwner = _effectOwner.lock();
    const Shared<TransformCom> ownerTransform =
        effectOwner != nullptr ? effectOwner->Get_Transform() : nullptr;

    if (ownerTransform != nullptr)
        return EffectAssetRuntimeLoad::Resolve_WorldScale(ownerTransform);

    return EffectAssetRuntimeLoad::Resolve_WorldScale(_transformCom);
}

ComputePointParticleDesc ComputeSpriteEmitter::Make_ComputePointParticleDesc() const
{
    ComputePointParticleDesc desc{};
    const Vec3 worldScale = Resolve_RuntimeWorldScale();
    const Vec2 planeScale = EffectAssetRuntimeLoad::Resolve_PlaneScale(_desc.planeRadialLocation.plane, worldScale);
    const float uniformLocationScale = max(worldScale.x, max(worldScale.y, worldScale.z));
    const Vec2 spriteScale{ uniformLocationScale, uniformLocationScale };
    const float uniformPlaneScale = max(planeScale.x, planeScale.y);

    desc.instanceCount = max(1u, _desc.spawn.instanceCount);
    desc.center = _desc.center;
    desc.effectPlaybackSeed = _effectPlaybackSeed;
    desc.range = EffectAssetRuntimeLoad::Scale_Vector3(_desc.spawn.spawnRange, worldScale);
    desc.sizeMin = EffectAssetRuntimeLoad::Scale_Vector2(_desc.initialSize.sizeMin, spriteScale);
    desc.sizeMax = EffectAssetRuntimeLoad::Scale_Vector2(_desc.initialSize.sizeMax, spriteScale);
    desc.initialSizeSeed = _desc.initialSize.sizeSeed;
    desc.lifeTime = _desc.lifetime.lifeTime;
    desc.lifetimeCurve = _desc.lifetime.lifeTimeCurve;
    desc.startColorMin = _desc.initialColor.startColorMin;
    desc.startColorMax = _desc.initialColor.startColorMax;
    desc.initialColorSeed = _desc.initialColor.colorSeed;
    desc.initialAlphaSeed = _desc.initialColor.alphaSeed;
    desc.endColorMin = _desc.colorOverLife.endColorMin;
    desc.endColorMax = _desc.colorOverLife.endColorMax;
    desc.colorOverLifeSeed = _desc.colorOverLife.colorSeed;
    desc.alphaOverLifeSeed = _desc.colorOverLife.alphaSeed;
    desc.colorOverLifeCurve = _desc.colorOverLife.curve;
    Vec3 coreColorRgbUniformMin{};
    Vec3 coreColorRgbUniformMax{};
    uint32 coreColorRgbUniformSeed{};
    if (Resolve_ParticleLifeCoreColorRgbUniformSampleDesc(
        _desc.required.material.coreColorRgbModulation,
        _effectPlaybackSeed,
        coreColorRgbUniformMin,
        coreColorRgbUniformMax,
        coreColorRgbUniformSeed
    ))
    {
        desc.coreColorRgbParticleLifeUniformParams = Vec4{ 1.f, 0.f, 0.f, 0.f };
        desc.coreColorRgbParticleLifeUniformMin = Vec4{ coreColorRgbUniformMin.x, coreColorRgbUniformMin.y, coreColorRgbUniformMin.z, 0.f };
        desc.coreColorRgbParticleLifeUniformMax = Vec4{ coreColorRgbUniformMax.x, coreColorRgbUniformMax.y, coreColorRgbUniformMax.z, 0.f };
        desc.coreColorRgbParticleLifeUniformSeed = XMUINT4{ coreColorRgbUniformSeed, 0u, 0u, 0u };
    }
    desc.subUVRows = max(1u, _desc.required.material.subUVRows);
    desc.subUVCols = max(1u, _desc.required.material.subUVCols);
    desc.subUvFrameOverLife = _desc.subUVFrameOverLife;
    desc.sizeByLife = _desc.sizeByLife;
    desc.motion = _desc.motion;
    desc.playbackDuration = Resolve_Duration();
    desc.orbitOverLife = _desc.orbitOverLife;
    desc.rotation = _desc.rotation;
    desc.spriteTilt = _desc.spriteTilt;
    desc.spawn = _desc.spawn.particleSpawn;
    desc.lifetimeSeed = _desc.lifetimeSeed;
    desc.initialLocation = _desc.initialLocation;
    desc.initialLocation.minOffset = EffectAssetRuntimeLoad::Scale_Vector3(desc.initialLocation.minOffset, worldScale);
    desc.initialLocation.maxOffset = EffectAssetRuntimeLoad::Scale_Vector3(desc.initialLocation.maxOffset, worldScale);
    desc.sphereLocation = _desc.sphereLocation;
    desc.sphereLocation.offset = EffectAssetRuntimeLoad::Scale_Vector3(desc.sphereLocation.offset, worldScale);
    desc.sphereLocation.radius *= uniformLocationScale;
    desc.planeRadialLocation = _desc.planeRadialLocation;
    desc.planeRadialLocation.offset = EffectAssetRuntimeLoad::Scale_Vector3(desc.planeRadialLocation.offset, worldScale);
    desc.planeRadialLocation.thickness *= uniformLocationScale;
    desc.planeRadialLocation.uRange = EffectAssetRuntimeLoad::Scale_Vector2(desc.planeRadialLocation.uRange, Vec2{ planeScale.x, planeScale.x });
    desc.planeRadialLocation.vRange = EffectAssetRuntimeLoad::Scale_Vector2(desc.planeRadialLocation.vRange, Vec2{ planeScale.y, planeScale.y });
    desc.planeRadialLocation.radiusRange = EffectAssetRuntimeLoad::Scale_Vector2(
        desc.planeRadialLocation.radiusRange,
        Vec2{ uniformPlaneScale, uniformPlaneScale }
    );
    desc.cylinderLocation = _desc.cylinderLocation;
    const Vec2 cylinderScale = EffectAssetRuntimeLoad::Resolve_CylinderScale(desc.cylinderLocation.axis, worldScale);
    desc.cylinderLocation.offset = EffectAssetRuntimeLoad::Scale_Vector3(desc.cylinderLocation.offset, worldScale);
    desc.cylinderLocation.radiusRange = EffectAssetRuntimeLoad::Scale_Vector2(
        desc.cylinderLocation.radiusRange,
        Vec2{ cylinderScale.x, cylinderScale.x }
    );
    desc.cylinderLocation.heightRange = EffectAssetRuntimeLoad::Scale_Vector2(
        desc.cylinderLocation.heightRange,
        Vec2{ cylinderScale.y, cylinderScale.y }
    );
    desc.planeRadialOrientation = _desc.planeRadialOrientation;
    desc.cylinderOrientation = _desc.cylinderOrientation;
    desc.initialLocationSeed = _desc.initialLocationSeed;
    desc.sphereLocationSeed = _desc.sphereLocationSeed;
    desc.subUVRandomFrameSeed = _desc.subUVRandomFrameSeed;
    if (nullptr != _transformCom)
    {
        desc.emitterRight = _transformCom->Get_WorldRight();
        desc.emitterUp = _transformCom->Get_WorldUp();
        desc.emitterLook = _transformCom->Get_WorldForward();
    }
    desc.spawn.maxActiveCount = min(max(1u, desc.spawn.maxActiveCount), desc.instanceCount);
    desc.useMaxDrawCount = _desc.required.drawLimit.useMaxDrawCount;
    desc.maxDrawCount = _desc.required.drawLimit.useMaxDrawCount
                        ? min(desc.instanceCount, max(1u, _desc.required.drawLimit.maxDrawCount))
                        : desc.instanceCount;
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    desc.computeShaderLevelIndex = ETOI(LevelType::Static);
    desc.computeShaderPrototypeTag = L"ComputeShader_PointParticle";

    return desc;
}

void ComputeSpriteEmitter::Update_RuntimeDescForCurrentScale(bool forceUpdate)
{
    CHECK_NULL_THROTTLED(_transformCom, 120);
    CHECK_NULL_THROTTLED(_viBuffer, 120);

    const Vec3 worldScale = Resolve_RuntimeWorldScale();
    if (!forceUpdate &&
        _hasRuntimeDescWorldScale &&
        EffectAssetRuntimeLoad::Nearly_EqualScale(_runtimeDescWorldScale, worldScale))
        return;

    _viBuffer->Update_RuntimeDesc(Make_ComputePointParticleDesc());
    _runtimeDescWorldScale = worldScale;
    _hasRuntimeDescWorldScale = true;
}

void ComputeSpriteEmitter::Sync_WorldCenterToComputeBuffer()
{
    CHECK_NULL_THROTTLED(_transformCom, 120);
    CHECK_NULL_THROTTLED(_viBuffer, 120);

    _viBuffer->Set_Center(_transformCom->Get_WorldPosition());
    _viBuffer->Set_EmitterBasis(
        _transformCom->Get_WorldRight(),
        _transformCom->Get_WorldUp(),
        _transformCom->Get_WorldForward()
    );
}

uint32 ComputeSpriteEmitter::Consume_SpawnRequest(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    const float previousTime = _emitterElapsedTime;
    _emitterElapsedTime += safeDeltaTime;

    uint32 spawnRequest = 0u;

    if (_desc.spawn.particleSpawn.processSpawnRate)
    {
        const float loopPhase = clamp(_emitterElapsedTime / Resolve_Duration(), 0.f, 1.f);
        const float spawnRateValue = EffectAssetRuntimeLoad::Evaluate_FloatCurve(
            _desc.spawn.particleSpawn.spawnRateCurve,
            loopPhase,
            _sampledSpawnRate
        );
        const float spawnRateScale = EffectAssetRuntimeLoad::Evaluate_FloatCurve(
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
            const float burstScale = EffectAssetRuntimeLoad::Evaluate_FloatCurve(
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

void ComputeSpriteEmitter::Resample_LoopSpawnDistributions()
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

float ComputeSpriteEmitter::Sample_SpawnDistribution(
    const Vec2& range,
    const PointParticleRandomSeedRuntimeDesc& seed,
    uint32 salt) const
{
    const float minValue = min(range.x, range.y);
    const float maxValue = max(range.x, range.y);
    if (minValue == maxValue)
        return minValue;

    const uint32 baseSeed = (_loopIndex + 1u) * 9781u + salt + EffectAssetRuntimeLoad::Resolve_SeedSalt(seed, _effectPlaybackSeed);
    return lerp(minValue, maxValue, EffectAssetRuntimeLoad::Hash01(baseSeed));
}

void ComputeSpriteEmitter::Reset_PlaybackRuntime()
{
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _spawnSerialCounter = 0u;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
    Resample_LoopSpawnDistributions();
}

ComputeSpriteEmitter::PlaybackTick ComputeSpriteEmitter::Advance_Playback(float timeDelta)
{
    PlaybackTick tick{};
    const float safeDeltaTime = max(0.f, timeDelta);

    if (PlaybackState::Completed == _playbackState)
    {
        tick.computeDeltaTime = _desc.required.playback.killOnCompleted ? 0.f : safeDeltaTime;
        if (_desc.required.playback.killOnCompleted && !_completedKillDispatched)
        {
            tick.killActiveParticles = true;
            _completedKillDispatched = true;
        }
        return tick;
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
            tick.computeDeltaTime += playbackStep;
            const uint32 spawnRequest = Consume_SpawnRequest(playbackStep);
            if (spawnRequest > 0u && tick.spawnRequest == 0u)
                tick.spawnSerialBase = _spawnSerialCounter;
            tick.spawnRequest += spawnRequest;
            if (spawnRequest > 0u)
                tick.lifetimeSamplePhase = clamp(_emitterElapsedTime / max(0.0001f, duration), 0.f, 1.f);
            _loopElapsedTime += playbackStep;
            remainingTime -= playbackStep;
        }

        if (_loopElapsedTime - delay < duration)
            break;

        if (Has_NextLoop())
        {
            Start_NextLoop();
            continue;
        }

        _playbackState = PlaybackState::Completed;
        if (_desc.required.playback.killOnCompleted && !_completedKillDispatched)
        {
            tick.killActiveParticles = true;
            _completedKillDispatched = true;
        }
        break;
    }

    if (PlaybackState::Completed == _playbackState && !_desc.required.playback.killOnCompleted)
        tick.computeDeltaTime += remainingTime;

    if (tick.spawnRequest > 0u)
        _spawnSerialCounter += tick.spawnRequest;

    return tick;
}

void ComputeSpriteEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
    Resample_LoopSpawnDistributions();
}

float ComputeSpriteEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.required.playback.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.required.playback.delay);
}

float ComputeSpriteEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.required.playback.duration);
}

bool ComputeSpriteEmitter::Has_NextLoop() const
{
    return _desc.required.playback.loopCount == 0u || _loopIndex + 1u < _desc.required.playback.loopCount;
}

bool ComputeSpriteEmitter::Can_SubmitRender() const
{
    if (PlaybackState::Delayed == _playbackState)
        return false;

    if (nullptr == _viBuffer)
        return false;

    if (!_viBuffer->Has_ActiveParticles())
        return false;

    return true;
}

Shared<ComputeSpriteEmitter> ComputeSpriteEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeSpriteEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeSpriteEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputeSpriteEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeSpriteEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeSpriteEmitter");
        MSG_BOX("Failed to Clone : ComputeSpriteEmitter");
        return nullptr;
    }

    return instance;
}

void ComputeSpriteEmitter::Free()
{
    _shader.reset();
    _texture.reset();
    _noiseTexture.reset();
    _maskTexture.reset();
    _viBuffer.reset();

    _desc = {};
    _opacitySourceIndex = kOpacitySourceAlpha;
    _runtimeDescWorldScale = Vec3::One;
    _hasRuntimeDescWorldScale = false;
    _burstExecuted.clear();
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _playbackState = PlaybackState::Delayed;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;

    __super::Free();
}
