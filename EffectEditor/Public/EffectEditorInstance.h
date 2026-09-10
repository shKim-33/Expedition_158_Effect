#pragma once

#include "Base.h"
#include "EffectAuthoring_Types.h"
#include "Engine_Enum.h"
#include "Shader_CBuffer_Define.h"

namespace DirectX::DX11
{
class BasicEffect;
template <typename TVertex>
class PrimitiveBatch;
struct VertexPositionColor;
}

NS_BEGIN(Engine)
class RenderTarget;
class ICommand;
class GameObject;
NS_END

NS_BEGIN(EffectEditor)

class Editor_Window;
class EffectEditor_Manager;
class EffectEditorCamera;
class ImGui_Manager;
class Editor_Context;
class CommandHistory;
class Notification_Manager;

struct EffectEditorRenderPerfSample
{
    double editorWindowsMs{};
    double notificationMs{};
    double imguiMs{};
};

struct EffectEditorPostProcessOptions
{
    PostProcessFeature features{ PostProcessFeature::None };
    PostProcessCB params{};
};

struct PreviewEmitterTransformOverride
{
    bool enabled{ false };
    Vec3 localPositionOffset{ 0.f, 0.f, 0.f };
    Vec3 localRotationOffsetDegrees{ 0.f, 0.f, 0.f };
};

enum class EffectEditorPostProcessPreset
{
    PreviewFriendly,
    ClientCurrent,
    Custom,
};

enum class TrailPreviewCharacterSlot
{
    Player,
    Monster,
};

enum class TrailPreviewMotion
{
    Auto,
    Manual,
};

class EffectEditorInstance final : public Base
{
    DECLARE_SINGLETON(EffectEditorInstance)

public:
    EffectEditorInstance();
    ~EffectEditorInstance() override;

public:
    HRESULT Initialize_Editor(const EffectEditorDesc& editorDesc, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Release_Editor();

    void Update_Editor(float timeDelta);
    void Render_Editor(EffectEditorRenderPerfSample* outPerfSample = nullptr);
    void Request_Exit();

public: //## Accessors::Graphics
    ComPtr<Device> Get_Device() const { return _device; }
    ComPtr<Context> Get_Context() const { return _context; }

public: //## Accessors::Window
    Shared<Editor_Window> Get_Window(const wstring& windowName) const;
    HWND Get_WindowHandle() const;
    Editor_Context* Get_EditorContext() const { return _editorContext.get(); }
    bool Is_SceneFocusModeActive() const;
    uint32 Get_ActiveDockSpaceId() const;

public: //## Accessors::Notification
    Notification_Manager* Get_Notification() { return _notificationManager.get(); }

public: //## Accessors::Camera
    Shared<EffectEditorCamera> Get_PreviewCamera() const { return _previewCamera; }

public: //## Accessors::Runtime
    bool IsPlaying() const;
    bool IsPaused() const;
    float Get_RuntimeTimeScale() const;

public: //## Accessors::PreviewOption
    bool Is_AutoReplayPreviewEnabled() const { return _autoReplayPreviewEnabled; }
    bool Is_DefaultSkyboxEnabled() const { return _isDefaultSkyboxEnabled; }
    bool Is_TrailPreviewVisible() const { return _isTrailPreviewVisible; }
    bool Is_PreviewPlaneVisible() const { return _isPreviewPlaneVisible; }
    TrailPreviewGateMode Get_TrailPreviewGateMode() const { return _trailPreviewGateMode; }
    float Get_TrailPreviewGateStartRatio() const { return _trailPreviewGateStartRatio; }
    float Get_TrailPreviewGateEndRatio() const { return _trailPreviewGateEndRatio; }
    TrailPreviewGateMode Get_SourceHistorySpriteTrailPreviewGateMode() const { return _sourceHistorySpriteTrailPreviewGateMode; }
    float Get_SourceHistorySpriteTrailPreviewGateStartRatio() const { return _sourceHistorySpriteTrailPreviewGateStartRatio; }
    float Get_SourceHistorySpriteTrailPreviewGateEndRatio() const { return _sourceHistorySpriteTrailPreviewGateEndRatio; }
    bool Is_TrailPreviewGateOpen() const;
    bool Is_TrailPreviewGateOpen(float sourceRatio) const;
    bool Is_SourceHistorySpriteTrailPreviewGateOpen(float sourceRatio) const;
    bool Is_TrailPreviewDebugRenderEnabled() const { return _isTrailPreviewDebugRenderEnabled; }
    float Get_TrailPreviewAnimationSpeed() const { return _trailPreviewAnimationSpeed; }
    TrailPreviewCharacterSlot Get_TrailPreviewCharacterSlot() const { return _trailPreviewCharacterSlot; }
    TrailPreviewMotion Get_TrailPreviewMotion() const { return _trailPreviewMotion; }
    const string& Get_TrailPreviewManualAnimationName() const { return _trailPreviewManualAnimationName; }
    float Get_DefaultTrailPreviewAnimationSpeed() const { return kDefaultTrailPreviewAnimationSpeed; }
    bool Is_EffectInfluenceOutlineEnabled() const { return _isEffectInfluenceOutlineEnabled; }
    Vec3 Get_TrailPreviewBaseLocalOffset() const;
    Vec3 Get_TrailPreviewTipLocalOffset() const;
    Vec3 Get_TrailPreviewSourceLocalOffset() const;
    Vec3 Get_SourceHistorySpriteTrailPreviewSourceLocalOffset() const;
    Vec3 Get_DefaultTrailPreviewBaseLocalOffset() const;
    Vec3 Get_DefaultTrailPreviewTipLocalOffset() const;
    Vec3 Get_DefaultTrailPreviewSourceLocalOffset() const;
    Vec3 Get_DefaultSourceHistorySpriteTrailPreviewSourceLocalOffset() const;
    float Get_DefaultTrailPreviewGateStartRatio() const { return kDefaultTrailPreviewGateStartRatio; }
    float Get_DefaultTrailPreviewGateEndRatio() const { return kDefaultTrailPreviewGateEndRatio; }
    float Get_DefaultSourceHistorySpriteTrailPreviewGateStartRatio() const { return kDefaultSourceHistorySpriteTrailPreviewGateStartRatio; }
    float Get_DefaultSourceHistorySpriteTrailPreviewGateEndRatio() const { return kDefaultSourceHistorySpriteTrailPreviewGateEndRatio; }
    const EffectEditorPostProcessOptions& Get_PostProcessOptions() const { return _postProcessOptions; }
    EffectEditorPostProcessOptions Get_DefaultPostProcessOptions() const;
    bool Has_ClientPostProcessSnapshot() const { return _hasClientPostProcessSnapshot; }
    const EffectEditorPostProcessOptions& Get_ClientPostProcessSnapshot() const { return _clientPostProcessSnapshot; }
    EffectEditorPostProcessPreset Get_PostProcessPreset() const { return _postProcessPreset; }
    static bool Can_UsePreviewEmitterTransform(const AuthoringEmitter& emitter);
    const PreviewEmitterTransformOverride* Find_PreviewEmitterTransformOverride(uint32 emitterId) const;
    bool Is_PreviewEmitterTransformEnabled(uint32 emitterId) const;
    bool Try_GetPreviewEmitterWorldMatrix(uint32 emitterId, Matrix& outWorldMatrix) const;

public: //## Accessors::InputPolicy
    bool Is_PreviewCameraControlEnabled() const { return _isPreviewCameraControlEnabled; }
    bool Is_PreviewCameraDragging() const { return _isPreviewCameraDragging; }

public: //## Accessors::Viewport
    ShaderResourceView* Get_SharedViewportSRV() const;
    PreviewViewportMode Get_PreviewViewportMode() const { return _previewViewportMode; }
    bool Is_ManualPreviewViewportEnabled() const { return _previewViewportMode == PreviewViewportMode::Manual; }
    float Get_EffectiveViewportAspect() const;

public: //## Behavior::RuntimeControl
    void Play();
    void Pause();
    void Stop();
    void Resume();

    void Set_RuntimeTimeScale(float timeScale);
    void Reset_RuntimeTimeScale();

    void Request_FrameStep();
    bool Consume_FrameStep();
    void Request_RestartPreview();
    void Tick_AutoReplayPreview(float timeDelta);
    void Tick_TrailPreviewGate(float timeDelta);
    void Reset_TrailPreviewGate();

public: //## Behavior::PreviewOption
    void Set_AutoReplayPreviewEnabled(bool enabled);
    void Set_DefaultSkyboxEnabled(bool enabled);
    void Set_TrailPreviewVisible(bool visible);
    void Set_PreviewPlaneVisible(bool visible);
    void Set_TrailPreviewGateMode(TrailPreviewGateMode mode);
    void Set_TrailPreviewGateWindow(float startRatio, float endRatio);
    void Set_TrailPreviewGateDuration(float duration);
    void Set_SourceHistorySpriteTrailPreviewGateMode(TrailPreviewGateMode mode);
    void Set_SourceHistorySpriteTrailPreviewGateWindow(float startRatio, float endRatio);
    void Set_TrailPreviewDebugRenderEnabled(bool enabled);
    void Set_TrailPreviewAnimationSpeed(float speed);
    void Set_TrailPreviewCharacterSlot(TrailPreviewCharacterSlot slot);
    void Set_TrailPreviewMotion(TrailPreviewMotion motion);
    void Set_TrailPreviewManualAnimationName(string animationName);
    void Set_EffectInfluenceOutlineEnabled(bool enabled);
    void Set_TrailPreviewLocalOffsets(const Vec3& baseLocalOffset, const Vec3& tipLocalOffset);
    void Set_TrailPreviewLocalOffsets(const Vec3& baseLocalOffset, const Vec3& tipLocalOffset, const Vec3& sourceLocalOffset);
    void Set_SourceHistorySpriteTrailPreviewSourceLocalOffset(const Vec3& sourceLocalOffset);
    void Reset_TrailPreviewLocalOffsets();
    void Set_PreviewEmitterTransformEnabled(uint32 emitterId, bool enabled);
    void Set_PreviewEmitterTransformOffsets(uint32 emitterId, const Vec3& localPositionOffset, const Vec3& localRotationOffsetDegrees);
    void Reset_PreviewEmitterTransformOffsets();
    void Clear_PreviewEmitterTransform(uint32 emitterId);
    void Sanitize_PreviewEmitterTransforms(const vector<AuthoringEmitter>& emitters);
    void Apply_PreviewEmitterTransformOverrides();
    void Apply_PostProcessOptions(const EffectEditorPostProcessOptions& options);
    void Capture_ClientPostProcessSnapshot(const EffectEditorPostProcessOptions& options);
    void Set_PostProcessFeature(PostProcessFeature feature, bool enabled);
    void Set_PostProcessParams(const PostProcessCB& params);

public: //## Behavior::Window
    void Request_WindowFocus(EditorViewportTarget target);
    bool Consume_WindowFocusRequest(EditorViewportTarget target);

public: //## Behavior::Command
    void Execute_Command(const Shared<ICommand>& command);
    void Undo();
    void Redo();
    void Clear_CommandHistory();

public: //## Behavior::Selection
    void Copy_SelectedObject();
    Shared<GameObject> Paste_CopiedObject();
    Shared<GameObject> Duplicate_SelectedObject();
    bool Delete_SelectedObject();

public: //## Behavior::CameraControl
    void Activate_TargetCamera();
    void Activate_PreviewCamera();
    void Register_PreviewCamera(const Shared<EffectEditorCamera>& previewCamera);
    bool Is_PreviewCameraActive() const;

public: //## Behavior::InputPolicy
    void Set_SceneViewportInputState(bool canInteract, bool blocked);
    void Set_GameViewportInputState(bool canInteract);
    void Set_PreviewCameraLockScreenRect(const RECT& lockRectScreen);
    void Clear_PreviewCameraLockScreenRect();
    void Begin_PreviewCameraDrag();
    void End_PreviewCameraDrag();

    void Apply_RuntimeInputPolicy();

public: //## Behavior::Viewport
    Shared<RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    void Capture_BackBuffer_ToViewport();
    void Render_SceneGrid();

    void Request_PreviewViewportSize(uint32 width, uint32 height);

    void Enable_AutoPreviewViewport();
    void Enable_ManualPreviewViewport();
    void Set_ManualPreviewViewportSize(uint32 width, uint32 height);
    void Sync_PreviewViewportMode();

private: //## Static::PreviewDefaults
    static constexpr uint32 kDefaultPreviewViewportWidth{ 900 };
    static constexpr uint32 kDefaultPreviewViewportHeight{ 900 };
    static constexpr int kSceneGridDivisions{ 40 };
    static constexpr float kSceneGridHalfExtent{ 30.f };
    static constexpr float kSceneGridHeight{ 0.f };
    inline static const Vec3 kDefaultTrailPreviewBaseLocalOffset{ 0.f, 1.f, 35.f };
    inline static const Vec3 kDefaultTrailPreviewTipLocalOffset{ 0.f, 1.f, 85.f };
    inline static const Vec3 kDefaultTrailPreviewSourceLocalOffset{ 0.f, 1.f, 85.f };
    inline static const Vec3 kDefaultMonsterTrailPreviewLocalOffset{ Vec3::Zero };
    static constexpr float kDefaultTrailPreviewGateStartRatio{ 0.01f };
    static constexpr float kDefaultTrailPreviewGateEndRatio{ 0.99f };
    static constexpr float kDefaultSourceHistorySpriteTrailPreviewGateStartRatio{ 0.01f };
    static constexpr float kDefaultSourceHistorySpriteTrailPreviewGateEndRatio{ 0.99f };
    static constexpr float kDefaultTrailPreviewGateDuration{ 1.f };
    static constexpr float kDefaultTrailPreviewAnimationSpeed{ 1.f };
    static constexpr float kMinTrailPreviewAnimationSpeed{ 0.f };
    static constexpr float kMaxTrailPreviewAnimationSpeed{ 10.f };
    static constexpr float kAutoReplayPreviewDelay{ 1.f };

private: //## Data::Core
    ComPtr<Device> _device{};
    ComPtr<Context> _context{};
    EffectEditorDesc _editorDesc{};

    Unique<EffectEditor_Manager> _editorManager{};
    Unique<ImGui_Manager> _imguiManager{};
    Unique<Editor_Context> _editorContext{};
    Unique<CommandHistory> _commandHistory{};
    Unique<Notification_Manager> _notificationManager{};

private: //## Data::Viewport
    Shared<RenderTarget> _sharedViewportRenderTarget{ nullptr };
    Shared<PrimitiveBatch<VertexPositionColor>> _sceneGridBatch{ nullptr };
    Shared<BasicEffect> _sceneGridEffect{ nullptr };
    ComPtr<ID3D11InputLayout> _sceneGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _sceneGridDepthState{};

    PreviewViewportMode _previewViewportMode{ PreviewViewportMode::Auto };
    bool _hasPendingPreviewViewportSize{ false };
    uint32 _pendingPreviewViewportWidth{ kDefaultPreviewViewportWidth };
    uint32 _pendingPreviewViewportHeight{ kDefaultPreviewViewportHeight };
    uint32 _appliedPreviewViewportWidth{ 0 };
    uint32 _appliedPreviewViewportHeight{ 0 };

private: //## Data::Runtime
    EffectEditorRuntimeState _runtimeState{ EffectEditorRuntimeState::Edit };
    float _runtimeTimeScale{ 1.f };
    bool _pendingSingleFrameStep{ false };
    bool _autoReplayPreviewEnabled{ false };
    float _autoReplayPreviewElapsedTime{ 0.f };
    string _copiedObjectJson{};
    wstring _copiedLayerTag{};

private: //## Data::PreviewOption
    bool _isDefaultSkyboxEnabled{ true };
    bool _isTrailPreviewVisible{ false };
    bool _isPreviewPlaneVisible{ false };
    TrailPreviewGateMode _trailPreviewGateMode{ TrailPreviewGateMode::Window };
    float _trailPreviewGateStartRatio{ kDefaultTrailPreviewGateStartRatio };
    float _trailPreviewGateEndRatio{ kDefaultTrailPreviewGateEndRatio };
    float _trailPreviewGateElapsedTime{ 0.f };
    float _trailPreviewGateDuration{ kDefaultTrailPreviewGateDuration };
    TrailPreviewGateMode _sourceHistorySpriteTrailPreviewGateMode{ TrailPreviewGateMode::Window };
    float _sourceHistorySpriteTrailPreviewGateStartRatio{ kDefaultSourceHistorySpriteTrailPreviewGateStartRatio };
    float _sourceHistorySpriteTrailPreviewGateEndRatio{ kDefaultSourceHistorySpriteTrailPreviewGateEndRatio };
    bool _isTrailPreviewDebugRenderEnabled{ true };
    float _trailPreviewAnimationSpeed{ kDefaultTrailPreviewAnimationSpeed };
    TrailPreviewCharacterSlot _trailPreviewCharacterSlot{ TrailPreviewCharacterSlot::Player };
    TrailPreviewMotion _trailPreviewMotion{ TrailPreviewMotion::Auto };
    string _trailPreviewManualAnimationName{};
    bool _isEffectInfluenceOutlineEnabled{ false };
    Vec3 _playerTrailPreviewBaseLocalOffset{ kDefaultTrailPreviewBaseLocalOffset };
    Vec3 _playerTrailPreviewTipLocalOffset{ kDefaultTrailPreviewTipLocalOffset };
    Vec3 _playerSourceHistorySpriteTrailPreviewSourceLocalOffset{ kDefaultTrailPreviewSourceLocalOffset };
    Vec3 _monsterTrailPreviewBaseLocalOffset{ kDefaultMonsterTrailPreviewLocalOffset };
    Vec3 _monsterTrailPreviewTipLocalOffset{ kDefaultMonsterTrailPreviewLocalOffset };
    Vec3 _monsterSourceHistorySpriteTrailPreviewSourceLocalOffset{ kDefaultMonsterTrailPreviewLocalOffset };
    unordered_map<uint32, PreviewEmitterTransformOverride> _previewEmitterTransformOverrides{};
    EffectEditorPostProcessOptions _postProcessOptions{};
    bool _hasClientPostProcessSnapshot{ false };
    EffectEditorPostProcessOptions _clientPostProcessSnapshot{};
    EffectEditorPostProcessPreset _postProcessPreset{ EffectEditorPostProcessPreset::PreviewFriendly };

private: //## Data::Window
    bool _hasPendingWindowFocus{ false };
    EditorViewportTarget _pendingWindowFocus{ EditorViewportTarget::Game };

private: //## Data::CameraControl
    Shared<EffectEditorCamera> _previewCamera{ nullptr };

private: //## Data::InputPolicy
    bool _isSceneViewportInputEnabled{ false };
    bool _isSceneViewportInputBlocked{ false };
    bool _isGameViewportInputEnabled{ false };
    bool _isPreviewCameraControlEnabled{ false };
    bool _isPreviewCameraDragging{ false };
    bool _hasPreviewCameraLockScreenRect{ false };
    RECT _previewCameraLockScreenRect{};

private: //## Helper::PreviewViewport
    void Ensure_SceneGridResources();
    void Queue_PreviewViewportSize(uint32 width, uint32 height);
    void Apply_PendingPreviewViewportSize();

private: //## Helper::Selection
    bool Find_ObjectLayerTag(const Shared<GameObject>& gameObject, wstring& outLayerTag) const;
    Shared<GameObject> Instantiate_GameObjectFromCopiedData() const;

public:
    void Free() override;
};

NS_END
