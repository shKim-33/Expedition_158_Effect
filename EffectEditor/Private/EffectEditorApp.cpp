#include "EffectEditorApp.h"

#include "ClientInstance.h"
#include "ComputeGeneratedBeamEmitter.h"
#include "ComputeSourceHistoryRibbonEmitter.h"
#include "ComputeSourceHistorySpriteTrailEmitter.h"
#include "Content_Browser.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Level_EffectEditor.h"
#include "PostProcessPipeline.h"
#include "Profile_View.h"
#include "ResourceLoader.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr long long kStartupResourceInfoThresholdMs = 200;

    bool EndsWith(const string& value, const string& suffix)
    {
        if (value.length() < suffix.length())
            return false;

        return equal(suffix.rbegin(), suffix.rend(), value.rbegin());
    }

    wstring Build_MetaPath(const wstring& assetPath)
    {
        return assetPath + L".meta";
    }

    bool Is_PathUnderRoot(const fs::path& path, const fs::path& root)
    {
        try
        {
            if (root.empty())
                return true;

            const fs::path normalizedPath = fs::absolute(path).lexically_normal();
            const fs::path normalizedRoot = fs::absolute(root).lexically_normal();
            const fs::path relativePath = fs::relative(normalizedPath, normalizedRoot);

            if (relativePath.is_absolute())
                return false;

            if (relativePath.empty())
                return true;

            const auto firstPart = relativePath.begin();
            if (firstPart == relativePath.end())
                return true;

            return *firstPart != L"..";
        }
        catch (...)
        {
            return false;
        }
    }

    bool Is_IgnoredAssetPath(const fs::path& path, const fs::path& resourceRoot)
    {
        static const vector<string> ignoreFolders{ ".git", ".vs", "Fonts", "Data/Assets" };

        const fs::path normalizedPath = path.lexically_normal();
        const fs::path normalizedRoot = resourceRoot.empty() ? fs::path() : resourceRoot.lexically_normal();

        for (const string& ignoreFolder : ignoreFolders)
        {
            const string lowerIgnore = String::ToLowerCopy(ignoreFolder);
            if (lowerIgnore.find('/') == string::npos && lowerIgnore.find('\\') == string::npos)
            {
                for (const fs::path& part : normalizedPath)
                {
                    if (String::ToLowerCopy(String::ToString(part.wstring())) == lowerIgnore)
                        return true;
                }

                continue;
            }

            if (normalizedRoot.empty())
                continue;

            string relativePath = {};
            try
            {
                relativePath = String::ToLowerCopy(fs::relative(normalizedPath, normalizedRoot).generic_string());
            }
            catch (...)
            {
                continue;
            }

            if (relativePath == lowerIgnore || 0 == relativePath.find(lowerIgnore + "/"))
                return true;
        }

        return false;
    }

    bool Should_RegisterJsonAsset(const fs::path& path, const fs::path& resourceRoot)
    {
        if (resourceRoot.empty())
            return false;

        string relativePath = {};
        try
        {
            relativePath = String::ToLowerCopy(fs::relative(path, resourceRoot).generic_string());
        }
        catch (...)
        {
            return false;
        }

        return 0 == relativePath.find("ui/ui_json/") ||
               0 == relativePath.find("data/json/animnotifies/") ||
               0 == relativePath.find("data/json/bt/") ||
               0 == relativePath.find("data/json/camerapreset/") ||
               0 == relativePath.find("data/json/cinematics/") ||
               0 == relativePath.find("data/json/prefabs/") ||
               0 == relativePath.find("materials/") ||
               0 == relativePath.find("effects/assets/") ||
               0 == relativePath.find("effects/materials/");
    }

    bool Should_RegisterAsset(const fs::path& path, const fs::path& resourceRoot)
    {
        const string name = String::ToLowerCopy(String::ToString(path.filename().wstring()));
        const string extension = String::ToLowerCopy(String::ToString(path.extension().wstring()));

        if (Is_IgnoredAssetPath(path, resourceRoot))
            return false;

        if (EndsWith(name, ".meta") ||
            name == "assetregistry.json" ||
            name == "assetregistry.cache.json" ||
            extension == ".xlsx" ||
            extension == ".csv")
            return false;

        if (extension == ".json")
            return Should_RegisterJsonAsset(path, resourceRoot);

        return extension == ".model"
               || extension == ".meshbin"
               || extension == ".anim"
               || extension == ".clip"
               || extension == ".psa"
               || extension == ".dds"
               || extension == ".png"
               || extension == ".jpg"
               || extension == ".jpeg"
               || extension == ".tga"
               || extension == ".hlsl"
               || extension == ".fx"
               || extension == ".wav"
               || extension == ".mp3"
               || extension == ".ogg"
               || extension == ".spritefont"
               || extension == ".ttf"
               || extension == ".otf";
    }

    string Detect_AssetType(const fs::path& filePath, const fs::path& resourceRoot)
    {
        const string extension = String::ToLowerCopy(String::ToString(filePath.extension().wstring()));

        if (extension == ".json")
        {
            string relativePath = String::ToLowerCopy(filePath.generic_string());
            try
            {
                if (!resourceRoot.empty())
                    relativePath = String::ToLowerCopy(fs::relative(filePath, resourceRoot).generic_string());
            }
            catch (...)
            {
            }

            if (0 == relativePath.find("ui/ui_json/"))
                return "UIJson";
            if (0 == relativePath.find("data/json/animnotifies/"))
                return "AnimNotify";
            if (0 == relativePath.find("data/json/bt/"))
                return "BehaviorTree";
            if (0 == relativePath.find("data/json/camerapreset/"))
                return "CameraPreset";
            if (0 == relativePath.find("data/json/cinematics/"))
                return "Cinematic";
            if (0 == relativePath.find("data/json/prefabs/"))
                return "Prefab";
            if (0 == relativePath.find("materials/"))
                return "Material";
            if (0 == relativePath.find("effects/assets/"))
                return "Effect";
            if (0 == relativePath.find("effects/materials/"))
                return "EffectMaterial";

            return "Json";
        }

        if (extension == ".model" || extension == ".meshbin")
            return "Model";
        if (extension == ".anim" || extension == ".clip" || extension == ".psa")
            return "Animation";
        if (extension == ".dds" || extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga")
            return "Texture";
        if (extension == ".hlsl" || extension == ".fx")
            return "Shader";
        if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
            return "Sound";
        if (extension == ".spritefont" || extension == ".ttf" || extension == ".otf")
            return "Font";

        return "Unknown";
    }

    bool Try_ReadMetaFile(const fs::path& metaPath, const fs::path& resourceRoot, AssetMeta& outAssetMeta)
    {
        try
        {
            ifstream file(metaPath);
            if (!file.is_open())
                return false;

            json root = {};
            file >> root;
            if (!root.contains("guid"))
                return false;

            const wstring metaPathString = metaPath.wstring();
            if (!EndsWith(String::ToLowerCopy(String::ToString(metaPath.filename().wstring())), ".meta"))
                return false;

            const wstring assetPathString = metaPathString.substr(0, metaPathString.length() - wcslen(L".meta"));
            const fs::path assetPath = fs::absolute(fs::path(assetPathString)).lexically_normal();
            if (!fs::exists(assetPath) || !fs::is_regular_file(assetPath))
                return false;

            outAssetMeta = {};
            outAssetMeta.guid = root.value("guid", "");
            outAssetMeta.type = root.value("type", Detect_AssetType(assetPath, resourceRoot));
            outAssetMeta.modelType = root.value("modelType", "");
            outAssetMeta.fullPath = assetPath.wstring();
            if (!resourceRoot.empty())
                outAssetMeta.relativePath = fs::relative(assetPath, resourceRoot).wstring();

            return !outAssetMeta.guid.empty();
        }
        catch (...)
        {
            return false;
        }
    }

    AssetDeltaScanResult Scan_EffectsAssetDelta(const fs::path& resourceRoot, const fs::path& scanRoot)
    {
        AssetDeltaScanResult result{};
        result.scanRoot = scanRoot.wstring();

        try
        {
            if (!fs::exists(scanRoot) || !fs::is_directory(scanRoot))
                return result;

            fs::recursive_directory_iterator iter(scanRoot);
            const fs::recursive_directory_iterator end = {};

            for (; iter != end; ++iter)
            {
                const fs::directory_entry& entry = *iter;
                if (entry.is_directory() && Is_IgnoredAssetPath(entry.path(), resourceRoot))
                {
                    iter.disable_recursion_pending();
                    continue;
                }

                if (!entry.is_regular_file())
                    continue;

                const fs::path path = fs::absolute(entry.path()).lexically_normal();
                const string fileName = String::ToLowerCopy(String::ToString(path.filename().wstring()));
                if (EndsWith(fileName, ".meta"))
                {
                    const wstring pathString = path.wstring();
                    const fs::path assetPath = fs::path(pathString.substr(0, pathString.length() - wcslen(L".meta")));
                    if (!fs::exists(assetPath))
                        result.staleAssetPaths.push_back(assetPath.wstring());
                    continue;
                }

                if (!Should_RegisterAsset(path, resourceRoot))
                    continue;

                result.discoveredAssetPaths.push_back(path.wstring());

                const fs::path metaPath = Build_MetaPath(path.wstring());
                if (fs::exists(metaPath))
                {
                    AssetMeta assetMeta{};
                    if (Try_ReadMetaFile(metaPath, resourceRoot, assetMeta))
                        result.existingMetaAssets.push_back(assetMeta);
                }
                else
                    result.missingMetaAssetPaths.push_back(path.wstring());
            }

            result.scanSucceeded = true;
        }
        catch (...)
        {
        }

        return result;
    }

    bool Is_EffectEditorStaticLevelAllowed(const wstring& prototypeTag)
    {
        return prototypeTag == L"VIBuffer_Cube" || prototypeTag == L"VIBuffer_Rect";
    }

    bool Is_EffectEditorPreviewTextureAllowed(const wstring& prototypeTag)
    {
        return
            prototypeTag == L"Texture_Effect_EditorSkybox" ||
            prototypeTag == L"Texture_Effect_BlackSkybox" ||
            prototypeTag == L"Texture_Effect_DefaultTexture" ||
            prototypeTag == L"Texture_Effect_Clouds4";
    }

    bool Is_EffectEditorPreviewShaderAllowed(const wstring& prototypeTag)
    {
        return
            prototypeTag == L"Shader_VtxCube" ||
            prototypeTag == L"Shader_VtxMesh" ||
            prototypeTag == L"Shader_VtxAnimMesh" ||
            prototypeTag == L"Shader_EffectMaterialPreview" ||
            prototypeTag == L"Shader_EffectMaterialPreviewDistortion" ||
            prototypeTag == L"Shader_EffectSprite" ||
            prototypeTag == L"Shader_EffectDistortionSprite" ||
            prototypeTag == L"Shader_EffectTrail" ||
            prototypeTag == L"Shader_EffectTrailDistortion" ||
            prototypeTag == L"Shader_EffectRibbon" ||
            prototypeTag == L"Shader_EffectRibbonDistortion" ||
            prototypeTag == L"Shader_EffectSourceHistorySpriteTrail" ||
            prototypeTag == L"Shader_EffectBeam" ||
            prototypeTag == L"Shader_EffectBeamDistortion" ||
            prototypeTag == L"Shader_VtxMeshInstanceExact" ||
            prototypeTag == L"Shader_EffectMesh" ||
            prototypeTag == L"Shader_EffectMeshDistortion" ||
            prototypeTag == L"Shader_EffectMeshGlass" ||
            prototypeTag == L"Shader_EffectMeshPreviewDistortion" ||
            prototypeTag == L"Shader_EffectMeshPreviewGlass" ||
            prototypeTag == L"Shader_EffectMeshPreview";
    }

    bool Is_EffectEditorPreviewComputeShaderAllowed(const wstring& prototypeTag)
    {
        return
            prototypeTag == L"ComputeShader_PointParticle" ||
            prototypeTag == L"ComputeShader_TrailStrip" ||
            prototypeTag == L"ComputeShader_RibbonStrip" ||
            prototypeTag == L"ComputeShader_BoneRemap" ||
            prototypeTag == L"ComputeShader_AnimMeshSkinning";
    }

    bool Is_EffectEditorPreviewComponentAllowed(const wstring& prototypeTag)
    {
        return
            prototypeTag == L"Com_Effect_ComputePointParticleBuffer" ||
            prototypeTag == L"Com_PlayerStat" ||
            prototypeTag == L"Com_Collider_OBB";
    }

    bool Is_EffectEditorPreviewModelAllowed(const wstring&)
    {
        return false;
    }

    bool Is_EffectEditorPreviewGameObjectAllowed(const wstring& prototypeTag)
    {
        return
            prototypeTag == L"GameObject_Body_Player" ||
            prototypeTag == L"GameObject_Head_Player" ||
            prototypeTag == L"GameObject_Hair_Player" ||
            prototypeTag == L"GameObject_EffectInstance" ||
            prototypeTag == L"GameObject_Effect_ComputeSpriteEmitter" ||
            prototypeTag == L"GameObject_Effect_ComputeTrailEmitter" ||
            prototypeTag == L"GameObject_Effect_ComputeSourceHistoryRibbonEmitter" ||
            prototypeTag == L"GameObject_Effect_ComputeSourceHistorySpriteTrailEmitter" ||
            prototypeTag == L"GameObject_Effect_ComputeGeneratedBeamEmitter" ||
            prototypeTag == L"GameObject_Effect_MeshEmitter";
    }

    bool Is_EffectEditorStaticLoadJobAllowed(const FLoadJob& loadJob)
    {
        switch (loadJob.eType)
        {
        case ELoadJobType::Shader:
            return Is_EffectEditorPreviewShaderAllowed(loadJob.strPrototypeTag);

        case ELoadJobType::ComputeShader:
            return Is_EffectEditorPreviewComputeShaderAllowed(loadJob.strPrototypeTag);

        case ELoadJobType::Component:
            return Is_EffectEditorPreviewComponentAllowed(loadJob.strPrototypeTag);

        case ELoadJobType::Model:
            return Is_EffectEditorPreviewModelAllowed(loadJob.strPrototypeTag);

        case ELoadJobType::GameObject:
            return Is_EffectEditorPreviewGameObjectAllowed(loadJob.strPrototypeTag);

        case ELoadJobType::StaticLevel:
            return Is_EffectEditorStaticLevelAllowed(loadJob.strPrototypeTag);

        case ELoadJobType::TextureCreate:
            return Is_EffectEditorPreviewTextureAllowed(loadJob.strPrototypeTag);

        default:
            return false;
        }
    }

    void Log_EffectEditorResourceFilterSummary(
        const char* scope,
        size_t totalCount,
        size_t includedCount,
        size_t excludedCount,
        size_t failedCount)
    {
        LOG_INFO(
            "[ResourceFilter] scope={}, total={}, included={}, excluded={}, failed={}",
            scope,
            static_cast<uint32>(totalCount),
            static_cast<uint32>(includedCount),
            static_cast<uint32>(excludedCount),
            static_cast<uint32>(failedCount)
        );
    }

    class PostProcessFeatureScope final
    {
    public:
        PostProcessFeatureScope()
            : _pipeline{ GAME->Get_PostProcessPipeline() }
        {
            if (_pipeline == nullptr || EDITOR == nullptr)
                return;

            _originalFeatures = _pipeline->Get_PostProcessFeatures();
            _originalParams = _pipeline->Get_PostProcessParams();
            _restore = true;

            const EffectEditorPostProcessOptions& options = EDITOR->Get_PostProcessOptions();
            _pipeline->Set_PostProcessFeatures(options.features);
            _pipeline->Set_PostProcessParams(options.params);
        }

        ~PostProcessFeatureScope()
        {
            if (!_restore || _pipeline == nullptr)
                return;

            _pipeline->Set_PostProcessFeatures(_originalFeatures);
            _pipeline->Set_PostProcessParams(_originalParams);
        }

    private:
        Shared<PostProcessPipeline> _pipeline{ nullptr };
        PostProcessFeature _originalFeatures{ PostProcessFeature::None };
        PostProcessCB _originalParams{};
        bool _restore{ false };
    };
}

EffectEditorApp::EffectEditorApp()
{
}

EffectEditorApp::~EffectEditorApp()
{
    Free();
}

HRESULT EffectEditorApp::Initialize()
{
    const fs::path assetRootPath(Get_ClientResourceRoot());
    if (!fs::exists(assetRootPath) || !fs::is_directory(assetRootPath))
        return E_FAIL;

    _assetRootPath = fs::absolute(assetRootPath).lexically_normal();
    _effectsAssetScanRoot = _assetRootPath / L"Effects";

    RECT clientRect{};
    if (!GetClientRect(g_hWnd, &clientRect))
        return E_FAIL;

    const uint32 viewportWidth = max(1L, clientRect.right - clientRect.left);
    const uint32 viewportHeight = max(1L, clientRect.bottom - clientRect.top);

    ENGINE_DESC engineDesc{};
    engineDesc.hInstance = g_hInst;
    engineDesc.hWnd = g_hWnd;
    engineDesc.winMode = WinMode::Win;
    engineDesc.numLevels = ETOI(LevelType::END);
    engineDesc.viewportWidth = viewportWidth;
    engineDesc.viewportHeight = viewportHeight;
    engineDesc.swapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    engineDesc.projectName = "EffectEditor";
    engineDesc.enableEditorRuntimeDebug = true;

    const auto startupStartTime = chrono::steady_clock::now();

    long long initializeEngineMs = 0;
    long long initializeAssetManagerMs = 0;
    long long initializeClientMs = 0;
    long long readyFontsMs = 0;
    long long readyStaticResourcesMs = 0;
    long long readyPreviewLevelMs = 0;
    long long initializeEditorMs = 0;

    const auto run_startup_step = [](long long& elapsedMs, const auto& step) -> HRESULT
    {
        const auto startTime = chrono::steady_clock::now();
        const HRESULT hr = step();
        elapsedMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startTime).count();

        return hr;
    };

    CHECK_FAILED(run_startup_step(initializeEngineMs, [&]{ return GAME->Initialize_Engine(engineDesc, _device, _context); }), E_FAIL);
    const fs::path missingMetaRootPath = assetRootPath / L"Effects";
    CHECK_FAILED(
        run_startup_step(initializeAssetManagerMs, [&]{ return GAME->Initialize_AssetManager(assetRootPath.wstring(), false, missingMetaRootPath.wstring()); }),
        E_FAIL
    );
    CHECK_FAILED(run_startup_step(initializeClientMs, [&]{ return CLIENT->Initialize_Client_ForEffectEditor(_device, _context); }), E_FAIL);
    CHECK_FAILED(run_startup_step(readyFontsMs, [&]{ return Ready_Fonts(); }), E_FAIL);
    CHECK_FAILED(run_startup_step(readyStaticResourcesMs, [&]{ return Ready_Prototype_For_Static_Level(); }), E_FAIL);
    CHECK_FAILED(run_startup_step(readyPreviewLevelMs, [&]{ return Ready_PreviewLevel(); }), E_FAIL);

    EffectEditorDesc editorDesc{};
    editorDesc.hWnd = g_hWnd;
    editorDesc.winMode = WinMode::Win;
    editorDesc.viewportWidth = viewportWidth;
    editorDesc.viewportHeight = viewportHeight;

    CHECK_FAILED(run_startup_step(initializeEditorMs, [&]{ return EDITOR->Initialize_Editor(editorDesc, _device, _context); }), E_FAIL);
    EDITOR->Request_RestartPreview();

    const auto totalStartupMs = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startupStartTime).count();
    LOG_INFO(
        "EffectEditor startup summary. total={}ms engine={}ms assetManager={}ms client={}ms fonts={}ms staticResources={}ms previewLevel={}ms editor={}ms",
        totalStartupMs,
        initializeEngineMs,
        initializeAssetManagerMs,
        initializeClientMs,
        readyFontsMs,
        readyStaticResourcesMs,
        readyPreviewLevelMs,
        initializeEditorMs
    );

    return S_OK;
}

void EffectEditorApp::Priority_Update(float)
{
}

void EffectEditorApp::Update(float timeDelta)
{
    Queue_EffectsAssetDeltaScan();
    Commit_PendingAssetDeltaScan();

    {
        ENGINE_PROFILE_SCOPE_BASIC("Editor::Update");
        EDITOR->Update_Editor(timeDelta);
    }

    float runtimeDelta;
    float freeCameraDelta;

    if (EDITOR->IsPlaying())
    {
        runtimeDelta = timeDelta * EDITOR->Get_RuntimeTimeScale();
        freeCameraDelta = 0.f;
    }
    else if (EDITOR->IsPaused())
    {
        if (EDITOR->Consume_FrameStep())
            runtimeDelta = timeDelta * EDITOR->Get_RuntimeTimeScale();
        else
            runtimeDelta = 0.f;

        freeCameraDelta = timeDelta;
    }
    else
    {
        runtimeDelta = 0.f;
        freeCameraDelta = timeDelta;
    }

    EDITOR->Tick_TrailPreviewGate(runtimeDelta);
    GAME->Set_FreeCameraTimeDelta(freeCameraDelta);
    {
        ENGINE_PROFILE_SCOPE_BASIC("Game::Update");
        GAME->Update_Engine(runtimeDelta);
    }

    EDITOR->Apply_RuntimeInputPolicy();
}

void EffectEditorApp::Late_Update(float)
{
}

void EffectEditorApp::Queue_EffectsAssetDeltaScan()
{
    if (_assetDeltaScanQueued || _assetDeltaScanCommitted)
        return;

    _assetDeltaScanQueued = true;

    if (_assetRootPath.empty() || _effectsAssetScanRoot.empty() ||
        !fs::exists(_effectsAssetScanRoot) || !fs::is_directory(_effectsAssetScanRoot))
    {
        LOG_INFO("[AssetDeltaScan] skipped missing Effects root");
        return;
    }

    const fs::path assetRootPath = _assetRootPath;
    const fs::path effectsScanRoot = _effectsAssetScanRoot;
    const bool enqueued = GAME->Enqueue_ThreadJob(
        [this, assetRootPath, effectsScanRoot]
        {
            const auto scanBegin = chrono::steady_clock::now();
            AssetDeltaScanResult scanResult = Scan_EffectsAssetDelta(assetRootPath, effectsScanRoot);
            const auto elapsedMs = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - scanBegin).count();

            LOG_INFO(
                "[AssetDeltaScan] background scan complete scanRoot={} discovered={} existingMeta={} missingMeta={} stale={} elapsed={}ms",
                String::ToString(effectsScanRoot.wstring()),
                static_cast<uint32>(scanResult.discoveredAssetPaths.size()),
                static_cast<uint32>(scanResult.existingMetaAssets.size()),
                static_cast<uint32>(scanResult.missingMetaAssetPaths.size()),
                static_cast<uint32>(scanResult.staleAssetPaths.size()),
                elapsedMs
            );

            lock_guard<mutex> lock(_assetDeltaScanMutex);
            _pendingAssetDeltaScanResult = std::move(scanResult);
        });

    if (!enqueued)
    {
        _assetDeltaScanQueued = false;
        LOG_WARN("[AssetDeltaScan] failed to enqueue Effects delta scan job");
    }
}

void EffectEditorApp::Commit_PendingAssetDeltaScan()
{
    optional<AssetDeltaScanResult> scanResult{};
    {
        lock_guard<mutex> lock(_assetDeltaScanMutex);
        if (!_pendingAssetDeltaScanResult.has_value())
            return;

        scanResult = std::move(_pendingAssetDeltaScanResult);
        _pendingAssetDeltaScanResult.reset();
    }

    if (!scanResult.has_value())
        return;

    const fs::path missingMetaRootPath = _assetRootPath / L"Effects";
    const HRESULT hr = GAME->Commit_AssetDeltaScan(scanResult.value(), missingMetaRootPath.wstring());
    if (FAILED(hr))
    {
        LOG_WARN("[AssetDeltaScan] commit failed");
        return;
    }

    _assetDeltaScanCommitted = true;
    if (hr != S_OK)
        return;

    const Shared<Editor_Window> browserWindow = EDITOR->Get_Window(L"Content Browser");
    const Shared<Content_Browser> contentBrowser = dynamic_pointer_cast<Content_Browser>(browserWindow);
    if (contentBrowser != nullptr)
        contentBrowser->Request_RefreshResources();
}

#ifdef _DEBUG
bool EffectEditorApp::Should_SampleEffectEditorRenderPerf()
{
    ++_renderPerfWindow.observedFrameCount;

    const auto now = chrono::steady_clock::now();
    if (!_hasRenderPerfLastSampleTime)
    {
        _renderPerfLastSampleTime = now;
        _hasRenderPerfLastSampleTime = true;
        return true;
    }

    const float elapsedSec = static_cast<float>(
        chrono::duration<double>(now - _renderPerfLastSampleTime).count());
    if (elapsedSec < kEffectEditorRenderPerfSampleIntervalSec)
        return false;

    _renderPerfLastSampleTime = now;
    return true;
}

void EffectEditorApp::Reset_EffectEditorRenderPerfWindowState(chrono::steady_clock::time_point now)
{
    _renderPerfWindow = {};
    _renderPerfWindowStartTime = now;
    _hasRenderPerfWindowStartTime = true;
}

void EffectEditorApp::Accumulate_EffectEditorRenderPerf(
    double totalRenderMs,
    double gameRenderMs,
    double captureMs,
    double bindBackBufferMs,
    double editorRenderMs,
    double editorWindowsMs,
    double notificationMs,
    double imguiMs,
    double presentMs)
{
    Accumulate_PassPerf(_renderPerfWindow.totalRender, totalRenderMs);
    Accumulate_PassPerf(_renderPerfWindow.gameRender, gameRenderMs);
    Accumulate_PassPerf(_renderPerfWindow.capture, captureMs);
    Accumulate_PassPerf(_renderPerfWindow.bindBackBuffer, bindBackBufferMs);
    Accumulate_PassPerf(_renderPerfWindow.editorRender, editorRenderMs);
    Accumulate_PassPerf(_renderPerfWindow.editorWindows, editorWindowsMs);
    Accumulate_PassPerf(_renderPerfWindow.notification, notificationMs);
    Accumulate_PassPerf(_renderPerfWindow.imgui, imguiMs);
    Accumulate_PassPerf(_renderPerfWindow.present, presentMs);
}

void EffectEditorApp::Try_FlushEffectEditorRenderPerfLog()
{
    const auto now = chrono::steady_clock::now();
    if (!_hasRenderPerfWindowStartTime)
    {
        Reset_EffectEditorRenderPerfWindowState(now);
        return;
    }

    const float windowSec = static_cast<float>(
        chrono::duration<double>(now - _renderPerfWindowStartTime).count());
    if (windowSec < kEffectEditorRenderPerfWindowSec)
        return;

    const size_t sampleCount = _renderPerfWindow.totalRender.sampleCount;
    if (0 == sampleCount)
    {
        Reset_EffectEditorRenderPerfWindowState(now);
        return;
    }

    GAME->Log_FileOnly(
        spdlog::source_loc{ __FILE__, __LINE__, SPDLOG_FUNCTION },
        spdlog::level::trace,
        fmt::format(
            "[EffectEditorRenderPerf] frames={} samples={} sample_interval={:.2f}s fps~{:.1f} "
            "total_render_ms(avg={:.2f} max={:.2f}) "
            "game_render_ms(avg={:.2f} max={:.2f}) "
            "capture_ms(avg={:.2f} max={:.2f}) "
            "bind_backbuffer_ms(avg={:.2f} max={:.2f}) "
            "editor_render_ms(avg={:.2f} max={:.2f}) "
            "editor_windows_ms(avg={:.2f} max={:.2f}) "
            "notification_ms(avg={:.2f} max={:.2f}) "
            "imgui_ms(avg={:.2f} max={:.2f}) "
            "present_ms(avg={:.2f} max={:.2f})",
            _renderPerfWindow.observedFrameCount,
            static_cast<uint32>(sampleCount),
            kEffectEditorRenderPerfSampleIntervalSec,
            Safe_Divide(static_cast<double>(_renderPerfWindow.observedFrameCount), windowSec),
            Safe_Divide(_renderPerfWindow.totalRender.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.totalRender.maxMs,
            Safe_Divide(_renderPerfWindow.gameRender.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.gameRender.maxMs,
            Safe_Divide(_renderPerfWindow.capture.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.capture.maxMs,
            Safe_Divide(_renderPerfWindow.bindBackBuffer.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.bindBackBuffer.maxMs,
            Safe_Divide(_renderPerfWindow.editorRender.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.editorRender.maxMs,
            Safe_Divide(_renderPerfWindow.editorWindows.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.editorWindows.maxMs,
            Safe_Divide(_renderPerfWindow.notification.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.notification.maxMs,
            Safe_Divide(_renderPerfWindow.imgui.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.imgui.maxMs,
            Safe_Divide(_renderPerfWindow.present.accumulatedMs, static_cast<double>(sampleCount)),
            _renderPerfWindow.present.maxMs
        )
    );

    Reset_EffectEditorRenderPerfWindowState(now);
}

void EffectEditorApp::Accumulate_PassPerf(PerfCounter& counter, double passMs)
{
    ++counter.sampleCount;
    counter.accumulatedMs += passMs;
    counter.maxMs = max(counter.maxMs, passMs);
}

double EffectEditorApp::Safe_Divide(double numerator, double denominator)
{
    if (denominator <= 0.0)
        return 0.0;

    return numerator / denominator;
}
#endif

HRESULT EffectEditorApp::Render()
{
#ifdef _DEBUG
    const bool sampleRenderPerf = Should_SampleEffectEditorRenderPerf();
    chrono::steady_clock::time_point totalRenderBegin{};
    chrono::steady_clock::time_point passBegin{};
    double totalRenderMs = 0.0;
    double gameRenderMs = 0.0;
    double captureMs = 0.0;
    double bindBackBufferMs = 0.0;
    double editorRenderMs = 0.0;
    double presentMs = 0.0;
    EffectEditorRenderPerfSample editorPerfSample{};

    if (sampleRenderPerf)
        totalRenderBegin = chrono::steady_clock::now();
#endif

#ifdef _DEBUG
    if (sampleRenderPerf)
        passBegin = chrono::steady_clock::now();
#endif
    {
        ENGINE_PROFILE_SCOPE_BASIC("Game::Render");

        GAME->Bind_BackBuffer();

        if (FAILED(GAME->Clear_Buffers(kDefaultClearColor)))
            return E_FAIL;

        const PostProcessFeatureScope postProcessFeatureScope{};

        if (FAILED(GAME->Draw()))
            return E_FAIL;
    }
#ifdef _DEBUG
    if (sampleRenderPerf)
        gameRenderMs = chrono::duration<double, milli>(chrono::steady_clock::now() - passBegin).count();
#endif

#ifdef _DEBUG
    if (sampleRenderPerf)
        passBegin = chrono::steady_clock::now();
#endif
    {
        ENGINE_PROFILE_SCOPE_BASIC("Editor::Render");

#ifdef _DEBUG
        chrono::steady_clock::time_point editorPassBegin{};
        if (sampleRenderPerf)
            editorPassBegin = chrono::steady_clock::now();
#endif
        EDITOR->Capture_BackBuffer_ToViewport();
#ifdef _DEBUG
        if (sampleRenderPerf)
        {
            captureMs = chrono::duration<double, milli>(chrono::steady_clock::now() - editorPassBegin).count();
            editorPassBegin = chrono::steady_clock::now();
        }
#endif
        GAME->Bind_BackBuffer();
        if (FAILED(GAME->Clear_Buffers(kDefaultClearColor)))
            return E_FAIL;
#ifdef _DEBUG
        if (sampleRenderPerf)
            bindBackBufferMs = chrono::duration<double, milli>(chrono::steady_clock::now() - editorPassBegin).count();
#endif
#ifdef _DEBUG
        EDITOR->Render_Editor(sampleRenderPerf ? &editorPerfSample : nullptr);
#else
        EDITOR->Render_Editor();
#endif
    }
#ifdef _DEBUG
    if (sampleRenderPerf)
        editorRenderMs = chrono::duration<double, milli>(chrono::steady_clock::now() - passBegin).count();
#endif

#ifdef _DEBUG
    if (sampleRenderPerf)
        passBegin = chrono::steady_clock::now();
#endif
    {
        ENGINE_PROFILE_SCOPE_BASIC("Engine::Present");
        if (FAILED(GAME->Present()))
            return E_FAIL;
    }
#ifdef _DEBUG
    if (sampleRenderPerf)
    {
        presentMs = chrono::duration<double, milli>(chrono::steady_clock::now() - passBegin).count();
        totalRenderMs = chrono::duration<double, milli>(chrono::steady_clock::now() - totalRenderBegin).count();
        Accumulate_EffectEditorRenderPerf(
            totalRenderMs,
            gameRenderMs,
            captureMs,
            bindBackBufferMs,
            editorRenderMs,
            editorPerfSample.editorWindowsMs,
            editorPerfSample.notificationMs,
            editorPerfSample.imguiMs,
            presentMs
        );
    }

    Try_FlushEffectEditorRenderPerfLog();
#endif

    return S_OK;
}

HRESULT EffectEditorApp::Ready_Fonts()
{
    const wstring fontPath = Get_ClientResourcePath(L"Fonts/158ex.spritefont");
    if (!fs::exists(fontPath))
        return E_FAIL;

    CHECK_FAILED(GAME->Add_Font(TEXT("Font_Default"), fontPath.c_str()), E_FAIL);

    return S_OK;
}

HRESULT EffectEditorApp::Ready_Prototype_For_Static_Level()
{
    const Shared<ResourceLoader> resourceLoader = CLIENT->Get_ResourceLoader();
    CHECK_NULL(resourceLoader, E_FAIL);

    vector<FLoadJob> loadJobs;
    CHECK_FAILED(resourceLoader->Build_LevelJobs(LevelType::Static, loadJobs), E_FAIL);

    size_t includedCount = 0;
    size_t excludedCount = 0;
    size_t failedCount = 0;

    for (const FLoadJob& loadJob : loadJobs)
    {
        const bool included = Is_EffectEditorStaticLoadJobAllowed(loadJob);

        if (!included)
        {
            ++excludedCount;
            continue;
        }

        ++includedCount;

        const auto jobStartTime = chrono::steady_clock::now();
        const HRESULT registerResult = resourceLoader->Register_LoadJob(loadJob);
        const auto jobElapsedMs = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - jobStartTime
        ).count();

        if (jobElapsedMs >= kStartupResourceInfoThresholdMs)
        {
            LOG_INFO(
                "[StartupResourceTiming] scope=Static type={} tag={} elapsed_ms={} path={}",
                string(magic_enum::enum_name(loadJob.eType)),
                String::ToString(loadJob.strPrototypeTag),
                jobElapsedMs,
                String::ToString(loadJob.strPath)
            );
        }

        if (FAILED(registerResult))
        {
            ++failedCount;
            LOG_WARN(
                "[ResourceFilter] scope=Static, register failed. type={}, tag={}",
                string(magic_enum::enum_name(loadJob.eType)),
                String::ToString(loadJob.strPrototypeTag)
            );
            continue;
        }
    }

    Log_EffectEditorResourceFilterSummary("Static", loadJobs.size(), includedCount, excludedCount, failedCount);

    constexpr auto sourceHistoryRibbonTag = L"GameObject_Effect_ComputeSourceHistoryRibbonEmitter";
    if (!GAME->Has_Prototype(ETOI(LevelType::Static), sourceHistoryRibbonTag))
    {
        CHECK_FAILED(
            GAME->Add_Prototype(
                ETOI(LevelType::Static),
                sourceHistoryRibbonTag,
                Client::ComputeSourceHistoryRibbonEmitter::Create(_device, _context)
            ),
            E_FAIL
        );
    }

    constexpr auto sourceHistorySpriteTrailTag = L"GameObject_Effect_ComputeSourceHistorySpriteTrailEmitter";
    if (!GAME->Has_Prototype(ETOI(LevelType::Static), sourceHistorySpriteTrailTag))
    {
        CHECK_FAILED(
            GAME->Add_Prototype(
                ETOI(LevelType::Static),
                sourceHistorySpriteTrailTag,
                Client::ComputeSourceHistorySpriteTrailEmitter::Create(_device, _context)
            ),
            E_FAIL
        );
    }

    constexpr auto beamTag = L"GameObject_Effect_ComputeGeneratedBeamEmitter";
    if (!GAME->Has_Prototype(ETOI(LevelType::Static), beamTag))
    {
        CHECK_FAILED(
            GAME->Add_Prototype(
                ETOI(LevelType::Static),
                beamTag,
                Client::ComputeGeneratedBeamEmitter::Create(_device, _context)
            ),
            E_FAIL
        );
    }

    return S_OK;
}

HRESULT EffectEditorApp::Ready_PreviewLevel()
{
    // preview level은 저장 level이 아니므로 Static resource index를 명시적으로 빌려 쓴다.
    const HRESULT hr = GAME->Change_Level(
        ETOI(LevelType::Static),
        Level_EffectEditor::Create(_device, _context)
    );
    CHECK_FAILED(hr, E_FAIL);

    return S_OK;
}

wstring EffectEditorApp::Get_ClientResourceRoot() const
{
    wchar_t modulePath[MAX_PATH] = {};
    if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};

    const fs::path runtimeRoot = fs::path(modulePath).parent_path();
    return fs::absolute(runtimeRoot / L"../../../Client/Bin/Resources").lexically_normal().wstring();
}

wstring EffectEditorApp::Get_ClientResourcePath(const wchar_t* relativePath) const
{
    if (nullptr == relativePath || L'\0' == *relativePath)
        return Get_ClientResourceRoot();

    return (fs::path(Get_ClientResourceRoot()) / relativePath).lexically_normal().wstring();
}

Unique<EffectEditorApp> EffectEditorApp::Create()
{
    auto instance = make_unique<EffectEditorApp>();

    if (FAILED(instance->Initialize()))
    {
        LOG_CRITICAL("Failed to Create : EffectEditorApp");
        MSG_BOX("Failed to Create : EffectEditorApp");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void EffectEditorApp::Free()
{
    __super::Free();

    EffectEditorInstance::DestroyInstance();
    ClientInstance::DestroyInstance();

    _context.Reset();
    _device.Reset();

    GameInstance::DestroyInstance();
}

NS_END
