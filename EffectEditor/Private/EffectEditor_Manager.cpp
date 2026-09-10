#include "EffectEditor_Manager.h"

#include "Console_View.h"
#include "Content_Browser.h"
#include "CurveEditor_View.h"
#include "Detail_View.h"
#include "Editor_Window.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialInstance_View.h"
#include "EffectMaterial_View.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "Hierarchy_View.h"
#include "HistoryBudget_View.h"
#include "MeshDataPreview_View.h"
#include "Profile_View.h"
#include "Scene_View.h"

NS_BEGIN(EffectEditor)

namespace
{
    enum class ExitDirtyChoice
    {
        SaveAndExit,
        ExitWithoutSaving,
        Cancel,
    };

    ExitDirtyChoice Confirm_SaveDirtyAuthoringDocumentBeforeExit()
    {
        const HWND owner = EDITOR != nullptr ? EDITOR->Get_WindowHandle() : nullptr;
        const int result = MessageBoxW(
            owner,
            L"저장되지 않은 변경 사항이 있습니다.\n종료하기 전에 저장할까요?",
            L"Exit EffectEditor",
            MB_YESNOCANCEL | MB_ICONWARNING
        );

        if (IDYES == result)
            return ExitDirtyChoice::SaveAndExit;
        if (IDNO == result)
            return ExitDirtyChoice::ExitWithoutSaving;
        return ExitDirtyChoice::Cancel;
    }
}

EffectEditor_Manager::~EffectEditor_Manager()
{
    Free();
}

HRESULT EffectEditor_Manager::Initialize()
{
    Add_Window(Scene_View::Create());
    Add_Window(Emitter_View::Create());
    Add_Window(Hierarchy_View::Create());
    Add_Window(Content_Browser::Create());
    Add_Window(Detail_View::Create());
    Add_Window(CurveEditor_View::Create());
    const Shared<HistoryBudget_View> historyBudgetView = HistoryBudget_View::Create();
    if (historyBudgetView)
        historyBudgetView->Set_Open(false);
    Add_Window(historyBudgetView);
    const Shared<EffectMaterial_View> effectMaterialView = EffectMaterial_View::Create();
    if (effectMaterialView)
        effectMaterialView->Set_Open(false);
    Add_Window(effectMaterialView);

    const Shared<EffectMaterialInstance_View> effectMaterialInstanceView = EffectMaterialInstance_View::Create();
    if (effectMaterialInstanceView)
        effectMaterialInstanceView->Set_Open(false);
    Add_Window(effectMaterialInstanceView);

    const Shared<MeshDataPreview_View> meshDataPreviewView = MeshDataPreview_View::Create();
    if (meshDataPreviewView)
        meshDataPreviewView->Set_Open(false);
    Add_Window(meshDataPreviewView);

    Add_Window(Console_View::Create());
    Add_Window(Profile_View::Create());

    CHECK_FAILED(Initialize_Windows(), E_FAIL);

    return S_OK;
}

void EffectEditor_Manager::Update(float timeDelta)
{
    Handle_Shortcuts();

    for (const auto& window : _windows | views::values)
    {
        if (window && window->Is_Open())
            window->Update(timeDelta);
    }

    for (const auto& window : _windows | views::values)
    {
        if (window && window->Is_Open())
            window->Pre_Render();
    }
}

void EffectEditor_Manager::Render()
{
    Render_DockSpace();

    for (const auto& window : _windows | views::values)
    {
        if (window != nullptr)
            window->Render();
    }
}

void EffectEditor_Manager::Request_Exit()
{
    const Shared<Emitter_View> emitterView = Find_EffectAuthoringView();
    if (emitterView != nullptr && emitterView->Is_Dirty())
    {
        const ExitDirtyChoice choice = Confirm_SaveDirtyAuthoringDocumentBeforeExit();
        if (ExitDirtyChoice::Cancel == choice)
            return;

        if (ExitDirtyChoice::SaveAndExit == choice)
        {
            Save_EffectDocument();
            if (emitterView->Is_Dirty())
                return;
        }
    }

    PostQuitMessage(0);
}

void EffectEditor_Manager::Add_Window(const Shared<Editor_Window>& window)
{
    if (nullptr == window)
        return;

    _windows[window->Get_WindowName()] = window;
}

Shared<Editor_Window> EffectEditor_Manager::Find_Window(const wstring& windowName) const
{
    const auto iter = _windows.find(windowName);
    if (iter == _windows.end())
        return nullptr;

    return iter->second;
}

HRESULT EffectEditor_Manager::Initialize_Windows()
{
    for (const auto& window : _windows | views::values)
    {
        if (nullptr == window)
            continue;

        CHECK_FAILED(window->Initialize(), E_FAIL);
    }

    return S_OK;
}

void EffectEditor_Manager::Render_MainMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        const bool canSaveEffectDocument = CanSave_EffectDocument();
        if (ImGui::MenuItem("Save", "Ctrl+S", false, canSaveEffectDocument))
            Save_EffectDocument();

        if (ImGui::MenuItem("Save As", "Ctrl+Shift+S", false, canSaveEffectDocument))
            SaveAs_EffectDocument();

        const bool canOpenEffectDocument = CanOpen_EffectDocument();
        if (ImGui::MenuItem("Open", "Ctrl+O", false, canOpenEffectDocument))
            Open_EffectDocument();

        ImGui::Separator();

        if (ImGui::MenuItem("Exit"))
            Request_Exit();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        for (auto& [name, window] : _windows)
        {
            if (nullptr == window)
                continue;

            bool isWindowOpen = window->Is_Open();
            const string windowName = String::ToString(name);
            if (ImGui::MenuItem(windowName.c_str(), nullptr, &isWindowOpen))
                window->Set_Open(isWindowOpen);
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void EffectEditor_Manager::Render_RuntimeToolbar()
{
    if (nullptr == EDITOR)
        return;

    const bool playing = EDITOR->IsPlaying();
    const bool paused = EDITOR->IsPaused();
    bool hasPreviewDirtyEmitter = false;
    string currentDocumentFileName{};
    if (const Shared<Editor_Window> emitterWindow = Find_Window(L"Emitter"))
    {
        const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
        if (emitterView != nullptr)
        {
            hasPreviewDirtyEmitter = emitterView->Has_PreviewDirtyEmitter();
            if (const optional<fs::path>& currentDocumentPath = emitterView->Get_CurrentDocumentPath();
                currentDocumentPath.has_value())
                currentDocumentFileName = String::ToString(currentDocumentPath->filename().wstring());
        }
    }

    constexpr float toolbarHeight = 34.f;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
    ImGui::SetCursorPosX(12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 4.f));

    const bool canSaveEffectDocument = CanSave_EffectDocument();
    ImGui::BeginDisabled(!canSaveEffectDocument);
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save"))
        Save_EffectDocument();
    ImGui::EndDisabled();
    if (!canSaveEffectDocument && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("저장 가능한 effect 문서가 없습니다.");

    ImGui::SameLine();
    ImGui::BeginDisabled(!canSaveEffectDocument);
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save As"))
        SaveAs_EffectDocument();
    ImGui::EndDisabled();
    if (!canSaveEffectDocument && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("저장 가능한 effect 문서가 없습니다.");

    ImGui::SameLine();
    const bool canOpenEffectDocument = CanOpen_EffectDocument();
    ImGui::BeginDisabled(!canOpenEffectDocument);
    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open"))
        Open_EffectDocument();
    ImGui::EndDisabled();
    if (!canOpenEffectDocument && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("열 수 있는 effect 문서가 없습니다.");

    ImGui::SameLine();
    const bool canAddEffectEmitters = CanAdd_EffectEmitters();
    ImGui::BeginDisabled(!canAddEffectEmitters);
    if (ImGui::Button(ICON_FA_PLUS " Add"))
        Add_EffectEmitters();
    ImGui::EndDisabled();
    if (!canAddEffectEmitters && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("추가할 수 있는 effect 문서가 없습니다.");

    ImGui::SameLine(0.f, 12.f);
    ImGui::TextDisabled("|");
    ImGui::SameLine(0.f, 12.f);

    if (!currentDocumentFileName.empty())
    {
        ImGui::Text("Editing: %s", currentDocumentFileName.c_str());
        ImGui::SameLine(0.f, 12.f);
        ImGui::TextDisabled("|");
        ImGui::SameLine(0.f, 12.f);
    }

    if (hasPreviewDirtyEmitter)
    {
        const ImVec4 dirtyButtonColor{ 0.72f, 0.40f, 0.12f, 1.f };
        const ImVec4 dirtyButtonHoveredColor{ 0.80f, 0.46f, 0.14f, 1.f };
        ImGui::PushStyleColor(ImGuiCol_Button, dirtyButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, dirtyButtonHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, dirtyButtonHoveredColor);
    }

    if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Restart Preview"))
        EDITOR->Request_RestartPreview();

    if (hasPreviewDirtyEmitter)
    {
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("변경 사항이 아직 preview에 반영되지 않았습니다.");

        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine(0.f, 24.f);

    if (playing)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.42f, 0.72f, 1.f));

    if (ImGui::Button("Play"))
    {
        if (paused)
            EDITOR->Resume();
        else
            EDITOR->Play();
    }

    if (playing)
        ImGui::PopStyleColor();

    ImGui::SameLine();

    if (paused)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.46f, 0.10f, 1.f));

    if (ImGui::Button("Pause"))
        EDITOR->Pause();

    if (paused)
        ImGui::PopStyleColor();

    ImGui::SameLine();

    if (!playing && !paused)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.32f, 0.36f, 0.40f, 1.f));

    if (ImGui::Button("Stop"))
        EDITOR->Stop();

    if (!playing && !paused)
        ImGui::PopStyleColor();

    ImGui::SameLine(0.f, 10.f);

    float timeScale = EDITOR->Get_RuntimeTimeScale();
    ImGui::SetNextItemWidth(74.f);
    if (ImGui::DragFloat("##RuntimeTimeScale", &timeScale, 0.01f, 0.01f, 8.f, "%.2fx"))
        EDITOR->Set_RuntimeTimeScale(timeScale);

    ImGui::SameLine();
    if (ImGui::Button("1x"))
        EDITOR->Reset_RuntimeTimeScale();

    ImGui::SameLine();
    if (!paused)
        ImGui::BeginDisabled();

    if (ImGui::Button("Step"))
        EDITOR->Request_FrameStep();

    if (!paused)
        ImGui::EndDisabled();

    ImGui::SameLine();
    Render_ShortcutHelp();

    ImGui::SameLine();
    Render_ExitButton(toolbarHeight);

    ImGui::PopStyleVar();
}

void EffectEditor_Manager::Render_DockSpace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (nullptr == viewport)
        return;

    Render_MainMenuBar();

    const float menuBarHeight = ImGui::GetFrameHeight();
    constexpr float toolbarHeight = 34.f;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - menuBarHeight));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

    bool isOpen = true;
    ImGui::Begin("EffectEditorDockSpaceHost", &isOpen, windowFlags);
    ImGui::PopStyleVar(3);

    Render_RuntimeToolbar();

    ImGui::SetCursorPosY(toolbarHeight);
    ImVec2 dockSpaceSize = ImGui::GetContentRegionAvail();
    dockSpaceSize.x = max(1.f, dockSpaceSize.x);
    dockSpaceSize.y = max(1.f, dockSpaceSize.y);

    const ImGuiID dockSpaceId = ImGui::GetID("EffectEditorDockSpaceId");
    _currentDockSpaceId = dockSpaceId;
    if (_isSceneFocusModeActive)
        Build_SceneFocusDockLayout(dockSpaceId, dockSpaceSize);
    else
        Build_DefaultDockLayout(dockSpaceId, dockSpaceSize);
    ImGui::DockSpace(dockSpaceId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

void EffectEditor_Manager::Build_DefaultDockLayout(ImGuiID dockSpaceId, ImVec2 dockSpaceSize)
{
    if (_defaultDockLayoutBuilt)
        return;

    _defaultDockLayoutBuilt = true;

    if (nullptr != ImGui::DockBuilderGetNode(dockSpaceId))
        return;

    ImGui::DockBuilderRemoveNode(dockSpaceId);
    ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockSpaceId, dockSpaceSize);

    ImGuiID mainDockId = dockSpaceId;
    const ImGuiID materialDockId = ImGui::DockBuilderSplitNode(mainDockId, ImGuiDir_Right, 0.24f, nullptr, &mainDockId);
    const ImGuiID leftDockId = ImGui::DockBuilderSplitNode(mainDockId, ImGuiDir_Left, 0.46f, nullptr, &mainDockId);

    ImGuiID sceneDockId = leftDockId;
    const ImGuiID detailDockId = ImGui::DockBuilderSplitNode(sceneDockId, ImGuiDir_Down, 0.59f, nullptr, &sceneDockId);

    ImGuiID emitterDockId = mainDockId;
    const ImGuiID browserDockId = ImGui::DockBuilderSplitNode(emitterDockId, ImGuiDir_Down, 0.37f, nullptr, &emitterDockId);

    const auto dock_registered_window = [this](const wstring& windowName, ImGuiID targetDockId)
    {
        const Shared<Editor_Window> window = Find_Window(windowName);
        if (window == nullptr)
            return;

        ImGui::DockBuilderDockWindow(window->Get_ImGuiWindowName().c_str(), targetDockId);
    };

    dock_registered_window(L"Scene", sceneDockId);
    dock_registered_window(L"Detail", detailDockId);
    dock_registered_window(L"Hierarchy", detailDockId);
    dock_registered_window(L"Emitter", emitterDockId);
    dock_registered_window(L"Content Browser", browserDockId);
    dock_registered_window(L"Curve Editor", browserDockId);
    dock_registered_window(L"History Budget", browserDockId);
    dock_registered_window(L"Profile", browserDockId);
    dock_registered_window(L"Console", browserDockId);
    dock_registered_window(L"Effect Material", materialDockId);
    dock_registered_window(L"Effect Material Instance", materialDockId);
    dock_registered_window(L"MeshData Preview", materialDockId);

    ImGui::DockBuilderFinish(dockSpaceId);
}

void EffectEditor_Manager::Build_SceneFocusDockLayout(ImGuiID dockSpaceId, ImVec2 dockSpaceSize)
{
    if (_sceneFocusDockLayoutBuilt)
        return;

    _sceneFocusDockLayoutBuilt = true;
    const Shared<Editor_Window> sceneWindow = Find_Window(L"Scene");

    ImGui::DockBuilderRemoveNode(dockSpaceId);
    ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockSpaceId, dockSpaceSize);
    if (sceneWindow != nullptr)
        ImGui::DockBuilderDockWindow(sceneWindow->Get_ImGuiWindowName().c_str(), dockSpaceId);
    ImGui::DockBuilderFinish(dockSpaceId);
}

void EffectEditor_Manager::Handle_Shortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        EDITOR->Undo();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        EDITOR->Redo();

    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
        SaveAs_EffectDocument();
    else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
        Save_EffectDocument();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
        Open_EffectDocument();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        Toggle_SceneFocusMode();
        return;
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_R, false))
    {
        EDITOR->Request_RestartPreview();
        return;
    }

    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        if (EDITOR->IsPlaying())
            EDITOR->Pause();
        else if (EDITOR->IsPaused())
            EDITOR->Resume();
        else
            EDITOR->Play();

        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
    {
        if (EDITOR->IsPaused())
            EDITOR->Resume();
        else if (!EDITOR->IsPlaying())
            EDITOR->Play();

        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F8, false))
    {
        GAME->SwitchTo_NextCamera();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F9, false))
    {
        if (EDITOR->IsPlaying())
            EDITOR->Pause();

        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        if (EDITOR->IsPlaying() || EDITOR->IsPaused())
            EDITOR->Stop();

        return;
    }

    constexpr float timeScaleStep = 0.25f;
    if (io.KeyShift &&
        (ImGui::IsKeyPressed(ImGuiKey_Equal, false) ||
         ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, false)))
    {
        const float nextTimeScale = EDITOR->Get_RuntimeTimeScale() + timeScaleStep;
        EDITOR->Set_RuntimeTimeScale(nextTimeScale);
        return;
    }

    if (io.KeyShift &&
        (ImGui::IsKeyPressed(ImGuiKey_Minus, false) ||
         ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, false)))
    {
        const float nextTimeScale = EDITOR->Get_RuntimeTimeScale() - timeScaleStep;
        EDITOR->Set_RuntimeTimeScale(nextTimeScale);
    }
}

void EffectEditor_Manager::Render_ShortcutHelp()
{
    if (ImGui::SmallButton("Shortcuts"))
    {
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(520.f, 0.f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::BeginTooltip();

        if (ImGui::BeginTable("ShortcutHelpTable", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);

            auto draw_shortcut_row = [](const char* key, const char* description)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(key);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(": %s", description);
            };

            auto draw_section_row = [](const char* sectionTitle)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", sectionTitle);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled(":");
            };

            draw_section_row("Playback / Runtime");
            draw_shortcut_row("Space", "Play / Pause");
            draw_shortcut_row("F5", "Play / Resume");
            draw_shortcut_row("R", "Restart Preview");
            draw_shortcut_row("F9", "Pause");
            draw_shortcut_row("Esc", "Stop");
            draw_shortcut_row("Shift +", "Time Scale Up");
            draw_shortcut_row("Shift -", "Time Scale Down");

            draw_section_row("Windows / Global");
            draw_shortcut_row("Ctrl + S", "Save Effect");
            draw_shortcut_row("Ctrl + Shift + S", "Save Effect As");
            draw_shortcut_row("Ctrl + O", "Open Effect");
            draw_shortcut_row("F8", "Switch Camera");
            draw_shortcut_row("Alt + Enter", "Fullscreen Toggle");
            draw_shortcut_row("Ctrl + Space", "Scene Focus Mode");

            draw_section_row("Hierarchy");
            draw_shortcut_row("Ctrl + C / V", "Copy / Paste Selected Object");
            draw_shortcut_row("Delete", "Delete Selected Object");
            draw_shortcut_row("Hierarchy Double Click", "Focus selected object in Scene");

            draw_section_row("이미터 편집");
            draw_shortcut_row("Ctrl + C", "선택한 이미터 또는 모듈 복사");
            draw_shortcut_row("Ctrl + V", "선택한 이미터 위치에 클립보드 붙여넣기");
            draw_shortcut_row("Delete", "선택한 이미터, 타입 데이터 또는 모듈 삭제");

            draw_section_row("Camera");
            draw_shortcut_row("F", "Home Preview Camera");
            draw_shortcut_row("RMB + Mouse", "Free Look");
            draw_shortcut_row("RMB + W A S D", "Move Free Camera");
            draw_shortcut_row("RMB + Q / E", "Move Down / Up");
            draw_shortcut_row("RMB + Wheel", "Preview Dolly");
            draw_shortcut_row("Alt + LMB Drag", "Orbit");
            draw_shortcut_row("Alt + MMB Drag", "Pan");
            draw_shortcut_row("Alt + RMB Drag", "Dolly");
            draw_shortcut_row("Ctrl + Wheel", "Free Camera Speed");
            draw_shortcut_row("Shift + Space", "Toggle Gizmo Local / World");

            ImGui::EndTable();
        }

        ImGui::EndTooltip();
    }
}

Shared<Emitter_View> EffectEditor_Manager::Find_EffectAuthoringView() const
{
    const Shared<Editor_Window> window = Find_Window(L"Emitter");
    if (window == nullptr)
        return nullptr;

    return dynamic_pointer_cast<Emitter_View>(window);
}

bool EffectEditor_Manager::CanSave_EffectDocument() const
{
    const Shared<Emitter_View> emitterView = Find_EffectAuthoringView();
    return emitterView != nullptr && emitterView->CanSave();
}

bool EffectEditor_Manager::Save_EffectDocument()
{
    const Shared<Emitter_View> emitterView = Find_EffectAuthoringView();
    if (emitterView == nullptr || !emitterView->CanSave())
        return false;

    emitterView->Save();
    return true;
}

bool EffectEditor_Manager::SaveAs_EffectDocument()
{
    const Shared<Emitter_View> emitterView = Find_EffectAuthoringView();
    if (emitterView == nullptr || !emitterView->CanSave())
        return false;

    emitterView->SaveAs();
    return true;
}

bool EffectEditor_Manager::CanOpen_EffectDocument() const
{
    const Shared<Emitter_View> emitterView = Find_EffectAuthoringView();
    return emitterView != nullptr && emitterView->CanOpen();
}

bool EffectEditor_Manager::Open_EffectDocument()
{
    const Shared<Emitter_View> emitterView = Find_EffectAuthoringView();
    if (emitterView == nullptr || !emitterView->CanOpen())
        return false;

    emitterView->Open();
    return true;
}

bool EffectEditor_Manager::CanAdd_EffectEmitters() const
{
    const Shared<Emitter_View> emitterView = Find_EffectAuthoringView();
    return emitterView != nullptr && emitterView->CanAdd();
}

bool EffectEditor_Manager::Add_EffectEmitters()
{
    const Shared<Emitter_View> emitterView = Find_EffectAuthoringView();
    if (emitterView == nullptr || !emitterView->CanAdd())
        return false;

    emitterView->Add();
    return true;
}

void EffectEditor_Manager::Toggle_SceneFocusMode()
{
    if (_isSceneFocusModeActive)
    {
        Exit_SceneFocusMode();
        return;
    }

    Enter_SceneFocusMode();
}

void EffectEditor_Manager::Enter_SceneFocusMode()
{
    ImGuiIO& io = ImGui::GetIO();

    _sceneFocusWindowOpenSnapshot.clear();
    for (const auto& [windowName, window] : _windows)
    {
        if (window == nullptr)
            continue;

        _sceneFocusWindowOpenSnapshot[windowName] = window->Is_Open();
    }

    size_t dockSettingsSize = 0;
    const char* dockSettingsData = ImGui::SaveIniSettingsToMemory(&dockSettingsSize);
    if (dockSettingsData != nullptr && dockSettingsSize > 0)
        _sceneFocusDockSettingsSnapshot.assign(dockSettingsData, dockSettingsSize);
    else
        _sceneFocusDockSettingsSnapshot.clear();

    _sceneFocusHadIniFilenameSnapshot = io.IniFilename != nullptr;
    _sceneFocusIniFilenameSnapshot = _sceneFocusHadIniFilenameSnapshot ? io.IniFilename : "";
    io.IniFilename = nullptr;

    _sceneFocusPreviewViewportSnapshot.mode = EDITOR->Get_PreviewViewportMode();
    _sceneFocusPreviewViewportSnapshot.width = max(1u, GAME->Get_ViewportWidth());
    _sceneFocusPreviewViewportSnapshot.height = max(1u, GAME->Get_ViewportHeight());

    for (const auto& [windowName, window] : _windows)
    {
        if (window == nullptr)
            continue;

        window->Set_Open(windowName == L"Scene");
    }

    static constexpr uint32 kSceneFocusPreviewViewportWidth = 1600;
    static constexpr uint32 kSceneFocusPreviewViewportHeight = 900;

    EDITOR->Set_ManualPreviewViewportSize(
        kSceneFocusPreviewViewportWidth,
        kSceneFocusPreviewViewportHeight
    );
    EDITOR->Request_WindowFocus(EditorViewportTarget::Scene);
    _sceneFocusDockLayoutBuilt = false;
    _isSceneFocusModeActive = true;
}

void EffectEditor_Manager::Exit_SceneFocusMode()
{
    ImGuiIO& io = ImGui::GetIO();

    if (!_sceneFocusDockSettingsSnapshot.empty())
        ImGui::LoadIniSettingsFromMemory(_sceneFocusDockSettingsSnapshot.c_str(), _sceneFocusDockSettingsSnapshot.size());

    for (const auto& [windowName, wasOpen] : _sceneFocusWindowOpenSnapshot)
    {
        const Shared<Editor_Window> window = Find_Window(windowName);
        if (window == nullptr)
            continue;

        window->Set_Open(wasOpen);
    }

    if (_sceneFocusPreviewViewportSnapshot.mode == PreviewViewportMode::Auto)
    {
        EDITOR->Enable_AutoPreviewViewport();
        EDITOR->Request_PreviewViewportSize(
            _sceneFocusPreviewViewportSnapshot.width,
            _sceneFocusPreviewViewportSnapshot.height
        );
    }
    else
    {
        EDITOR->Set_ManualPreviewViewportSize(
            _sceneFocusPreviewViewportSnapshot.width,
            _sceneFocusPreviewViewportSnapshot.height
        );
    }

    _sceneFocusDockLayoutBuilt = false;
    _sceneFocusDockSettingsSnapshot.clear();
    _sceneFocusWindowOpenSnapshot.clear();
    io.IniFilename = _sceneFocusHadIniFilenameSnapshot ? _sceneFocusIniFilenameSnapshot.c_str() : nullptr;
    _sceneFocusHadIniFilenameSnapshot = false;
    _sceneFocusIniFilenameSnapshot.clear();
    _isSceneFocusModeActive = false;
}

void EffectEditor_Manager::Toggle_Window(const wstring& windowName)
{
    const Shared<Editor_Window> window = Find_Window(windowName);
    if (nullptr == window)
        return;

    window->Set_Open(!window->Is_Open());
}

void EffectEditor_Manager::Toggle_WindowPair(const wstring& firstWindowName, const wstring& secondWindowName)
{
    const Shared<Editor_Window> firstWindow = Find_Window(firstWindowName);
    const Shared<Editor_Window> secondWindow = Find_Window(secondWindowName);

    if (nullptr == firstWindow || nullptr == secondWindow)
        return;

    const bool shouldOpen = !(firstWindow->Is_Open() || secondWindow->Is_Open());

    firstWindow->Set_Open(shouldOpen);
    secondWindow->Set_Open(shouldOpen);
}

void EffectEditor_Manager::Render_ExitButton(float toolbarHeight)
{
    const float buttonWidth = 60.f;
    const float rightPadding = 12.f;

    const float cursorX = ImGui::GetWindowWidth() - buttonWidth - rightPadding;
    if (cursorX > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(cursorX);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.20f, 0.20f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.24f, 0.24f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.14f, 0.14f, 1.f));

    if (ImGui::Button("Exit", ImVec2(buttonWidth, 0.f)))
        Request_Exit();

    ImGui::PopStyleColor(3);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 lineStart = ImGui::GetCursorScreenPos();
    drawList->AddLine(
        ImVec2(lineStart.x, lineStart.y + 4.f),
        ImVec2(lineStart.x + ImGui::GetWindowWidth(), lineStart.y + 4.f),
        IM_COL32(55, 55, 55, 255)
    );

    ImGui::SetCursorPosY(toolbarHeight);
}

Unique<EffectEditor_Manager> EffectEditor_Manager::Create()
{
    auto instance = make_unique<EffectEditor_Manager>();

    if (FAILED(instance->Initialize()))
    {
        LOG_CRITICAL("Failed to Create : Editor_Manager");
        MSG_BOX("Failed to Create : Editor_Manager");
        return nullptr;
    }

    return instance;
}

void EffectEditor_Manager::Free()
{
}

NS_END
