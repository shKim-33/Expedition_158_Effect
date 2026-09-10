#include "MeshDataPreview_View.h"

#include "EffectEditorInstance.h"
#include "EffectMaterialInstance_View.h"
#include "EffectMaterialPresetReader.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "EffectPreviewOrbitCamera.h"
#include "EffectPreviewPostProcessRenderer.h"
#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"
#include "Helper_Math.h"
#include "ModelCom.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"

#include <cmath>

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kWindowName = L"MeshData Preview";
    constexpr auto kMeshShaderId = L"Shader_EffectMeshPreview";
    constexpr auto kMeshDistortionShaderId = L"Shader_EffectMeshPreviewDistortion";
    constexpr auto kMeshGlassShaderId = L"Shader_EffectMeshPreviewGlass";
    constexpr auto kEffectMaterialPreviewShaderId = L"Shader_EffectMaterialPreview";
    constexpr auto kEffectMaterialPayloadType = "CONTENT_BROWSER_EFFECT_MATERIAL";
    constexpr auto kEffectModelPayloadType = "CONTENT_BROWSER_EFFECT_MODEL";
    constexpr auto kModelDialogDefaultFolder = L"Effects\\Models";
    constexpr auto kMaterialDialogDefaultFolder = L"Effects\\Materials";

    constexpr float kMeshPreviewHomeDistance{ 6.f };
    constexpr float kMeshPreviewHomeYawDegrees{ 0.f };
    constexpr float kMeshPreviewHomePitchDegrees{ 30.f };
    constexpr float kMeshPreviewHomeTransitionSpeed{ 10.f };
    constexpr float kMeshPreviewOrbitSensor{ 0.105f };
    constexpr float kPreviewPaneWidthRatio{ 0.42f };
    constexpr float kPreviewPaneMinWidth{ 420.f };
    constexpr float kPreviewPaneMaxWidth{ 640.f };
    constexpr float kSideBySideLayoutMinWidth{ 760.f };
    constexpr int kOpacitySourceAlpha{ 0 };
    constexpr int kOpacitySourceRed{ 1 };
    constexpr int kOpacitySourceLuminance{ 2 };

    template <typename T>
    Shared<T> Clone_Component(uint32 levelIndex, const wstring& prototypeTag, void* arg = nullptr)
    {
        if (nullptr == GAME)
            return nullptr;

        return dynamic_pointer_cast<T>(
            GAME->Clone_Prototype(Prototype::Comopnent, levelIndex, prototypeTag, arg)
        );
    }

    bool Has_DuplicateEmitterName(const vector<AuthoringEmitter>& emitters, const AuthoringEmitter& target)
    {
        if (target.name.empty())
            return false;

        uint32 matchCount = 0;
        for (const AuthoringEmitter& emitter : emitters)
        {
            if (emitter.name == target.name)
                ++matchCount;
        }

        return matchCount > 1;
    }

    string Build_EmitterPreviewLabel(const vector<AuthoringEmitter>& emitters, const AuthoringEmitter& emitter, const char* previewKind)
    {
        string label = emitter.name.empty() ? "Emitter" : emitter.name;
        if (emitter.name.empty() || Has_DuplicateEmitterName(emitters, emitter))
            label += " #" + std::to_string(emitter.id);

        label += " - ";
        label += previewKind;
        return label;
    }

    string To_DisplayPath(const fs::path& path)
    {
        return path.empty() ? string{} : String::ToString(path.lexically_normal().wstring());
    }

    string Make_AssetRelativePath(const fs::path& filePath)
    {
        if (GAME == nullptr)
            return String::ToString(filePath.lexically_normal().generic_wstring());

        const fs::path assetRoot = fs::path(GAME->Get_AssetRoot()).lexically_normal();
        const fs::path normalizedPath = filePath.lexically_normal();
        const fs::path relativePath = normalizedPath.lexically_relative(assetRoot);
        if (!relativePath.empty() && relativePath.native().find(L"..") != 0)
            return String::ToString(relativePath.generic_wstring());

        return String::ToString(normalizedPath.wstring());
    }

    fs::path Resolve_AssetPath(const string& guid, const string& path)
    {
        if (nullptr == GAME)
            return fs::path(String::ToWString(path));

        if (!guid.empty())
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(guid);
            if (!resolvedPath.empty())
                return fs::path(resolvedPath).lexically_normal();
        }

        fs::path resolvedPath = fs::path(String::ToWString(path));
        if (resolvedPath.is_relative())
            resolvedPath = fs::path(GAME->Get_AssetRoot()) / resolvedPath;

        return resolvedPath.lexically_normal();
    }

    bool Try_PickAssetDialog(
        const wchar_t* title,
        const wchar_t* filterName,
        const wchar_t* filterSpec,
        const wchar_t* defaultFolder,
        wstring& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || nullptr == dialog)
        {
            if (shouldUninit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(title);

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        if (nullptr != GAME && nullptr != defaultFolder)
        {
            const fs::path folderPath = fs::path(GAME->Get_AssetRoot()) / defaultFolder;
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(folderPath.lexically_normal().wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) &&
                folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[] = {
            { filterName, filterSpec },
            { L"All", L"*.*" },
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);

        hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();
        if (shouldUninit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    bool Apply_ModelFileToMeshData(const wchar_t* payloadPath, MeshTypeData& data)
    {
        if (nullptr == payloadPath || nullptr == GAME)
            return false;

        const fs::path filePath = fs::path(payloadPath).lexically_normal();
        if (String::ToLowerCopy(filePath.extension().string()) != ".model")
            return false;

        data.modelGuid = GAME->Ensure_AssetGUID(filePath.wstring(), "Model");
        data.modelPath = Make_AssetRelativePath(filePath);
        return true;
    }

    bool Apply_EffectMaterialFileToMeshData(const wchar_t* payloadPath, MeshTypeData& data)
    {
        if (nullptr == payloadPath || nullptr == GAME)
            return false;

        const fs::path filePath = fs::path(payloadPath).lexically_normal();
        if (String::ToLowerCopy(filePath.extension().string()) != ".json")
            return false;

        data.assignedEffectMaterialGuid = GAME->Ensure_AssetGUID(filePath.wstring(), "EffectMaterial");
        data.assignedEffectMaterialPath = Make_AssetRelativePath(filePath);
        data.hasAssignedMaterialInstance = EffectMaterialPresetReader::Read(filePath, data.assignedMaterial);
        if (!data.hasAssignedMaterialInstance)
            data.assignedMaterial = {};
        return true;
    }

    bool Resolve_EffectiveMaterial(const MeshTypeData& meshData, EffectMaterialInstanceData& outMaterial)
    {
        if (meshData.hasAssignedMaterialInstance)
        {
            outMaterial = meshData.assignedMaterial;
            return true;
        }

        const fs::path materialPath = Resolve_AssetPath(
            meshData.assignedEffectMaterialGuid,
            meshData.assignedEffectMaterialPath
        );
        return !materialPath.empty() && EffectMaterialPresetReader::Read(materialPath, outMaterial);
    }

    void Apply_RequiredRenderPolicy(const RequiredModuleData* required, EffectMaterialInstanceData& material)
    {
        if (required == nullptr)
            return;

        material.blendMode = required->material.blendMode;
        material.alphaCutoff = required->material.alphaCutoff;
    }

    int Resolve_MaterialSourceIndex(const string& source)
    {
        if (source == "Red" || source == "red")
            return kOpacitySourceRed;

        if (source == "Luminance" || source == "luminance")
            return kOpacitySourceLuminance;

        return kOpacitySourceAlpha;
    }

    bool Is_MeshDistortionPreviewFamily(const EffectMaterialInstanceData& material)
    {
        return material.materialFamily == EffectMaterialFamily::SpriteDistortion;
    }

    bool Is_MeshGlassPreviewFamily(const EffectMaterialInstanceData& material)
    {
        return material.materialFamily == EffectMaterialFamily::MeshGlass;
    }

    bool Draw_AssetDropSlot(
        const char* id,
        const char* title,
        const char* payloadType,
        const string& displayPath,
        const char* placeholder,
        const char* summary,
        const function<bool(const wchar_t*)>& onDrop,
        const function<bool()>& onPick)
    {
        constexpr float slotHeight = 46.f;
        constexpr float browseButtonWidth = 34.f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float slotWidth = max(160.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        bool changed = false;

        ImGui::PushID(id);
        ImGui::InvisibleButton("DropSlot", ImVec2(slotWidth, slotHeight));

        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isMatchingPayload = activePayload != nullptr && activePayload->IsDataType(payloadType);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                if (payload->IsDelivery())
                    changed = onDrop(static_cast<const wchar_t*>(payload->Data));
            }
            ImGui::EndDragDropTarget();
        }

        const ImVec2 slotMin = ImGui::GetItemRectMin();
        const ImVec2 slotMax = ImGui::GetItemRectMax();
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            slotMin,
            slotMax,
            hovered || isMatchingPayload ? IM_COL32(34, 42, 58, 255) : IM_COL32(30, 34, 42, 255),
            6.f
        );
        drawList->AddRect(
            slotMin,
            slotMax,
            hovered || isMatchingPayload ? IM_COL32(116, 156, 235, 255) : IM_COL32(78, 88, 108, 255),
            6.f,
            0,
            isMatchingPayload ? 2.f : 1.f
        );

        const float textLeft = slotMin.x + 10.f;
        drawList->PushClipRect(ImVec2(textLeft, slotMin.y), ImVec2(slotMax.x - 8.f, slotMax.y), true);
        drawList->AddText(ImVec2(textLeft, slotMin.y + 6.f), IM_COL32(235, 238, 245, 255), title);
        drawList->AddText(
            ImVec2(textLeft, slotMin.y + 25.f),
            IM_COL32(150, 160, 176, 255),
            displayPath.empty() ? placeholder : displayPath.c_str()
        );
        drawList->PopClipRect();

        if (hovered)
            ImGui::SetTooltip("%s\n%s", summary, displayPath.empty() ? placeholder : displayPath.c_str());

        ImGui::SameLine(0.f, spacing);
        if (ImGui::Button(ICON_FA_FOLDER_OPEN, ImVec2(browseButtonWidth, slotHeight)))
            changed = onPick();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("파일 선택");

        ImGui::PopID();
        return changed;
    }
}

struct MeshDataPreview_View::PreviewRenderer
{
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

    struct MaterialPreviewTime
    {
        float duration{ 1.f };
        float cycleTime{};
        float phase{};
    };

    uint32 width{ 512 };
    uint32 height{ 512 };
    Unique<EffectPreviewPostProcessRenderer> sceneResolve{};
    Shared<RenderTarget> checkerSourceTarget{};
    Shared<ModelCom> model{};
    Shared<ShaderCom> shader{};
    Shared<ShaderCom> checkerShader{};
    Shared<Texture> mainTexture{};
    Shared<Texture> noiseTexture{};
    Shared<Texture> maskTexture{};
    Shared<Texture> flowTexture{};
    PreviewMeshResource checkerQuad{};
    string loadedModelKey{};
    wstring loadedShaderKey{};
    string loadedMainTextureKey{};
    string loadedNoiseTextureKey{};
    string loadedMaskTextureKey{};
    string loadedFlowTextureKey{};
    string statusMessage{};
    bool hasRendered{ false };
    EffectPreviewOrbitCameraState cameraState{};
    float materialElapsedTime{};
    bool isViewportDragging{ false };
    bool hasPendingHome{ false };

    PreviewRenderer()
        : sceneResolve(
            make_unique<EffectPreviewPostProcessRenderer>(
                L"MeshDataPreviewDisplay",
                L"MeshDataPreviewHDR",
                L"MeshDataPreviewBloom",
                width,
                height))
    {
        Reset_View();
    }

    ~PreviewRenderer()
    {
        End_ViewportDrag();
    }

    void Render_Model(const AuthoringEmitter& emitter, const MeshTypeData& meshData, const fs::path& modelPath)
    {
        statusMessage.clear();
        hasRendered = false;

        if (EDITOR == nullptr || GAME == nullptr)
        {
            statusMessage = "Preview renderer is not ready.";
            return;
        }

        if (modelPath.empty() || !fs::exists(modelPath))
        {
            statusMessage = "Model asset is missing.";
            return;
        }

        EffectMaterialInstanceData effectMaterial{};
        if (!Resolve_EffectiveMaterial(meshData, effectMaterial))
        {
            statusMessage = "Assigned effect material is missing.";
            return;
        }

        if (const MaterialScalarModulationModuleData* modulation = Find_MaterialScalarModulationModuleData(emitter))
        {
            const RequiredModuleData* required = Find_RequiredModuleData(emitter);
            const MaterialPreviewTime previewTime = Resolve_MaterialPreviewTime(required);
            const MaterialScalarModulationPreviewResult previewMaterial =
                Evaluate_MaterialScalarModulationPreview(
                    effectMaterial,
                    Build_CoreColorRgbModulationRuntimeDesc(*modulation),
                    Build_MaterialVec2ModulationRuntimeDesc(*modulation),
                    Build_MaterialScalarModulationRuntimeDesc(*modulation),
                    previewTime.phase,
                    0u,
                    false
                );
            effectMaterial = previewMaterial.material;
        }

        Apply_RequiredRenderPolicy(Find_RequiredModuleData(emitter), effectMaterial);
        const MaterialPreviewTime previewTime = Resolve_MaterialPreviewTime(Find_RequiredModuleData(emitter));

        if (!Ready_RenderTargets() || !Ready_CheckerShader() || !Ready_CheckerQuad() || !Ready_Shader(effectMaterial) || !Ready_Model(modelPath))
            return;

        if (!Ready_EffectMaterialTextures(effectMaterial))
            return;

        ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
        EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

        if (FAILED(checkerSourceTarget->Begin(true, true)) ||
            FAILED(Render_CheckerBackground(previewTime.cycleTime)))
        {
            GAME->Bind_BackBuffer();
            statusMessage = "Preview checker source draw failed.";
            return;
        }

        if (FAILED(sceneResolve->Begin_HDRTarget()))
        {
            statusMessage = "Preview render target begin has failed.";
            return;
        }

        HRESULT hr = Render_CheckerBackground(previewTime.cycleTime);
        if (SUCCEEDED(hr))
            hr = Draw_Model(meshData.previewScale, effectMaterial, previewTime.cycleTime, meshData.useModelMaterials);

        if (SUCCEEDED(hr))
            hr = sceneResolve->Resolve();

        GAME->Bind_BackBuffer();

        if (FAILED(hr))
        {
            statusMessage = "Model preview draw failed.";
            return;
        }

        hasRendered = true;
    }

    void Draw_Viewport(
        const char* id,
        const AuthoringEmitter& emitter,
        const MeshTypeData& meshData,
        const fs::path& modelPath,
        const char* label,
        bool fillAvailableHeight = false)
    {
        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        const float previewHeight = fillAvailableHeight
                                    ? max(280.f, availableSize.y)
                                    : max(280.f, min(560.f, availableSize.y > 0.f ? availableSize.y : 360.f));
        const ImVec2 canvasSize{ max(1.f, availableSize.x), previewHeight };

        ImGui::BeginChild(id, canvasSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            ImVec2 innerSize = ImGui::GetContentRegionAvail();
            innerSize.x = max(1.f, innerSize.x);
            innerSize.y = max(1.f, innerSize.y);
            Resize_RenderTarget(static_cast<uint32>(innerSize.x), static_cast<uint32>(innerSize.y));
            materialElapsedTime += max(ImGui::GetIO().DeltaTime, 0.f);
            Render_Model(emitter, meshData, modelPath);

            const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
            const ImVec2 canvasMax{ canvasMin.x + innerSize.x, canvasMin.y + innerSize.y };
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImGui::InvisibleButton("MeshDataPreviewCanvas", innerSize);
            const bool isHovered = ImGui::IsItemHovered();

            if (ShaderResourceView* srv = Get_SRV())
            {
                const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srv));
                drawList->AddImage(textureId, canvasMin, canvasMax);
            }
            else
            {
                const char* message = statusMessage.empty() ? "Model preview is not ready." : statusMessage.c_str();
                drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 34.f), IM_COL32(245, 198, 120, 255), message);
            }

            Draw_ViewportTextOverlay(canvasMin, canvasMax, label);
            Handle_ViewportInput(isHovered, canvasMin, canvasMax);
        }
        ImGui::EndChild();
    }

    ShaderResourceView* Get_SRV() const
    {
        return hasRendered && sceneResolve != nullptr ? sceneResolve->Get_SRV() : nullptr;
    }

    bool Ready_RenderTargets()
    {
        if (sceneResolve != nullptr && checkerSourceTarget != nullptr)
            return true;

        if (sceneResolve == nullptr)
        {
            sceneResolve = make_unique<EffectPreviewPostProcessRenderer>(
                L"MeshDataPreviewDisplay",
                L"MeshDataPreviewHDR",
                L"MeshDataPreviewBloom",
                width,
                height
            );
        }

        if (sceneResolve == nullptr || !sceneResolve->Ready(statusMessage))
        {
            if (statusMessage.empty())
                statusMessage = "Preview render target creation failed.";
            return false;
        }

        if (checkerSourceTarget == nullptr)
        {
            checkerSourceTarget = RenderTarget::Create(
                L"MeshDataPreviewCheckerSource",
                EDITOR->Get_Device(),
                EDITOR->Get_Context(),
                width,
                height,
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                Vec4{ 0.f, 0.f, 0.f, 0.f },
                true
            );
        }

        if (checkerSourceTarget == nullptr)
        {
            statusMessage = "Preview checker source target creation failed.";
            return false;
        }

        return true;
    }

    bool Ready_CheckerShader()
    {
        if (checkerShader != nullptr)
            return true;

        checkerShader = Clone_Component<ShaderCom>(ETOI(LevelType::Static), kEffectMaterialPreviewShaderId);
        if (checkerShader == nullptr)
        {
            statusMessage = "Shader_EffectMaterialPreview prototype is missing.";
            return false;
        }

        return true;
    }

    bool Ready_CheckerQuad()
    {
        if (checkerQuad.vertexBuffer != nullptr && checkerQuad.indexBuffer != nullptr)
            return true;

        const vector<PreviewVertex> vertices{
            { Vec3{ -1.f, 1.f, 0.f }, Vec3{ 0.f, 0.f, -1.f }, Vec2{ 0.f, 0.f } },
            { Vec3{ 1.f, 1.f, 0.f }, Vec3{ 0.f, 0.f, -1.f }, Vec2{ 1.f, 0.f } },
            { Vec3{ 1.f, -1.f, 0.f }, Vec3{ 0.f, 0.f, -1.f }, Vec2{ 1.f, 1.f } },
            { Vec3{ -1.f, -1.f, 0.f }, Vec3{ 0.f, 0.f, -1.f }, Vec2{ 0.f, 1.f } },
        };
        const vector<uint32> indices{ 0, 1, 2, 0, 2, 3 };

        return Create_Mesh(vertices, indices, checkerQuad);
    }

    bool Create_Mesh(const vector<PreviewVertex>& vertices, const vector<uint32>& indices, PreviewMeshResource& mesh)
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
            statusMessage = "Preview checker vertex buffer creation failed.";
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
            statusMessage = "Preview checker index buffer creation failed.";
            return false;
        }

        mesh.indexCount = static_cast<uint32>(indices.size());
        return true;
    }

    bool Ready_Shader(const EffectMaterialInstanceData& material)
    {
        wstring requestedShaderId = kMeshShaderId;
        if (Is_MeshDistortionPreviewFamily(material))
            requestedShaderId = kMeshDistortionShaderId;
        else if (Is_MeshGlassPreviewFamily(material))
            requestedShaderId = kMeshGlassShaderId;
        if (shader != nullptr && loadedShaderKey == requestedShaderId)
            return true;

        shader.reset();
        loadedShaderKey.clear();

        shader = Clone_Component<ShaderCom>(ETOI(LevelType::Static), requestedShaderId);
        if (shader == nullptr)
        {
            if (requestedShaderId == kMeshDistortionShaderId)
                statusMessage = "Shader_EffectMeshPreviewDistortion prototype is missing.";
            else if (requestedShaderId == kMeshGlassShaderId)
                statusMessage = "Shader_EffectMeshPreviewGlass prototype is missing.";
            else
                statusMessage = "Shader_EffectMeshPreview prototype is missing.";
            return false;
        }

        loadedShaderKey = requestedShaderId;
        return true;
    }

    bool Ready_Model(const fs::path& modelPath)
    {
        const string requestedKey = String::ToString(modelPath.lexically_normal().wstring());
        if (model != nullptr && loadedModelKey == requestedKey)
            return true;

        model.reset();
        loadedModelKey.clear();

        model = ModelCom::Create(
            EDITOR->Get_Device(),
            EDITOR->Get_Context(),
            ModelType::NonAnim,
            requestedKey.c_str(),
            Matrix::CreateScale(0.01f)
        );
        if (model == nullptr)
        {
            statusMessage = "Model load failed.";
            return false;
        }

        loadedModelKey = requestedKey;
        Reset_ViewToModel();
        return true;
    }

    bool Ready_Texture(
        const string& textureGuid,
        const string& texturePath,
        bool required,
        Shared<Texture>& outTexture,
        string& loadedTextureKey,
        const char* missingMessage)
    {
        if (textureGuid.empty() && texturePath.empty())
        {
            outTexture.reset();
            loadedTextureKey.clear();
            if (required)
                statusMessage = missingMessage;
            return !required;
        }

        const fs::path texturePathResolved = Resolve_AssetPath(textureGuid, texturePath);
        if (texturePathResolved.empty() || !fs::exists(texturePathResolved))
        {
            outTexture.reset();
            loadedTextureKey.clear();
            statusMessage = missingMessage;
            return !required;
        }

        const string requestedKey = String::ToString(texturePathResolved.lexically_normal().wstring());
        if (outTexture != nullptr && loadedTextureKey == requestedKey)
            return true;

        outTexture = Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), texturePathResolved.wstring().c_str(), 1);
        if (outTexture == nullptr)
        {
            loadedTextureKey.clear();
            statusMessage = missingMessage;
            return !required;
        }

        loadedTextureKey = requestedKey;
        return true;
    }

    bool Ready_EffectMaterialTextures(const EffectMaterialInstanceData& material)
    {
        if (Is_MeshDistortionPreviewFamily(material))
        {
            mainTexture.reset();
            noiseTexture.reset();
            maskTexture.reset();
            loadedMainTextureKey.clear();
            loadedNoiseTextureKey.clear();
            loadedMaskTextureKey.clear();

            Ready_Texture(
                material.flowTextureGuid,
                material.flowTexturePath,
                false,
                flowTexture,
                loadedFlowTextureKey,
                "Flow texture missing. Distortion preview continues without offset."
            );
            return true;
        }

        flowTexture.reset();
        loadedFlowTextureKey.clear();

        if (!Ready_Texture(
            material.mainTextureGuid,
            material.mainTexturePath,
            !Is_MeshGlassPreviewFamily(material),
            mainTexture,
            loadedMainTextureKey,
            "Assigned effect material main texture is missing."
        ))
            return false;

        Ready_Texture(
            material.noiseTextureGuid,
            material.noiseTexturePath,
            false,
            noiseTexture,
            loadedNoiseTextureKey,
            "Noise texture missing. Preview continues without noise."
        );

        Ready_Texture(
            material.maskTextureGuid,
            material.maskTexturePath,
            false,
            maskTexture,
            loadedMaskTextureKey,
            "Mask texture missing. Preview continues without mask."
        );

        return true;
    }

    MaterialPreviewTime Resolve_MaterialPreviewTime(const RequiredModuleData* required) const
    {
        constexpr float kMinDuration = 0.0001f;
        const float safeDuration =
            required != nullptr
            ? max(required->duration, kMinDuration)
            : 1.f;
        const float positiveElapsedTime = max(materialElapsedTime, 0.f);
        const float cycleTime = std::fmod(positiveElapsedTime, safeDuration);
        const float phase = cycleTime / safeDuration;

        return MaterialPreviewTime{
            safeDuration,
            cycleTime,
            phase
        };
    }

    HRESULT Bind_DistortionPreviewResources(const EffectMaterialInstanceData& material, float previewCycleTime)
    {
        CHECK_NULL(shader, E_FAIL);
        CHECK_NULL(checkerSourceTarget, E_FAIL);

        if (flowTexture != nullptr)
            CHECK_FAILED(flowTexture->Bind_ShaderResourceView(shader.get(), "g_FlowTexture", 0), E_FAIL);
        else
            CHECK_FAILED(shader->Bind_SRV("g_FlowTexture", nullptr), E_FAIL);

        const Vec4 distortionParams(
            material.refractionIntensity,
            max(0.f, material.refractionPresence),
            flowTexture != nullptr ? 1.f : 0.f,
            previewCycleTime
        );
        const Vec4 screenSize(static_cast<float>(max(1u, width)), static_cast<float>(max(1u, height)), 0.f, 0.f);
        const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(material.flowUVTilingMode));
        const float flowUVRotation = static_cast<float>(static_cast<uint32>(material.flowUVRotation));
        const Vec2 flowUVPolicyParams(
            static_cast<float>(static_cast<uint32>(material.flowUVPolicy.uPolicy)),
            static_cast<float>(static_cast<uint32>(material.flowUVPolicy.vPolicy))
        );

        CHECK_FAILED(checkerSourceTarget->Bind_ShaderResource(shader.get(), "g_DistortionSourceTexture"), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_DistortionScreenSize", &screenSize, sizeof(screenSize)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVPolicyParams", &flowUVPolicyParams, sizeof(flowUVPolicyParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVScale", &material.flowUVScale, sizeof(material.flowUVScale)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVOffset", &material.flowUVOffset, sizeof(material.flowUVOffset)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVScrollSpeed", &material.flowUVScrollSpeed, sizeof(material.flowUVScrollSpeed)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVRotation", &flowUVRotation, sizeof(flowUVRotation)), E_FAIL);
        return S_OK;
    }

    HRESULT Bind_EffectMaterialResources(const EffectMaterialInstanceData& material, float previewCycleTime)
    {
        CHECK_NULL(shader, E_FAIL);
        if (mainTexture != nullptr)
            CHECK_FAILED(mainTexture->Bind_ShaderResourceView(shader.get(), "g_Texture", 0), E_FAIL);
        else
            CHECK_FAILED(shader->Bind_SRV("g_Texture", nullptr), E_FAIL);

        if (noiseTexture != nullptr)
            CHECK_FAILED(noiseTexture->Bind_ShaderResourceView(shader.get(), "g_NoiseTexture", 0), E_FAIL);
        else
            CHECK_FAILED(shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);

        if (maskTexture != nullptr)
            CHECK_FAILED(maskTexture->Bind_ShaderResourceView(shader.get(), "g_MaskTexture", 0), E_FAIL);
        else
            CHECK_FAILED(shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

        CHECK_FAILED(shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);

        const Vec4 effectMeshParams = Vec4(
            material.intensity,
            material.opacityPower,
            material.noiseStrength,
            noiseTexture != nullptr ? 1.f : 0.f
        );
        const Vec4 effectMeshAlphaParams = Vec4(
            clamp(material.alphaCutoff, 0.f, 1.f),
            clamp(material.alphaErosion, 0.f, 1.f),
            maskTexture != nullptr ? 1.f : 0.f,
            clamp(material.alphaMultiplier, 0.f, 1.f)
        );
        const Vec4 effectMeshMainUVParams = Vec4(material.mainUVScale.x, material.mainUVScale.y, material.mainUVScrollSpeed.x, material.mainUVScrollSpeed.y);
        const Vec4 effectMeshNoiseUVParams = Vec4(
            material.noiseUVScale.x,
            material.noiseUVScale.y,
            material.noiseUVScrollSpeed.x,
            material.noiseUVScrollSpeed.y
        );
        const Vec4 effectMeshMaskUVParams = Vec4(material.maskUVScale.x, material.maskUVScale.y, material.maskUVScrollSpeed.x, material.maskUVScrollSpeed.y);
        const Vec4 effectMeshUVOffsetParams = Vec4(material.mainUVOffset.x, material.mainUVOffset.y, material.noiseUVOffset.x, material.noiseUVOffset.y);
        const Vec4 effectMeshMaskUVOffsetParams = Vec4(material.maskUVOffset.x, material.maskUVOffset.y, 0.f, 0.f);
        const Vec4 effectMeshUVModeParams = Vec4(
            static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
            static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
            static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
            0.f
        );
        const Vec4 effectMeshUVAxisPolicyParams = Vec4(
            static_cast<float>(static_cast<uint32>(material.mainUVPolicy.uPolicy)),
            static_cast<float>(static_cast<uint32>(material.mainUVPolicy.vPolicy)),
            static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.uPolicy)),
            static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.vPolicy))
        );
        const Vec4 effectMeshMaskUVAxisPolicyParams = Vec4(
            static_cast<float>(static_cast<uint32>(material.maskUVPolicy.uPolicy)),
            static_cast<float>(static_cast<uint32>(material.maskUVPolicy.vPolicy)),
            0.f,
            0.f
        );
        const Vec4 effectMeshUVRotationParams = Vec4(
            static_cast<float>(static_cast<uint32>(material.mainUVRotation)),
            static_cast<float>(static_cast<uint32>(material.noiseUVRotation)),
            static_cast<float>(static_cast<uint32>(material.maskUVRotation)),
            0.f
        );
        const EffectMaterialAdditiveContributionData& additive = material.additive;
        const EffectMaterialCoreEmissiveData& coreEmissive = material.coreEmissive;
        const Vec4 effectMeshAdditiveParams = Vec4(
            static_cast<float>(static_cast<uint32>(additive.colorSource)),
            static_cast<float>(static_cast<uint32>(additive.amountSource)),
            static_cast<float>(static_cast<uint32>(additive.coveragePolicy)),
            additive.intensityScale
        );
        const Vec4 effectMeshAdditiveFlags = Vec4(additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
        const Vec4 effectMeshCoreEmissiveParams = Vec4(
            coreEmissive.enabled ? 1.f : 0.f,
            coreEmissive.corePower,
            coreEmissive.coreIntensity,
            coreEmissive.outerPower
        );
        const Vec4 effectMeshCoreEmissiveColor = Vec4(
            coreEmissive.coreColor.x,
            coreEmissive.coreColor.y,
            coreEmissive.coreColor.z,
            coreEmissive.outerIntensity
        );
        const int opacitySource = Resolve_MaterialSourceIndex(material.opacitySource);
        const Vec4 effectMeshPreviewScreenSize(
            static_cast<float>(max(1u, width)),
            static_cast<float>(max(1u, height)),
            0.f,
            0.f
        );
        const Vec4 effectMeshSourceParams = Vec4(
            static_cast<float>(Resolve_MaterialSourceIndex(material.noiseSource)),
            static_cast<float>(Resolve_MaterialSourceIndex(material.maskSource)),
            material.noiseInvert ? 1.f : 0.f,
            material.maskInvert ? 1.f : 0.f
        );

        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshParams", &effectMeshParams, sizeof(effectMeshParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAlphaParams", &effectMeshAlphaParams, sizeof(effectMeshAlphaParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshMainUVParams", &effectMeshMainUVParams, sizeof(effectMeshMainUVParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshNoiseUVParams", &effectMeshNoiseUVParams, sizeof(effectMeshNoiseUVParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshMaskUVParams", &effectMeshMaskUVParams, sizeof(effectMeshMaskUVParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshUVOffsetParams", &effectMeshUVOffsetParams, sizeof(effectMeshUVOffsetParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshMaskUVOffsetParams", &effectMeshMaskUVOffsetParams, sizeof(effectMeshMaskUVOffsetParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshUVModeParams", &effectMeshUVModeParams, sizeof(effectMeshUVModeParams)), E_FAIL);
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMeshUVAxisPolicyParams", &effectMeshUVAxisPolicyParams, sizeof(effectMeshUVAxisPolicyParams)),
            E_FAIL
        );
        CHECK_FAILED(
            shader->Bind_RawValue(
                "g_EffectMeshMaskUVAxisPolicyParams",
                &effectMeshMaskUVAxisPolicyParams,
                sizeof(effectMeshMaskUVAxisPolicyParams)
            ),
            E_FAIL
        );
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshUVRotationParams", &effectMeshUVRotationParams, sizeof(effectMeshUVRotationParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshSourceParams", &effectMeshSourceParams, sizeof(effectMeshSourceParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshMaterialTime", &previewCycleTime, sizeof(previewCycleTime)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAdditiveParams", &effectMeshAdditiveParams, sizeof(effectMeshAdditiveParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAdditiveFlags", &effectMeshAdditiveFlags, sizeof(effectMeshAdditiveFlags)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshCoreEmissiveParams", &effectMeshCoreEmissiveParams, sizeof(effectMeshCoreEmissiveParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshCoreEmissiveColor", &effectMeshCoreEmissiveColor, sizeof(effectMeshCoreEmissiveColor)), E_FAIL);
        if (Is_MeshGlassPreviewFamily(material))
        {
            const Vec4 glassSurfaceParams(
                max(0.f, material.glassAlpha),
                max(0.0001f, material.glassAlphaPower),
                max(0.f, material.glassNormalStrength),
                clamp(material.glassMainInfluence, 0.f, 1.f)
            );
            const Vec4 glassRimParams(
                max(0.f, material.glassRimIntensity),
                max(0.0001f, material.glassRimPower),
                clamp(material.glassNoiseBreakup, 0.f, 1.f),
                clamp(material.glassMaskStrength, 0.f, 1.f)
            );
            const Vec4 glassLightDirection(
                material.glassLightDirection.x,
                material.glassLightDirection.y,
                material.glassLightDirection.z,
                max(0.f, material.glassLightIntensity)
            );
            const Vec4 glassSpecularParams(
                max(0.0001f, material.glassSpecularPower),
                max(0.0001f, material.glassSpecularSoftness),
                0.f,
                0.f
            );

            CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshGlassSurfaceParams", &glassSurfaceParams, sizeof(glassSurfaceParams)), E_FAIL);
            CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshGlassRimColor", &material.glassRimColor, sizeof(material.glassRimColor)), E_FAIL);
            CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshGlassRimParams", &glassRimParams, sizeof(glassRimParams)), E_FAIL);
            CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshGlassLightDirection", &glassLightDirection, sizeof(glassLightDirection)), E_FAIL);
            CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshGlassLightColor", &material.glassLightColor, sizeof(material.glassLightColor)), E_FAIL);
            CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshGlassSpecularParams", &glassSpecularParams, sizeof(glassSpecularParams)), E_FAIL);
        }
        CHECK_FAILED(
            shader->Bind_RawValue("g_EffectMeshPreviewScreenSize", &effectMeshPreviewScreenSize, sizeof(effectMeshPreviewScreenSize)),
            E_FAIL
        );
        CHECK_FAILED(shader->Bind_RawValue("g_OpacitySource", &opacitySource, sizeof(opacitySource)), E_FAIL);
        return S_OK;
    }

    HRESULT Bind_ModelMaterialResourcesForMesh(uint32 meshIndex, const EffectMaterialInstanceData& material, bool useModelMaterials)
    {
        Vec4 modelMaterialFlags = Vec4::Zero;
        const bool supportsModelOrm = !Is_MeshGlassPreviewFamily(material);
        if (!useModelMaterials || model == nullptr)
        {
            if (mainTexture != nullptr)
                CHECK_FAILED_THROTTLED(mainTexture->Bind_ShaderResourceView(shader.get(), "g_Texture", 0), 60, E_FAIL);
            else
                CHECK_FAILED_THROTTLED(shader->Bind_SRV("g_Texture", nullptr), 60, E_FAIL);
            CHECK_FAILED_THROTTLED(shader->Bind_SRV("g_ModelNormalTexture", nullptr), 60, E_FAIL);
            CHECK_FAILED_THROTTLED(shader->Bind_SRV("g_ModelEmissiveTexture", nullptr), 60, E_FAIL);
            if (supportsModelOrm)
                CHECK_FAILED_THROTTLED(shader->Bind_SRV("g_ModelOrmTexture", nullptr), 60, E_FAIL);
            CHECK_FAILED_THROTTLED(shader->Bind_RawValue("g_EffectMeshModelMaterialFlags", &modelMaterialFlags, sizeof(modelMaterialFlags)), 60, E_FAIL);
            return S_OK;
        }

        if (SUCCEEDED(model->Bind_Material(shader.get(), "g_Texture", meshIndex, MaterialTextureSlot::BaseColor)))
            modelMaterialFlags.x = 1.f;
        else if (mainTexture == nullptr)
            CHECK_FAILED_THROTTLED(shader->Bind_SRV("g_Texture", nullptr), 60, E_FAIL);
        else
            CHECK_FAILED_THROTTLED(mainTexture->Bind_ShaderResourceView(shader.get(), "g_Texture", 0), 60, E_FAIL);

        if (SUCCEEDED(model->Bind_Material(shader.get(), "g_ModelNormalTexture", meshIndex, MaterialTextureSlot::Normal)))
            modelMaterialFlags.y = 1.f;
        else
            CHECK_FAILED_THROTTLED(shader->Bind_SRV("g_ModelNormalTexture", nullptr), 60, E_FAIL);

        if (SUCCEEDED(model->Bind_Material(shader.get(), "g_ModelEmissiveTexture", meshIndex, MaterialTextureSlot::Emissive)))
            modelMaterialFlags.z = 1.f;
        else
            CHECK_FAILED_THROTTLED(shader->Bind_SRV("g_ModelEmissiveTexture", nullptr), 60, E_FAIL);

        if (supportsModelOrm)
        {
            if (SUCCEEDED(model->Bind_Material(shader.get(), "g_ModelOrmTexture", meshIndex, MaterialTextureSlot::ORM)))
                modelMaterialFlags.w = 1.f;
            else
                CHECK_FAILED_THROTTLED(shader->Bind_SRV("g_ModelOrmTexture", nullptr), 60, E_FAIL);
        }

        CHECK_FAILED_THROTTLED(shader->Bind_RawValue("g_EffectMeshModelMaterialFlags", &modelMaterialFlags, sizeof(modelMaterialFlags)), 60, E_FAIL);
        return S_OK;
    }

    HRESULT Render_CheckerBackground(float previewCycleTime) const
    {
        CHECK_NULL(checkerShader, E_FAIL);
        CHECK_NULL(checkerQuad.vertexBuffer, E_FAIL);
        CHECK_NULL(checkerQuad.indexBuffer, E_FAIL);

        const Vec4 checkerParams(
            static_cast<float>(max(1u, width)),
            static_cast<float>(max(1u, height)),
            previewCycleTime,
            0.f
        );

        CHECK_FAILED(
            checkerShader->Bind_RawValue("g_EffectMaterialPreviewCheckerParams", &checkerParams, sizeof(checkerParams)),
            E_FAIL
        );
        CHECK_FAILED(checkerShader->Begin(4), E_FAIL);
        CHECK_FAILED(Render_Mesh(checkerQuad), E_FAIL);
        return S_OK;
    }

    HRESULT Render_Mesh(const PreviewMeshResource& mesh) const
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

    uint32 Resolve_ShaderPassIndex(const EffectMaterialInstanceData& material) const
    {
        if (Is_MeshDistortionPreviewFamily(material))
            return 0u;
        if (Is_MeshGlassPreviewFamily(material))
            return 0u;

        const bool additive = material.blendMode == EffectMaterialBlendMode::Additive;
        const bool masked = material.blendMode == EffectMaterialBlendMode::Masked;
        const bool twoSided = material.twoSided;
        if (additive)
            return twoSided ? 3u : 1u;
        if (masked)
            return twoSided ? 5u : 4u;
        return twoSided ? 2u : 0u;
    }

    HRESULT Draw_Model(const Vec3& previewScale, const EffectMaterialInstanceData& material, float previewCycleTime, bool useModelMaterials)
    {
        CHECK_NULL(shader, E_FAIL);
        CHECK_NULL(model, E_FAIL);

        const float aspect = static_cast<float>(width) / static_cast<float>(max(1u, height));
        const EffectPreviewOrbitCameraFrame cameraFrame = EffectPreviewOrbitCamera::Build_Frame(
            cameraState,
            aspect,
            XMConvertToRadians(35.f),
            0.1f,
            100.f
        );
        const Matrix world = Matrix::CreateScale(
            max(0.0001f, previewScale.x),
            max(0.0001f, previewScale.y),
            max(0.0001f, previewScale.z)
        );

        CameraCB cameraCB{};
        cameraCB.viewMatrix = cameraFrame.view;
        cameraCB.projMatrix = cameraFrame.proj;
        cameraCB.viewInverseMatrix = XMMatrixInverse(nullptr, cameraFrame.view);
        cameraCB.projInverseMatrix = XMMatrixInverse(nullptr, cameraFrame.proj);
        cameraCB.cameraPosition = Vec4(cameraFrame.eye.x, cameraFrame.eye.y, cameraFrame.eye.z, 1.f);
        cameraCB.farPlane = 100.f;

        CHECK_FAILED(shader->Bind_ObjectCB(world), E_FAIL);
        CHECK_FAILED(shader->Bind_CBufferData(cameraCB), E_FAIL);
        if (Is_MeshDistortionPreviewFamily(material))
            CHECK_FAILED(Bind_DistortionPreviewResources(material, previewCycleTime), E_FAIL);
        else
            CHECK_FAILED(Bind_EffectMaterialResources(material, previewCycleTime), E_FAIL);

        const uint32 passIndex = Resolve_ShaderPassIndex(material);
        const uint32 numMeshes = static_cast<uint32>(model->Get_NumMeshes());
        for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
        {
            if (!Is_MeshDistortionPreviewFamily(material))
                CHECK_FAILED_THROTTLED(Bind_ModelMaterialResourcesForMesh(meshIndex, material, useModelMaterials), 60, E_FAIL);

            CHECK_FAILED_THROTTLED(shader->Begin(passIndex), 60, E_FAIL);
            CHECK_FAILED_THROTTLED(model->Render(meshIndex), 60, E_FAIL);
        }

        return S_OK;
    }

    void Resize_RenderTarget(uint32 requestedWidth, uint32 requestedHeight)
    {
        requestedWidth = clamp(requestedWidth, 128u, 2048u);
        requestedHeight = clamp(requestedHeight, 128u, 2048u);
        if (width == requestedWidth && height == requestedHeight)
            return;

        width = requestedWidth;
        height = requestedHeight;
        if (sceneResolve != nullptr)
            sceneResolve->Resize(width, height);
        checkerSourceTarget.reset();
        hasRendered = false;
    }

    void Handle_ViewportInput(bool isHovered, const ImVec2& canvasMin, const ImVec2& canvasMax)
    {
        const ImGuiIO& io = ImGui::GetIO();
        Update_HomeTransition(max(io.DeltaTime, 0.f));

        if (isHovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
            Start_HomeTransition();

        const bool dragInputHeld =
            ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Right);

        if (isViewportDragging && !dragInputHeld)
            End_ViewportDrag();

        const bool shouldBeginDrag =
            isHovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Right));

        if (shouldBeginDrag)
        {
            Begin_ViewportDrag(canvasMin, canvasMax);
            hasPendingHome = false;
            return;
        }

        if (isViewportDragging)
        {
            const Vec2 mouseDelta = GAME->Get_MouseDelta();
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                EffectPreviewOrbitCamera::Orbit(cameraState, mouseDelta, kMeshPreviewOrbitSensor);
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                const EffectPreviewOrbitCameraFrame cameraFrame = EffectPreviewOrbitCamera::Build_Frame(
                    cameraState,
                    static_cast<float>(width) / static_cast<float>(max(1u, height)),
                    XMConvertToRadians(35.f),
                    0.1f,
                    100.f
                );
                const float panScale = max(0.01f, cameraState.distance * 0.003f);
                EffectPreviewOrbitCamera::Pan(cameraState, mouseDelta, cameraFrame.right, Vec3::Up, panScale);
            }
        }

        if (isHovered && io.KeyCtrl && io.MouseWheel != 0.f)
        {
            hasPendingHome = false;
            EffectPreviewOrbitCamera::Dolly(cameraState, io.MouseWheel, 0.46f, 0.25f, 80.f);
        }
    }

    void Begin_ViewportDrag(const ImVec2& canvasMin, const ImVec2& canvasMax)
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
        isViewportDragging = true;
    }

    void End_ViewportDrag()
    {
        if (!isViewportDragging || GAME == nullptr)
            return;

        isViewportDragging = false;
        GAME->Clear_MouseLockOverrideRect();
        GAME->End_EditorMouseLockSession();
        GAME->Unlock_Mouse();
    }

    void Start_HomeTransition()
    {
        hasPendingHome = true;
    }

    void Update_HomeTransition(float timeDelta)
    {
        if (!hasPendingHome)
            return;

        EffectPreviewOrbitCameraState target{};
        Build_HomeState(target);
        cameraState.pivot = Math::Lerp_Damp(cameraState.pivot, target.pivot, kMeshPreviewHomeTransitionSpeed, timeDelta);
        cameraState.distance = Math::Lerp_Damp(cameraState.distance, target.distance, kMeshPreviewHomeTransitionSpeed, timeDelta);
        cameraState.yawDegrees = Math::Lerp_Damp(cameraState.yawDegrees, target.yawDegrees, kMeshPreviewHomeTransitionSpeed, timeDelta);
        cameraState.pitchDegrees = Math::Lerp_Damp(cameraState.pitchDegrees, target.pitchDegrees, kMeshPreviewHomeTransitionSpeed, timeDelta);

        if ((cameraState.pivot - target.pivot).LengthSquared() <= 0.001f &&
            fabsf(cameraState.distance - target.distance) <= 0.01f &&
            fabsf(cameraState.yawDegrees - target.yawDegrees) <= 0.01f &&
            fabsf(cameraState.pitchDegrees - target.pitchDegrees) <= 0.01f)
        {
            cameraState = target;
            hasPendingHome = false;
        }
    }

    void Build_HomeState(EffectPreviewOrbitCameraState& outState) const
    {
        Vec3 pivot = Vec3::Zero;
        float distance = kMeshPreviewHomeDistance;
        if (model != nullptr)
        {
            Vec3 localMin{}, localMax{};
            if (model->Try_Get_LocalBounds(localMin, localMax))
            {
                const Vec3 center = (localMin + localMax) * 0.5f * 0.01f;
                const Vec3 extents = (localMax - localMin) * 0.5f * 0.01f;
                pivot = center;
                distance = max(3.f, extents.Length() * 3.f);
            }
        }

        EffectPreviewOrbitCamera::Reset(
            outState,
            pivot,
            distance,
            kMeshPreviewHomeYawDegrees,
            kMeshPreviewHomePitchDegrees
        );
    }

    void Reset_View()
    {
        EffectPreviewOrbitCamera::Reset(
            cameraState,
            Vec3::Zero,
            kMeshPreviewHomeDistance,
            kMeshPreviewHomeYawDegrees,
            kMeshPreviewHomePitchDegrees
        );
    }

    void Reset_ViewToModel()
    {
        Build_HomeState(cameraState);
    }

    void Draw_ViewportTextOverlay(const ImVec2& canvasMin, const ImVec2& canvasMax, const char* label)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool hasLabel = label != nullptr && label[0] != '\0';
        if (hasLabel)
            drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f), IM_COL32(214, 220, 230, 255), label);
        if (!statusMessage.empty())
            drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + (hasLabel ? 34.f : 12.f)), IM_COL32(245, 198, 120, 255), statusMessage.c_str());
        drawList->AddText(
            ImVec2(canvasMin.x + 12.f, canvasMax.y - 24.f),
            IM_COL32(180, 190, 205, 255),
            "LMB orbit / MMB or RMB pan / Ctrl+Wheel zoom / F reset"
        );
    }
};

MeshDataPreview_View::MeshDataPreview_View()
    : Editor_Window{ kWindowName, ICON_FA_CUBES }
    , _previewRenderer{ make_unique<PreviewRenderer>() }
{
}

MeshDataPreview_View::~MeshDataPreview_View()
{
    Free();
}

void MeshDataPreview_View::Update(float timeDelta)
{
    __super::Update(timeDelta);
    _timeDelta = max(0.f, timeDelta);
}

void MeshDataPreview_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    ImGui::SetNextWindowSize(ImVec2{ 1180.f, 760.f }, ImGuiCond_FirstUseEver);
    Apply_PendingFocusBeforeBegin();

    if (ImGui::Begin(Get_ImGuiWindowName().c_str(), &isOpen))
    {
        Clear_PendingFocusAfterBegin();
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        const Shared<Emitter_View> emitterView = Resolve_EmitterView();
        AuthoringEmitter* emitter = Find_TargetEmitter(emitterView);
        MeshTypeData* meshData = Find_TargetMeshData(emitter);
        if (emitterView == nullptr || emitter == nullptr || meshData == nullptr)
            ImGui::TextDisabled("Mesh TypeData selection is not available.");
        else
        {
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            const bool useSideBySideLayout = contentWidth >= kSideBySideLayoutMinWidth;
            const fs::path modelPath = Resolve_AuthoredAssetPath(meshData->modelGuid, meshData->modelPath);
            const string previewLabel = Build_EmitterPreviewLabel(
                emitterView->Get_Emitters(),
                *emitter,
                "Mesh Preview"
            );

            if (useSideBySideLayout)
            {
                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const float previewPaneWidth = clamp(
                    contentWidth * kPreviewPaneWidthRatio,
                    kPreviewPaneMinWidth,
                    kPreviewPaneMaxWidth
                );
                ImGui::BeginChild("MeshDataPreviewViewportPane", ImVec2(previewPaneWidth, 0.f), false);
                {
                    if (_previewRenderer != nullptr)
                        _previewRenderer->Draw_Viewport("MeshDataPreviewViewport", *emitter, *meshData, modelPath, previewLabel.c_str(), true);
                }
                ImGui::EndChild();

                ImGui::SameLine(0.f, spacing);
                ImGui::BeginChild("MeshDataPreviewInspectorPane", ImVec2(0.f, 0.f), false);
                {
                    Render_Inspector(*emitterView, *emitter, *meshData);
                }
                ImGui::EndChild();
            }
            else
            {
                if (_previewRenderer != nullptr)
                    _previewRenderer->Draw_Viewport("MeshDataPreviewViewport", *emitter, *meshData, modelPath, previewLabel.c_str());
                ImGui::Separator();
                Render_Inspector(*emitterView, *emitter, *meshData);
            }
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void MeshDataPreview_View::Open_Target(uint32 emitterId)
{
    _targetEmitterId = emitterId;
    Set_Open(true);
    Request_FocusOnOpen();
}

Shared<Emitter_View> MeshDataPreview_View::Resolve_EmitterView() const
{
    if (EDITOR == nullptr)
        return nullptr;

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    return dynamic_pointer_cast<Emitter_View>(emitterWindow);
}

AuthoringEmitter* MeshDataPreview_View::Find_TargetEmitter(const Shared<Emitter_View>& emitterView) const
{
    return emitterView != nullptr ? emitterView->Find_Emitter(_targetEmitterId) : nullptr;
}

MeshTypeData* MeshDataPreview_View::Find_TargetMeshData(AuthoringEmitter* emitter) const
{
    if (emitter == nullptr || emitter->typeData.kind != AuthoringTypeDataKind::Mesh)
        return nullptr;

    return get_if<MeshTypeData>(&emitter->typeData.payload);
}

fs::path MeshDataPreview_View::Resolve_AuthoredAssetPath(const string& guid, const string& path) const
{
    return Resolve_AssetPath(guid, path);
}

void MeshDataPreview_View::Render_Inspector(Emitter_View& emitterView, AuthoringEmitter& emitter, MeshTypeData& meshData)
{
    bool changed = false;
    bool restartPreview = false;
    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView.Capture_AuthoringSnapshot();

    ImGui::TextUnformatted("MeshData");
    ImGui::TextDisabled("Model asset reference and embedded material copy are stored in this MeshData.");
    ImGui::Separator();

    changed |= Render_ModelAssetSection(meshData);
    changed |= Render_MaterialAssetSection(meshData);
    changed |= Render_TransformSection(meshData);
    changed |= Render_MaterialInstanceSection(emitter, meshData, &restartPreview);

    if (ImGui::CollapsingHeader("Validation", ImGuiTreeNodeFlags_DefaultOpen))
        Render_ValidationSummary(meshData);

    if (changed)
    {
        Apply_MeshDataChanged(emitterView, emitter);
        Begin_PendingAuthoringEdit(emitterView, beforeSnapshot, "Edit MeshData");

        if (restartPreview && EDITOR != nullptr)
            EDITOR->Request_RestartPreview();
    }

    Commit_PendingAuthoringEditIfIdle(emitterView);
}

bool MeshDataPreview_View::Render_ModelAssetSection(MeshTypeData& meshData)
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Mesh Asset", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (_detailPropertyContext.Begin_PropertyTable("MeshDataPreviewModelAsset"))
        {
            _detailPropertyContext.Draw_PropertyLabel(
                "Model Asset",
                "이 MeshData가 참조하는 model asset 정본입니다. preview와 runtime mesh emitter가 이 모델을 기준으로 그립니다."
            );
            changed |= Draw_AssetDropSlot(
                "MeshModelSlot",
                "Model Asset",
                kEffectModelPayloadType,
                meshData.modelPath,
                "(drop .model here)",
                "MeshData source model",
                [&meshData](const wchar_t* payloadPath)
                {
                    return Apply_ModelFileToMeshData(payloadPath, meshData);
                },
                [&meshData]
                {
                    wstring pickedFilePath{};
                    if (!Try_PickAssetDialog(
                        L"Select Mesh Model",
                        L"Model Assets",
                        L"*.model",
                        kModelDialogDefaultFolder,
                        pickedFilePath
                    ))
                        return false;

                    return Apply_ModelFileToMeshData(pickedFilePath.c_str(), meshData);
                }
            );
            _detailPropertyContext.Draw_ResetButton(false);
            _detailPropertyContext.End_PropertyTable();
        }
    }
    return changed;
}

bool MeshDataPreview_View::Render_MaterialAssetSection(MeshTypeData& meshData)
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Assigned Effect Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (_detailPropertyContext.Begin_PropertyTable("MeshDataPreviewMaterialAsset"))
        {
            _detailPropertyContext.Draw_PropertyLabel(
                "Source Material",
                "Embedded material copy를 만들 때 읽어오는 .effectmaterial 원본 preset입니다. 이후 세부 편집은 MeshData 내부 copy에 저장됩니다."
            );
            changed |= Draw_AssetDropSlot(
                "MeshAssignedMaterialSlot",
                "Source Material",
                kEffectMaterialPayloadType,
                meshData.assignedEffectMaterialPath,
                "(drop .effectmaterial.json here)",
                "Material source preset",
                [&meshData](const wchar_t* payloadPath)
                {
                    return Apply_EffectMaterialFileToMeshData(payloadPath, meshData);
                },
                [&meshData]
                {
                    wstring pickedFilePath{};
                    if (!Try_PickAssetDialog(
                        L"Select Assigned Effect Material",
                        L"Effect Materials",
                        L"*.effectmaterial.json",
                        kMaterialDialogDefaultFolder,
                        pickedFilePath
                    ))
                        return false;

                    return Apply_EffectMaterialFileToMeshData(pickedFilePath.c_str(), meshData);
                }
            );
            _detailPropertyContext.Draw_ResetButton(false);
            _detailPropertyContext.End_PropertyTable();
        }

        ImGui::TextDisabled("Embedded material copy: emitter-local material instance stored inside this MeshData.");
        if (ImGui::Button("Reset Copy From Source"))
        {
            const fs::path materialPath = Resolve_AuthoredAssetPath(
                meshData.assignedEffectMaterialGuid,
                meshData.assignedEffectMaterialPath
            );
            changed = !materialPath.empty() && EffectMaterialPresetReader::Read(materialPath, meshData.assignedMaterial);
            if (changed)
                meshData.hasAssignedMaterialInstance = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Source Material 파일을 다시 읽어 MeshData 내부 material copy를 덮어씁니다. 기존 embedded 편집값은 사라집니다.");

        if (!meshData.hasAssignedMaterialInstance)
            ImGui::TextDisabled("No embedded material copy yet. Assign or reset from source to edit values.");
    }
    return changed;
}

bool MeshDataPreview_View::Render_TransformSection(MeshTypeData& meshData)
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Mesh Preview Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const MeshTypeData defaultData{};
        if (_detailPropertyContext.Begin_PropertyTable("MeshDataPreviewTransform"))
        {
            changed |= _detailPropertyContext.Draw_Vec3Property(
                "Preview Scale",
                meshData.previewScale,
                defaultData.previewScale,
                0.01f,
                nullptr,
                "MeshData Preview 표시와 mesh emitter 기준 크기에 쓰는 저장 authoring 값입니다. 모델 파일 자체의 transform을 수정하지 않습니다."
            );
            changed |= _detailPropertyContext.Draw_BoolProperty(
                "Use Model Materials",
                meshData.useModelMaterials,
                defaultData.useModelMaterials,
                "Debug fallback입니다. 켜면 model 원본 material을 우선 확인하고, 기본 authoring은 MeshData material copy를 slot override로 사용합니다."
            );
            _detailPropertyContext.End_PropertyTable();
        }
    }
    return changed;
}

bool MeshDataPreview_View::Render_MaterialInstanceSection(const AuthoringEmitter& emitter, MeshTypeData& meshData, bool* outRestartPreview)
{
    if (!meshData.hasAssignedMaterialInstance)
    {
        ImGui::TextDisabled("Assign an effect material source before editing the MeshData material instance.");
        return false;
    }

    ImGui::TextDisabled("Embedded material copy is emitter-local MeshData authoring data.");

    const RequiredModuleData* required = Find_RequiredModuleData(emitter);
    const EffectMaterialRenderPolicyBinding renderPolicyBinding{
        required != nullptr ? required->material.blendMode : meshData.assignedMaterial.blendMode,
        required != nullptr ? required->material.alphaCutoff : meshData.assignedMaterial.alphaCutoff,
        required != nullptr ? "Required" : "Assigned Material"
    };

    return EffectMaterialInstance_View::Draw_InstanceProperties(
        _detailPropertyContext,
        _textureThumbnailCache,
        meshData.assignedMaterial,
        outRestartPreview,
        EffectMaterialTwoSidedDisplayMode::EditableImplemented,
        EffectMaterialDistortionShapeControlMode::MeshUnsupported,
        &renderPolicyBinding,
        meshData.useModelMaterials
        ? "Use Model Materials가 켜져 있어 모델의 BaseColor/Normal/Emissive 슬롯을 사용합니다.\n끄면 이펙트 머티리얼의 Main Texture를 직접 지정할 수 있습니다."
        : nullptr
    );
}

void MeshDataPreview_View::Render_ValidationSummary(const MeshTypeData& meshData)
{
    const fs::path modelPath = Resolve_AuthoredAssetPath(meshData.modelGuid, meshData.modelPath);

    if (meshData.modelGuid.empty() && meshData.modelPath.empty())
        ImGui::TextColored(ImVec4{ 1.f, 0.55f, 0.25f, 1.f }, "Model missing: no model asset is assigned.");
    else if (!fs::exists(modelPath))
        ImGui::TextColored(ImVec4{ 1.f, 0.55f, 0.25f, 1.f }, "Model missing: %s", To_DisplayPath(modelPath).c_str());
    else if (String::ToLowerCopy(modelPath.extension().string()) != ".model")
        ImGui::TextColored(ImVec4{ 1.f, 0.35f, 0.35f, 1.f }, "Unsupported model extension.");
    else
        ImGui::TextColored(ImVec4{ 0.45f, 0.9f, 0.55f, 1.f }, "Model asset is valid for static preview and MeshData authoring.");

    const fs::path materialPath = Resolve_AuthoredAssetPath(
        meshData.assignedEffectMaterialGuid,
        meshData.assignedEffectMaterialPath
    );
    if (meshData.assignedEffectMaterialGuid.empty() && meshData.assignedEffectMaterialPath.empty())
        ImGui::TextDisabled("Material missing: assigned effect material source is not set.");
    else if (!fs::exists(materialPath))
        ImGui::TextColored(ImVec4{ 1.f, 0.55f, 0.25f, 1.f }, "Material missing: %s", To_DisplayPath(materialPath).c_str());
    else
        ImGui::TextColored(ImVec4{ 0.45f, 0.9f, 0.55f, 1.f }, "Assigned effect material source is readable for copy/reset.");

    if (meshData.hasAssignedMaterialInstance)
        ImGui::TextColored(ImVec4{ 0.45f, 0.9f, 0.55f, 1.f }, "MeshData material instance copy is editable.");
    else
        ImGui::TextDisabled("MeshData material instance copy is not initialized.");
}

void MeshDataPreview_View::Apply_MeshDataChanged(Emitter_View& emitterView, AuthoringEmitter& emitter)
{
    emitter.previewDirty = true;
    emitter.rendererType = Authoring::Resolve_RendererType(emitter.typeData);
    if (const MeshTypeData* meshData = Find_TargetMeshData(&emitter))
    {
        emitter.textureId = meshData->hasAssignedMaterialInstance ? meshData->assignedMaterial.mainTexturePath : meshData->assignedEffectMaterialPath;
        emitter.resourceSummary = meshData->modelPath;
    }

    emitterView.MarkDirty();
    MarkDirty();
}

void MeshDataPreview_View::Begin_PendingAuthoringEdit(
    Emitter_View&,
    const Emitter_View::AuthoringSnapshot& beforeSnapshot,
    const string& description)
{
    if (_hasPendingAuthoringEdit)
        return;

    _hasPendingAuthoringEdit = true;
    _pendingAuthoringSnapshot = beforeSnapshot;
    _pendingAuthoringDescription = description;
}

void MeshDataPreview_View::Commit_PendingAuthoringEditIfIdle(Emitter_View& emitterView)
{
    if (!_hasPendingAuthoringEdit || ImGui::IsAnyItemActive())
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void MeshDataPreview_View::Commit_PendingAuthoringEdit(Emitter_View& emitterView)
{
    if (!_hasPendingAuthoringEdit)
        return;

    Emitter_View::AuthoringSnapshot afterSnapshot = emitterView.Capture_AuthoringSnapshot();
    afterSnapshot.selectedEmitterIndex = _pendingAuthoringSnapshot.selectedEmitterIndex;
    afterSnapshot.selectedTypeData = _pendingAuthoringSnapshot.selectedTypeData;
    afterSnapshot.selectedModuleIndex = _pendingAuthoringSnapshot.selectedModuleIndex;
    emitterView.Execute_AuthoringSnapshotCommand(
        _pendingAuthoringSnapshot,
        afterSnapshot,
        _pendingAuthoringDescription
    );

    _hasPendingAuthoringEdit = false;
    _pendingAuthoringSnapshot = {};
    _pendingAuthoringDescription.clear();
}

Shared<MeshDataPreview_View> MeshDataPreview_View::Create()
{
    return make_shared<MeshDataPreview_View>();
}

void MeshDataPreview_View::Free()
{
    _previewRenderer.reset();
    _textureThumbnailCache.clear();
    __super::Free();
}

NS_END
