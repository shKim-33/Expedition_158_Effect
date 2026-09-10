#include "pch.h"
#include "EffectEditorSceneGrid.h"

#include "EffectEditorInstance.h"
#include "GameInstance.h"

NS_BEGIN(EffectEditor)

EffectEditorSceneGrid::EffectEditorSceneGrid(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

EffectEditorSceneGrid::EffectEditorSceneGrid(const EffectEditorSceneGrid& prototype)
    : GameObject{ prototype }
{
}

HRESULT EffectEditorSceneGrid::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    return S_OK;
}

void EffectEditorSceneGrid::Late_Update(float)
{
    GAME->Add_RenderGroup(RenderGroup::NonLight, GetSharedPtr<GameObject>());
}

HRESULT EffectEditorSceneGrid::Render()
{
    EDITOR->Render_SceneGrid();

    return S_OK;
}

Shared<EffectEditorSceneGrid> EffectEditorSceneGrid::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectEditorSceneGrid>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : EffectEditorSceneGrid");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> EffectEditorSceneGrid::Clone(void* arg)
{
    auto instance = make_shared<EffectEditorSceneGrid>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : EffectEditorSceneGrid");
        MSG_BOX("Failed to Clone : EffectEditorSceneGrid");
        return nullptr;
    }

    return instance;
}

void EffectEditorSceneGrid::Free()
{
    __super::Free();
}

NS_END
