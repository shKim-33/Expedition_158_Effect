#include "Level_EffectEditor.h"

#include "Camera.h"
#include "ClientInstance.h"
#include "EffectEditorCamera.h"
#include "EffectEditorInstance.h"
#include "EffectEditorPreviewPlane.h"
#include "EffectEditorPreviewRuntime.h"
#include "EffectEditorSceneGrid.h"
#include "EffectEditorSkybox.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ResourceLoader.h"
#include "TrailPreviewController.h"
#include "TrailPreviewMonsterObject.h"
#include "TrailPreviewPlayerObject.h"
#include "TrailPreviewSampleRouter.h"
#include "TrailPreviewWeaponObject.h"

NS_BEGIN(EffectEditor)

namespace
{
    bool Is_TrailPreviewModelResource(const wstring& prototypeTag)
    {
        return prototypeTag == L"Model_GustaveBody" ||
               prototypeTag == L"Model_GustaveHead" ||
               prototypeTag == L"Model_GustaveHair" ||
               prototypeTag == L"Model_Simon" ||
               prototypeTag == L"Model_Simon_Hair" ||
               prototypeTag == L"Model_Simon_Weapon";
    }
}

Level_EffectEditor::Level_EffectEditor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
    , _previewRuntime{ make_unique<EffectEditorPreviewRuntime>() }
{
}

Level_EffectEditor::~Level_EffectEditor()
{
    Free();
}

HRESULT Level_EffectEditor::Initialize()
{
    CHECK_FAILED(Level::Initialize(), E_FAIL);
    CHECK_FAILED(Ready_Lights(), E_FAIL);
    CHECK_FAILED(Ready_Layer_Camera(L"Layer_Camera"), E_FAIL);
    CHECK_FAILED(Ready_Layer_Grid(L"Layer_Grid"), E_FAIL);
    CHECK_FAILED(Ready_Layer_Sky(L"Layer_Sky"), E_FAIL);
    CHECK_FAILED(Ready_Layer_Preview(kPreviewLayerTag), E_FAIL);
    CHECK_FAILED(_previewRuntime->Initialize(kPreviewLayerTag), E_FAIL);

    return S_OK;
}

void Level_EffectEditor::Update(float timeDelta)
{
    _previewRuntime->Update(timeDelta);

    __super::Update(timeDelta);

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (emitterView != nullptr)
        _previewRuntime->Apply_PreviewEmitterTransformOverrides(emitterView->Get_Emitters());
}

HRESULT Level_EffectEditor::Render()
{
    CHECK_FAILED(__super::Render(), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Restart_PreviewFromAuthoring()
{
    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (nullptr == emitterView)
    {
        LOG_ERROR("Emitter view is not available for preview restart.");
        return E_FAIL;
    }

    CHECK_FAILED(_previewRuntime->Restart_FromAuthoring(emitterView->Build_PreviewEmitters(), emitterView->Get_HistoryBudget()), E_FAIL);

    for (const AuthoringEmitter& emitter : emitterView->Get_Emitters())
        emitterView->Clear_PreviewDirty(emitter.id);

    return S_OK;
}

HRESULT Level_EffectEditor::Reset_PreviewRuntime()
{
    return _previewRuntime->Reset_Runtime();
}

bool Level_EffectEditor::Is_PreviewFinished() const
{
    return _previewRuntime->Is_Finished();
}

void Level_EffectEditor::Apply_PreviewEmitterTransformOverrides(const vector<AuthoringEmitter>& emitters)
{
    _previewRuntime->Apply_PreviewEmitterTransformOverrides(emitters);
}

bool Level_EffectEditor::Try_GetPreviewEmitterWorldMatrix(uint32 emitterId, Matrix& outWorldMatrix) const
{
    return _previewRuntime->Try_GetPreviewEmitterWorldMatrix(emitterId, outWorldMatrix);
}

HRESULT Level_EffectEditor::Ensure_TrailPreviewFixture()
{
    if (_trailPreviewFixtureReady)
        return S_OK;

    CHECK_NULL(_previewRuntime, E_FAIL);

    const Shared<ResourceLoader> resourceLoader = CLIENT->Get_ResourceLoader();
    CHECK_NULL(resourceLoader, E_FAIL);

    vector<FLoadJob> loadJobs;
    CHECK_FAILED(resourceLoader->Build_LevelJobs(LevelType::Static, loadJobs), E_FAIL);

    uint32 registeredModelCount = 0;
    for (const FLoadJob& loadJob : loadJobs)
    {
        if (loadJob.eType == ELoadJobType::Model &&
            Is_TrailPreviewModelResource(loadJob.strPrototypeTag))
        {
            CHECK_FAILED(resourceLoader->Register_LoadJob(loadJob), E_FAIL);
            ++registeredModelCount;
            continue;
        }
    }

    if (registeredModelCount < 6)
    {
        LOG_ERROR("[TrailPreview] failed to find all visual fixture model resources. registered={}", registeredModelCount);
        return E_FAIL;
    }

    const Shared<TrailPreviewPlayerObject> trailPreviewPlayer =
        TrailPreviewPlayerObject::Create(_device, _context);
    CHECK_NULL(trailPreviewPlayer, E_FAIL);
    CHECK_FAILED(trailPreviewPlayer->Initialize(nullptr), E_FAIL);

    const Shared<TrailPreviewWeaponObject> trailPreviewWeapon = trailPreviewPlayer->Get_PreviewWeapon();
    if (nullptr == trailPreviewWeapon)
    {
        LOG_ERROR("[TrailPreview] TrailPreviewPlayerObject did not create Part_Weapon.");
        return E_FAIL;
    }

    const Shared<TrailPreviewMonsterObject> trailPreviewMonster =
        TrailPreviewMonsterObject::Create(_device, _context);
    CHECK_NULL(trailPreviewMonster, E_FAIL);
    CHECK_FAILED(trailPreviewMonster->Initialize(nullptr), E_FAIL);

    TrailPreviewController::PreviewDesc controllerDesc{};
    controllerDesc.player = trailPreviewPlayer;
    controllerDesc.monster = trailPreviewMonster;

    const Shared<TrailPreviewController> trailPreviewController =
        TrailPreviewController::Create(_device, _context);
    CHECK_NULL(trailPreviewController, E_FAIL);
    CHECK_FAILED(trailPreviewController->Initialize(&controllerDesc), E_FAIL);

    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), kPreviewLayerTag, trailPreviewPlayer), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), kPreviewLayerTag, trailPreviewMonster), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), kPreviewLayerTag, trailPreviewController), E_FAIL);

    _trailPreviewSampleRouter = make_shared<TrailPreviewSampleRouter>();
    _trailPreviewSampleRouter->Bind_PlayerProviders(trailPreviewWeapon, trailPreviewWeapon);
    _trailPreviewSampleRouter->Bind_MonsterProviders(trailPreviewMonster, trailPreviewMonster);
    _previewRuntime->Bind_TrailPreviewSources(
        _trailPreviewSampleRouter,
        _trailPreviewSampleRouter,
        trailPreviewController);
    _trailPreviewController = trailPreviewController;
    _trailPreviewFixtureReady = true;

    LOG_INFO("[TrailPreview] lazy fixture initialized.");

    return S_OK;
}

void Level_EffectEditor::Get_TrailPreviewAnimationNames(vector<string>& outNames) const
{
    outNames.clear();

    const Shared<TrailPreviewController> trailPreviewController = _trailPreviewController.lock();
    if (trailPreviewController == nullptr)
        return;

    trailPreviewController->Get_AnimationNames(outNames);
}

HRESULT Level_EffectEditor::Ready_Lights()
{
    LIGHT_DESC lightDesc{};
    lightDesc.type = LightType::Directional;
    lightDesc.direction = Vec4(1.f, -1.f, 1.f, 0.f);
    lightDesc.direction.Normalize();
    lightDesc.diffuse = Vec4(1.f, 1.f, 1.f, 1.f);
    lightDesc.ambient = Vec4(0.6f, 0.6f, 0.6f, 1.f);
    lightDesc.specular = Vec4(1.f, 1.f, 1.f, 1.f);

    CHECK_FAILED(GAME->Add_Light(lightDesc), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Camera(const wstring& layerTag)
{
    EffectEditorCamera::EffectEditorCameraDesc cameraDesc{};
    cameraDesc.speedPerSec = 5.f;
    cameraDesc.degreePerSec = 180.f;
    cameraDesc.eye = Vec4(0.f, 3.f, -3.5f, 1.f);
    cameraDesc.at = Vec4(0.f, 0.f, 0.f, 1.f);
    cameraDesc.fovy = XMConvertToRadians(60.f);
    cameraDesc.nearPlane = 0.1f;
    cameraDesc.farPlane = 500.f;
    cameraDesc.mouseSensor = 0.065f;
    cameraDesc.yaw = 0.f;
    cameraDesc.pitch = 0.f;
    cameraDesc.pivot = Vec3(0.f, 0.f, 0.f);

    const Shared<EffectEditorCamera> previewCamera =
        EffectEditorCamera::Create(_device, _context);
    CHECK_NULL(previewCamera, E_FAIL);
    CHECK_FAILED(previewCamera->Initialize(&cameraDesc), E_FAIL);
    EDITOR->Register_PreviewCamera(previewCamera);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, previewCamera), E_FAIL);

    GAME->Change_ActiveCamera(previewCamera);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Grid(const wstring& layerTag)
{
    if (layerTag.empty())
        return E_FAIL;

    const Shared<EffectEditorSceneGrid> sceneGrid =
        EffectEditorSceneGrid::Create(_device, _context);
    CHECK_NULL(sceneGrid, E_FAIL);
    CHECK_FAILED(sceneGrid->Initialize(nullptr), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, sceneGrid), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Sky(const wstring& layerTag)
{
    if (layerTag.empty())
        return E_FAIL;

    const Shared<EffectEditorSkybox> skybox =
        EffectEditorSkybox::Create(_device, _context);
    CHECK_NULL(skybox, E_FAIL);
    CHECK_FAILED(skybox->Initialize(nullptr), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, skybox), E_FAIL);

    return S_OK;
}

HRESULT Level_EffectEditor::Ready_Layer_Preview(const wstring& layerTag)
{
    if (layerTag.empty())
        return E_FAIL;

    const Shared<EffectEditorPreviewPlane> previewPlane =
        EffectEditorPreviewPlane::Create(_device, _context);
    CHECK_NULL(previewPlane, E_FAIL);
    CHECK_FAILED(previewPlane->Initialize(nullptr), E_FAIL);
    CHECK_FAILED(GAME->Add_GameObject(ETOI(LevelType::Static), layerTag, previewPlane), E_FAIL);

    return S_OK;
}

Shared<Level_EffectEditor> Level_EffectEditor::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Level_EffectEditor>(device, context);

    if (FAILED(instance->Initialize()))
    {
        LOG_CRITICAL("Failed to Create : Level_EffectEditor");
        return nullptr;
    }

    return instance;
}

void Level_EffectEditor::Free()
{
    if (nullptr != _previewRuntime)
        _previewRuntime->Clear();

    __super::Free();
}

NS_END
