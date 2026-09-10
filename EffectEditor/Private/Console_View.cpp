#include "Console_View.h"

#include "GameInstance.h"

NS_BEGIN(EffectEditor)

namespace
{
    string Strip_LogPrefix(const string& line)
    {
        static constexpr const char* levels[] =
        {
            "[trace]",
            "[debug]",
            "[info]",
            "[warning]",
            "[error]",
            "[critical]",
        };

        for (const char* level : levels)
        {
            const size_t pos = line.find(level);
            if (pos == string::npos)
                continue;

            string trimmed = line.substr(pos);

            const size_t levelEnd = trimmed.find(']');
            if (levelEnd == string::npos)
                return trimmed;

            trimmed.erase(0, levelEnd + 1);

            while (!trimmed.empty() && trimmed.front() == ' ')
            {
                trimmed.erase(trimmed.begin());
            }

            const size_t nextBlockBegin = trimmed.find('[');
            if (nextBlockBegin == string::npos)
                return trimmed;

            const size_t nextBlockEnd = trimmed.find(']', nextBlockBegin + 1);
            if (nextBlockEnd == string::npos)
                return trimmed;

            const string blockContent =
                trimmed.substr(nextBlockBegin + 1, nextBlockEnd - nextBlockBegin - 1);

            const bool isNumericBlock =
                !blockContent.empty() &&
                ranges::all_of(
                    blockContent,
                    [](unsigned char ch)
                    {
                        return isdigit(ch) != 0;
                    }
                );

            if (!isNumericBlock)
                return trimmed;

            trimmed.erase(nextBlockBegin, nextBlockEnd - nextBlockBegin + 1);

            if (nextBlockBegin < trimmed.size() && trimmed[nextBlockBegin] == ' ')
                trimmed.erase(nextBlockBegin, 1);

            return trimmed;
        }

        return line;
    }
}

Console_View::Console_View()
    : Editor_Window{ L"Console", ICON_FA_TERMINAL }
{
}

HRESULT Console_View::Initialize()
{
    return S_OK;
}

void Console_View::Render()
{
    if (!Is_Open())
        return;

    Refresh_Logs();

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        if (ImGui::Button("Clear"))
        {
            GAME->Clear_ConsoleLogs();
            _cachedLogs.clear();
            _cachedRevision = GAME->Get_ConsoleLogRevision();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto Scroll", &_autoScroll);

        ImGui::Separator();

        ImGui::Checkbox("Trace", &_showTrace);
        ImGui::SameLine();
        ImGui::Checkbox("Debug", &_showDebug);
        ImGui::SameLine();
        ImGui::Checkbox("Info", &_showInfo);
        ImGui::SameLine();
        ImGui::Checkbox("Warn", &_showWarn);
        ImGui::SameLine();
        ImGui::Checkbox("Error", &_showError);
        ImGui::SameLine();
        ImGui::Checkbox("Critical", &_showCritical);

        ImGui::Separator();

        if (ImGui::BeginChild("ConsoleScroll"))
        {
            ImGui::PushTextWrapPos(0.0f);

            for (const auto& log : _cachedLogs)
            {
                if (!Pass_Filter(log))
                    continue;

                ImGui::PushStyleColor(ImGuiCol_Text, Get_LogColor(log));
                const string displayLine = Strip_LogPrefix(log.formattedLine);
                ImGui::TextUnformatted(displayLine.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::PopTextWrapPos();

            if (_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 8.f)
                ImGui::SetScrollHereY(1.f);
        }
        ImGui::EndChild();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Console_View::Refresh_Logs()
{
    const uint32 revision = GAME->Get_ConsoleLogRevision();
    if (revision == _cachedRevision)
        return;

    _cachedLogs.clear();
    GAME->Visit_ConsoleLogs(
        [this](const ConsoleLogEntry& entry)
        {
            _cachedLogs.push_back(entry);
        }
    );

    _cachedRevision = revision;
}

bool Console_View::Pass_Filter(const ConsoleLogEntry& entry) const
{
    switch (entry.logLevel)
    {
    case LogLevel::Trace: return _showTrace;
    case LogLevel::Debug: return _showDebug;
    case LogLevel::Info: return _showInfo;
    case LogLevel::Warn: return _showWarn;
    case LogLevel::Error: return _showError;
    case LogLevel::Critical: return _showCritical;
    default: return true;
    }
}

ImVec4 Console_View::Get_LogColor(const ConsoleLogEntry& entry) const
{
    switch (entry.logLevel)
    {
    case LogLevel::Trace: return ImVec4(0.65f, 0.65f, 0.65f, 1.f);
    case LogLevel::Debug: return ImVec4(0.55f, 0.75f, 1.00f, 1.f);
    case LogLevel::Info: return ImVec4(0.90f, 0.90f, 0.90f, 1.f);
    case LogLevel::Warn: return ImVec4(1.00f, 0.82f, 0.25f, 1.f);
    case LogLevel::Error: return ImVec4(1.00f, 0.42f, 0.42f, 1.f);
    case LogLevel::Critical: return ImVec4(1.00f, 0.15f, 0.15f, 1.f);
    default: return ImVec4(1.f, 1.f, 1.f, 1.f);
    }
}

Shared<Console_View> Console_View::Create()
{
    return make_shared<Console_View>();
}

NS_END
