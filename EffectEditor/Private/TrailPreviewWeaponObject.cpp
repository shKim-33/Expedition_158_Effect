#include "pch.h"
#include "TrailPreviewWeaponObject.h"

#include "GameInstance.h"
#include "EffectEditorInstance.h"
#include "Material.h"
#include "ModelCom.h"
#include "PlayableParts.h"
#include "ShaderPassFinder.h"
#include "ShaderCom.h"
#include "TrailPreviewPlayerObject.h"

NS_BEGIN(EffectEditor)

namespace
{
Quat Resolve_NormalizedRotation(const Matrix& worldMatrix)
{
    Matrix decomposableWorldMatrix = worldMatrix;
    Vec3 scale = Vec3::One;
    Vec3 position = Vec3::Zero;
    Quat rotation = Quat::Identity;
    if (!decomposableWorldMatrix.Decompose(scale, rotation, position) || rotation.LengthSquared() <= 0.000001f)
        return Quat::Identity;

    rotation.Normalize();
    return rotation;
}
}

TrailPreviewWeaponObject::TrailPreviewWeaponObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PartObject{ device, context }
{
}

TrailPreviewWeaponObject::TrailPreviewWeaponObject(const TrailPreviewWeaponObject& prototype)
    : PartObject{ prototype }
{
}

HRESULT TrailPreviewWeaponObject::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    Set_Name(L"Lanceram_TrailPreview_Weapon");
    Set_Visible(false);

    return S_OK;
}

void TrailPreviewWeaponObject::Update(float)
{
    if (!Is_Visible())
    {
        _isSocketAttached = false;
        _weaponWorldMatrix = Matrix::Identity;
        _trailBaseWorldPosition = {};
        _trailTipWorldPosition = {};
        _sourcePointWorldPosition = {};
        _hasTrailSample = false;
        return;
    }

    Update_WeaponWorldMatrix();
}

void TrailPreviewWeaponObject::Late_Update(float)
{
    if (!Is_Visible())
        return;

    if (nullptr != _weaponModel && nullptr != _weaponShader)
    {
        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
        Draw_TrailAnchorMarkers();
    }
}

HRESULT TrailPreviewWeaponObject::Render()
{
    CHECK_FAILED(_weaponShader->Bind_ObjectCB(_weaponWorldMatrix), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_weaponShader), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_weaponModel->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        if (FAILED(Bind_ShaderResources(meshIndex)))
            continue;

        CHECK_FAILED(_weaponModel->Render(meshIndex), E_FAIL);
    }

    return S_OK;
}

bool TrailPreviewWeaponObject::Has_TrailSample() const
{
    return Is_Visible() && _hasTrailSample;
}

bool TrailPreviewWeaponObject::Try_GetTrailSample(EffectTrailSample& outSample) const
{
    if (!Has_TrailSample())
        return false;

    outSample.baseWorldPosition = _trailBaseWorldPosition;
    outSample.tipWorldPosition = _trailTipWorldPosition;
    outSample.baseWorldRotation = _weaponWorldRotation;
    outSample.tipWorldRotation = _weaponWorldRotation;
    return true;
}

bool TrailPreviewWeaponObject::Try_GetSourcePointSample(EffectSourcePointSample& outSample) const
{
    if (!Has_TrailSample())
        return false;

    outSample.worldPosition = _sourcePointWorldPosition;
    outSample.worldRotation = _weaponWorldRotation;
    return true;
}

wstring TrailPreviewWeaponObject::Resolve_ClientResourcePath(const wchar_t* relativePath) const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
        return {};

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / relativePath)
           .lexically_normal()
           .wstring();
}

HRESULT TrailPreviewWeaponObject::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Shader_VtxMesh", _weaponShader), E_FAIL);
    CHECK_FAILED(Ready_WeaponModel(), E_FAIL);

    return S_OK;
}

HRESULT TrailPreviewWeaponObject::Ready_WeaponModel()
{
    const wstring weaponPath = Resolve_ClientResourcePath(kWeaponModelRelativePath);
    if (weaponPath.empty())
        return E_FAIL;

    const string weaponPathUtf8 = String::ToString(weaponPath);
    _weaponModel = ModelCom::Create(_device, _context, ModelType::NonAnim, weaponPathUtf8.c_str());
    CHECK_NULL(_weaponModel, E_FAIL);
    Configure_WeaponMaterials();

    LOG_INFO(
        "[TrailPreview] Lanceram weapon loaded. path={}, meshes={}, materials={}",
        weaponPathUtf8,
        static_cast<uint32>(_weaponModel->Get_NumMeshes()),
        static_cast<uint32>(_weaponModel->Get_NumMaterials())
    );

    return S_OK;
}

void TrailPreviewWeaponObject::Configure_WeaponMaterials()
{
    CHECK_NULL(_weaponModel);

    const uint32 numMaterials = static_cast<uint32>(_weaponModel->Get_NumMaterials());
    for (uint32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
    {
        const Shared<Material> material = _weaponModel->Get_Material(materialIndex);
        if (nullptr == material)
            continue;

        MaterialCB materialCB = material->Get_CB();
        materialCB.emissiveColor = Vec4(0.48f, 0.75f, 0.66f, 1.f);
        materialCB.emissiveIntensity = 1.f;
        materialCB.emissiveMaskMul = 0.7f;
        materialCB.normalStrength = 1.f;
        materialCB.roughnessScale = 1.f;
        materialCB.metalnessScale = 1.f;

        material->Set_CB(materialCB);
        material->Set_ForcedPassIndex(static_cast<int32>(VtxMeshPassIndex::PackedAR_NMEPass));
    }
}

Shared<TrailPreviewPlayerObject> TrailPreviewWeaponObject::Get_PlayerOwner() const
{
    return dynamic_pointer_cast<TrailPreviewPlayerObject>(Get_Owner());
}

PlayableParts* TrailPreviewWeaponObject::Get_BodyPart() const
{
    const Shared<TrailPreviewPlayerObject> player = Get_PlayerOwner();
    if (nullptr == player)
        return nullptr;

    return dynamic_cast<PlayableParts*>(player->Get_PartObject(L"Part_Body"));
}

Matrix TrailPreviewWeaponObject::Get_PlayerWorldMatrix() const
{
    const Shared<TrailPreviewPlayerObject> player = Get_PlayerOwner();
    if (nullptr == player || nullptr == player->Get_Transform())
        return Matrix::Identity;

    return player->Get_Transform()->Get_WorldMatrix();
}

Matrix TrailPreviewWeaponObject::Build_WeaponOffsetMatrix() const
{
    return Matrix::CreateRotationX(XMConvertToRadians(kWeaponAttachRotationDegrees.x)) *
           Matrix::CreateRotationY(XMConvertToRadians(kWeaponAttachRotationDegrees.y)) *
           Matrix::CreateRotationZ(XMConvertToRadians(kWeaponAttachRotationDegrees.z));
}

void TrailPreviewWeaponObject::Update_WeaponWorldMatrix()
{
    const Matrix playerWorldMatrix = Get_PlayerWorldMatrix();
    const PlayableParts* bodyPart = Get_BodyPart();

    if (nullptr != bodyPart)
        Log_AttachmentSocket(*bodyPart, playerWorldMatrix);
    else if (!_socketDebugLogged)
    {
        LOG_INFO("[TrailPreview] body part missing. attachment socket unavailable.");
        _socketDebugLogged = true;
    }

    if (nullptr == bodyPart)
    {
        Update_FallbackWorldMatrix(playerWorldMatrix);
        return;
    }

    const Matrix* socketMatrix = bodyPart->Get_SocketBoneMatrixPtr(kWeaponSocketName);
    if (nullptr == socketMatrix)
    {
        if (!_socketWarningLogged)
        {
            LOG_WARN("[TrailPreview] socket '{}' not found. Lanceram uses fallback preview pose.", kWeaponSocketName);
            _socketWarningLogged = true;
        }

        Update_FallbackWorldMatrix(playerWorldMatrix);
        return;
    }

    _weaponWorldMatrix = Build_WeaponOffsetMatrix() * *socketMatrix * playerWorldMatrix;
    _isSocketAttached = true;
    Update_TrailPoints();
}

void TrailPreviewWeaponObject::Update_FallbackWorldMatrix(const Matrix& playerWorldMatrix)
{
    _weaponWorldMatrix =
        Build_WeaponOffsetMatrix() *
        Matrix::CreateTranslation(kFallbackLocalPosition) *
        playerWorldMatrix;
    _isSocketAttached = false;
    Update_TrailPoints();
}

void TrailPreviewWeaponObject::Update_TrailPoints()
{
    if (EDITOR == nullptr)
    {
        _trailBaseWorldPosition = {};
        _trailTipWorldPosition = {};
        _sourcePointWorldPosition = {};
        _hasTrailSample = false;
        return;
    }

    const Vec3 baseLocal = EDITOR->Get_TrailPreviewBaseLocalOffset();
    const Vec3 tipLocal = EDITOR->Get_TrailPreviewTipLocalOffset();
    const Vec3 sourceLocal = EDITOR->Get_SourceHistorySpriteTrailPreviewSourceLocalOffset();
    _trailBaseWorldPosition = Vec3::Transform(baseLocal, _weaponWorldMatrix);
    _trailTipWorldPosition = Vec3::Transform(tipLocal, _weaponWorldMatrix);
    _sourcePointWorldPosition = Vec3::Transform(sourceLocal, _weaponWorldMatrix);
    _weaponWorldRotation = Resolve_NormalizedRotation(_weaponWorldMatrix);
    _hasTrailSample = true;
}

void TrailPreviewWeaponObject::Draw_TrailAnchorMarkers() const
{
    if (!Has_TrailSample())
        return;

    if (EDITOR != nullptr && !EDITOR->Is_TrailPreviewDebugRenderEnabled())
        return;

    Draw_TrailAnchorMarker(_trailBaseWorldPosition, Color(0.2f, 0.9f, 1.f, 1.f));
    Draw_TrailAnchorMarker(_trailTipWorldPosition, Color(1.f, 0.72f, 0.18f, 1.f));
    Draw_TrailAnchorMarker(_sourcePointWorldPosition, Color(0.95f, 0.3f, 1.f, 1.f));

    DebugLineDesc lineDesc{};
    lineDesc.start = _trailBaseWorldPosition;
    lineDesc.end = _trailTipWorldPosition;
    lineDesc.style.color = Color(0.95f, 1.f, 0.25f, 1.f);
    lineDesc.style.depthEnabled = false;
    GAME->Draw_DebugLine(lineDesc);
}

void TrailPreviewWeaponObject::Draw_TrailAnchorMarker(const Vec3& worldPosition, const Color& color) const
{
    DebugSphereDesc sphereDesc{};
    sphereDesc.center = worldPosition;
    sphereDesc.radius = kTrailAnchorMarkerRadius;
    sphereDesc.style.color = color;
    sphereDesc.style.depthEnabled = false;
    GAME->Draw_DebugSphere(sphereDesc);
}

void TrailPreviewWeaponObject::Log_AttachmentSocket(
    const PlayableParts& bodyPart,
    const Matrix& playerWorldMatrix)
{
    if (_socketDebugLogged)
        return;

    const Matrix* socketMatrix = bodyPart.Get_SocketBoneMatrixPtr(kWeaponSocketName);
    if (nullptr == socketMatrix)
    {
        LOG_INFO("[TrailPreview] attachment socket '{}' missing.", kWeaponSocketName);
        _socketDebugLogged = true;
        return;
    }

    const Matrix socketWorldMatrix = *socketMatrix * playerWorldMatrix;
    const Vec3 socketWorldPosition = socketWorldMatrix.Translation();
    LOG_INFO(
        "[TrailPreview] attachment socket '{}' world position=({}, {}, {})",
        kWeaponSocketName,
        socketWorldPosition.x,
        socketWorldPosition.y,
        socketWorldPosition.z
    );

    _socketDebugLogged = true;
}

HRESULT TrailPreviewWeaponObject::Bind_ShaderResources(uint32 meshIndex)
{
    const uint32 materialIndex = _weaponModel->Get_MeshMaterialIndex(meshIndex);
    const Shared<Material> material = _weaponModel->Get_Material(materialIndex);
    CHECK_NULL(material, E_FAIL);

    CHECK_FAILED(_weaponShader->Bind_MaterialTextures(*material), E_FAIL);
    CHECK_FAILED(_weaponShader->Bind_CBufferData(material->Get_CB()), E_FAIL);
    CHECK_FAILED(_weaponShader->Begin(static_cast<uint32>(material->Get_PassIndex())), E_FAIL);

    return S_OK;
}

Shared<TrailPreviewWeaponObject> TrailPreviewWeaponObject::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<TrailPreviewWeaponObject>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : TrailPreviewWeaponObject");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TrailPreviewWeaponObject::Clone(void* arg)
{
    auto instance = make_shared<TrailPreviewWeaponObject>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : TrailPreviewWeaponObject");
        MSG_BOX("Failed to Clone : TrailPreviewWeaponObject");
        return nullptr;
    }

    return instance;
}

void TrailPreviewWeaponObject::Free()
{
    __super::Free();
}

NS_END
