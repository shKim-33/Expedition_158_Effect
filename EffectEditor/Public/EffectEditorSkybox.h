#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class Texture;
class VIBuffer_Cube;
NS_END

NS_BEGIN(EffectEditor)

class EffectEditorSkybox final : public GameObject
{
public:
    EffectEditorSkybox(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectEditorSkybox(const EffectEditorSkybox& prototype);
    ~EffectEditorSkybox() override = default;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private: //## Data::Components
    Shared<ShaderCom> _shader{};
    Shared<VIBuffer_Cube> _viBuffer{};
    Shared<Texture> _defaultTexture{};
    Shared<Texture> _blackTexture{};

private: //## Helper::Render
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();

public:
    static Shared<EffectEditorSkybox> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
