#include "Emitter_View.h"

#include "Action_Command.h"
#include "CurveEditor_View.h"
#include "Editor_Context.h"
#include "EffectAuthoringJsonSerializer.h"
#include "EffectAuthoringModuleMetadata.h"
#include "EffectAuthoring_Types.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialPresetReader.h"
#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"
#include "Helper_ImGui.h"
#include "Helper_String.h"
#include "Notification_Manager.h"

#include <ctime>
#include <iomanip>
#include <sstream>

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kEffectAuthoringSaveDefaultFolder = L"../../../Client/Bin/Resources/Effects/Assets";
    constexpr auto kEffectAuthoringSaveDefaultFileName = L"NewEffect.effect.json";
    constexpr auto kEffectAuthoringExtension = L".effect.json";
    constexpr auto kEffectAuthoringBackupFolderName = L"Backup";

    fs::path Normalize_EffectSaveDialogPath(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {}

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {}

        return filePath.lexically_normal();
    }

    fs::path Ensure_EffectAuthoringExtension(const fs::path& filePath)
    {
        const string lowerPath = String::ToLowerCopy(filePath.generic_string());
        if (lowerPath.ends_with(".effect.json"))
            return filePath;

        return fs::path(filePath.wstring() + kEffectAuthoringExtension);
    }

    bool Try_MakeRelativePathUnderRoot(const fs::path& filePath, const fs::path& rootPath, fs::path& outRelativePath)
    {
        outRelativePath.clear();
        if (filePath.empty() || rootPath.empty())
            return false;

        try
        {
            const fs::path relativePath = fs::relative(filePath, rootPath);
            if (relativePath.empty())
                return false;

            for (const fs::path& part : relativePath)
            {
                if (part.native() == L"..")
                    return false;
            }

            if (relativePath.is_absolute())
                return false;

            outRelativePath = relativePath;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    fs::path Resolve_EffectAuthoringBackupRoot()
    {
        if (GAME != nullptr)
        {
            const wstring assetRoot = GAME->Get_AssetRoot();
            if (!assetRoot.empty())
                return (fs::path(assetRoot) / L"Effects" / L"Assets" / kEffectAuthoringBackupFolderName).lexically_normal();
        }

        const fs::path defaultFolder = Normalize_EffectSaveDialogPath(kEffectAuthoringSaveDefaultFolder);
        return (defaultFolder / kEffectAuthoringBackupFolderName).lexically_normal();
    }

    wstring Make_BackupTimestamp()
    {
        const time_t now = time(nullptr);
        tm localTime{};
        localtime_s(&localTime, &now);

        wstringstream stream{};
        stream << put_time(&localTime, L"%Y%m%d_%H%M%S");
        return stream.str();
    }

    fs::path Build_EffectAuthoringBackupPath(const fs::path& sourcePath)
    {
        const fs::path normalizedSourcePath = Normalize_EffectSaveDialogPath(sourcePath);
        const fs::path backupRoot = Resolve_EffectAuthoringBackupRoot();
        fs::path backupFolder = backupRoot;

        fs::path relativeSourcePath{};
        if (Try_MakeRelativePathUnderRoot(normalizedSourcePath, backupRoot.parent_path(), relativeSourcePath))
            backupFolder /= relativeSourcePath.parent_path();

        const wstring backupFileName =
            normalizedSourcePath.filename().wstring() + L"." + Make_BackupTimestamp() + L".bak";
        return backupFolder / backupFileName;
    }

    bool Try_BackupExistingEffectFile(const fs::path& sourcePath, fs::path& outBackupPath)
    {
        outBackupPath = Build_EffectAuthoringBackupPath(sourcePath);

        try
        {
            fs::create_directories(outBackupPath.parent_path());
            fs::copy_file(sourcePath, outBackupPath, fs::copy_options::none);
            return true;
        }
        catch (...)
        {
            outBackupPath.clear();
            return false;
        }
    }

    bool Try_PickEffectSaveFileDialog(fs::path& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninit = SUCCEEDED(initHr);

        IFileSaveDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUninit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(L"Save Effect");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT);

        const fs::path defaultFolder = Normalize_EffectSaveDialogPath(kEffectAuthoringSaveDefaultFolder);
        if (!defaultFolder.empty() && fs::exists(defaultFolder))
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[]
        {
            { L"Effect Authoring", L"*.effect.json" },
            { L"All", L"*.*" },
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetDefaultExtension(L"effect.json");
        dialog->SetFileName(kEffectAuthoringSaveDefaultFileName);

        const HWND owner = EDITOR != nullptr ? EDITOR->Get_WindowHandle() : nullptr;
        hr = dialog->Show(owner);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = Ensure_EffectAuthoringExtension(fs::path(path));
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();

        if (shouldUninit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    bool Try_PickEffectOpenFileDialog(fs::path& outFilePath, const wchar_t* title = L"Open Effect")
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUninit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(title);

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        const fs::path defaultFolder = Normalize_EffectSaveDialogPath(kEffectAuthoringSaveDefaultFolder);
        if (!defaultFolder.empty() && fs::exists(defaultFolder))
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[]
        {
            { L"Effect Authoring", L"*.effect.json" },
            { L"All", L"*.*" },
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        dialog->SetFileTypeIndex(1);

        const HWND owner = EDITOR != nullptr ? EDITOR->Get_WindowHandle() : nullptr;
        hr = dialog->Show(owner);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = fs::path(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();

        if (shouldUninit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    bool Confirm_DiscardDirtyAuthoringDocument()
    {
        const HWND owner = EDITOR != nullptr ? EDITOR->Get_WindowHandle() : nullptr;
        const int result = MessageBoxW(
            owner,
            L"저장되지 않은 변경 사항이 사라질 수 있습니다.\n계속 열겠습니까?",
            L"Open Effect",
            MB_OKCANCEL | MB_ICONWARNING
        );

        return result == IDOK;
    }
}

Emitter_View::Emitter_View()
    : Editor_Window{ L"Emitter", ICON_FA_FIRE }
{
}

HRESULT Emitter_View::Initialize()
{
    CHECK_FAILED(Editor_Window::Initialize(), E_FAIL);

    _particleSystemData = {};
    _emitters.clear();
    _emitters.push_back(Make_DefaultSpriteEmitter());
    _selectedEmitterIndex = 0;
    _selectedTypeData = false;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(_emitters.front().id);

    return S_OK;
}

void Emitter_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();
    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        Handle_Shortcuts();

        if (_modulePickerEmitterIndex.has_value() && _modulePickerEmitterIndex.value() >= _emitters.size())
            Close_ModulePicker();

        Draw_EmitterBoard();
        Draw_ModulePickerPopup();
    }

    Set_Open(isOpen);
    ImGui::End();
}

bool Emitter_View::CanSave() const
{
    return true;
}

void Emitter_View::Save()
{
    error_code errorCode{};
    if (!_currentDocumentPath.has_value() || !fs::exists(_currentDocumentPath.value(), errorCode))
    {
        SaveAs();
        return;
    }

    Save_ToPath(_currentDocumentPath.value(), true);
}

void Emitter_View::SaveAs()
{
    fs::path savePath{};
    if (!Try_PickEffectSaveFileDialog(savePath))
        return;

    Save_ToPath(savePath, false);
}

bool Emitter_View::Save_ToPath(const fs::path& savePath, bool backupExistingFile)
{
    const fs::path normalizedSavePath = Normalize_EffectSaveDialogPath(savePath);
    error_code errorCode{};
    if (backupExistingFile &&
        fs::exists(normalizedSavePath, errorCode) &&
        !errorCode &&
        fs::is_regular_file(normalizedSavePath, errorCode) &&
        !errorCode)
    {
        fs::path backupPath{};
        if (!Try_BackupExistingEffectFile(normalizedSavePath, backupPath))
        {
            if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
            {
                EDITOR->Get_Notification()->Add_Notification_With_Type(
                    NotifyType::Warning,
                    "Effect 백업 실패, 저장 중단: {}",
                    String::ToString(normalizedSavePath.filename().wstring())
                );
            }
            return false;
        }
    }

    ofstream file(normalizedSavePath);
    if (!file.is_open())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 저장 실패: {}",
                String::ToString(normalizedSavePath.filename().wstring())
            );
        }
        return false;
    }

    const json documentJson = Build_AuthoringDocumentJson();
    file << documentJson.dump(4);
    file.close();
    if (!file.good())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 저장 실패: {}",
                String::ToString(normalizedSavePath.filename().wstring())
            );
        }
        return false;
    }

    const string effectGuid = GAME != nullptr ? GAME->Ensure_AssetGUID(normalizedSavePath.wstring(), "Effect") : string{};
    _currentDocumentPath = normalizedSavePath;

    ClearDirty();

    if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
    {
        if (!effectGuid.empty())
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Success,
                "Effect 저장 완료: {}",
                String::ToString(normalizedSavePath.filename().wstring())
            );
        }
        else
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 저장 완료, meta 생성 실패: {}",
                String::ToString(normalizedSavePath.filename().wstring())
            );
        }
    }

    return true;
}

bool Emitter_View::CanOpen() const
{
    return true;
}

void Emitter_View::Open()
{
    if (Is_Dirty() && !Confirm_DiscardDirtyAuthoringDocument())
        return;

    fs::path openPath{};
    if (!Try_PickEffectOpenFileDialog(openPath))
        return;

    ifstream file(openPath);
    if (!file.is_open())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 열기 실패: {}",
                String::ToString(openPath.filename().wstring())
            );
        }
        return;
    }

    json documentJson{};
    try
    {
        file >> documentJson;
    }
    catch (const json::exception&)
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect JSON 해석 실패: {}",
                String::ToString(openPath.filename().wstring())
            );
        }
        return;
    }

    if (!Restore_AuthoringDocumentJson(documentJson))
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 복원 실패: {}",
                String::ToString(openPath.filename().wstring())
            );
        }
        return;
    }

    _currentDocumentPath = Normalize_EffectSaveDialogPath(openPath);
    ClearDirty();

    if (EDITOR != nullptr)
        EDITOR->Request_RestartPreview();

    if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(
            NotifyType::Success,
            "Effect 열기 완료: {}",
            String::ToString(openPath.filename().wstring())
        );
    }
}

bool Emitter_View::CanAdd() const
{
    return true;
}

void Emitter_View::Add()
{
    fs::path addPath{};
    if (!Try_PickEffectOpenFileDialog(addPath, L"Add Effect Emitters"))
        return;

    ifstream file(addPath);
    if (!file.is_open())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect Add 실패: {}",
                String::ToString(addPath.filename().wstring())
            );
        }
        return;
    }

    json documentJson{};
    try
    {
        file >> documentJson;
    }
    catch (const json::exception&)
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect JSON 해석 실패: {}",
                String::ToString(addPath.filename().wstring())
            );
        }
        return;
    }

    EffectAuthoringDocument document{};
    if (!EffectAuthoringJsonSerializer::From_Json(documentJson, document))
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect Add 실패: {}",
                String::ToString(addPath.filename().wstring())
            );
        }
        return;
    }

    if (document.emitters.empty())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect Add 실패: import 가능한 emitter가 없습니다."
            );
        }
        return;
    }

    for (AuthoringEmitter& emitter : document.emitters)
    {
        for (AuthoringModule& module : emitter.modules)
            Apply_AuthoringModuleMetadata(module);
    }

    const size_t importedCount = document.emitters.size();
    Execute_AuthoringEdit(
        "Add Emitters",
        [this, sourceEmitters = document.emitters]
        {
            Append_ImportedEmitters(sourceEmitters);
        }
    );

    if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(
            NotifyType::Success,
            "Effect Add 완료: {} ({} emitters)",
            String::ToString(addPath.filename().wstring()),
            importedCount
        );
    }
}

vector<AuthoringEmitter> Emitter_View::Build_PreviewEmitters() const
{
    vector<AuthoringEmitter> previewEmitters = _emitters;
    if (!_soloPreviewEmitterId.has_value())
        return previewEmitters;

    const uint32 soloEmitterId = _soloPreviewEmitterId.value();
    for (AuthoringEmitter& emitter : previewEmitters)
    {
        if (emitter.id != soloEmitterId)
            emitter.enabled = false;
    }

    return previewEmitters;
}

void Emitter_View::Set_HistoryBudget(const HistoryBudgetData& historyBudget, const string& description)
{
    Execute_AuthoringEdit(
        description,
        [this, historyBudget]
        {
            _historyBudget = historyBudget;
            Mark_AllPreviewDirty();
            MarkDirty();
        }
    );
}

AuthoringEmitter* Emitter_View::Find_Emitter(uint32 emitterId)
{
    for (AuthoringEmitter& emitter : _emitters)
    {
        if (emitter.id == emitterId)
            return &emitter;
    }

    return nullptr;
}

AuthoringModule* Emitter_View::Find_Module(uint32 emitterId, uint32 moduleId)
{
    AuthoringEmitter* emitter = Find_Emitter(emitterId);
    if (nullptr == emitter)
        return nullptr;

    for (AuthoringModule& module : emitter->modules)
    {
        if (module.id == moduleId)
            return &module;
    }

    return nullptr;
}

AuthoringEmitter* Emitter_View::Get_SelectedOrFirstEmitter()
{
    Clamp_Selection();

    if (_selectedEmitterIndex.has_value())
        return &_emitters[_selectedEmitterIndex.value()];

    if (!_emitters.empty())
        return &_emitters.front();

    return nullptr;
}

void Emitter_View::Clear_PreviewDirty(uint32 emitterId)
{
    AuthoringEmitter* emitter = Find_Emitter(emitterId);
    if (nullptr == emitter)
        return;

    emitter->previewDirty = false;
}

bool Emitter_View::Has_PreviewDirtyEmitter() const
{
    for (const AuthoringEmitter& emitter : _emitters)
    {
        if (emitter.previewDirty)
            return true;
    }

    return false;
}

Emitter_View::AuthoringSnapshot Emitter_View::Capture_AuthoringSnapshot() const
{
    AuthoringSnapshot snapshot{};
    snapshot.particleSystemData = _particleSystemData;
    snapshot.historyBudget = _historyBudget;
    snapshot.emitters = _emitters;
    snapshot.selectedEmitterIndex = _selectedEmitterIndex;
    snapshot.selectedTypeData = _selectedTypeData;
    snapshot.selectedModuleIndex = _selectedModuleIndex;
    snapshot.nextAuthoringId = _nextAuthoringId;
    return snapshot;
}

void Emitter_View::Restore_AuthoringSnapshot(const AuthoringSnapshot& snapshot)
{
    Close_ModulePicker();
    _particleSystemData = snapshot.particleSystemData;
    _historyBudget = snapshot.historyBudget;
    _emitters = snapshot.emitters;
    _selectedEmitterIndex = snapshot.selectedEmitterIndex;
    _selectedTypeData = snapshot.selectedTypeData;
    _selectedModuleIndex = snapshot.selectedModuleIndex;
    _nextAuthoringId = snapshot.nextAuthoringId;

    Sync_AllEmitterMirrors();
    Sanitize_SoloPreviewState();
    Sanitize_PreviewTransformState();
    Mark_AllPreviewDirty();
    Clamp_Selection();
    Sync_SelectionContext();
    MarkDirty();
}

void Emitter_View::Execute_AuthoringSnapshotCommand(
    const AuthoringSnapshot& beforeSnapshot,
    const AuthoringSnapshot& afterSnapshot,
    const string& description)
{
    if (EDITOR == nullptr)
        return;

    EDITOR->Execute_Command(
        Action_Command::Create(
            [this, beforeSnapshot]
            {
                Restore_AuthoringSnapshot(beforeSnapshot);
            },
            [this, afterSnapshot]
            {
                Restore_AuthoringSnapshot(afterSnapshot);
            },
            description
        )
    );
}

json Emitter_View::Build_AuthoringDocumentJson() const
{
    EffectAuthoringDocument document{};
    document.particleSystemData = _particleSystemData;
    document.historyBudget = _historyBudget;
    document.emitters = _emitters;
    return EffectAuthoringJsonSerializer::To_Json(document);
}

bool Emitter_View::Restore_AuthoringDocumentJson(const json& root)
{
    EffectAuthoringDocument document{};
    if (!EffectAuthoringJsonSerializer::From_Json(root, document))
        return false;

    for (AuthoringEmitter& emitter : document.emitters)
    {
        for (AuthoringModule& module : emitter.modules)
            Apply_AuthoringModuleMetadata(module);
    }

    const uint32 nextAuthoringId = EffectAuthoringJsonSerializer::Find_NextAuthoringId(document);

    Close_ModulePicker();
    _particleSystemData = document.particleSystemData;
    _historyBudget = document.historyBudget;
    _emitters = move(document.emitters);
    _selectedEmitterIndex = _emitters.empty() ? optional<size_t>{} : optional<size_t>{ 0 };
    _selectedTypeData = false;
    _selectedModuleIndex.reset();
    _nextAuthoringId = nextAuthoringId;

    Sync_AllEmitterMirrors();
    Clear_SoloPreviewEmitter();
    Sanitize_PreviewTransformState();
    Mark_AllPreviewDirty();
    Clamp_Selection();
    Sync_SelectionContext();
    if (EDITOR != nullptr)
    {
        if (const Shared<CurveEditor_View> curveEditor = dynamic_pointer_cast<CurveEditor_View>(EDITOR->Get_Window(L"Curve Editor")))
            curveEditor->Clear_PinnedTracks();
    }
    MarkDirty();
    return true;
}

void Emitter_View::Execute_AuthoringEdit(const string& description, const function<void()>& edit)
{
    if (!edit)
        return;

    const AuthoringSnapshot beforeSnapshot = Capture_AuthoringSnapshot();
    edit();
    const AuthoringSnapshot afterSnapshot = Capture_AuthoringSnapshot();
    Execute_AuthoringSnapshotCommand(beforeSnapshot, afterSnapshot, description);
}

Shared<Emitter_View> Emitter_View::Create()
{
    return make_shared<Emitter_View>();
}
NS_END
