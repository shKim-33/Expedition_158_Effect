#include "Editor_Context.h"

NS_BEGIN(EffectEditor)

Editor_Context::~Editor_Context()
{
    Free();
}

void Editor_Context::Set_SelectObject(const Shared<GameObject>& gameObject)
{
    if (_selection.selectedObject == gameObject)
        return;

    _selection.selectedObject = gameObject;
    ++_selectionRevision;
}

void Editor_Context::Clear_Selection()
{
    if (_selection.selectedObject == nullptr)
        return;

    _selection.selectedObject.reset();
    ++_selectionRevision;
}

void Editor_Context::Set_SelectEffectEmitter(uint32 emitterId)
{
    if (_effectSelection.kind == EffectAuthoringSelectionKind::Emitter &&
        _effectSelection.emitterId == emitterId)
        return;

    _effectSelection.kind = EffectAuthoringSelectionKind::Emitter;
    _effectSelection.emitterId = emitterId;
    _effectSelection.moduleId = 0;
    ++_effectSelectionRevision;
}

void Editor_Context::Set_SelectEffectTypeData(uint32 emitterId)
{
    if (_effectSelection.kind == EffectAuthoringSelectionKind::TypeData &&
        _effectSelection.emitterId == emitterId)
        return;

    _effectSelection.kind = EffectAuthoringSelectionKind::TypeData;
    _effectSelection.emitterId = emitterId;
    _effectSelection.moduleId = 0;
    ++_effectSelectionRevision;
}

void Editor_Context::Set_SelectEffectModule(uint32 emitterId, uint32 moduleId)
{
    if (_effectSelection.kind == EffectAuthoringSelectionKind::Module &&
        _effectSelection.emitterId == emitterId &&
        _effectSelection.moduleId == moduleId)
        return;

    _effectSelection.kind = EffectAuthoringSelectionKind::Module;
    _effectSelection.emitterId = emitterId;
    _effectSelection.moduleId = moduleId;
    ++_effectSelectionRevision;
}

void Editor_Context::Clear_EffectSelection()
{
    if (_effectSelection.kind == EffectAuthoringSelectionKind::None)
        return;

    _effectSelection = {};
    ++_effectSelectionRevision;
}

void Editor_Context::Request_FocusObject(const Shared<GameObject>& gameObject)
{
    _focusRequest.targetObject = gameObject;
}

Shared<GameObject> Editor_Context::Consume_FocusObjectRequest()
{
    Shared<GameObject> targetObject = _focusRequest.targetObject;
    _focusRequest.targetObject.reset();
    return targetObject;
}

Unique<Editor_Context> Editor_Context::Create()
{
    return make_unique<Editor_Context>();
}

void Editor_Context::Free()
{
    Base::Free();
}

NS_END
