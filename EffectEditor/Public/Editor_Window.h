#pragma once

#include "Base.h"

NS_BEGIN(EffectEditor)

class Editor_Window abstract : public Base
{
public:
    explicit Editor_Window(const wstring& windowName, const char* windowIcon = nullptr);
    ~Editor_Window() override = default;

public:
    virtual HRESULT Initialize();
    virtual void Update(float timeDelta);
    virtual void Pre_Render();
    virtual void Render();

    virtual bool CanSave() const;
    virtual void Save();

public: //## Accessors
    const wstring& Get_WindowName() const { return _window; }
    const string& Get_ImGuiWindowName() const { return _imguiWindowName; }

    bool Is_Open() const { return _isOpen; }
    void Set_Open(bool isOpen) { _isOpen = isOpen; }

    bool Is_Focused() const { return _isFocused; }
    bool Is_Dirty() const { return _isDirty; }
    bool Is_Hovered() const { return _isHovered; }

    void MarkDirty() { _isDirty = true; }
    void ClearDirty() { _isDirty = false; }

public:
    void Request_FocusOnOpen();

protected: //## Data::WindowState
    wstring _window{};
    string _imguiWindowName{};

    bool _isOpen{ true };
    bool _isFocused{ false };
    bool _isDirty{ false };
    bool _isHovered{ false };
    bool _requestFocusOnOpen{ false };

protected:
    void Apply_PendingFocusBeforeBegin();
    void Clear_PendingFocusAfterBegin();
};

NS_END
