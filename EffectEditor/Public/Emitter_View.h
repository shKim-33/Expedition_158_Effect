#pragma once

#include "Editor_Window.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class Emitter_View final : public Editor_Window
{
public:
    struct AuthoringSnapshot
    {
        ParticleSystemAuthoringData particleSystemData{};
        HistoryBudgetData historyBudget{};
        vector<AuthoringEmitter> emitters{};
        optional<size_t> selectedEmitterIndex{};
        bool selectedTypeData{ false };
        optional<size_t> selectedModuleIndex{};
        uint32 nextAuthoringId{ 1 };
    };

public:
    Emitter_View();
    ~Emitter_View() override = default;

public:
    HRESULT Initialize() override;
    void Render() override;

    bool CanSave() const override;
    void Save() override;
    void SaveAs();
    bool CanOpen() const;
    void Open();
    bool CanAdd() const;
    void Add();

public: //## Accessors::Authoring
    ParticleSystemAuthoringData& Get_ParticleSystemData() { return _particleSystemData; }
    const ParticleSystemAuthoringData& Get_ParticleSystemData() const { return _particleSystemData; }
    HistoryBudgetData& Get_HistoryBudget() { return _historyBudget; }
    const HistoryBudgetData& Get_HistoryBudget() const { return _historyBudget; }
    const vector<AuthoringEmitter>& Get_Emitters() const { return _emitters; }
    vector<AuthoringEmitter> Build_PreviewEmitters() const;
    const optional<fs::path>& Get_CurrentDocumentPath() const { return _currentDocumentPath; }
    void Set_HistoryBudget(const HistoryBudgetData& historyBudget, const string& description);
    AuthoringEmitter* Find_Emitter(uint32 emitterId);
    AuthoringModule* Find_Module(uint32 emitterId, uint32 moduleId);
    AuthoringEmitter* Get_SelectedOrFirstEmitter();
    void Clear_PreviewDirty(uint32 emitterId);
    bool Has_PreviewDirtyEmitter() const;

public: //## Behavior::AuthoringHistory
    AuthoringSnapshot Capture_AuthoringSnapshot() const;
    void Restore_AuthoringSnapshot(const AuthoringSnapshot& snapshot);
    void Execute_AuthoringSnapshotCommand(
        const AuthoringSnapshot& beforeSnapshot,
        const AuthoringSnapshot& afterSnapshot,
        const string& description);
    json Build_AuthoringDocumentJson() const;
    bool Restore_AuthoringDocumentJson(const json& root);

private: //## Types::PendingAction
    enum class AuthoringClipboardKind
    {
        None,
        Emitter,
        Module
    };

    enum class PendingEmitterAction
    {
        None,
        Duplicate,
        Delete,
        InsertBefore,
        InsertAfter
    };

    enum class PendingModuleAction
    {
        None,
        Delete,
        ResetData
    };

    enum class PendingTypeDataAction
    {
        None,
        AddTrail,
        AddMesh,
        AddRibbon,
        AddSourceHistorySpriteTrail,
        AddBeam,
        ResetData,
        Remove
    };

    enum class PendingClipboardAction
    {
        None,
        PasteEmitterAfter,
        PasteEmitterToEnd,
        PasteModule,
        PasteModuleValues
    };

private: //## Types::ModulePicker
    struct ModulePickerEntry
    {
        optional<AuthoringModuleType> moduleType{};
        wstring displayName{};
        string stateText{};
        string tooltipText{};
        bool implemented{ false };
        bool compatible{ true };
        bool alreadyAdded{ false };
        bool canAdd{ false };
    };

private: //## Static::Authoring
    inline static const Color kEmitterAccentColor{ 1.0f, 0.55f, 0.20f, 1.0f };
    inline static const Color kEmitterColumnBackgroundColor{ 0.06f, 0.065f, 0.08f, 1.0f };
    inline static const Color kEmitterDividerColor{ 0.015f, 0.018f, 0.025f, 1.0f };
    inline static const Color kEmitterHeaderColor{ 0.36f, 0.36f, 0.36f, 1.0f };
    inline static const Color kEmitterSelectedTextColor{ 0.05f, 0.05f, 0.05f, 1.0f };
    inline static const Color kEmitterCategoryTextColor{ 0.08f, 0.08f, 0.08f, 1.0f };
    inline static const Color kEmitterTypeDataRowColor{ 0.45f, 0.75f, 0.50f, 1.0f };
    inline static const Color kEmitterRequiredRowColor{ 0.9f, 0.35f, 0.35f, 1.0f };
    inline static const Color kEmitterSpawnRowColor{ 0.75f, 0.75f, 0.3f, 1.0f };
    inline static const Color kEmitterCommonModuleRowColor{ 0.18f, 0.19f, 0.24f, 1.0f };
    inline static const Color kEmitterModuleDisabledRowColor{ 0.16f, 0.16f, 0.17f, 1.0f };
    inline static const Color kEmitterUnsupportedModuleTextColor{ 0.9f, 0.25f, 0.25f, 1.0f };

    static constexpr float kEmitterColumnWidth{ 270.f };
    static constexpr float kEmitterHeaderHeight{ 90.f };
    static constexpr float kEmitterTypeDataRowHeight{ 28.f };
    static constexpr float kEmitterDividerWidth{ 1.f };
    static constexpr float kEmitterRightBlankWidth{ 96.f };

    static constexpr float kModulePickerModalWidth{ 700.f };
    static constexpr float kModulePickerModalHeight{ 460.f };
    static constexpr float kModulePickerNameColumnStretch{ 0.5f };
    static constexpr float kModulePickerStateColumnStretch{ 1.f - kModulePickerNameColumnStretch };

private: //## Data::Authoring
    ParticleSystemAuthoringData _particleSystemData{};
    HistoryBudgetData _historyBudget{};
    vector<AuthoringEmitter> _emitters{};
    optional<fs::path> _currentDocumentPath{};
    uint32 _nextAuthoringId{ 1 };

private: //## Data::Selection
    optional<size_t> _selectedEmitterIndex{};
    bool _selectedTypeData{ false };
    optional<size_t> _selectedModuleIndex{};

private: //## Data::PreviewState
    optional<uint32> _soloPreviewEmitterId{};

private: //## Data::PendingAction
    PendingEmitterAction _pendingEmitterAction{ PendingEmitterAction::None };
    optional<size_t> _pendingEmitterActionIndex{};
    PendingModuleAction _pendingModuleAction{ PendingModuleAction::None };
    optional<size_t> _pendingModuleEmitterIndex{};
    optional<size_t> _pendingModuleIndex{};
    PendingTypeDataAction _pendingTypeDataAction{ PendingTypeDataAction::None };
    optional<size_t> _pendingTypeDataEmitterIndex{};
    PendingClipboardAction _pendingClipboardAction{ PendingClipboardAction::None };
    optional<size_t> _pendingClipboardEmitterIndex{};
    optional<size_t> _pendingClipboardModuleIndex{};

private: //## Data::ModulePicker
    optional<size_t> _modulePickerEmitterIndex{};
    bool _modulePickerPendingOpen{ false };
    char _modulePickerSearchBuffer[128]{};

private: //## Data::Clipboard
    AuthoringClipboardKind _clipboardKind{ AuthoringClipboardKind::None };
    optional<AuthoringEmitter> _clipboardEmitter{};
    optional<AuthoringModule> _clipboardModule{};

private: //## Helper::DocumentSave
    bool Save_ToPath(const fs::path& savePath, bool backupExistingFile);

private: //## Helper::AuthoringFactory
    uint32 Issue_AuthoringId();
    AuthoringEmitter Make_DefaultSpriteEmitter();
    AuthoringEmitter Clone_Emitter(const AuthoringEmitter& sourceEmitter);
    AuthoringModule Clone_Module(const AuthoringModule& sourceModule);
    RequiredModuleData Make_DefaultRequiredModuleData();
    AuthoringModule Make_DefaultModuleByType(AuthoringModuleType type);
    AuthoringModule Make_RequiredModule();
    AuthoringModule Make_SpawnModule();
    AuthoringModule Make_LifetimeModule();
    AuthoringModule Make_InitialLocationModule();
    AuthoringModule Make_SphereLocationModule();
    AuthoringModule Make_PlaneRadialLocationModule();
    AuthoringModule Make_CylinderLocationModule();
    AuthoringModule Make_InitialSizeModule();
    AuthoringModule Make_InitialMeshSizeModule();
    AuthoringModule Make_InitialVelocityModule();
    AuthoringModule Make_InitialRadialVelocityModule();
    AuthoringModule Make_VelocityConeModule();
    AuthoringModule Make_SourceMotionVelocityModule();
    AuthoringModule Make_AccelerationModule();
    AuthoringModule Make_DragModule();
    AuthoringModule Make_VelocityOverLifeModule();
    AuthoringModule Make_OrbitOverLifeModule();
    AuthoringModule Make_InitialRotationModule();
    AuthoringModule Make_SphereRadialOrientationModule();
    AuthoringModule Make_PlaneRadialOrientationModule();
    AuthoringModule Make_CylinderOrientationModule();
    AuthoringModule Make_RotationOverLifeModule();
    AuthoringModule Make_SpriteTiltModule();
    AuthoringModule Make_SpriteTiltOverLifeModule();
    AuthoringModule Make_InitialRotationRateModule();
    AuthoringModule Make_RotationRateScaleByLifeModule();
    AuthoringModule Make_InitialMeshRotationModule();
    AuthoringModule Make_MeshRotationOverLifeModule();
    AuthoringModule Make_MeshDirectionAlignOverLifeModule();
    AuthoringModule Make_InitialMeshRotationRateModule();
    AuthoringModule Make_MeshRotationRateScaleByLifeModule();
    AuthoringModule Make_InitialColorModule();
    AuthoringModule Make_ColorOverLifeModule();
    AuthoringModule Make_SubUVFrameOverLifeModule();
    AuthoringModule Make_SizeByLifeModule();
    AuthoringModule Make_BeamEnvelopeOverLifeModule();
    AuthoringModule Make_MeshSizeByLifeModule();
    AuthoringModule Make_SpawnPerUnitModule();
    AuthoringModule Make_SourceHistorySpriteTrailPathFollowModule();
    AuthoringModule Make_SourceHistorySpriteTrailPathReplayModule();
    AuthoringModule Make_RibbonOrientationModule();
    AuthoringModule Make_MaterialScalarModulationModule();

private: //## Helper::ModulePolicy
    bool Has_ModuleType(const AuthoringEmitter& emitter, AuthoringModuleType type) const;
    bool Is_ModuleCompatibleWithEmitter(const AuthoringEmitter& emitter, AuthoringModuleType type) const;
    bool Can_AddModuleType(const AuthoringEmitter& emitter, AuthoringModuleType type) const;
    void Add_Module(size_t emitterIndex, AuthoringModuleType type);

private: //## Helper::Clipboard
    void Copy_EmitterToClipboard(size_t emitterIndex);
    void Copy_ModuleToClipboard(size_t emitterIndex, size_t moduleIndex);
    bool Can_PasteEmitterAfter(size_t emitterIndex, string* disabledReason = nullptr) const;
    bool Can_PasteEmitterToEnd(string* disabledReason = nullptr) const;
    bool Can_PasteModuleInto(size_t emitterIndex, string* disabledReason = nullptr) const;
    bool Can_PasteModuleValues(size_t emitterIndex, size_t moduleIndex, string* disabledReason = nullptr) const;
    void Paste_EmitterAfter(size_t emitterIndex);
    void Paste_EmitterToEnd();
    void Paste_ModuleInto(size_t emitterIndex);
    void Paste_ModuleValues(size_t emitterIndex, size_t moduleIndex);
    void Notify_ClipboardStatus(const string& message, bool warning = false) const;
    void Handle_Shortcuts();
    void Process_PendingClipboardAction();
    void Queue_ClipboardAction(PendingClipboardAction action, size_t emitterIndex, optional<size_t> moduleIndex = {});

private: //## Helper::Selection
    void Clear_Selection();
    void Clamp_Selection();
    void Select_Emitter(size_t emitterIndex);
    void Select_Module(size_t emitterIndex, size_t moduleIndex);
    void Select_TypeData(size_t emitterIndex);
    void Sync_SelectionContext();

private: //## Helper::EmitterAction
    void Delete_Emitter(size_t emitterIndex);
    void Duplicate_Emitter(size_t emitterIndex);
    void Append_ImportedEmitters(const vector<AuthoringEmitter>& sourceEmitters);
    void Insert_NewEmitter(size_t insertIndex);
    void Process_PendingEmitterAction();
    void Queue_EmitterAction(PendingEmitterAction action, size_t emitterIndex);

private: //## Helper::ModuleAction
    void Delete_Module(size_t emitterIndex, size_t moduleIndex);
    void Process_PendingModuleAction();
    void Queue_ModuleAction(PendingModuleAction action, size_t emitterIndex, size_t moduleIndex);
    void Reset_ModuleData(size_t emitterIndex, size_t moduleIndex);

private: //## Helper::TypeDataAction
    void Apply_TypeDataAction(PendingTypeDataAction action, size_t emitterIndex);
    void Process_PendingTypeDataAction();
    void Queue_TypeDataAction(PendingTypeDataAction action, size_t emitterIndex);

private: //## Helper::PreviewState
    void Mark_AllPreviewDirty();
    void Set_AllEmittersEnabled(bool enabled);
    void Sync_EmitterMirror(AuthoringEmitter& emitter);
    void Sync_AllEmitterMirrors();
    bool Is_SoloPreviewActive() const;
    bool Is_SoloPreviewEmitter(uint32 emitterId) const;
    void Toggle_SoloPreviewEmitter(uint32 emitterId);
    void Clear_SoloPreviewEmitter();
    void Sanitize_SoloPreviewState();
    void Sanitize_PreviewTransformState();

private: //## Helper::ModulePicker
    vector<ModulePickerEntry> Build_ModulePickerEntries(const AuthoringEmitter& emitter) const;
    void Close_ModulePicker();
    void Draw_ModulePickerPopup();
    void Open_ModulePicker(size_t emitterIndex);

private: //## Helper::AuthoringHistory
    void Execute_AuthoringEdit(const string& description, const function<void()>& edit);

private: //## Helper::Render
    string Build_ModuleWarningTooltip(const AuthoringEmitter& emitter, const AuthoringModule& module) const;
    void Draw_EmitterContextMenu(size_t emitterIndex);
    void Draw_TypeDataContextMenu(size_t emitterIndex);
    void Draw_ModuleContextMenu(size_t emitterIndex, size_t moduleIndex, const AuthoringModule& module);

    void Draw_EmitterBoard();
    void Draw_EmitterColumn(size_t emitterIndex, AuthoringEmitter& emitter, float columnHeight);
    void Draw_TypeDataRow(size_t emitterIndex);
    void Draw_ModuleRow(size_t emitterIndex, size_t moduleIndex, AuthoringModule& module);

public:
    static Shared<Emitter_View> Create();
};

NS_END
