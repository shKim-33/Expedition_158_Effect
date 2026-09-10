#include "EffectMaterialPreviewRenderer.h"

#include "EffectEditorInstance.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "EffectPreviewPostProcessRenderer.h"
#include "GameInstance.h"
#include "Helper_Math.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"

#include <cmath>

NS_BEGIN(EffectEditor)

namespace
{
    struct MaterialPreviewTime
    {
        float duration{ 1.f };
        float cycleTime{};
        float phase{};
    };

    MaterialPreviewTime Resolve_MaterialPreviewTime(float elapsedTime, float previewDuration)
    {
        constexpr float kMinDuration = 0.0001f;
        const float safeDuration = max(previewDuration, kMinDuration);
        const float positiveElapsedTime = max(elapsedTime, 0.f);
        const float cycleTime = std::fmod(positiveElapsedTime, safeDuration);
        const float phase = cycleTime / safeDuration;

        return MaterialPreviewTime{
            safeDuration,
            cycleTime,
            phase
        };
    }

    int Resolve_MaterialSourceIndex(const string& source)
    {
        constexpr int32 sourceAlpha = 0;
        constexpr int32 sourceRed = 1;
        constexpr int32 sourceLuminance = 2;

        if (source == "Alpha" || source == "alpha")
            return sourceAlpha;

        if (source == "Red" || source == "red")
            return sourceRed;

        if (source == "Luminance" || source == "luminance")
            return sourceLuminance;

        return sourceAlpha;
    }

    template <typename T>
    Shared<T> Clone_Component(uint32 levelIndex, const wstring& prototypeTag, void* arg = nullptr)
    {
        if (GAME == nullptr)
            return nullptr;

        return dynamic_pointer_cast<T>(
            GAME->Clone_Prototype(Prototype::Comopnent, levelIndex, prototypeTag, arg)
        );
    }

    wstring Resolve_PreviewTexturePathByGuid(const string& textureGuid)
    {
        if (GAME == nullptr || textureGuid.empty())
            return {};

        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (assetMeta == nullptr || assetMeta->type != "Texture")
            return {};

        const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
        return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
    }

    wstring Resolve_PreviewTexturePathByPath(const string& texturePath)
    {
        if (!texturePath.empty())
        {
            fs::path candidatePath = String::ToWString(texturePath);
            if (candidatePath.is_relative() && GAME != nullptr)
                candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

            candidatePath = candidatePath.lexically_normal();
            if (fs::exists(candidatePath))
                return candidatePath.wstring();
        }

        return {};
    }

    bool Is_SamePath(const wstring& lhs, const wstring& rhs)
    {
        if (lhs.empty() || rhs.empty())
            return false;

        return fs::path(lhs).lexically_normal() == fs::path(rhs).lexically_normal();
    }

    Shared<RenderTarget> Create_PreviewRenderTarget(
        const wchar_t* targetTag,
        uint32 width,
        uint32 height,
        DXGI_FORMAT format,
        const Vec4& clearColor,
        bool useDepthStencil)
    {
        return RenderTarget::Create(
            targetTag,
            EDITOR->Get_Device(),
            EDITOR->Get_Context(),
            width,
            height,
            format,
            clearColor,
            useDepthStencil
        );
    }

    wstring Resolve_PreviewTexturePath(const string& textureGuid, const string& texturePath, bool& outUsedPathFallback, bool& outGuidPathMismatch)
    {
        outUsedPathFallback = false;
        outGuidPathMismatch = false;

        wstring resolvedPath = Resolve_PreviewTexturePathByGuid(textureGuid);
        if (!resolvedPath.empty())
        {
            const wstring pathResolved = Resolve_PreviewTexturePathByPath(texturePath);
            outGuidPathMismatch = !pathResolved.empty() && !Is_SamePath(resolvedPath, pathResolved);
            return resolvedPath;
        }

        resolvedPath = Resolve_PreviewTexturePathByPath(texturePath);
        if (!resolvedPath.empty())
        {
            outUsedPathFallback = true;
            return resolvedPath;
        }

        return {};
    }
}

EffectMaterialPreviewRenderer::EffectMaterialPreviewRenderer(uint32 previewTextureSize)
    : _previewTextureWidth{ max(1u, previewTextureSize) }
    , _previewTextureHeight{ max(1u, previewTextureSize) }
    , _sceneResolve{
        make_unique<EffectPreviewPostProcessRenderer>(
            L"EffectMaterialPreviewDisplay",
            L"EffectMaterialPreviewHDR",
            L"EffectMaterialPreviewBloom",
            _previewTextureWidth,
            _previewTextureHeight) }
{
    Reset_View();
}

EffectMaterialPreviewRenderer::~EffectMaterialPreviewRenderer()
{
    Cleanup();
}

void EffectMaterialPreviewRenderer::Render_Material(
    const EffectMaterialInstanceData& material,
    EffectMaterialPreviewDistortionContext distortionContext)
{
    Render_Material(
        material,
        EffectMaterialCoreColorRgbModulationRuntimeDesc{},
        EffectMaterialVec2ModulationRuntimeDesc{},
        EffectMaterialScalarModulationRuntimeDesc{},
        1.f,
        false,
        distortionContext
    );
}

void EffectMaterialPreviewRenderer::Render_Material(
    const EffectMaterialInstanceData& material,
    const EffectMaterialCoreColorRgbModulationRuntimeDesc& coreColorRgbModulation,
    const EffectMaterialVec2ModulationRuntimeDesc& vec2Modulation,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float previewDuration,
    bool evaluateParticleLife,
    EffectMaterialPreviewDistortionContext distortionContext)
{
    _statusMessage.clear();
    _hasRenderedPreview = false;
    const MaterialPreviewTime previewTime =
        Resolve_MaterialPreviewTime(_materialPreviewElapsedTime, previewDuration);
    const MaterialScalarModulationPreviewResult modulationResult =
        Evaluate_MaterialScalarModulationPreview(
            material,
            coreColorRgbModulation,
            vec2Modulation,
            modulation,
            previewTime.phase,
            0u,
            evaluateParticleLife
        );
    const EffectMaterialInstanceData& previewMaterial = modulationResult.material;
    const bool isDistortionPreview = previewMaterial.materialFamily == EffectMaterialFamily::SpriteDistortion;

    if (EDITOR == nullptr || GAME == nullptr)
    {
        _statusMessage = "Preview renderer is not ready.";
        return;
    }

    if (_sceneResolve == nullptr ||
        !_sceneResolve->Ready(_statusMessage) ||
        (isDistortionPreview && !Ready_DistortionSourceTarget()) ||
        !Ready_Shader() ||
        (isDistortionPreview && !Ready_DistortionShader()) ||
        !Ready_Texture(previewMaterial.mainTextureGuid, previewMaterial.mainTexturePath) ||
        !Ready_NoiseTexture(previewMaterial.noiseTextureGuid, previewMaterial.noiseTexturePath) ||
        !Ready_MaskTexture(previewMaterial.maskTextureGuid, previewMaterial.maskTexturePath) ||
        (isDistortionPreview && !Ready_FlowTexture(previewMaterial.flowTextureGuid, previewMaterial.flowTexturePath)) ||
        !Ready_Meshes())
        return;

    ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    if (isDistortionPreview)
    {
        if (FAILED(_distortionSourceTarget->Begin(true, true)) ||
            FAILED(Render_CheckerBackground(previewTime.cycleTime)))
        {
            GAME->Bind_BackBuffer();
            _statusMessage = "Distortion preview source draw failed.";
            return;
        }
    }

    if (FAILED(_sceneResolve->Begin_HDRTarget()))
    {
        _statusMessage = "Preview HDR render target begin is failed.";
        return;
    }

    HRESULT hr = Render_CheckerBackground(previewTime.cycleTime);
    if (SUCCEEDED(hr))
    {
        hr = isDistortionPreview
             ? Render_DistortionPreviewMesh(previewMaterial, previewTime.cycleTime, distortionContext)
             : Render_PreviewMesh(previewMaterial, previewTime.cycleTime);
    }

    if (SUCCEEDED(hr))
        hr = _sceneResolve->Resolve();

    GAME->Bind_BackBuffer();

    if (FAILED(hr))
    {
        _statusMessage = "Preview draw failed.";
        return;
    }

    _hasRenderedPreview = true;
}

void EffectMaterialPreviewRenderer::Update_MaterialTime(float timeDelta)
{
    _materialPreviewElapsedTime += max(0.f, timeDelta);
}

void EffectMaterialPreviewRenderer::Draw_Viewport(
    const char* id,
    const EffectMaterialInstanceData&,
    const char* label,
    bool fillAvailableHeight)
{
    const ImVec2 availableSize = ImGui::GetContentRegionAvail();
    const float previewHeight = fillAvailableHeight
                                ? max(280.f, availableSize.y)
                                : max(280.f, min(560.f, availableSize.y > 0.f ? availableSize.y : 360.f));

    ImGui::BeginChild(id, ImVec2(0.f, previewHeight), true, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.x = max(1.f, canvasSize.x);
        canvasSize.y = max(1.f, canvasSize.y);
        Resize_RenderTarget(
            static_cast<uint32>(max(1.f, canvasSize.x)),
            static_cast<uint32>(max(1.f, canvasSize.y))
        );

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMax = ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(10, 12, 16, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(70, 78, 92, 255));

        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("EffectMaterialPreviewCanvas", canvasSize);
        const bool isHovered = ImGui::IsItemHovered();

        ShaderResourceView* previewSrv = Get_SRV();
        if (previewSrv != nullptr)
        {
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSrv));
            drawList->AddImage(textureId, canvasMin, canvasMax);
        }
        else
        {
            const char* message = _statusMessage.empty() ? "Preview render target is not ready." : _statusMessage.c_str();
            drawList->AddText(
                ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f),
                IM_COL32(214, 220, 230, 255),
                message
            );
        }

        Draw_ViewportTextOverlay(canvasMin, canvasMax, label);
        const bool meshModeHovered = Draw_MeshModeControls(canvasMin, canvasMax);
        Handle_ViewportInput(isHovered && !meshModeHovered, canvasMin, canvasMax);
    }
    ImGui::EndChild();
}

ShaderResourceView* EffectMaterialPreviewRenderer::Get_SRV() const
{
    return !_hasRenderedPreview || _sceneResolve == nullptr ? nullptr : _sceneResolve->Get_SRV();
}

const string& EffectMaterialPreviewRenderer::Get_StatusMessage() const
{
    return _statusMessage;
}

bool EffectMaterialPreviewRenderer::Ready_DistortionSourceTarget()
{
    if (_distortionSourceTarget != nullptr)
        return true;

    _distortionSourceTarget = Create_PreviewRenderTarget(
        L"EffectMaterialPreviewDistortionSource",
        _previewTextureWidth,
        _previewTextureHeight,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        Vec4{ 0.18f, 0.18f, 0.18f, 1.f },
        true
    );

    if (_distortionSourceTarget == nullptr)
    {
        _statusMessage = "Distortion preview source target creation failed.";
        return false;
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_Shader()
{
    if (_shader != nullptr)
        return true;

    _shader = Clone_Component<ShaderCom>(ETOI(LevelType::Static), kEffectMaterialPreviewShaderId);
    if (_shader == nullptr)
    {
        _statusMessage = "Shader_EffectMaterialPreview prototype is missing.";
        return false;
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_DistortionShader()
{
    if (_distortionShader != nullptr)
        return true;

    _distortionShader = Clone_Component<ShaderCom>(ETOI(LevelType::Static), kEffectMaterialPreviewDistortionShaderId);
    if (_distortionShader == nullptr)
    {
        _statusMessage = "Shader_EffectMaterialPreviewDistortion prototype is missing.";
        return false;
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_Texture(const string& textureGuid, const string& texturePath)
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Resolve_PreviewTexturePath(textureGuid, texturePath, usedPathFallback, guidPathMismatch);
    if (resolvedPath.empty())
    {
        _texture = nullptr;
        _loadedTextureKey.clear();
        _statusMessage = texturePath.empty() && textureGuid.empty()
                         ? "Main texture reference is empty."
                         : "Main texture path/GUID resolve failed.";
        LOG_ERROR(
            "EffectMaterialPreview main texture path/GUID resolve failed. mainTextureGuid='{}', mainTexturePath='{}'.",
            textureGuid,
            texturePath
        );
        return false;
    }

    const string requestedTextureKey = String::ToString(resolvedPath);
    if (_texture != nullptr && _loadedTextureKey == requestedTextureKey)
        return true;

    if (guidPathMismatch)
    {
        LOG_WARN(
            "EffectMaterialPreview main texture GUID/path mismatch. mainTextureGuid='{}', mainTexturePath='{}', guidResolvedPath='{}'.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "EffectMaterialPreview main texture fell back to path. mainTextureGuid='{}', mainTexturePath='{}', resolvedPath='{}'.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }

    _texture = Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), resolvedPath.c_str(), 1);
    _loadedTextureKey = requestedTextureKey;

    if (_texture != nullptr)
        return true;

    _loadedTextureKey.clear();
    _statusMessage = "Preview texture create failed.";
    LOG_ERROR(
        "EffectMaterialPreview main texture create failed. mainTextureGuid='{}', mainTexturePath='{}', resolvedPath='{}'.",
        textureGuid,
        texturePath,
        requestedTextureKey
    );
    return false;
}

bool EffectMaterialPreviewRenderer::Ready_NoiseTexture(const string& textureGuid, const string& texturePath)
{
    if (textureGuid.empty() && texturePath.empty())
    {
        _noiseTexture = nullptr;
        _loadedNoiseTextureKey.clear();
        return true;
    }

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Resolve_PreviewTexturePath(textureGuid, texturePath, usedPathFallback, guidPathMismatch);

    if (resolvedPath.empty())
    {
        _noiseTexture = nullptr;
        _loadedNoiseTextureKey.clear();
        _statusMessage = "Noise texture missing. Preview continues without noise.";
        LOG_WARN_THROTTLED(
            60,
            "EffectMaterialPreview noise texture missing. noiseTextureGuid='{}', noiseTexturePath='{}'. Preview continues without noise.",
            textureGuid,
            texturePath
        );
        return true;
    }

    const string requestedTextureKey = String::ToString(resolvedPath);
    if (_noiseTexture != nullptr && _loadedNoiseTextureKey == requestedTextureKey)
        return true;

    if (guidPathMismatch)
    {
        LOG_WARN(
            "EffectMaterialPreview noise texture GUID/path mismatch. noiseTextureGuid='{}', noiseTexturePath='{}', guidResolvedPath='{}'. Preview continues with GUID texture.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "EffectMaterialPreview noise texture fell back to path. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'. Preview continues with fallback texture.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }

    _noiseTexture = Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), resolvedPath.c_str(), 1);
    _loadedNoiseTextureKey = requestedTextureKey;

    if (_noiseTexture == nullptr)
    {
        _loadedNoiseTextureKey.clear();
        _statusMessage = "Noise texture missing. Preview continues without noise.";
        LOG_WARN_THROTTLED(
            60,
            "EffectMaterialPreview noise texture create failed. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'. Preview continues without noise.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_MaskTexture(const string& textureGuid, const string& texturePath)
{
    if (textureGuid.empty() && texturePath.empty())
    {
        _maskTexture = nullptr;
        _loadedMaskTextureKey.clear();
        return true;
    }

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Resolve_PreviewTexturePath(textureGuid, texturePath, usedPathFallback, guidPathMismatch);

    if (resolvedPath.empty())
    {
        _maskTexture = nullptr;
        _loadedMaskTextureKey.clear();
        _statusMessage = "Mask texture missing. Preview continues without mask.";
        LOG_WARN_THROTTLED(
            60,
            "EffectMaterialPreview mask texture missing. maskTextureGuid='{}', maskTexturePath='{}'. Preview continues without mask.",
            textureGuid,
            texturePath
        );
        return true;
    }

    const string requestedTextureKey = String::ToString(resolvedPath);
    if (_maskTexture != nullptr && _loadedMaskTextureKey == requestedTextureKey)
        return true;

    if (guidPathMismatch)
    {
        LOG_WARN(
            "EffectMaterialPreview mask texture GUID/path mismatch. maskTextureGuid='{}', maskTexturePath='{}', guidResolvedPath='{}'. Preview continues with GUID texture.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "EffectMaterialPreview mask texture fell back to path. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'. Preview continues with fallback texture.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }

    _maskTexture = Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), resolvedPath.c_str(), 1);
    _loadedMaskTextureKey = requestedTextureKey;

    if (_maskTexture == nullptr)
    {
        _loadedMaskTextureKey.clear();
        _statusMessage = "Mask texture missing. Preview continues without mask.";
        LOG_WARN_THROTTLED(
            60,
            "EffectMaterialPreview mask texture create failed. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'. Preview continues without mask.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_FlowTexture(const string& textureGuid, const string& texturePath)
{
    if (textureGuid.empty() && texturePath.empty())
    {
        _flowTexture = nullptr;
        _loadedFlowTextureKey.clear();
        return true;
    }

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Resolve_PreviewTexturePath(textureGuid, texturePath, usedPathFallback, guidPathMismatch);

    if (resolvedPath.empty())
    {
        _flowTexture = nullptr;
        _loadedFlowTextureKey.clear();
        _statusMessage = "Flow texture missing. Distortion preview uses neutral flow.";
        LOG_WARN_THROTTLED(
            60,
            "EffectMaterialPreview flow texture missing. flowTextureGuid='{}', flowTexturePath='{}'. Distortion preview uses neutral flow.",
            textureGuid,
            texturePath
        );
        return true;
    }

    const string requestedTextureKey = String::ToString(resolvedPath);
    if (_flowTexture != nullptr && _loadedFlowTextureKey == requestedTextureKey)
        return true;

    if (guidPathMismatch)
    {
        LOG_WARN(
            "EffectMaterialPreview flow texture GUID/path mismatch. flowTextureGuid='{}', flowTexturePath='{}', guidResolvedPath='{}'. Distortion preview continues with GUID texture.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "EffectMaterialPreview flow texture fell back to path. flowTextureGuid='{}', flowTexturePath='{}', resolvedPath='{}'. Distortion preview continues with fallback texture.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }

    _flowTexture = Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), resolvedPath.c_str(), 1);
    _loadedFlowTextureKey = requestedTextureKey;

    if (_flowTexture == nullptr)
    {
        _loadedFlowTextureKey.clear();
        _statusMessage = "Flow texture missing. Distortion preview uses neutral flow.";
        LOG_WARN_THROTTLED(
            60,
            "EffectMaterialPreview flow texture create failed. flowTextureGuid='{}', flowTexturePath='{}', resolvedPath='{}'. Distortion preview uses neutral flow.",
            textureGuid,
            texturePath,
            requestedTextureKey
        );
    }

    return true;
}

bool EffectMaterialPreviewRenderer::Ready_Meshes()
{
    return Ready_PlaneMesh() && Ready_SphereMesh();
}

bool EffectMaterialPreviewRenderer::Ready_PlaneMesh()
{
    if (_planeMesh.vertexBuffer != nullptr && _planeMesh.indexBuffer != nullptr)
        return true;

    const vector<PreviewVertex> vertices{
        { Vec3{ -1.f, 1.f, 0.f }, Vec3{ 0.f, 0.f, -1.f }, Vec2{ 0.f, 0.f } },
        { Vec3{ 1.f, 1.f, 0.f }, Vec3{ 0.f, 0.f, -1.f }, Vec2{ 1.f, 0.f } },
        { Vec3{ 1.f, -1.f, 0.f }, Vec3{ 0.f, 0.f, -1.f }, Vec2{ 1.f, 1.f } },
        { Vec3{ -1.f, -1.f, 0.f }, Vec3{ 0.f, 0.f, -1.f }, Vec2{ 0.f, 1.f } },
    };
    const vector<uint32> indices{ 0, 1, 2, 0, 2, 3 };

    return Create_Mesh(vertices, indices, _planeMesh);
}

bool EffectMaterialPreviewRenderer::Ready_SphereMesh()
{
    if (_sphereMesh.vertexBuffer != nullptr && _sphereMesh.indexBuffer != nullptr)
        return true;

    static constexpr uint32 latitudeSegments = 24;
    static constexpr uint32 longitudeSegments = 32;

    vector<PreviewVertex> vertices{};
    vector<uint32> indices{};
    vertices.reserve((latitudeSegments + 1) * (longitudeSegments + 1));
    indices.reserve(latitudeSegments * longitudeSegments * 6);

    for (uint32 lat = 0; lat <= latitudeSegments; ++lat)
    {
        const float v = static_cast<float>(lat) / static_cast<float>(latitudeSegments);
        const float theta = v * XM_PI;
        const float sinTheta = sinf(theta);
        const float cosTheta = cosf(theta);

        for (uint32 lon = 0; lon <= longitudeSegments; ++lon)
        {
            const float u = static_cast<float>(lon) / static_cast<float>(longitudeSegments);
            const float phi = u * XM_2PI;
            const Vec3 normal{
                sinf(phi) * sinTheta,
                cosTheta,
                cosf(phi) * sinTheta
            };

            vertices.push_back(
                PreviewVertex{
                    normal,
                    normal,
                    Vec2{ u, v }
                }
            );
        }
    }

    for (uint32 lat = 0; lat < latitudeSegments; ++lat)
    {
        for (uint32 lon = 0; lon < longitudeSegments; ++lon)
        {
            const uint32 current = lat * (longitudeSegments + 1) + lon;
            const uint32 next = current + longitudeSegments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    return Create_Mesh(vertices, indices, _sphereMesh);
}

bool EffectMaterialPreviewRenderer::Create_Mesh(
    const vector<PreviewVertex>& vertices,
    const vector<uint32>& indices,
    PreviewMeshResource& mesh)
{
    if (EDITOR == nullptr || vertices.empty() || indices.empty())
        return false;

    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = static_cast<uint32>(sizeof(PreviewVertex) * vertices.size());
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData{};
    vertexData.pSysMem = vertices.data();

    if (FAILED(
        EDITOR->Get_Device()->CreateBuffer(
            &vertexBufferDesc,
            &vertexData,
            mesh.vertexBuffer.ReleaseAndGetAddressOf())
    ))
    {
        _statusMessage = "Preview vertex buffer creation failed.";
        return false;
    }

    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth = static_cast<uint32>(sizeof(uint32) * indices.size());
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData{};
    indexData.pSysMem = indices.data();

    if (FAILED(
        EDITOR->Get_Device()->CreateBuffer(
            &indexBufferDesc,
            &indexData,
            mesh.indexBuffer.ReleaseAndGetAddressOf())
    ))
    {
        _statusMessage = "Preview index buffer creation failed.";
        return false;
    }

    mesh.indexCount = static_cast<uint32>(indices.size());
    return true;
}

HRESULT EffectMaterialPreviewRenderer::Render_PreviewMesh(const EffectMaterialInstanceData& material, float previewCycleTime) const
{
    const float aspect = static_cast<float>(_previewTextureWidth) / static_cast<float>(max(1u, _previewTextureHeight));
    constexpr float nearPlane = 0.1f;
    constexpr float farPlane = 100.f;
    const EffectPreviewOrbitCameraFrame cameraFrame = EffectPreviewOrbitCamera::Build_Frame(
        _cameraState,
        aspect,
        XMConvertToRadians(35.f),
        nearPlane,
        farPlane
    );
    const Matrix worldMatrix = XMMatrixIdentity();
    CameraCB cameraCB{};
    cameraCB.viewMatrix = cameraFrame.view;
    cameraCB.projMatrix = cameraFrame.proj;
    cameraCB.viewInverseMatrix = XMMatrixInverse(nullptr, cameraFrame.view);
    cameraCB.projInverseMatrix = XMMatrixInverse(nullptr, cameraFrame.proj);
    cameraCB.cameraPosition = Vec4(cameraFrame.eye.x, cameraFrame.eye.y, cameraFrame.eye.z, 1.f);
    cameraCB.farPlane = farPlane;
    const Vec4 effectMaterialPreviewParams = Vec4(
        material.intensity,
        max(material.opacityPower, 0.0001f),
        material.noiseStrength,
        _noiseTexture != nullptr ? 1.f : 0.f
    );
    const Vec4 effectMaterialPreviewAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        _maskTexture != nullptr ? 1.f : 0.f,
        clamp(material.alphaMultiplier, 0.f, 1.f)
    );
    const Vec4 effectMaterialPreviewMainUVParams = Vec4(
        material.mainUVScale.x,
        material.mainUVScale.y,
        material.mainUVScrollSpeed.x,
        material.mainUVScrollSpeed.y
    );
    const Vec4 effectMaterialPreviewNoiseUVParams = Vec4(
        material.noiseUVScale.x,
        material.noiseUVScale.y,
        material.noiseUVScrollSpeed.x,
        material.noiseUVScrollSpeed.y
    );
    const Vec4 effectMaterialPreviewMaskUVParams = Vec4(
        material.maskUVScale.x,
        material.maskUVScale.y,
        material.maskUVScrollSpeed.x,
        material.maskUVScrollSpeed.y
    );
    const Vec4 effectMaterialPreviewUVOffsetParams = Vec4(
        material.mainUVOffset.x,
        material.mainUVOffset.y,
        material.noiseUVOffset.x,
        material.noiseUVOffset.y
    );
    const Vec4 effectMaterialPreviewMaskUVOffsetParams = Vec4(
        material.maskUVOffset.x,
        material.maskUVOffset.y,
        0.f,
        0.f
    );
    const Vec4 effectMaterialPreviewUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
        0.f
    );
    const Vec4 effectMaterialPreviewUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.vPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.vPolicy))
    );
    const Vec4 effectMaterialPreviewMaskUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.vPolicy)),
        0.f,
        0.f
    );
    const Vec4 effectMaterialPreviewUVRotationParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVRotation)),
        static_cast<float>(static_cast<uint32>(material.noiseUVRotation)),
        static_cast<float>(static_cast<uint32>(material.maskUVRotation)),
        0.f
    );
    const EffectMaterialAdditiveContributionData& additive = material.additive;
    const Vec4 effectMaterialPreviewAdditiveParams = Vec4(
        static_cast<float>(static_cast<uint32>(additive.colorSource)),
        static_cast<float>(static_cast<uint32>(additive.amountSource)),
        static_cast<float>(static_cast<uint32>(additive.coveragePolicy)),
        additive.intensityScale
    );
    const Vec4 effectMaterialPreviewAdditiveFlags = Vec4(additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
    const EffectMaterialCoreEmissiveData& coreEmissive = material.coreEmissive;
    const Vec4 effectMaterialPreviewCoreEmissiveParams = Vec4(
        coreEmissive.enabled ? 1.f : 0.f,
        coreEmissive.corePower,
        coreEmissive.coreIntensity,
        coreEmissive.outerPower
    );
    const Vec4 effectMaterialPreviewCoreEmissiveColor = Vec4(
        coreEmissive.coreColor.x,
        coreEmissive.coreColor.y,
        coreEmissive.coreColor.z,
        coreEmissive.outerIntensity
    );
    const int opacitySource = Resolve_MaterialSourceIndex(material.opacitySource);
    const Vec4 effectMaterialPreviewSourceParams = Vec4(
        static_cast<float>(Resolve_MaterialSourceIndex(material.noiseSource)),
        static_cast<float>(Resolve_MaterialSourceIndex(material.maskSource)),
        material.noiseInvert ? 1.f : 0.f,
        material.maskInvert ? 1.f : 0.f
    );

    CHECK_FAILED(_shader->Bind_ObjectCB(worldMatrix), E_FAIL);
    CHECK_FAILED(_shader->Bind_CBufferData(cameraCB), E_FAIL);
    CHECK_FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);

    if (_noiseTexture != nullptr)
        CHECK_FAILED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);

    if (_maskTexture != nullptr)
        CHECK_FAILED(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

    CHECK_FAILED(
        _shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewParams", &effectMaterialPreviewParams, sizeof(effectMaterialPreviewParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewAlphaParams", &effectMaterialPreviewAlphaParams, sizeof(effectMaterialPreviewAlphaParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewMainUVParams", &effectMaterialPreviewMainUVParams, sizeof(effectMaterialPreviewMainUVParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewNoiseUVParams", &effectMaterialPreviewNoiseUVParams, sizeof(effectMaterialPreviewNoiseUVParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewMaskUVParams", &effectMaterialPreviewMaskUVParams, sizeof(effectMaterialPreviewMaskUVParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewUVOffsetParams", &effectMaterialPreviewUVOffsetParams, sizeof(effectMaterialPreviewUVOffsetParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewMaskUVOffsetParams", &effectMaterialPreviewMaskUVOffsetParams, sizeof(
            effectMaterialPreviewMaskUVOffsetParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewUVModeParams", &effectMaterialPreviewUVModeParams, sizeof(effectMaterialPreviewUVModeParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue(
            "g_EffectMaterialPreviewUVAxisPolicyParams",
            &effectMaterialPreviewUVAxisPolicyParams,
            sizeof(effectMaterialPreviewUVAxisPolicyParams)
        ),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue(
            "g_EffectMaterialPreviewMaskUVAxisPolicyParams",
            &effectMaterialPreviewMaskUVAxisPolicyParams,
            sizeof(effectMaterialPreviewMaskUVAxisPolicyParams)
        ),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue(
            "g_EffectMaterialPreviewUVRotationParams",
            &effectMaterialPreviewUVRotationParams,
            sizeof(effectMaterialPreviewUVRotationParams)
        ),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewSourceParams", &effectMaterialPreviewSourceParams, sizeof(effectMaterialPreviewSourceParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewAdditiveParams", &effectMaterialPreviewAdditiveParams, sizeof(effectMaterialPreviewAdditiveParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewAdditiveFlags", &effectMaterialPreviewAdditiveFlags, sizeof(effectMaterialPreviewAdditiveFlags)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewCoreEmissiveParams", &effectMaterialPreviewCoreEmissiveParams, sizeof(
            effectMaterialPreviewCoreEmissiveParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewCoreEmissiveColor", &effectMaterialPreviewCoreEmissiveColor, sizeof(
            effectMaterialPreviewCoreEmissiveColor)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewTime", &previewCycleTime, sizeof(previewCycleTime)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_OpacitySource", &opacitySource, sizeof(opacitySource)),
        E_FAIL
    );
    const uint32 meshPassOffset = _meshMode == EffectMaterialPreviewMeshMode::Sphere ? 1u : 0u;
    const uint32 blendPassOffset = material.blendMode == EffectMaterialBlendMode::Additive ? 2u : 0u;
    const uint32 passIndex = blendPassOffset + meshPassOffset;
    CHECK_FAILED(_shader->Begin(passIndex), E_FAIL);

    const PreviewMeshResource& mesh =
        _meshMode == EffectMaterialPreviewMeshMode::Sphere ? _sphereMesh : _planeMesh;
    CHECK_FAILED(Render_Mesh(mesh), E_FAIL);

    return S_OK;
}

HRESULT EffectMaterialPreviewRenderer::Render_CheckerBackground(float previewCycleTime) const
{
    CHECK_NULL(_shader, E_FAIL);
    CHECK_NULL(_planeMesh.vertexBuffer, E_FAIL);
    CHECK_NULL(_planeMesh.indexBuffer, E_FAIL);

    const Vec4 checkerParams = Vec4(
        static_cast<float>(_previewTextureWidth),
        static_cast<float>(_previewTextureHeight),
        previewCycleTime,
        0.f
    );

    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectMaterialPreviewCheckerParams", &checkerParams, sizeof(checkerParams)),
        E_FAIL
    );
    CHECK_FAILED(_shader->Begin(4), E_FAIL);
    CHECK_FAILED(Render_Mesh(_planeMesh), E_FAIL);

    return S_OK;
}

HRESULT EffectMaterialPreviewRenderer::Render_DistortionPreviewMesh(
    const EffectMaterialInstanceData& material,
    float previewCycleTime,
    EffectMaterialPreviewDistortionContext distortionContext) const
{
    CHECK_NULL(_distortionShader, E_FAIL);
    CHECK_NULL(_distortionSourceTarget, E_FAIL);

    const float aspect = static_cast<float>(_previewTextureWidth) / static_cast<float>(max(1u, _previewTextureHeight));
    constexpr float nearPlane = 0.1f;
    constexpr float farPlane = 100.f;
    const EffectPreviewOrbitCameraFrame cameraFrame = EffectPreviewOrbitCamera::Build_Frame(
        _cameraState,
        aspect,
        XMConvertToRadians(35.f),
        nearPlane,
        farPlane
    );
    const Matrix worldMatrix = XMMatrixIdentity();
    CameraCB cameraCB{};
    cameraCB.viewMatrix = cameraFrame.view;
    cameraCB.projMatrix = cameraFrame.proj;
    cameraCB.viewInverseMatrix = XMMatrixInverse(nullptr, cameraFrame.view);
    cameraCB.projInverseMatrix = XMMatrixInverse(nullptr, cameraFrame.proj);
    cameraCB.cameraPosition = Vec4(cameraFrame.eye.x, cameraFrame.eye.y, cameraFrame.eye.z, 1.f);
    cameraCB.farPlane = farPlane;

    const Vec4 alphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        _maskTexture != nullptr ? 1.f : 0.f,
        clamp(material.alphaMultiplier, 0.f, 1.f)
    );
    const Vec4 mainUVParams = Vec4(
        material.mainUVScale.x,
        material.mainUVScale.y,
        material.mainUVScrollSpeed.x,
        material.mainUVScrollSpeed.y
    );
    const Vec4 maskUVParams = Vec4(
        material.maskUVScale.x,
        material.maskUVScale.y,
        material.maskUVScrollSpeed.x,
        material.maskUVScrollSpeed.y
    );
    const Vec4 flowUVParams = Vec4(
        material.flowUVScale.x,
        material.flowUVScale.y,
        material.flowUVScrollSpeed.x,
        material.flowUVScrollSpeed.y
    );
    const Vec4 uvOffsetParams = Vec4(
        material.mainUVOffset.x,
        material.mainUVOffset.y,
        material.flowUVOffset.x,
        material.flowUVOffset.y
    );
    const Vec4 maskUVOffsetParams = Vec4(
        material.maskUVOffset.x,
        material.maskUVOffset.y,
        0.f,
        0.f
    );
    const Vec4 uvPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.flowUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.flowUVPolicy.vPolicy)),
        0.f,
        0.f
    );
    const Vec4 materialUVPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.vPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.vPolicy))
    );
    const Vec4 uvRotationParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVRotation)),
        static_cast<float>(static_cast<uint32>(material.maskUVRotation)),
        static_cast<float>(static_cast<uint32>(material.flowUVRotation)),
        0.f
    );
    const Vec4 distortionParams = Vec4(
        material.refractionIntensity,
        max(0.f, material.refractionPresence),
        _flowTexture != nullptr ? 1.f : 0.f,
        previewCycleTime
    );
    EffectDistortionShapeMode distortionShapeMode = material.distortionShapeMode;
    if (distortionContext == EffectMaterialPreviewDistortionContext::Trail)
    {
        if (distortionShapeMode != EffectDistortionShapeMode::AirSheath)
            distortionShapeMode = EffectDistortionShapeMode::None;
    }
    else if (distortionShapeMode == EffectDistortionShapeMode::AirSheath)
        distortionShapeMode = EffectDistortionShapeMode::None;

    const Vec4 distortionShapeParams = Vec4(
        static_cast<float>(static_cast<uint32>(distortionShapeMode)),
        max(0.f, material.distortionShapeRadius),
        max(0.f, material.distortionShapeThickness),
        max(0.f, material.distortionShapeSoftness)
    );
    const Vec4 screenSize = Vec4(
        static_cast<float>(_previewTextureWidth),
        static_cast<float>(_previewTextureHeight),
        0.f,
        0.f
    );
    const int opacitySource = Resolve_MaterialSourceIndex(material.opacitySource);
    const Vec4 sourceParams = Vec4(
        max(material.opacityPower, 0.0001f),
        static_cast<float>(Resolve_MaterialSourceIndex(material.maskSource)),
        0.f,
        material.maskInvert ? 1.f : 0.f
    );

    CHECK_FAILED(_distortionShader->Bind_ObjectCB(worldMatrix), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_CBufferData(cameraCB), E_FAIL);
    CHECK_FAILED(_texture->Bind_ShaderResourceView(_distortionShader.get(), "g_Texture", 0), E_FAIL);

    if (_maskTexture != nullptr)
        CHECK_FAILED(_maskTexture->Bind_ShaderResourceView(_distortionShader.get(), "g_MaskTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_distortionShader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

    if (_flowTexture != nullptr)
        CHECK_FAILED(_flowTexture->Bind_ShaderResourceView(_distortionShader.get(), "g_FlowTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_distortionShader->Bind_SRV("g_FlowTexture", nullptr), E_FAIL);

    CHECK_FAILED(_distortionSourceTarget->Bind_ShaderResource(_distortionShader.get(), "g_DistortionSourceTexture"), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewAlphaParams", &alphaParams, sizeof(alphaParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewMainUVParams", &mainUVParams, sizeof(mainUVParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewMaskUVParams", &maskUVParams, sizeof(maskUVParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewFlowUVParams", &flowUVParams, sizeof(flowUVParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewUVOffsetParams", &uvOffsetParams, sizeof(uvOffsetParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewMaskUVOffsetParams", &maskUVOffsetParams, sizeof(maskUVOffsetParams)), E_FAIL);
    CHECK_FAILED(
        _distortionShader->Bind_RawValue(
            "g_EffectMaterialPreviewMaterialUVPolicyParams",
            &materialUVPolicyParams,
            sizeof(materialUVPolicyParams)
        ),
        E_FAIL
    );
    const Vec4 airSheathMapParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.airSheathMapInterpretation)),
        static_cast<float>(static_cast<uint32>(material.airSheathMapXSource)),
        static_cast<float>(static_cast<uint32>(material.airSheathMapYSource)),
        static_cast<float>(static_cast<uint32>(material.airSheathMapComposition))
    );
    const Vec2 airSheathMapSpaceInfluence = Vec2(
        static_cast<float>(static_cast<uint32>(material.airSheathMapVectorSpace)),
        clamp(material.airSheathMapInfluence, 0.f, 1.f)
    );
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewUVPolicyParams", &uvPolicyParams, sizeof(uvPolicyParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewUVRotationParams", &uvRotationParams, sizeof(uvRotationParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewSourceParams", &sourceParams, sizeof(sourceParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewDistortionParams", &distortionParams, sizeof(distortionParams)), E_FAIL);
    CHECK_FAILED(
        _distortionShader->Bind_RawValue("g_EffectMaterialPreviewDistortionShapeParams", &distortionShapeParams, sizeof(distortionShapeParams)),
        E_FAIL
    );
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectAirSheathMapParams", &airSheathMapParams, sizeof(airSheathMapParams)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectAirSheathMapSpaceInfluence", &airSheathMapSpaceInfluence, sizeof(airSheathMapSpaceInfluence)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_EffectMaterialPreviewScreenSize", &screenSize, sizeof(screenSize)), E_FAIL);
    CHECK_FAILED(_distortionShader->Bind_RawValue("g_OpacitySource", &opacitySource, sizeof(opacitySource)), E_FAIL);

    const uint32 passIndex = _meshMode == EffectMaterialPreviewMeshMode::Sphere ? 1u : 0u;
    CHECK_FAILED(_distortionShader->Begin(passIndex), E_FAIL);

    const PreviewMeshResource& mesh =
        _meshMode == EffectMaterialPreviewMeshMode::Sphere ? _sphereMesh : _planeMesh;
    CHECK_FAILED(Render_Mesh(mesh), E_FAIL);

    return S_OK;
}

HRESULT EffectMaterialPreviewRenderer::Render_Mesh(const PreviewMeshResource& mesh) const
{
    if (mesh.vertexBuffer == nullptr || mesh.indexBuffer == nullptr || mesh.indexCount == 0)
        return E_FAIL;

    constexpr uint32 stride = sizeof(PreviewVertex);
    constexpr uint32 offset = 0;
    ID3D11Buffer* vertexBuffer = mesh.vertexBuffer.Get();

    EDITOR->Get_Context()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    EDITOR->Get_Context()->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    EDITOR->Get_Context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    EDITOR->Get_Context()->DrawIndexed(mesh.indexCount, 0, 0);

    return S_OK;
}

void EffectMaterialPreviewRenderer::Resize_RenderTarget(uint32 width, uint32 height)
{
    width = clamp(width, 128u, 2048u);
    height = clamp(height, 128u, 2048u);

    if (_previewTextureWidth == width && _previewTextureHeight == height)
        return;

    _previewTextureWidth = width;
    _previewTextureHeight = height;
    if (_sceneResolve != nullptr)
        _sceneResolve->Resize(width, height);
    _distortionSourceTarget.reset();
    _hasRenderedPreview = false;
}

void EffectMaterialPreviewRenderer::Handle_ViewportInput(
    bool isHovered,
    const ImVec2& canvasMin,
    const ImVec2& canvasMax)
{
    const ImGuiIO& io = ImGui::GetIO();
    Update_HomeTransition(max(io.DeltaTime, 0.f));

    if (isHovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
        Start_HomeTransition();

    const bool dragInputHeld =
        ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
        ImGui::IsMouseDown(ImGuiMouseButton_Right);

    if (_isViewportDragging && !dragInputHeld)
        End_ViewportDrag();

    const bool shouldBeginDrag =
        isHovered &&
        (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right));

    if (shouldBeginDrag)
    {
        Begin_ViewportDrag(canvasMin, canvasMax);
        _hasPendingHome = false;
        return;
    }

    if (_isViewportDragging)
    {
        const Vec2 mouseDelta = GAME->Get_MouseDelta();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            EffectPreviewOrbitCamera::Orbit(_cameraState, mouseDelta, kPreviewOrbitSensor);
        else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            const EffectPreviewOrbitCameraFrame cameraFrame = EffectPreviewOrbitCamera::Build_Frame(
                _cameraState,
                static_cast<float>(_previewTextureWidth) / static_cast<float>(max(1u, _previewTextureHeight)),
                XMConvertToRadians(35.f),
                0.1f,
                100.f
            );
            const float panScale = max(0.01f, _cameraState.distance * 0.003f);
            EffectPreviewOrbitCamera::Pan(
                _cameraState,
                mouseDelta,
                cameraFrame.right,
                Vec3::Up,
                panScale
            );
        }
    }

    if (isHovered && io.KeyCtrl && io.MouseWheel != 0.f)
    {
        _hasPendingHome = false;
        EffectPreviewOrbitCamera::Dolly(_cameraState, io.MouseWheel, 0.46f, 1.5f, 8.f);
    }
}

void EffectMaterialPreviewRenderer::Begin_ViewportDrag(const ImVec2& canvasMin, const ImVec2& canvasMax)
{
    if (GAME == nullptr)
        return;

    RECT lockRect{};
    lockRect.left = static_cast<LONG>(canvasMin.x);
    lockRect.top = static_cast<LONG>(canvasMin.y);
    lockRect.right = static_cast<LONG>(canvasMax.x);
    lockRect.bottom = static_cast<LONG>(canvasMax.y);

    GAME->Set_MouseLockOverrideRect(lockRect);
    GAME->Begin_EditorMouseLockSession();
    GAME->Lock_Mouse();
    _isViewportDragging = true;
}

void EffectMaterialPreviewRenderer::End_ViewportDrag()
{
    if (!_isViewportDragging || GAME == nullptr)
        return;

    _isViewportDragging = false;
    GAME->Clear_MouseLockOverrideRect();
    GAME->End_EditorMouseLockSession();
    GAME->Unlock_Mouse();
}

void EffectMaterialPreviewRenderer::Start_HomeTransition()
{
    _hasPendingHome = true;
}

void EffectMaterialPreviewRenderer::Update_HomeTransition(float timeDelta)
{
    if (!_hasPendingHome)
        return;

    _cameraState.pivot = Math::Lerp_Damp(_cameraState.pivot, Vec3::Zero, kHomeTransitionSpeed, timeDelta);
    _cameraState.distance = Math::Lerp_Damp(_cameraState.distance, kHomeDistance, kHomeTransitionSpeed, timeDelta);
    _cameraState.yawDegrees = Math::Lerp_Damp(_cameraState.yawDegrees, kHomeYawDegrees, kHomeTransitionSpeed, timeDelta);
    _cameraState.pitchDegrees = Math::Lerp_Damp(_cameraState.pitchDegrees, kHomePitchDegrees, kHomeTransitionSpeed, timeDelta);

    if (_cameraState.pivot.LengthSquared() <= 0.001f &&
        fabsf(_cameraState.distance - kHomeDistance) <= 0.01f &&
        fabsf(_cameraState.yawDegrees - kHomeYawDegrees) <= 0.01f &&
        fabsf(_cameraState.pitchDegrees - kHomePitchDegrees) <= 0.01f)
    {
        Reset_View();
        _hasPendingHome = false;
    }
}

void EffectMaterialPreviewRenderer::Reset_View()
{
    EffectPreviewOrbitCamera::Reset(
        _cameraState,
        Vec3::Zero,
        kHomeDistance,
        kHomeYawDegrees,
        kHomePitchDegrees
    );
}

void EffectMaterialPreviewRenderer::Draw_ViewportTextOverlay(
    const ImVec2& canvasMin,
    const ImVec2& canvasMax,
    const char* label)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool hasLabel = label != nullptr && label[0] != '\0';
    if (hasLabel)
    {
        drawList->AddText(
            ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f),
            IM_COL32(214, 220, 230, 255),
            label
        );
    }

    if (!_statusMessage.empty())
    {
        drawList->AddText(
            ImVec2(canvasMin.x + 12.f, canvasMin.y + (hasLabel ? 34.f : 12.f)),
            IM_COL32(245, 198, 120, 255),
            _statusMessage.c_str()
        );
    }

    drawList->AddText(
        ImVec2(canvasMin.x + 12.f, canvasMax.y - 24.f),
        IM_COL32(180, 190, 205, 255),
        "LMB orbit / MMB or RMB pan / Ctrl+Wheel zoom / F reset"
    );
}

bool EffectMaterialPreviewRenderer::Draw_MeshModeControls(const ImVec2& canvasMin, const ImVec2& canvasMax)
{
    constexpr float buttonWidth = 64.f;
    constexpr float buttonHeight = 24.f;
    constexpr float overlayPadding = 10.f;
    const float totalWidth = buttonWidth * 2.f + ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 overlayPos{
        max(canvasMin.x + overlayPadding, canvasMax.x - totalWidth - overlayPadding),
        canvasMin.y + overlayPadding
    };

    ImGui::SetCursorScreenPos(overlayPos);

    ImGui::PushID("EffectMaterialPreviewMeshMode");

    bool isHovered = false;
    const bool isPlane = _meshMode == EffectMaterialPreviewMeshMode::Plane;
    if (isPlane)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.18f, 0.36f, 0.62f, 1.f });
    if (ImGui::Button("Plane", ImVec2{ buttonWidth, buttonHeight }))
        _meshMode = EffectMaterialPreviewMeshMode::Plane;
    isHovered |= ImGui::IsItemHovered();
    if (isPlane)
        ImGui::PopStyleColor();

    ImGui::SameLine(0.f, 6.f);

    const bool isSphere = _meshMode == EffectMaterialPreviewMeshMode::Sphere;
    if (isSphere)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.18f, 0.36f, 0.62f, 1.f });
    if (ImGui::Button("Sphere", ImVec2{ buttonWidth, buttonHeight }))
        _meshMode = EffectMaterialPreviewMeshMode::Sphere;
    isHovered |= ImGui::IsItemHovered();
    if (isSphere)
        ImGui::PopStyleColor();

    ImGui::PopID();
    return isHovered;
}

void EffectMaterialPreviewRenderer::Cleanup()
{
    End_ViewportDrag();
    _isViewportDragging = false;
    _sceneResolve.reset();
    _distortionSourceTarget.reset();
    _shader.reset();
    _distortionShader.reset();
    _texture.reset();
    _noiseTexture.reset();
    _maskTexture.reset();
    _flowTexture.reset();
    _planeMesh.vertexBuffer.Reset();
    _planeMesh.indexBuffer.Reset();
    _planeMesh.indexCount = 0;
    _sphereMesh.vertexBuffer.Reset();
    _sphereMesh.indexBuffer.Reset();
    _sphereMesh.indexCount = 0;
    _loadedTextureKey.clear();
    _loadedNoiseTextureKey.clear();
    _loadedMaskTextureKey.clear();
    _loadedFlowTextureKey.clear();
    _statusMessage.clear();
    _hasRenderedPreview = false;
    _hasPendingHome = false;
}

Unique<EffectMaterialPreviewRenderer> EffectMaterialPreviewRenderer::Create(uint32 previewTextureSize)
{
    return make_unique<EffectMaterialPreviewRenderer>(previewTextureSize);
}

void EffectMaterialPreviewRenderer::Free()
{
    Cleanup();
    __super::Free();
}

NS_END
