#pragma once

#include "Base.h"

#include <format>
#include <string_view>

NS_BEGIN(EffectEditor)

class Notification_Manager final : public Base
{
public:
    Notification_Manager();
    ~Notification_Manager() override;

public:
    void Initialize();
    void Update(float timeDelta);
    void Render();

public:
    template <typename... Args>
    void Add_Notification(string_view format, Args&&... args)
    {
        Add_Notification_With_Type(NotifyType::Info, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Add_Notification_With_Type(NotifyType type, string_view format, Args&&... args)
    {
        string finalMessage{};
        try
        {
            if constexpr (sizeof...(args) > 0)
                finalMessage = std::vformat(format, std::make_format_args(args...));
            else
                finalMessage = string(format);
        }
        catch (...)
        {
            finalMessage = "Format Error: " + string(format);
        }

        Add_Internal(finalMessage, type);
    }

private: //## Data::Notifications
    list<NotificationEntry> _notifications{};

private: //## Helper::Notifications
    void Add_Internal(const string& message, NotifyType type = NotifyType::Info);

public:
    static Unique<Notification_Manager> Create();
};

NS_END
