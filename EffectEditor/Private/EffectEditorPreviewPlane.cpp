#include "pch.h"
#include "EffectEditorPreviewPlane.h"

#include "EffectEditorInstance.h"
#include "EffectMaterial_Types.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "TransformCom.h"

NS_BEGIN(EffectEditor)

EffectEditorPreviewPlane::EffectEditorPreviewPlane(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

EffectEditorPreviewPlane::EffectEditorPreviewPlane(const EffectEditorPreviewPlane& prototype)
    : GameObject{ prototype }
{
}

HRESULT EffectEditorPreviewPlane::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    Set_Name(L"EffectEditor_PreviewPlane");
    _transformCom->Set_Scale(kWorldScale);
    _transformCom->Set_RotationEuler(kWorldRotationDegrees);
    _transformCom->Set_WorldPosition(kWorldPosition);

    return S_OK;
}

void EffectEditorPreviewPlane::Late_Update(float)
{
    if (nullptr == EDITOR || !EDITOR->Is_PreviewPlaneVisible())
        return;

    if (nullptr == _model || nullptr == _shader || nullptr == _texture || nullptr == _instanceBuffer)
        return;

    GAME->Add_RenderGroup(RenderGroup::EffectMasked, GetSharedPtr<GameObject>());
}

HRESULT EffectEditorPreviewPlane::Render()
{
    if (nullptr == EDITOR || !EDITOR->Is_PreviewPlaneVisible())
        return S_OK;

    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);
    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        CHECK_FAILED(_shader->Begin(kMaskedTwoSidedPassIndex), E_FAIL);
        CHECK_FAILED(_model->RenderInstanced(meshIndex, _instanceBuffer.Get(), sizeof(PreviewPlaneInstance), 1u), E_FAIL);
    }

    return S_OK;
}

HRESULT EffectEditorPreviewPlane::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), kShaderId, _shader), E_FAIL);
    CHECK_FAILED(Ready_Model(), E_FAIL);
    CHECK_FAILED(Ready_Texture(), E_FAIL);
    CHECK_FAILED(Ready_InstanceBuffer(), E_FAIL);

    return S_OK;
}

HRESULT EffectEditorPreviewPlane::Ready_Model()
{
    const wstring modelPath = Resolve_ResourcePath(kModelPath);
    if (modelPath.empty())
        return E_FAIL;

    _model = ModelCom::Create(
        _device,
        _context,
        ModelType::NonAnim,
        String::ToString(modelPath).c_str(),
        Matrix::CreateScale(kModelPreTransformScale)
    );
    CHECK_NULL(_model, E_FAIL);

    return S_OK;
}

HRESULT EffectEditorPreviewPlane::Ready_Texture()
{
    const wstring texturePath = Resolve_ResourcePath(kTexturePath);
    if (texturePath.empty())
        return E_FAIL;

    _texture = Texture::Create(_device, _context, texturePath.c_str(), 1u);
    CHECK_NULL(_texture, E_FAIL);

    return S_OK;
}

HRESULT EffectEditorPreviewPlane::Ready_InstanceBuffer()
{
    const PreviewPlaneInstance instance{
        Matrix::CreateScale(kWorldScale) *
        Matrix::CreateRotationX(XMConvertToRadians(kWorldRotationDegrees.x)) *
        Matrix::CreateTranslation(kWorldPosition),
        Vec4{ 1.f, 1.f, 1.f, 1.f },
        Vec2{ 0.f, 1.f },
        Vec2{}
    };

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(PreviewPlaneInstance);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0u;
    bufferDesc.MiscFlags = 0u;
    bufferDesc.StructureByteStride = sizeof(PreviewPlaneInstance);

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = &instance;

    CHECK_FAILED(_device->CreateBuffer(&bufferDesc, &initialData, _instanceBuffer.GetAddressOf()), E_FAIL);

    return S_OK;
}

wstring EffectEditorPreviewPlane::Resolve_ResourcePath(const wchar_t* relativePath) const
{
    if (nullptr == relativePath || relativePath[0] == L'\0')
        return {};

    fs::path resourcePath{ relativePath };
    if (resourcePath.is_relative() && nullptr != GAME)
        resourcePath = fs::path(GAME->Get_AssetRoot()) / resourcePath;

    resourcePath = resourcePath.lexically_normal();
    return fs::exists(resourcePath) ? resourcePath.wstring() : wstring{};
}

HRESULT EffectEditorPreviewPlane::Bind_ShaderResources()
{
    const Matrix identity = Matrix::Identity;
    CHECK_FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &identity), E_FAIL);
    CHECK_FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    CHECK_FAILED(_shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);
    CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);
    CHECK_FAILED(_shader->Bind_SRV("g_ModelNormalTexture", nullptr), E_FAIL);
    CHECK_FAILED(_shader->Bind_SRV("g_ModelEmissiveTexture", nullptr), E_FAIL);
    CHECK_FAILED(_shader->Bind_SRV("g_ModelOrmTexture", nullptr), E_FAIL);

    const Vec4 tint{ 1.f, 1.f, 1.f, 1.f };
    const Vec4 modelMaterialFlags{ 0.f, 0.f, 0.f, 0.f };
    const Vec4 effectMeshParams{ 1.f, 1.f, 0.f, 0.f };
    const Vec4 effectMeshAlphaParams{ 0.f, 0.f, 0.f, 1.f };
    const Vec4 effectMeshMainUVParams{ kMainUVScale.x, kMainUVScale.y, 0.f, 0.f };
    const Vec4 effectMeshNoiseUVParams{ 1.f, 1.f, 0.f, 0.f };
    const Vec4 effectMeshMaskUVParams{ 1.f, 1.f, 0.f, 0.f };
    const Vec4 effectMeshUVOffsetParams{ 0.f, 0.f, 0.f, 0.f };
    const Vec4 effectMeshMaskUVOffsetParams{ 0.f, 0.f, 0.f, 0.f };
    const Vec4 effectMeshUVModeParams{
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Wrap)),
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Stretch)),
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Stretch)),
        0.f };
    const Vec4 effectMeshUVAxisPolicyParams{
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Wrap)),
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Wrap)),
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Stretch)),
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Stretch)) };
    const Vec4 effectMeshMaskUVAxisPolicyParams{
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Stretch)),
        static_cast<float>(static_cast<uint32>(EffectTextureUVTilingMode::Stretch)),
        0.f,
        0.f };
    const Vec4 effectMeshUVRotationParams{ 0.f, 0.f, 0.f, 0.f };
    const Vec4 effectMeshSourceParams{ 1.f, 0.f, 0.f, 0.f };
    const Vec4 effectMeshAdditiveParams{ 0.f, 0.f, 0.f, 1.f };
    const Vec4 effectMeshAdditiveColor{ 1.f, 1.f, 1.f, 1.f };
    const Vec4 effectMeshAdditiveFlags{ 1.f, 0.f, 0.f, 0.f };
    const Vec4 effectMeshCoreEmissiveParams{ 0.f, 4.f, 2.f, 1.f };
    const Vec4 effectMeshCoreEmissiveColor{ 1.f, 0.85f, 0.45f, 1.f };
    const float materialTime = 0.f;
    const int opacitySource = kOpacitySourceAlpha;

    CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &tint, sizeof(tint)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshParams", &effectMeshParams, sizeof(effectMeshParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshAlphaParams", &effectMeshAlphaParams, sizeof(effectMeshAlphaParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshMainUVParams", &effectMeshMainUVParams, sizeof(effectMeshMainUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshNoiseUVParams", &effectMeshNoiseUVParams, sizeof(effectMeshNoiseUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshMaskUVParams", &effectMeshMaskUVParams, sizeof(effectMeshMaskUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshUVOffsetParams", &effectMeshUVOffsetParams, sizeof(effectMeshUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshMaskUVOffsetParams", &effectMeshMaskUVOffsetParams, sizeof(effectMeshMaskUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshUVModeParams", &effectMeshUVModeParams, sizeof(effectMeshUVModeParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshUVAxisPolicyParams", &effectMeshUVAxisPolicyParams, sizeof(effectMeshUVAxisPolicyParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshMaskUVAxisPolicyParams", &effectMeshMaskUVAxisPolicyParams, sizeof(effectMeshMaskUVAxisPolicyParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshUVRotationParams", &effectMeshUVRotationParams, sizeof(effectMeshUVRotationParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshSourceParams", &effectMeshSourceParams, sizeof(effectMeshSourceParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshMaterialTime", &materialTime, sizeof(materialTime)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshAdditiveParams", &effectMeshAdditiveParams, sizeof(effectMeshAdditiveParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshAdditiveEmissiveColor", &effectMeshAdditiveColor, sizeof(effectMeshAdditiveColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshAdditiveConstantColor", &effectMeshAdditiveColor, sizeof(effectMeshAdditiveColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshAdditiveFlags", &effectMeshAdditiveFlags, sizeof(effectMeshAdditiveFlags)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshCoreEmissiveParams", &effectMeshCoreEmissiveParams, sizeof(effectMeshCoreEmissiveParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshCoreEmissiveColor", &effectMeshCoreEmissiveColor, sizeof(effectMeshCoreEmissiveColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectMeshModelMaterialFlags", &modelMaterialFlags, sizeof(modelMaterialFlags)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_OpacitySource", &opacitySource, sizeof(opacitySource)), E_FAIL);

    return S_OK;
}

Shared<EffectEditorPreviewPlane> EffectEditorPreviewPlane::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectEditorPreviewPlane>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : EffectEditorPreviewPlane");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> EffectEditorPreviewPlane::Clone(void* arg)
{
    auto instance = make_shared<EffectEditorPreviewPlane>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : EffectEditorPreviewPlane");
        MSG_BOX("Failed to Clone : EffectEditorPreviewPlane");
        return nullptr;
    }

    return instance;
}

void EffectEditorPreviewPlane::Free()
{
    _instanceBuffer.Reset();
    __super::Free();
}

NS_END
