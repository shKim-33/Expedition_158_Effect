#include "Emitter_View.h"

#include "CurveEditor_View.h"
#include "Editor_Context.h"
#include "EffectAuthoring_Types.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialPresetReader.h"
#include "Helper_EffectAuthoring.h"
#include "Notification_Manager.h"

NS_BEGIN(EffectEditor)

void Emitter_View::Clear_Selection()
{
    _selectedEmitterIndex.reset();
    _selectedTypeData = false;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Clear_EffectSelection();
}

void Emitter_View::Clamp_Selection()
{
    if (_emitters.empty())
    {
        _selectedEmitterIndex.reset();
        _selectedTypeData = false;
        _selectedModuleIndex.reset();
        return;
    }

    if (!_selectedEmitterIndex.has_value())
    {
        _selectedTypeData = false;
        _selectedModuleIndex.reset();
        return;
    }

    _selectedEmitterIndex = min(_selectedEmitterIndex.value(), _emitters.size() - 1);

    const auto& modules = _emitters[_selectedEmitterIndex.value()].modules;
    if (_selectedModuleIndex.has_value() && _selectedModuleIndex.value() >= modules.size())
        _selectedModuleIndex.reset();

    if (_selectedTypeData)
        _selectedModuleIndex.reset();
}

void Emitter_View::Select_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
    {
        Clear_Selection();
        return;
    }

    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(_emitters[emitterIndex].id);
}

void Emitter_View::Select_Module(size_t emitterIndex, size_t moduleIndex)
{
    if (emitterIndex >= _emitters.size() || moduleIndex >= _emitters[emitterIndex].modules.size())
    {
        Clear_Selection();
        return;
    }

    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex = moduleIndex;

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectModule(_emitters[emitterIndex].id, _emitters[emitterIndex].modules[moduleIndex].id);
}

void Emitter_View::Select_TypeData(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
    {
        Clear_Selection();
        return;
    }

    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = true;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectTypeData(_emitters[emitterIndex].id);
}

void Emitter_View::Sync_SelectionContext()
{
    if (EDITOR == nullptr || EDITOR->Get_EditorContext() == nullptr)
        return;

    if (!_selectedEmitterIndex.has_value() || _selectedEmitterIndex.value() >= _emitters.size())
    {
        EDITOR->Get_EditorContext()->Clear_EffectSelection();
        return;
    }

    const AuthoringEmitter& emitter = _emitters[_selectedEmitterIndex.value()];

    if (_selectedTypeData)
    {
        EDITOR->Get_EditorContext()->Set_SelectEffectTypeData(emitter.id);
        return;
    }

    if (_selectedModuleIndex.has_value() && _selectedModuleIndex.value() < emitter.modules.size())
    {
        EDITOR->Get_EditorContext()->Set_SelectEffectModule(
            emitter.id,
            emitter.modules[_selectedModuleIndex.value()].id
        );
        return;
    }

    EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(emitter.id);
}

void Emitter_View::Delete_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    Close_ModulePicker();
    _emitters.erase(_emitters.begin() + static_cast<ptrdiff_t>(emitterIndex));
    Clamp_Selection();

    if (_emitters.empty())
        Clear_Selection();
    else
    {
        const size_t nextIndex = min(emitterIndex, _emitters.size() - 1);
        Select_Emitter(nextIndex);
    }

    Sanitize_PreviewTransformState();
    MarkDirty();
}

void Emitter_View::Duplicate_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    Close_ModulePicker();
    const size_t insertIndex = emitterIndex + 1;
    _emitters.insert(
        _emitters.begin() + static_cast<ptrdiff_t>(insertIndex),
        Clone_Emitter(_emitters[emitterIndex])
    );
    Select_Emitter(insertIndex);
    Sanitize_PreviewTransformState();
    MarkDirty();
}

void Emitter_View::Append_ImportedEmitters(const vector<AuthoringEmitter>& sourceEmitters)
{
    if (sourceEmitters.empty())
        return;

    Close_ModulePicker();
    const size_t firstImportedIndex = _emitters.size();
    _emitters.reserve(_emitters.size() + sourceEmitters.size());

    for (const AuthoringEmitter& sourceEmitter : sourceEmitters)
        _emitters.push_back(Clone_Emitter(sourceEmitter));

    Select_Emitter(firstImportedIndex);
    Sanitize_PreviewTransformState();
    MarkDirty();
}

void Emitter_View::Insert_NewEmitter(size_t insertIndex)
{
    Close_ModulePicker();
    const size_t clampedIndex = min(insertIndex, _emitters.size());
    _emitters.insert(_emitters.begin() + static_cast<ptrdiff_t>(clampedIndex), Make_DefaultSpriteEmitter());
    Select_Emitter(clampedIndex);
    Sanitize_PreviewTransformState();
    MarkDirty();
}

void Emitter_View::Process_PendingEmitterAction()
{
    if (_pendingEmitterAction == PendingEmitterAction::None || !_pendingEmitterActionIndex.has_value())
        return;

    const size_t emitterIndex = _pendingEmitterActionIndex.value();
    switch (_pendingEmitterAction)
    {
    case PendingEmitterAction::Duplicate:
        Execute_AuthoringEdit(
            "Duplicate Emitter",
            [this, emitterIndex]
            {
                Duplicate_Emitter(emitterIndex);
            }
        );
        break;
    case PendingEmitterAction::Delete:
        Execute_AuthoringEdit(
            "Delete Emitter",
            [this, emitterIndex]
            {
                Delete_Emitter(emitterIndex);
            }
        );
        break;
    case PendingEmitterAction::InsertBefore:
        Execute_AuthoringEdit(
            "Insert Emitter",
            [this, emitterIndex]
            {
                Insert_NewEmitter(emitterIndex);
            }
        );
        break;
    case PendingEmitterAction::InsertAfter:
        Execute_AuthoringEdit(
            "Insert Emitter",
            [this, emitterIndex]
            {
                Insert_NewEmitter(emitterIndex + 1);
            }
        );
        break;
    case PendingEmitterAction::None:
    default:
        break;
    }

    _pendingEmitterAction = PendingEmitterAction::None;
    _pendingEmitterActionIndex.reset();
}

void Emitter_View::Queue_EmitterAction(PendingEmitterAction action, size_t emitterIndex)
{
    _pendingEmitterAction = action;
    _pendingEmitterActionIndex = emitterIndex;
}

void Emitter_View::Delete_Module(size_t emitterIndex, size_t moduleIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (moduleIndex >= emitter.modules.size())
        return;

    const AuthoringModule& module = emitter.modules[moduleIndex];
    if (!module.removable)
        return;

    const bool isSelectedEmitter = _selectedEmitterIndex.has_value() && _selectedEmitterIndex.value() == emitterIndex;
    const bool isSelectedModule = isSelectedEmitter && _selectedModuleIndex.has_value();
    const bool isDeletingSelectedModule = isSelectedModule && _selectedModuleIndex.value() == moduleIndex;

    emitter.modules.erase(emitter.modules.begin() + static_cast<ptrdiff_t>(moduleIndex));

    if (isDeletingSelectedModule)
        Select_Emitter(emitterIndex);
    else if (isSelectedModule && _selectedModuleIndex.value() > moduleIndex)
        _selectedModuleIndex = _selectedModuleIndex.value() - 1;

    emitter.previewDirty = true;
    MarkDirty();
}

void Emitter_View::Process_PendingModuleAction()
{
    if (_pendingModuleAction == PendingModuleAction::None ||
        !_pendingModuleEmitterIndex.has_value() ||
        !_pendingModuleIndex.has_value())
        return;

    const size_t emitterIndex = _pendingModuleEmitterIndex.value();
    const size_t moduleIndex = _pendingModuleIndex.value();
    switch (_pendingModuleAction)
    {
    case PendingModuleAction::Delete:
        Execute_AuthoringEdit(
            "Delete Module",
            [this, emitterIndex, moduleIndex]
            {
                Delete_Module(emitterIndex, moduleIndex);
            }
        );
        break;
    case PendingModuleAction::ResetData:
        Execute_AuthoringEdit(
            "Reset Module Data",
            [this, emitterIndex, moduleIndex]
            {
                Reset_ModuleData(emitterIndex, moduleIndex);
            }
        );
        break;
    case PendingModuleAction::None:
    default:
        break;
    }

    _pendingModuleAction = PendingModuleAction::None;
    _pendingModuleEmitterIndex.reset();
    _pendingModuleIndex.reset();
}

void Emitter_View::Queue_ModuleAction(PendingModuleAction action, size_t emitterIndex, size_t moduleIndex)
{
    _pendingModuleAction = action;
    _pendingModuleEmitterIndex = emitterIndex;
    _pendingModuleIndex = moduleIndex;
}

void Emitter_View::Reset_ModuleData(size_t emitterIndex, size_t moduleIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (moduleIndex >= emitter.modules.size())
        return;

    AuthoringModule& module = emitter.modules[moduleIndex];
    switch (module.type)
    {
    case AuthoringModuleType::Required:
    {
        RequiredModuleData data = Make_DefaultRequiredModuleData();
        module.data = data;
        emitter.textureId = data.material.mainTexturePath;
        emitter.resourceSummary = data.material.mainTexturePath;
        break;
    }

    case AuthoringModuleType::Spawn:
        module.data = SpawnModuleData{};
        break;

    case AuthoringModuleType::Lifetime:
        module.data = LifetimeModuleData{};
        break;

    case AuthoringModuleType::InitialLocation:
        module.data = InitialLocationModuleData{};
        break;

    case AuthoringModuleType::SphereLocation:
        module.data = SphereLocationModuleData{};
        break;

    case AuthoringModuleType::PlaneRadialLocation:
        module.data = PlaneRadialLocationModuleData{};
        break;

    case AuthoringModuleType::CylinderLocation:
        module.data = CylinderLocationModuleData{};
        break;

    case AuthoringModuleType::InitialSize:
        module.data = InitialSizeModuleData{};
        break;

    case AuthoringModuleType::InitialMeshSize:
        module.data = InitialMeshSizeModuleData{};
        break;

    case AuthoringModuleType::InitialVelocity:
        module.data = InitialVelocityModuleData{};
        break;

    case AuthoringModuleType::InitialRadialVelocity:
        module.data = InitialRadialVelocityModuleData{};
        break;

    case AuthoringModuleType::VelocityCone:
        module.data = VelocityConeModuleData{};
        break;

    case AuthoringModuleType::SourceMotionVelocity:
        module.data = SourceMotionVelocityModuleData{};
        break;

    case AuthoringModuleType::Acceleration:
        module.data = AccelerationModuleData{};
        break;

    case AuthoringModuleType::Drag:
        module.data = DragModuleData{};
        break;

    case AuthoringModuleType::VelocityOverLife:
        module.data = VelocityOverLifeModuleData{};
        break;

    case AuthoringModuleType::OrbitOverLife:
        module.data = OrbitOverLifeModuleData{};
        break;

    case AuthoringModuleType::InitialRotation:
        module.data = InitialRotationModuleData{};
        break;

    case AuthoringModuleType::SphereRadialOrientation:
        module.data = SphereRadialOrientationModuleData{};
        break;

    case AuthoringModuleType::PlaneRadialOrientation:
        module.data = PlaneRadialOrientationModuleData{};
        break;

    case AuthoringModuleType::CylinderOrientation:
        module.data = CylinderOrientationModuleData{};
        break;

    case AuthoringModuleType::RotationOverLife:
        module.data = RotationOverLifeModuleData{};
        break;

    case AuthoringModuleType::SpriteTilt:
        module.data = SpriteTiltModuleData{};
        break;

    case AuthoringModuleType::SpriteTiltOverLife:
        module.data = SpriteTiltOverLifeModuleData{};
        break;

    case AuthoringModuleType::InitialRotationRate:
        module.data = InitialRotationRateModuleData{};
        break;

    case AuthoringModuleType::RotationRateScaleByLife:
        module.data = RotationRateScaleByLifeModuleData{};
        break;

    case AuthoringModuleType::InitialMeshRotation:
        module.data = InitialMeshRotationModuleData{};
        break;

    case AuthoringModuleType::MeshRotationOverLife:
        module.data = MeshRotationOverLifeModuleData{};
        break;

    case AuthoringModuleType::MeshDirectionAlignOverLife:
        module.data = MeshDirectionAlignOverLifeModuleData{};
        break;

    case AuthoringModuleType::InitialMeshRotationRate:
        module.data = InitialMeshRotationRateModuleData{};
        break;

    case AuthoringModuleType::MeshRotationRateScaleByLife:
        module.data = MeshRotationRateScaleByLifeModuleData{};
        break;

    case AuthoringModuleType::InitialColor:
        module.data = InitialColorModuleData{};
        break;

    case AuthoringModuleType::ColorOverLife:
        module.data = ColorOverLifeModuleData{};
        break;

    case AuthoringModuleType::SubUVFrameOverLife:
        module.data = SubUVFrameOverLifeModuleData{};
        break;

    case AuthoringModuleType::SizeByLife:
        module.data = SizeByLifeModuleData{};
        break;

    case AuthoringModuleType::BeamEnvelopeOverLife:
        module.data = BeamEnvelopeOverLifeModuleData{};
        break;

    case AuthoringModuleType::MeshSizeByLife:
        module.data = MeshSizeByLifeModuleData{};
        break;

    case AuthoringModuleType::SpawnPerUnit:
        module.data = SpawnPerUnitModuleData{};
        break;

    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        module.data = SourceHistorySpriteTrailPathFollowModuleData{};
        break;

    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        module.data = SourceHistorySpriteTrailPathReplayModuleData{};
        break;

    case AuthoringModuleType::RibbonOrientation:
        module.data = RibbonOrientationModuleData{};
        break;

    case AuthoringModuleType::MaterialScalarModulation:
        module.data = MaterialScalarModulationModuleData{};
        break;

    default:
        return;
    }

    Sync_EmitterMirror(emitter);
    Sanitize_PreviewTransformState();
    emitter.previewDirty = true;
    MarkDirty();
}

void Emitter_View::Apply_TypeDataAction(PendingTypeDataAction action, size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    switch (action)
    {
    case PendingTypeDataAction::AddTrail:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::Trail;
        emitter.typeData.payload = TrailTypeData{};
        break;
    }
    case PendingTypeDataAction::AddMesh:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::Mesh;
        emitter.typeData.payload = MeshTypeData{};
        break;
    }
    case PendingTypeDataAction::AddRibbon:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::Ribbon;
        emitter.typeData.payload = RibbonTypeData{};
        break;
    }
    case PendingTypeDataAction::AddSourceHistorySpriteTrail:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::SourceHistorySpriteTrail;
        emitter.typeData.payload = SourceHistorySpriteTrailTypeData{};
        break;
    }
    case PendingTypeDataAction::AddBeam:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::Beam;
        emitter.typeData.payload = BeamTypeData{};
        break;
    }
    case PendingTypeDataAction::ResetData:
    {
        if (emitter.typeData.kind == AuthoringTypeDataKind::Mesh)
            emitter.typeData.payload = MeshTypeData{};
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Ribbon)
            emitter.typeData.payload = RibbonTypeData{};
        else if (emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail)
            emitter.typeData.payload = SourceHistorySpriteTrailTypeData{};
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Beam)
            emitter.typeData.payload = BeamTypeData{};
        else
        {
            emitter.typeData.kind = AuthoringTypeDataKind::Trail;
            emitter.typeData.payload = TrailTypeData{};
        }
        break;
    }
    case PendingTypeDataAction::Remove:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::None;
        emitter.typeData.payload = EmptyTypeData{};
        break;
    }
    case PendingTypeDataAction::None:
    default:
        return;
    }

    Sync_EmitterMirror(emitter);
    Sanitize_PreviewTransformState();
    emitter.previewDirty = true;
    MarkDirty();
}

void Emitter_View::Process_PendingTypeDataAction()
{
    if (_pendingTypeDataAction == PendingTypeDataAction::None ||
        !_pendingTypeDataEmitterIndex.has_value())
        return;

    const PendingTypeDataAction action = _pendingTypeDataAction;
    const size_t emitterIndex = _pendingTypeDataEmitterIndex.value();
    auto description = "Edit TypeData";
    switch (action)
    {
    case PendingTypeDataAction::AddTrail:
        description = "Add Trail TypeData";
        break;

    case PendingTypeDataAction::AddMesh:
        description = "Add Mesh TypeData";
        break;

    case PendingTypeDataAction::AddRibbon:
        description = "Add Ribbon TypeData";
        break;

    case PendingTypeDataAction::AddSourceHistorySpriteTrail:
        description = "Add Source History Sprite Trail TypeData";
        break;

    case PendingTypeDataAction::AddBeam:
        description = "Add Beam TypeData";
        break;

    case PendingTypeDataAction::ResetData:
        description = "Reset TypeData";
        break;

    case PendingTypeDataAction::Remove:
        description = "Remove TypeData";
        break;

    case PendingTypeDataAction::None:
    default:
        break;
    }

    Execute_AuthoringEdit(
        description,
        [this, action, emitterIndex]
        {
            Apply_TypeDataAction(action, emitterIndex);
        }
    );

    _pendingTypeDataAction = PendingTypeDataAction::None;
    _pendingTypeDataEmitterIndex.reset();
}

void Emitter_View::Queue_TypeDataAction(PendingTypeDataAction action, size_t emitterIndex)
{
    _pendingTypeDataAction = action;
    _pendingTypeDataEmitterIndex = emitterIndex;
}

void Emitter_View::Mark_AllPreviewDirty()
{
    for (AuthoringEmitter& emitter : _emitters)
        emitter.previewDirty = true;
}

void Emitter_View::Set_AllEmittersEnabled(bool enabled)
{
    bool hasChange = false;
    for (const AuthoringEmitter& emitter : _emitters)
    {
        if (emitter.enabled != enabled)
        {
            hasChange = true;
            break;
        }
    }

    const bool shouldClearSoloPreview = !enabled && Is_SoloPreviewActive();
    if (!hasChange && !shouldClearSoloPreview)
        return;

    Execute_AuthoringEdit(
        enabled ? "Enable All Emitters" : "Disable All Emitters",
        [this, enabled, shouldClearSoloPreview]
        {
            for (AuthoringEmitter& emitter : _emitters)
            {
                emitter.enabled = enabled;
                emitter.previewDirty = true;
            }

            if (shouldClearSoloPreview)
                Clear_SoloPreviewEmitter();

            MarkDirty();
        }
    );

    if (shouldClearSoloPreview && EDITOR != nullptr)
        EDITOR->Request_RestartPreview();
}

void Emitter_View::Sync_EmitterMirror(AuthoringEmitter& emitter)
{
    emitter.rendererType = Authoring::Resolve_RendererType(emitter.typeData);

    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type != AuthoringModuleType::Required)
            continue;

        const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
        if (requiredData == nullptr)
            return;

        emitter.textureId = requiredData->material.mainTexturePath;
        emitter.resourceSummary = requiredData->material.mainTexturePath;
        return;
    }
}

void Emitter_View::Sync_AllEmitterMirrors()
{
    for (AuthoringEmitter& emitter : _emitters)
        Sync_EmitterMirror(emitter);
}

bool Emitter_View::Is_SoloPreviewActive() const
{
    return _soloPreviewEmitterId.has_value();
}

bool Emitter_View::Is_SoloPreviewEmitter(uint32 emitterId) const
{
    return _soloPreviewEmitterId.has_value() && _soloPreviewEmitterId.value() == emitterId;
}

void Emitter_View::Toggle_SoloPreviewEmitter(uint32 emitterId)
{
    const AuthoringEmitter* emitter = Find_Emitter(emitterId);
    if (emitter == nullptr || !emitter->enabled)
        return;

    if (Is_SoloPreviewEmitter(emitterId))
        _soloPreviewEmitterId.reset();
    else
        _soloPreviewEmitterId = emitterId;

    if (EDITOR != nullptr)
        EDITOR->Request_RestartPreview();
}

void Emitter_View::Clear_SoloPreviewEmitter()
{
    _soloPreviewEmitterId.reset();
}

void Emitter_View::Sanitize_SoloPreviewState()
{
    if (!_soloPreviewEmitterId.has_value())
        return;

    const uint32 soloEmitterId = _soloPreviewEmitterId.value();
    const AuthoringEmitter* emitter = nullptr;
    for (const AuthoringEmitter& candidate : _emitters)
    {
        if (candidate.id == soloEmitterId)
        {
            emitter = &candidate;
            break;
        }
    }

    if (emitter == nullptr || !emitter->enabled)
        _soloPreviewEmitterId.reset();
}

void Emitter_View::Sanitize_PreviewTransformState()
{
    Sanitize_SoloPreviewState();

    if (EDITOR != nullptr)
        EDITOR->Sanitize_PreviewEmitterTransforms(_emitters);
}

NS_END
