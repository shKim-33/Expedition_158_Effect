#pragma once

#include "Base.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(EffectEditor)

class Editor_Context final : public Base
{
public:
    Editor_Context() = default;
    ~Editor_Context() override;

public: //## Accessors
    const EditorSelection& Get_Selection() const { return _selection; }
    const EffectAuthoringSelection& Get_EffectSelection() const { return _effectSelection; }

    void Set_SelectObject(const Shared<GameObject>& gameObject);
    void Clear_Selection();
    void Set_SelectEffectEmitter(uint32 emitterId);
    void Set_SelectEffectTypeData(uint32 emitterId);
    void Set_SelectEffectModule(uint32 emitterId, uint32 moduleId);
    void Clear_EffectSelection();
    void Request_FocusObject(const Shared<GameObject>& gameObject);
    Shared<GameObject> Consume_FocusObjectRequest();

    uint32 Get_SelectionRevision() const { return _selectionRevision; }
    uint32 Get_EffectSelectionRevision() const { return _effectSelectionRevision; }

private: //## Data::Selection
    EditorSelection _selection{};
    EffectAuthoringSelection _effectSelection{};
    EditorFocusRequest _focusRequest{};
    uint32 _selectionRevision{ 0 };
    uint32 _effectSelectionRevision{ 0 };

public:
    static Unique<Editor_Context> Create();
    void Free() override;
};

NS_END
