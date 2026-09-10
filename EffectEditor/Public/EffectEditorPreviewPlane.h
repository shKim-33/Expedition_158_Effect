#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
class Texture;
NS_END

NS_BEGIN(EffectEditor)

class EffectEditorPreviewPlane final : public GameObject
{
public:
    EffectEditorPreviewPlane(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectEditorPreviewPlane(const EffectEditorPreviewPlane& prototype);
    ~EffectEditorPreviewPlane() override = default;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private: //## Types::Render
    struct PreviewPlaneInstance
    {
        Matrix world{};
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        Vec2 lifeTime{};
        Vec2 padding{};
    };

private: //## Static::Render
    static constexpr auto kModelPath{ L"Effects/Models/Plane.model" };
    static constexpr auto kTexturePath{ L"Effects/Textures/Shared/DefaultDiffuse.dds" };
    static constexpr auto kShaderId{ L"Shader_EffectMesh" };
    static constexpr uint32 kMaskedTwoSidedPassIndex{ 5u };
    static constexpr int kOpacitySourceAlpha{ 0 };
    static constexpr float kModelPreTransformScale{ 0.01f };

    inline static const Vec3 kWorldScale{ 60.f, 60.f, 60.f };
    inline static const Vec3 kWorldPosition{ 0.f, -0.01f, 0.f };
    inline static const Vec3 kWorldRotationDegrees{ 90.f, 0.f, 0.f };
    inline static const Vec2 kMainUVScale{ 10.f, 10.f };

private: //## Data::Components
    Shared<ModelCom> _model{};
    Shared<ShaderCom> _shader{};
    Shared<Texture> _texture{};
    ComPtr<Buffer> _instanceBuffer{};

private: //## Helper::Setup
    HRESULT Ready_Components();
    HRESULT Ready_Model();
    HRESULT Ready_Texture();
    HRESULT Ready_InstanceBuffer();
    wstring Resolve_ResourcePath(const wchar_t* relativePath) const;

private: //## Helper::Render
    HRESULT Bind_ShaderResources();

public:
    static Shared<EffectEditorPreviewPlane> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
