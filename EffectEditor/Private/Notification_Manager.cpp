#include "Notification_Manager.h"

NS_BEGIN(EffectEditor)

Notification_Manager::Notification_Manager()
{
}

Notification_Manager::~Notification_Manager()
{
}

void Notification_Manager::Initialize()
{
    _notifications.clear();
}

void Notification_Manager::Update(float timeDelta)
{
    if (_notifications.empty())
        return;

    for (auto iter = _notifications.begin(); iter != _notifications.end();)
    {
        iter->timer += timeDelta;

        if (iter->timer > iter->duration - 0.5f)
        {
            const float remaining = iter->duration - iter->timer;
            iter->alpha = clamp(remaining / 0.5f, 0.0f, 1.0f);
        }

        if (iter->timer >= iter->duration)
            iter = _notifications.erase(iter);
        else
            ++iter;
    }
}

void Notification_Manager::Render()
{
    if (_notifications.empty())
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport->WorkPos;
    const ImVec2 workSize = viewport->WorkSize;
    constexpr float pad = 10.0f;

    ImVec2 windowPos;
    windowPos.x = workPos.x + workSize.x - pad;
    windowPos.y = workPos.y + workSize.y - pad;

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs;

    int id = 0;
    for (auto& note : _notifications)
    {
        ImGui::SetNextWindowBgAlpha(0.8f * note.alpha);
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));

        const string windowName = "##Notify_" + to_string(id++);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, note.alpha);

        ImVec4 textColor = ImVec4(1.f, 1.f, 1.f, 1.f);
        switch (note.type)
        {
        case NotifyType::Success: textColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
            break;
        case NotifyType::Warning: textColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
            break;
        case NotifyType::Error: textColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
            break;
        default: break;
        }

        if (ImGui::Begin(windowName.c_str(), nullptr, flags))
        {
            ImGui::TextColored(textColor, note.message.c_str());

            const float height = ImGui::GetWindowHeight();
            windowPos.y -= height + pad;
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

void Notification_Manager::Add_Internal(const string& message, NotifyType type)
{
    NotificationEntry note;
    note.message = message;
    note.type = type;
    note.duration = 3.0f;
    note.timer = 0.0f;
    note.alpha = 1.0f;

    _notifications.push_back(note);

    LOG_INFO(message);
}

Unique<Notification_Manager> Notification_Manager::Create()
{
    auto instance = make_unique<Notification_Manager>();
    instance->Initialize();
    return instance;
}

NS_END
