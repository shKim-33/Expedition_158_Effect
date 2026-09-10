#pragma once

namespace EffectEditor
{
struct EffectEditorDesc
{
    HWND hWnd{};
    WinMode winMode{ WinMode::Win };
    uint32 viewportWidth{};
    uint32 viewportHeight{};
};

struct EditorSelection
{
    Shared<GameObject> selectedObject{};
};

struct EditorFocusRequest
{
    Shared<GameObject> targetObject{};
};

struct NotificationEntry
{
    string message{};
    NotifyType type{ NotifyType::Info };

    float duration{ 3.f };
    float timer{ 0.f };
    float alpha{ 1.f };
};
}
