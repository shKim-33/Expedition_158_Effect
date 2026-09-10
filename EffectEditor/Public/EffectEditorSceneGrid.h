#pragma once

#include "GameObject.h"

NS_BEGIN(EffectEditor)

class EffectEditorSceneGrid final : public GameObject
{
public:
    EffectEditorSceneGrid(const ComPtr<Device>& device, const ComPtr<Context>& context);
    EffectEditorSceneGrid(const EffectEditorSceneGrid& prototype);
    ~EffectEditorSceneGrid() override = default;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    static Shared<EffectEditorSceneGrid> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
