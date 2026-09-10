#pragma once

#include "Base.h"
#include "EffectMaterialAuthoring_Types.h"
#include "EffectPreviewOrbitCamera.h"

NS_BEGIN(Engine)
class RenderTarget;
class ShaderCom;
class Texture;
class VIBuffer_Rect;
struct EffectMaterialScalarModulationRuntimeDesc;
struct EffectMaterialCoreColorRgbModulationRuntimeDesc;
struct EffectMaterialVec2ModulationRuntimeDesc;
NS_END

NS_BEGIN(EffectEditor)

class EffectPreviewPostProcessRenderer;

enum class EffectMaterialPreviewMeshMode : uint8
{
    Plane,
    Sphere,
};

// Distortion material preview가 renderer 전용 shape를 해석할 문맥이다.
enum class EffectMaterialPreviewDistortionContext : uint8
{
    Generic,
    Trail,
};

class EffectMaterialPreviewRenderer final : public Base
{
public:
    explicit EffectMaterialPreviewRenderer(uint32 previewTextureSize = 512);
    ~EffectMaterialPreviewRenderer() override;

public:
    void Render_Material(
        const EffectMaterialInstanceData& material,
        EffectMaterialPreviewDistortionContext distortionContext = EffectMaterialPreviewDistortionContext::Generic);
    void Render_Material(
        const EffectMaterialInstanceData& material,
        const EffectMaterialCoreColorRgbModulationRuntimeDesc& coreColorRgbModulation,
        const EffectMaterialVec2ModulationRuntimeDesc& vec2Modulation,
        const EffectMaterialScalarModulationRuntimeDesc& modulation,
        float previewDuration,
        bool evaluateParticleLife,
        EffectMaterialPreviewDistortionContext distortionContext = EffectMaterialPreviewDistortionContext::Generic);
    void Update_MaterialTime(float timeDelta);
    void Draw_Viewport(const char* id, const EffectMaterialInstanceData& material, const char* label, bool fillAvailableHeight = false);
    ShaderResourceView* Get_SRV() const;
    const string& Get_StatusMessage() const;

private: //## Types::PreviewRuntime
    struct PreviewVertex
    {
        Vec3 position{};
        Vec3 normal{};
        Vec2 texcoord{};
    };

    struct PreviewMeshResource
    {
        ComPtr<ID3D11Buffer> vertexBuffer{};
        ComPtr<ID3D11Buffer> indexBuffer{};
        uint32 indexCount{ 0 };
    };

private: //## Static::Render
    static constexpr auto kEffectMaterialPreviewShaderId{ L"Shader_EffectMaterialPreview" };
    static constexpr auto kEffectMaterialPreviewDistortionShaderId{ L"Shader_EffectMaterialPreviewDistortion" };
    static constexpr float kHomeDistance{ 6.f };
    static constexpr float kHomeYawDegrees{ 0.f };
    static constexpr float kHomePitchDegrees{ 0.f };
    static constexpr float kHomeTransitionSpeed{ 10.f };
    static constexpr float kPreviewOrbitSensor{ 0.105f };

private: //## Data::PreviewRuntime
    uint32 _previewTextureWidth{ 512 };
    uint32 _previewTextureHeight{ 512 };
    Unique<EffectPreviewPostProcessRenderer> _sceneResolve{};
    Shared<RenderTarget> _distortionSourceTarget{};
    Shared<ShaderCom> _shader{};
    Shared<ShaderCom> _distortionShader{};
    Shared<Texture> _texture{};
    Shared<Texture> _noiseTexture{};
    Shared<Texture> _maskTexture{};
    Shared<Texture> _flowTexture{};
    PreviewMeshResource _planeMesh{};
    PreviewMeshResource _sphereMesh{};
    string _loadedTextureKey{};
    string _loadedNoiseTextureKey{};
    string _loadedMaskTextureKey{};
    string _loadedFlowTextureKey{};
    string _statusMessage{};
    bool _hasRenderedPreview{ false };
    EffectMaterialPreviewMeshMode _meshMode{ EffectMaterialPreviewMeshMode::Plane };
    EffectPreviewOrbitCameraState _cameraState{};
    float _materialPreviewElapsedTime{};
    bool _isViewportDragging{ false };
    bool _hasPendingHome{ false };

private: //## Helper::Resource
    bool Ready_DistortionSourceTarget();
    bool Ready_Shader();
    bool Ready_DistortionShader();
    bool Ready_Texture(const string& textureGuid, const string& texturePath);
    bool Ready_NoiseTexture(const string& textureGuid, const string& texturePath);
    bool Ready_MaskTexture(const string& textureGuid, const string& texturePath);
    bool Ready_FlowTexture(const string& textureGuid, const string& texturePath);
    bool Ready_Meshes();
    bool Ready_PlaneMesh();
    bool Ready_SphereMesh();
    bool Create_Mesh(const vector<PreviewVertex>& vertices, const vector<uint32>& indices, PreviewMeshResource& mesh);

private: //## Helper::Render
    HRESULT Render_PreviewMesh(const EffectMaterialInstanceData& material, float previewCycleTime) const;
    HRESULT Render_CheckerBackground(float previewCycleTime) const;
    HRESULT Render_DistortionPreviewMesh(
        const EffectMaterialInstanceData& material,
        float previewCycleTime,
        EffectMaterialPreviewDistortionContext distortionContext) const;
    HRESULT Render_Mesh(const PreviewMeshResource& mesh) const;
    void Resize_RenderTarget(uint32 width, uint32 height);

private: //## Helper::ViewportInput
    void Handle_ViewportInput(bool isHovered, const ImVec2& canvasMin, const ImVec2& canvasMax);
    void Begin_ViewportDrag(const ImVec2& canvasMin, const ImVec2& canvasMax);
    void End_ViewportDrag();

private: //## Helper::Camera
    void Start_HomeTransition();
    void Update_HomeTransition(float timeDelta);
    void Reset_View();

private: //## Helper::Overlay
    void Draw_ViewportTextOverlay(const ImVec2& canvasMin, const ImVec2& canvasMax, const char* label);
    bool Draw_MeshModeControls(const ImVec2& canvasMin, const ImVec2& canvasMax);

private: //## Helper::Lifecycle
    void Cleanup();

public:
    static Unique<EffectMaterialPreviewRenderer> Create(uint32 previewTextureSize = 512);
    void Free() override;
};

NS_END
