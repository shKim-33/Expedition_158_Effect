#pragma once

#include "Editor_Window.h"
#include "Log_Manager.h"

NS_BEGIN(EffectEditor)

class Console_View final : public Editor_Window
{
public:
    Console_View();
    ~Console_View() override = default;

public:
    HRESULT Initialize() override;
    void Render() override;

private: //## Data::LogCache
    vector<ConsoleLogEntry> _cachedLogs{};
    uint32 _cachedRevision{ 0 };
    bool _autoScroll{ true };

    bool _showTrace{ true };
    bool _showDebug{ true };
    bool _showInfo{ true };
    bool _showWarn{ true };
    bool _showError{ true };
    bool _showCritical{ true };

private: //## Helper::LogView
    void Refresh_Logs();
    bool Pass_Filter(const ConsoleLogEntry& entry) const;
    ImVec4 Get_LogColor(const ConsoleLogEntry& entry) const;

public:
    static Shared<Console_View> Create();
};

NS_END
