#include "Scene_View.h"

#include "Camera.h"
#include "Editor_Context.h"
#include "Emitter_View.h"
#include "EffectEditorCamera.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr float kAutoPreviewViewportScale = 1.5f;

    bool Should_BeginPreviewCameraDrag()
    {
        const bool altHeld =
            GAME->KeyPress(KEY_TYPE::ALT) || GAME->KeyDown(KEY_TYPE::ALT);

        const bool orbitClicked = altHeld && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool panClicked = altHeld && ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
        const bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

        return orbitClicked || panClicked || rightClicked;
    }

    struct ViewportImageRect
    {
        ImVec2 imageMin;
        ImVec2 imageMax;
    };

    struct OverlayRect
    {
        ImVec2 min;
        ImVec2 max;
    };

    ViewportImageRect Compute_AspectFitRect(const ImVec2& canvasMin, const ImVec2& canvasSize, float aspect)
    {
        if (aspect <= 0.f || canvasSize.x <= 1.f || canvasSize.y <= 1.f)
        {
            return {
                canvasMin,
                ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y)
            };
        }

        float imageWidth = canvasSize.x;
        float imageHeight = imageWidth / aspect;

        if (imageHeight > canvasSize.y)
        {
            imageHeight = canvasSize.y;
            imageWidth = imageHeight * aspect;
        }

        const ImVec2 imageMin = ImVec2(
            canvasMin.x + (canvasSize.x - imageWidth) * 0.5f,
            canvasMin.y + (canvasSize.y - imageHeight) * 0.5f
        );

        return {
            imageMin,
            ImVec2(imageMin.x + imageWidth, imageMin.y + imageHeight)
        };
    }

    OverlayRect Compute_GizmoOverlayRect(const ImVec2& viewportMin, const ImVec2& viewportMax)
    {
        const ImVec2 panelSize = ImVec2(370.f, 34.f);
        const ImVec2 panelMin = ImVec2(viewportMax.x - panelSize.x - 16.f, viewportMin.y + 16.f);

        return {
            panelMin,
            ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y)
        };
    }

    OverlayRect Compute_ToolbarHandleRect(const ImVec2& viewportMin, const ImVec2& viewportMax)
    {
        const ImVec2 panelSize = ImVec2(34.f, 30.f);
        const ImVec2 panelMin = ImVec2(viewportMax.x - panelSize.x - 16.f, viewportMin.y + 16.f);

        return {
            panelMin,
            ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y)
        };
    }

    OverlayRect Compute_ToolbarPinRect(const ImVec2& viewportMin, const ImVec2& viewportMax)
    {
        const OverlayRect gizmoRect = Compute_GizmoOverlayRect(viewportMin, viewportMax);
        const ImVec2 panelSize = ImVec2(34.f, 30.f);
        const ImVec2 panelMin = ImVec2(gizmoRect.min.x - panelSize.x - 6.f, gizmoRect.min.y + 2.f);

        return {
            panelMin,
            ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y)
        };
    }

    OverlayRect Compute_PreviewViewportOverlayRect(const ImVec2& viewportMin, const ImVec2& viewportMax)
    {
        const ImVec2 panelSize = ImVec2(370.f, 34.f);
        const ImVec2 panelMin = ImVec2(viewportMax.x - panelSize.x - 16.f, viewportMin.y + 58.f);

        return {
            panelMin,
            ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y)
        };
    }

    OverlayRect Compute_DebugOptionOverlayRect(const ImVec2& viewportMin, const ImVec2& viewportMax)
    {
        const OverlayRect previewRect = Compute_PreviewViewportOverlayRect(viewportMin, viewportMax);
        const ImVec2 panelSize = ImVec2(370.f, 34.f);
        const ImVec2 panelMin = ImVec2(previewRect.max.x - panelSize.x, previewRect.max.y + 8.f);

        return {
            panelMin,
            ImVec2(panelMin.x + panelSize.x, panelMin.y + panelSize.y)
        };
    }

    bool Is_PointInRect(const ImVec2& point, const OverlayRect& rect)
    {
        return point.x >= rect.min.x && point.x <= rect.max.x &&
               point.y >= rect.min.y && point.y <= rect.max.y;
    }

    bool Should_OpenToolbarOverlay(const ImVec2& viewportMin, const ImVec2& viewportMax, const ImVec2& mousePos)
    {
        return Is_PointInRect(mousePos, Compute_ToolbarHandleRect(viewportMin, viewportMax));
    }

    bool Should_OpenToolbarOverlay(const ImVec2& viewportMin, const ImVec2& viewportMax)
    {
        return Should_OpenToolbarOverlay(viewportMin, viewportMax, ImGui::GetIO().MousePos);
    }

    OverlayRect Compute_ToolbarKeepAliveRect(const ImVec2& viewportMin, const ImVec2& viewportMax)
    {
        const OverlayRect pinRect = Compute_ToolbarPinRect(viewportMin, viewportMax);
        const OverlayRect gizmoRect = Compute_GizmoOverlayRect(viewportMin, viewportMax);
        const OverlayRect previewRect = Compute_PreviewViewportOverlayRect(viewportMin, viewportMax);
        const OverlayRect debugRect = Compute_DebugOptionOverlayRect(viewportMin, viewportMax);

        return {
            ImVec2(
                min(min(min(pinRect.min.x, gizmoRect.min.x), previewRect.min.x), debugRect.min.x),
                min(min(min(pinRect.min.y, gizmoRect.min.y), previewRect.min.y), debugRect.min.y)
            ),
            ImVec2(
                max(max(max(pinRect.max.x, gizmoRect.max.x), previewRect.max.x), debugRect.max.x),
                max(max(max(pinRect.max.y, gizmoRect.max.y), previewRect.max.y), debugRect.max.y)
            )
        };
    }

    bool Should_KeepToolbarOverlay(const ImVec2& viewportMin, const ImVec2& viewportMax, const ImVec2& mousePos)
    {
        return Is_PointInRect(mousePos, Compute_ToolbarKeepAliveRect(viewportMin, viewportMax));
    }

    bool Should_KeepToolbarOverlay(const ImVec2& viewportMin, const ImVec2& viewportMax)
    {
        return Should_KeepToolbarOverlay(viewportMin, viewportMax, ImGui::GetIO().MousePos);
    }

    bool Should_ShowToolbarOverlay(const ImVec2& viewportMin, const ImVec2& viewportMax, bool isPinned, bool isInteracting)
    {
        if (isPinned || isInteracting)
            return true;

        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        return Should_OpenToolbarOverlay(viewportMin, viewportMax, mousePos);
    }

    bool Is_MouseOverViewportOverlay(
        const ImVec2& viewportMin,
        const ImVec2& viewportMax,
        bool isToolbarPinned,
        bool isToolbarInteracting)
    {
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        if (Should_ShowToolbarOverlay(viewportMin, viewportMax, isToolbarPinned, isToolbarInteracting))
        {
            return Should_OpenToolbarOverlay(viewportMin, viewportMax, mousePos) ||
                   Should_KeepToolbarOverlay(viewportMin, viewportMax, mousePos);
        }

        return Should_OpenToolbarOverlay(viewportMin, viewportMax, mousePos);
    }

    const RequiredModuleData* Find_RequiredModuleData(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            return get_if<RequiredModuleData>(&module.data);
        }

        return nullptr;
    }

    Matrix Build_LocalTransformMatrix(const Vec3& position, const Vec3& rotationDegrees)
    {
        const Matrix rotation = Matrix::CreateFromYawPitchRoll(
            XMConvertToRadians(rotationDegrees.y),
            XMConvertToRadians(rotationDegrees.x),
            XMConvertToRadians(rotationDegrees.z)
        );
        return rotation * Matrix::CreateTranslation(position);
    }
}

Scene_View::Scene_View()
    : Editor_Window{ L"Scene", ICON_FA_WINDOW_MAXIMIZE }
{
}

void Scene_View::Update(float)
{
    Handle_GizmoShortcut();
}

void Scene_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();
    if (EDITOR->Is_SceneFocusModeActive())
    {
        const uint32 dockSpaceId = EDITOR->Get_ActiveDockSpaceId();
        if (dockSpaceId != 0)
            ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_Always);
    }

    if (EDITOR->Consume_WindowFocusRequest(EditorViewportTarget::Scene))
        ImGui::SetNextWindowFocus();

    if (ImGui::Begin(windowName.c_str(), &isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        Set_Open(isOpen);
        ImGuizmo::BeginFrame();
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

        Render_ViewportCanvas();
        Handle_FocusRequest();
        if (!Render_SelectedEmitterPreviewGizmo())
            Render_SelectedObjectGizmo();

        Render_CameraAxisOverlay();
        Render_ViewportToolbarOverlay();

        const bool overlayBlocked =
            Is_MouseOverViewportOverlay(
                _viewportMin,
                _viewportMax,
                _isViewportToolbarPinned,
                _isViewportToolbarInteracting
            ) ||
            ImGuizmo::IsOver() || ImGuizmo::IsUsing();

        if (!overlayBlocked &&
            _isViewportHovered &&
            Should_BeginPreviewCameraDrag())
            EDITOR->Begin_PreviewCameraDrag();

        EDITOR->Set_SceneViewportInputState(_isViewportHovered, overlayBlocked);
    }
    else
    {
        EDITOR->Set_SceneViewportInputState(false, false);
        Set_Open(isOpen);
    }

    ImGui::End();
}

void Scene_View::Handle_GizmoShortcut()
{
    if (!_isFocused)
        return;

    if (GAME->KeyPress(KEY_TYPE::RBUTTON))
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_W, false))
    {
        _gizmoOperation = ImGuizmo::TRANSLATE;
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_E, false))
    {
        _gizmoOperation = ImGuizmo::ROTATE;
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl && io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Space, false))
        _gizmoMode = _gizmoMode == ImGuizmo::LOCAL ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
}

void Scene_View::Render_CameraAxisOverlay()
{
    const float viewportWidth = _viewportMax.x - _viewportMin.x;
    const float viewportHeight = _viewportMax.y - _viewportMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera == nullptr)
        return;

    const Shared<TransformCom> cameraTransform = activeCamera->Get_Transform();
    if (cameraTransform == nullptr)
        return;

    Vec3 cameraRight = cameraTransform->Get_WorldRight();
    Vec3 cameraUp = cameraTransform->Get_WorldUp();
    if (cameraRight.LengthSquared() <= 0.0001f || cameraUp.LengthSquared() <= 0.0001f)
        return;

    cameraRight.Normalize();
    cameraUp.Normalize();

    const ImVec2 origin = ImVec2(_viewportMin.x + 34.f, _viewportMax.y - 36.f);
    const float axisLength = 28.f;

    struct AxisDrawDesc
    {
        Vec3 worldAxis;
        ImU32 color;
        const char* label;
    };

    const AxisDrawDesc axes[] = {
        { Vec3::Right, IM_COL32(235, 72, 72, 255), "X" },
        { Vec3::Up, IM_COL32(72, 220, 96, 255), "Y" },
        { Vec3::Look, IM_COL32(78, 132, 245, 255), "Z" }
    };

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCircleFilled(origin, 3.f, IM_COL32(220, 220, 220, 230));

    for (const AxisDrawDesc& axis : axes)
    {
        ImVec2 screenDir = ImVec2(
            axis.worldAxis.Dot(cameraRight),
            -axis.worldAxis.Dot(cameraUp)
        );

        const float lengthSq = screenDir.x * screenDir.x + screenDir.y * screenDir.y;
        if (lengthSq <= 0.0001f)
            continue;

        const float invLength = 1.f / sqrtf(lengthSq);
        screenDir.x *= invLength;
        screenDir.y *= invLength;

        const ImVec2 axisEnd = ImVec2(
            origin.x + screenDir.x * axisLength,
            origin.y + screenDir.y * axisLength
        );

        drawList->AddLine(origin, axisEnd, axis.color, 2.f);
        drawList->AddCircleFilled(axisEnd, 3.f, axis.color);
        drawList->AddText(ImVec2(axisEnd.x + 4.f, axisEnd.y - 6.f), axis.color, axis.label);
    }
}

void Scene_View::Render_ViewportToolbarOverlay()
{
    const float viewportWidth = _viewportMax.x - _viewportMin.x;
    const float viewportHeight = _viewportMax.y - _viewportMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    const bool showToolbar =
        Should_ShowToolbarOverlay(
            _viewportMin,
            _viewportMax,
            _isViewportToolbarPinned,
            _isViewportToolbarInteracting
        );
    _isViewportToolbarInteracting = false;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!showToolbar)
    {
        const OverlayRect handleRect = Compute_ToolbarHandleRect(_viewportMin, _viewportMax);

        drawList->AddRectFilled(handleRect.min, handleRect.max, IM_COL32(36, 38, 42, 220), 6.f);
        drawList->AddRect(handleRect.min, handleRect.max, IM_COL32(92, 96, 104, 255), 6.f);

        ImGui::SetCursorScreenPos(ImVec2(handleRect.min.x + 4.f, handleRect.min.y + 3.f));
        ImGui::PushID("ViewportToolbarHandle");
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 3.f));
        ImGui::Button(ICON_FA_SQUARE_CARET_LEFT, ImVec2(26.f, 24.f));
        _isViewportToolbarInteracting = ImGui::IsItemActive();
        ImGui::PopStyleVar();
        ImGui::PopID();
        return;
    }

    const OverlayRect pinRect = Compute_ToolbarPinRect(_viewportMin, _viewportMax);

    drawList->AddRectFilled(pinRect.min, pinRect.max, IM_COL32(36, 38, 42, 220), 6.f);
    drawList->AddRect(pinRect.min, pinRect.max, IM_COL32(92, 96, 104, 255), 6.f);

    ImGui::SetCursorScreenPos(ImVec2(pinRect.min.x + 4.f, pinRect.min.y + 3.f));
    ImGui::PushID("ViewportToolbarPin");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 3.f));

    const bool wasToolbarPinned = _isViewportToolbarPinned;
    if (wasToolbarPinned)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.79f, 0.56f, 0.18f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.64f, 0.23f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.09f, 0.09f, 0.09f, 1.f));
    }

    if (ImGui::Button(ICON_FA_THUMBTACK, ImVec2(26.f, 24.f)))
        _isViewportToolbarPinned = !_isViewportToolbarPinned;
    _isViewportToolbarInteracting = _isViewportToolbarInteracting || ImGui::IsItemActive();

    if (wasToolbarPinned)
        ImGui::PopStyleColor(3);

    ImGui::PopStyleVar();
    ImGui::PopID();

    Render_GizmoOverlay();
    _isViewportToolbarInteracting =
        _isViewportToolbarInteracting ||
        ImGui::IsAnyItemActive() ||
        Should_OpenToolbarOverlay(_viewportMin, _viewportMax) ||
        Should_KeepToolbarOverlay(_viewportMin, _viewportMax);
    Render_PreviewViewportOverlay();
    _isViewportToolbarInteracting =
        _isViewportToolbarInteracting ||
        ImGui::IsAnyItemActive() ||
        Should_OpenToolbarOverlay(_viewportMin, _viewportMax) ||
        Should_KeepToolbarOverlay(_viewportMin, _viewportMax);

    const OverlayRect debugRect = Compute_DebugOptionOverlayRect(_viewportMin, _viewportMax);
    drawList->AddRectFilled(debugRect.min, debugRect.max, IM_COL32(36, 38, 42, 220), 6.f);
    drawList->AddRect(debugRect.min, debugRect.max, IM_COL32(92, 96, 104, 255), 6.f);

    ImGui::SetCursorScreenPos(ImVec2(debugRect.min.x + 8.f, debugRect.min.y + 5.f));
    ImGui::PushID("DebugOptionOverlay");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 3.f));

    bool defaultSkyboxEnabled = EDITOR->Is_DefaultSkyboxEnabled();
    if (ImGui::Checkbox("Skybox", &defaultSkyboxEnabled))
        EDITOR->Set_DefaultSkyboxEnabled(defaultSkyboxEnabled);

    ImGui::SameLine(0.f, 10.f);
    bool previewPlaneVisible = EDITOR->Is_PreviewPlaneVisible();
    if (ImGui::Checkbox("Preview Plane", &previewPlaneVisible))
        EDITOR->Set_PreviewPlaneVisible(previewPlaneVisible);

    ImGui::SameLine(0.f, 10.f);
    bool trailPreviewVisible = EDITOR->Is_TrailPreviewVisible();
    if (ImGui::Checkbox("Trail Preview Obj", &trailPreviewVisible))
        EDITOR->Set_TrailPreviewVisible(trailPreviewVisible);

    ImGui::PopStyleVar();
    ImGui::PopID();

    _isViewportToolbarInteracting =
        _isViewportToolbarInteracting ||
        ImGui::IsAnyItemActive() ||
        Should_OpenToolbarOverlay(_viewportMin, _viewportMax) ||
        Should_KeepToolbarOverlay(_viewportMin, _viewportMax);
}

void Scene_View::Render_GizmoOverlay()
{
    const float viewportWidth = _viewportMax.x - _viewportMin.x;
    const float viewportHeight = _viewportMax.y - _viewportMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const OverlayRect panelRect = Compute_GizmoOverlayRect(_viewportMin, _viewportMax);

    drawList->AddRectFilled(panelRect.min, panelRect.max, IM_COL32(36, 38, 42, 220), 6.f);
    drawList->AddRect(panelRect.min, panelRect.max, IM_COL32(92, 96, 104, 255), 6.f);

    ImGui::SetCursorScreenPos(ImVec2(panelRect.min.x + 8.f, panelRect.min.y + 5.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 3.f));

    const ImVec4 activeButtonColor = ImVec4(0.79f, 0.56f, 0.18f, 1.f);
    const ImVec4 activeHoveredColor = ImVec4(0.88f, 0.64f, 0.23f, 1.f);
    const ImVec4 activeTextColor = ImVec4(0.09f, 0.09f, 0.09f, 1.f);
    const bool isTranslate = _gizmoOperation == ImGuizmo::TRANSLATE;
    const bool isRotate = _gizmoOperation == ImGuizmo::ROTATE;
    const bool isScale = _gizmoOperation == ImGuizmo::SCALE;
    const bool isLocal = _gizmoMode == ImGuizmo::LOCAL;
    const bool isWorld = _gizmoMode == ImGuizmo::WORLD;

    const Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    const Shared<EffectEditorCamera> effectCamera =
        dynamic_pointer_cast<EffectEditorCamera>(activeCamera);
    const bool isEffectCamera = effectCamera != nullptr;

    if (!isEffectCamera)
        ImGui::BeginDisabled();

    ImGui::TextUnformatted("Speed");
    ImGui::SameLine();

    if (isEffectCamera)
    {
        const Shared<TransformCom> transform = activeCamera->Get_Transform();
        float speed = transform ? transform->Get_SpeedPerSec() : 0.f;

        const ImGuiIO& io = ImGui::GetIO();
        if (transform && (_isFocused || _isHovered) && io.KeyCtrl && io.MouseWheel != 0.f)
        {
            speed += io.MouseWheel > 0.f ? 2.f : -2.f;
            speed = clamp(speed, 0.1f, 200.f);
            transform->Set_SpeedPerSec(speed);
        }

        ImGui::SetNextItemWidth(78.f);
        if (ImGui::DragFloat("##FreeCameraSpeed", &speed, 0.1f, 0.1f, 200.f, "%.2f"))
        {
            if (transform)
                transform->Set_SpeedPerSec(speed);
        }
    }
    else
    {
        ImGui::SetNextItemWidth(78.f);
        float dummySpeed = 0.f;
        ImGui::DragFloat("##FreeCameraSpeed", &dummySpeed, 0.f, 0.f, 0.f, "N/A");
    }

    if (!isEffectCamera)
        ImGui::EndDisabled();

    ImGui::SameLine(0.f, 10.f);

    if (isTranslate)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("W", ImVec2(24.f, 22.f)))
        _gizmoOperation = ImGuizmo::TRANSLATE;
    if (isTranslate)
        ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (isRotate)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("E", ImVec2(24.f, 22.f)))
        _gizmoOperation = ImGuizmo::ROTATE;
    if (isRotate)
        ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (isScale)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("R", ImVec2(24.f, 22.f)))
        _gizmoOperation = ImGuizmo::SCALE;
    if (isScale)
        ImGui::PopStyleColor(3);

    ImGui::SameLine(0.f, 8.f);
    if (isLocal)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("Local", ImVec2(56.f, 22.f)))
        _gizmoMode = ImGuizmo::LOCAL;
    if (isLocal)
        ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (isWorld)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
    }
    if (ImGui::Button("World", ImVec2(56.f, 22.f)))
        _gizmoMode = ImGuizmo::WORLD;
    if (isWorld)
        ImGui::PopStyleColor(3);

    ImGui::PopStyleVar();
}

void Scene_View::Render_PreviewViewportOverlay()
{
    const float viewportWidth = _viewportMax.x - _viewportMin.x;
    const float viewportHeight = _viewportMax.y - _viewportMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    const PreviewViewportMode previewMode = EDITOR->Get_PreviewViewportMode();
    const bool isAutoMode = previewMode == PreviewViewportMode::Auto;
    const bool isManualMode = previewMode == PreviewViewportMode::Manual;

    if (!isManualMode || _pendingPreviewViewportWidth <= 0 || _pendingPreviewViewportHeight <= 0)
        Sync_PreviewViewportInput();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const OverlayRect panelRect = Compute_PreviewViewportOverlayRect(_viewportMin, _viewportMax);

    drawList->AddRectFilled(panelRect.min, panelRect.max, IM_COL32(36, 38, 42, 220), 6.f);
    drawList->AddRect(panelRect.min, panelRect.max, IM_COL32(92, 96, 104, 255), 6.f);

    ImGui::SetCursorScreenPos(ImVec2(panelRect.min.x + 8.f, panelRect.min.y + 5.f));
    ImGui::PushID("PreviewViewportOverlay");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 3.f));

    const ImVec4 activeButtonColor = ImVec4(0.79f, 0.56f, 0.18f, 1.f);
    const ImVec4 activeHoveredColor = ImVec4(0.88f, 0.64f, 0.23f, 1.f);
    const ImVec4 activeTextColor = ImVec4(0.09f, 0.09f, 0.09f, 1.f);
    const auto draw_mode_button = [&](const char* label, const ImVec2& size, bool active)
    {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, activeHoveredColor);
            ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
        }

        const bool clicked = ImGui::Button(label, size);

        if (active)
            ImGui::PopStyleColor(3);

        return clicked;
    };

    if (draw_mode_button("Auto", ImVec2(42.f, 22.f), isAutoMode))
    {
        EDITOR->Enable_AutoPreviewViewport();
        EDITOR->Request_PreviewViewportSize(
            static_cast<uint32>(max(1.f, _viewportSize.x * kAutoPreviewViewportScale)),
            static_cast<uint32>(max(1.f, _viewportSize.y * kAutoPreviewViewportScale))
        );
        Sync_PreviewViewportInput();
    }

    ImGui::SameLine();
    if (draw_mode_button("Manual", ImVec2(62.f, 22.f), isManualMode))
    {
        EDITOR->Enable_ManualPreviewViewport();
        Sync_PreviewViewportInput();
        static constexpr uint32 defaultManualPreviewViewportWidth = 1600;
        static constexpr uint32 defaultManualPreviewViewportHeight = 900;

        EDITOR->Set_ManualPreviewViewportSize(defaultManualPreviewViewportWidth, defaultManualPreviewViewportHeight);
        _pendingPreviewViewportWidth = static_cast<int>(defaultManualPreviewViewportWidth);
        _pendingPreviewViewportHeight = static_cast<int>(defaultManualPreviewViewportHeight);
    }

    ImGui::SameLine(0.f, 8.f);
    if (!isManualMode)
        ImGui::BeginDisabled();

    ImGui::TextUnformatted("W");
    ImGui::SameLine(0.f, 4.f);
    ImGui::SetNextItemWidth(48.f);
    ImGui::InputInt("##PreviewViewportWidth", &_pendingPreviewViewportWidth, 0, 0);

    ImGui::SameLine(0.f, 4.f);
    ImGui::TextUnformatted("x");

    ImGui::SameLine(0.f, 4.f);
    ImGui::TextUnformatted("H");
    ImGui::SameLine(0.f, 4.f);
    ImGui::SetNextItemWidth(48.f);
    ImGui::InputInt("##PreviewViewportHeight", &_pendingPreviewViewportHeight, 0, 0);

    ImGui::SameLine(0.f, 8.f);
    if (ImGui::Button("Apply", ImVec2(48.f, 22.f)))
    {
        const uint32 width = static_cast<uint32>(max(1, _pendingPreviewViewportWidth));
        const uint32 height = static_cast<uint32>(max(1, _pendingPreviewViewportHeight));

        _pendingPreviewViewportWidth = static_cast<int>(width);
        _pendingPreviewViewportHeight = static_cast<int>(height);
        EDITOR->Set_ManualPreviewViewportSize(width, height);
    }

    if (!isManualMode)
        ImGui::EndDisabled();

    ImGui::PopStyleVar();
    ImGui::PopID();
}

void Scene_View::Sync_PreviewViewportInput()
{
    _pendingPreviewViewportWidth = static_cast<int>(GAME->Get_ViewportWidth());
    _pendingPreviewViewportHeight = static_cast<int>(GAME->Get_ViewportHeight());
}

void Scene_View::Render_ViewportCanvas()
{
    _viewportSize = ImGui::GetContentRegionAvail();

    if (_viewportSize.x < 1.f)
        _viewportSize.x = 1.f;
    if (_viewportSize.y < 1.f)
        _viewportSize.y = 1.f;

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasMax = ImVec2(canvasMin.x + _viewportSize.x, canvasMin.y + _viewportSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ShaderResourceView* viewportSrv = EDITOR->Get_SharedViewportSRV();
    if (nullptr != viewportSrv)
    {
        const float aspect = EDITOR->Get_EffectiveViewportAspect();
        const ViewportImageRect imageRect =
            Compute_AspectFitRect(canvasMin, _viewportSize, aspect);

        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 24, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(55, 60, 70, 255));

        const bool mouseOverOverlay = Is_MouseOverViewportOverlay(
            imageRect.imageMin,
            imageRect.imageMax,
            _isViewportToolbarPinned,
            _isViewportToolbarInteracting
        );

        _isViewportHovered =
            !mouseOverOverlay &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
            ImGui::IsMouseHoveringRect(imageRect.imageMin, imageRect.imageMax, false);

        const ImTextureID textureId =
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(viewportSrv));

        drawList->AddImage(textureId, imageRect.imageMin, imageRect.imageMax);
        _viewportMin = imageRect.imageMin;
        _viewportMax = imageRect.imageMax;
    }
    else
    {
        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 24, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(55, 60, 70, 255));

        const bool mouseOverOverlay = Is_MouseOverViewportOverlay(
            canvasMin,
            canvasMax,
            _isViewportToolbarPinned,
            _isViewportToolbarInteracting
        );

        _isViewportHovered =
            !mouseOverOverlay &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
            ImGui::IsMouseHoveringRect(canvasMin, canvasMax, false);

        drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f), IM_COL32(210, 216, 224, 255), "Scene RenderTarget Missing");
        _viewportMin = canvasMin;
        _viewportMax = canvasMax;
    }

    ImGui::SetCursorScreenPos(canvasMax);

    RECT viewportLockRect = {};
    viewportLockRect.left = static_cast<LONG>(_viewportMin.x);
    viewportLockRect.top = static_cast<LONG>(_viewportMin.y);
    viewportLockRect.right = static_cast<LONG>(_viewportMax.x);
    viewportLockRect.bottom = static_cast<LONG>(_viewportMax.y);
    EDITOR->Set_PreviewCameraLockScreenRect(viewportLockRect);

    if (EDITOR->Get_PreviewViewportMode() == PreviewViewportMode::Auto)
    {
        EDITOR->Request_PreviewViewportSize(
            static_cast<uint32>(max(1.f, _viewportSize.x * kAutoPreviewViewportScale)),
            static_cast<uint32>(max(1.f, _viewportSize.y * kAutoPreviewViewportScale))
        );
    }
}

void Scene_View::Handle_FocusRequest()
{
    Editor_Context* context = EDITOR->Get_EditorContext();
    if (context == nullptr)
        return;

    const Shared<GameObject> targetObject = context->Consume_FocusObjectRequest();
    if (targetObject == nullptr)
        return;

    const Shared<TransformCom> transform = targetObject->Get_Transform();
    if (transform == nullptr)
        return;

    EDITOR->Activate_PreviewCamera();

    const Shared<EffectEditorCamera> previewCamera =
        dynamic_pointer_cast<EffectEditorCamera>(GAME->Get_ActiveCamera());
    if (previewCamera == nullptr)
        return;

    previewCamera->Request_Focus(transform->Get_WorldPosition());
}

bool Scene_View::Render_SelectedEmitterPreviewGizmo()
{
    const Editor_Context* context = EDITOR->Get_EditorContext();
    if (context == nullptr)
        return false;

    const EffectAuthoringSelection& selection = context->Get_EffectSelection();
    if (selection.kind != EffectAuthoringSelectionKind::Emitter)
        return false;

    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(EDITOR->Get_Window(L"Emitter"));
    if (emitterView == nullptr)
        return false;

    const AuthoringEmitter* emitter = emitterView->Find_Emitter(selection.emitterId);
    if (emitter == nullptr ||
        !EffectEditorInstance::Can_UsePreviewEmitterTransform(*emitter) ||
        !EDITOR->Is_PreviewEmitterTransformEnabled(emitter->id))
        return false;

    const Matrix* viewMatrix = GAME->Get_Transform(D3DTS::View);
    const Matrix* projMatrix = GAME->Get_Transform(D3DTS::Proj);
    if (viewMatrix == nullptr || projMatrix == nullptr)
        return true;

    const float viewportWidth = _viewportMax.x - _viewportMin.x;
    const float viewportHeight = _viewportMax.y - _viewportMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return true;

    const RequiredModuleData* required = Find_RequiredModuleData(*emitter);
    const Vec3 authoredPosition = required != nullptr ? required->emitterOrigin : Vec3::Zero;
    const Vec3 authoredRotation = required != nullptr ? required->emitterRotationDegrees : Vec3::Zero;
    const PreviewEmitterTransformOverride* overrideState =
        EDITOR->Find_PreviewEmitterTransformOverride(emitter->id);
    const Vec3 positionOffset = overrideState != nullptr ? overrideState->localPositionOffset : Vec3::Zero;
    const Vec3 rotationOffset = overrideState != nullptr ? overrideState->localRotationOffsetDegrees : Vec3::Zero;

    Matrix worldMatrix = Matrix::Identity;
    if (!EDITOR->Try_GetPreviewEmitterWorldMatrix(emitter->id, worldMatrix))
        worldMatrix = Build_LocalTransformMatrix(authoredPosition + positionOffset, authoredRotation + rotationOffset);

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(_viewportMin.x, _viewportMin.y, viewportWidth, viewportHeight);

    const ImGuizmo::OPERATION operation =
        _gizmoOperation == ImGuizmo::SCALE ? ImGuizmo::TRANSLATE : _gizmoOperation;
    const bool manipulated = ImGuizmo::Manipulate(
        reinterpret_cast<float*>(const_cast<Matrix*>(viewMatrix)),
        reinterpret_cast<float*>(const_cast<Matrix*>(projMatrix)),
        operation,
        _gizmoMode,
        reinterpret_cast<float*>(&worldMatrix)
    );

    if (!manipulated)
        return true;

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;
    if (!worldMatrix.Decompose(scale, rotation, translation))
        return true;

    if (rotation.LengthSquared() <= 0.0001f)
        rotation = Quat::Identity;
    else
        rotation.Normalize();

    const Vec3 eulerRadians = rotation.ToEuler();
    const Vec3 rotationDegrees{
        XMConvertToDegrees(eulerRadians.x),
        XMConvertToDegrees(eulerRadians.y),
        XMConvertToDegrees(eulerRadians.z)
    };

    EDITOR->Set_PreviewEmitterTransformOffsets(
        emitter->id,
        translation - authoredPosition,
        rotationDegrees - authoredRotation
    );
    return true;
}

void Scene_View::Render_SelectedObjectGizmo()
{
    const Editor_Context* context = EDITOR->Get_EditorContext();
    if (context == nullptr)
        return;

    const Shared<GameObject> selectedObject = context->Get_Selection().selectedObject;
    if (selectedObject == nullptr)
        return;

    const Shared<TransformCom> transform = selectedObject->Get_Transform();
    if (transform == nullptr)
        return;

    const Matrix* viewMatrix = GAME->Get_Transform(D3DTS::View);
    const Matrix* projMatrix = GAME->Get_Transform(D3DTS::Proj);
    if (viewMatrix == nullptr || projMatrix == nullptr)
        return;

    const float viewportWidth = _viewportMax.x - _viewportMin.x;
    const float viewportHeight = _viewportMax.y - _viewportMin.y;
    if (viewportWidth <= 1.f || viewportHeight <= 1.f)
        return;

    Matrix worldMatrix = transform->Get_WorldMatrix();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(_viewportMin.x, _viewportMin.y, viewportWidth, viewportHeight);

    const bool manipulated = ImGuizmo::Manipulate(
        reinterpret_cast<float*>(const_cast<Matrix*>(viewMatrix)),
        reinterpret_cast<float*>(const_cast<Matrix*>(projMatrix)),
        _gizmoOperation,
        _gizmoMode,
        reinterpret_cast<float*>(&worldMatrix)
    );

    if (!manipulated)
        return;

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!worldMatrix.Decompose(scale, rotation, translation))
        return;

    transform->Set_WorldPosition(translation);
    transform->Set_RotationQuaternion(rotation);
    transform->Set_Scale(scale);
}

Shared<Scene_View> Scene_View::Create()
{
    return make_shared<Scene_View>();
}

NS_END
