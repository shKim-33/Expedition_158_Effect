#pragma once

#include "Base.h"
#include "EffectEditor_Enum.h"

NS_BEGIN(EffectEditor)

class Editor_Window;
class Emitter_View;

class EffectEditor_Manager final : public Base
{
public:
    EffectEditor_Manager() = default;
    ~EffectEditor_Manager() override;

public:
    HRESULT Initialize();
    void Update(float timeDelta);
    void Render();
    void Request_Exit();

    void Add_Window(const Shared<Editor_Window>& window);
    Shared<Editor_Window> Find_Window(const wstring& windowName) const;
    bool Is_SceneFocusModeActive() const { return _isSceneFocusModeActive; }
    uint32 Get_CurrentDockSpaceId() const { return _currentDockSpaceId; }

private: //## Types::SceneFocus
    struct PreviewViewportSnapshot
    {
        PreviewViewportMode mode{ PreviewViewportMode::Auto };
        uint32 width{ 0 };
        uint32 height{ 0 };
    };

private: //## Data::Windows
    map<wstring, Shared<Editor_Window>> _windows{};
    bool _defaultDockLayoutBuilt{ false };
    bool _isSceneFocusModeActive{ false };
    bool _sceneFocusDockLayoutBuilt{ false };
    uint32 _currentDockSpaceId{ 0 };
    map<wstring, bool> _sceneFocusWindowOpenSnapshot{};
    PreviewViewportSnapshot _sceneFocusPreviewViewportSnapshot{};
    string _sceneFocusDockSettingsSnapshot{};
    bool _sceneFocusHadIniFilenameSnapshot{ false };
    string _sceneFocusIniFilenameSnapshot{};

private: //## Helper::WindowLifecycle
    HRESULT Initialize_Windows();

private: //## Helper::Layout
    void Render_MainMenuBar();
    void Render_RuntimeToolbar();
    void Render_DockSpace();
    void Build_DefaultDockLayout(ImGuiID dockSpaceId, ImVec2 dockSpaceSize);
    void Build_SceneFocusDockLayout(ImGuiID dockSpaceId, ImVec2 dockSpaceSize);
    void Handle_Shortcuts();
    void Render_ShortcutHelp();

private: //## Helper::EffectDocument
    Shared<Emitter_View> Find_EffectAuthoringView() const;
    bool CanSave_EffectDocument() const;
    bool Save_EffectDocument();
    bool SaveAs_EffectDocument();
    bool CanOpen_EffectDocument() const;
    bool Open_EffectDocument();
    bool CanAdd_EffectEmitters() const;
    bool Add_EffectEmitters();

private: //## Helper::SceneFocus
    void Toggle_SceneFocusMode();
    void Enter_SceneFocusMode();
    void Exit_SceneFocusMode();

    void Toggle_Window(const wstring& windowName);
    void Toggle_WindowPair(const wstring& firstWindowName, const wstring& secondWindowName);

private: //## Helper::Exit
    void Render_ExitButton(float toolbarHeight);

public:
    static Unique<EffectEditor_Manager> Create();
    void Free() override;
};

NS_END
