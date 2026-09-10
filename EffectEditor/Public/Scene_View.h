#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Scene_View final : public Editor_Window
{
public:
    Scene_View();
    ~Scene_View() override = default;

public:
    void Update(float timeDelta) override;
    void Render() override;

private: //## Data::Viewport
    ImVec2 _viewportSize{};
    ImVec2 _viewportMin{};
    ImVec2 _viewportMax{};

private: //## Data::Gizmo
    ImGuizmo::OPERATION _gizmoOperation{ ImGuizmo::TRANSLATE };
    ImGuizmo::MODE _gizmoMode{ ImGuizmo::LOCAL };

private: //## Data::Toolbar
    int _pendingPreviewViewportWidth{ 0 };
    int _pendingPreviewViewportHeight{ 0 };
    bool _isViewportHovered{ false };
    bool _isViewportToolbarPinned{ false };
    bool _isViewportToolbarInteracting{ false };

private: //## Helper::Overlay
    void Handle_GizmoShortcut();
    void Render_CameraAxisOverlay();
    void Render_ViewportToolbarOverlay();
    void Render_GizmoOverlay();
    void Render_PreviewViewportOverlay();

private: //## Helper::Viewport
    void Sync_PreviewViewportInput();
    void Render_ViewportCanvas();
    void Handle_FocusRequest();
    bool Render_SelectedEmitterPreviewGizmo();
    void Render_SelectedObjectGizmo();

public:
    static Shared<Scene_View> Create();
};

NS_END
