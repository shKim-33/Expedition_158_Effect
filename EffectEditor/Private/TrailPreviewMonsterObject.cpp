#include "TrailPreviewMonsterObject.h"

#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Material.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "TransformCom.h"
#include "Weapon.h"
#include "WeaponPrefabLoader.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kSourcePrefabName{ "MB_Simon" };
    constexpr auto kPartBody{ L"Part_Body" };
    constexpr auto kPartHair{ L"Part_Hair" };
    constexpr auto kPartWeapon{ L"Part_Weapon" };
    constexpr auto kBodyModelTag{ L"Model_Simon" };
    constexpr auto kHairModelTag{ L"Model_Simon_Hair" };
    constexpr auto kWeaponModelTag{ L"Model_Simon_Weapon" };
    constexpr auto kHeadSocketName{ "headSocket" };
    constexpr auto kWeaponSocketName{ "Weapon_R" };
    constexpr float kSimonWeaponAttachScaleCompensation{ 10.f };

    Quat Resolve_NormalizedRotation(const Matrix& worldMatrix)
    {
        Matrix decomposableWorldMatrix = worldMatrix;
        Vec3 scale = Vec3::One;
        Vec3 position = Vec3::Zero;
        Quat rotation = Quat::Identity;
        if (!decomposableWorldMatrix.Decompose(scale, rotation, position) || rotation.LengthSquared() <= 0.000001f)
            return Quat::Identity;

        rotation.Normalize();
        return rotation;
    }
    constexpr float kSimonWeaponAttachOffsetDegreesX{ -90.f };

    bool Is_RestorableMonsterVisualPart(const wstring& partTag)
    {
        return partTag == kPartBody || partTag == kPartHair;
    }

    bool Is_ValidTrailAnchorPoint(const TrailAnchorPointDesc& anchor)
    {
        return !anchor.boneName.empty();
    }

    vector<WeaponSourceHistoryAnchorDesc> Read_SourceHistoryAnchors(const json& rootJson)
    {
        vector<WeaponSourceHistoryAnchorDesc> anchors{};
        if (!rootJson.contains("custom_properties") || !rootJson["custom_properties"].is_object())
            return anchors;

        const json& customProperties = rootJson["custom_properties"];
        if (!customProperties.contains("_sourceHistoryAnchors") || !customProperties["_sourceHistoryAnchors"].is_array())
            return anchors;

        for (const json& anchorJson : customProperties["_sourceHistoryAnchors"])
        {
            if (!anchorJson.is_object())
                continue;

            WeaponSourceHistoryAnchorDesc anchor{};
            anchor.name = anchorJson.value("name", string{});
            anchor.boneName = anchorJson.value("boneName", string{});

            if (anchorJson.contains("localPosition") &&
                anchorJson["localPosition"].is_array() &&
                anchorJson["localPosition"].size() >= 3)
            {
                anchor.localPosition.x = anchorJson["localPosition"][0].get<float>();
                anchor.localPosition.y = anchorJson["localPosition"][1].get<float>();
                anchor.localPosition.z = anchorJson["localPosition"][2].get<float>();
            }

            if (!anchor.name.empty() && !anchor.boneName.empty())
                anchors.push_back(std::move(anchor));
        }

        return anchors;
    }
}

class TrailPreviewMonsterBodyObject final : public PartObject
{
public:
    TrailPreviewMonsterBodyObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
        : PartObject{ device, context }
    {
    }

    TrailPreviewMonsterBodyObject(const TrailPreviewMonsterBodyObject& prototype)
        : PartObject{ prototype }
    {
    }

    HRESULT Initialize_Prototype() override { return S_OK; }

    HRESULT Initialize(void* arg) override
    {
        CHECK_FAILED(PartObject::Initialize(arg), E_FAIL);
        CHECK_FAILED(Ready_Components(), E_FAIL);

        Set_Name(L"TrailPreview_Monster_Body");
        Set_Visible(false);
        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());

        if (_model != nullptr)
        {
            const int32 rootIndex = _model->Get_BoneIndex("root");
            if (rootIndex >= 0)
                _model->Set_LocalRootNode(rootIndex);
            _model->Refresh_AnimStateMappingFromOwnerContext();
        }

        return S_OK;
    }

    void Update(float timeDelta) override
    {
        if (!Is_Visible())
            return;

        if (_model != nullptr)
            _model->Play_Animation(timeDelta);

        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
    }

    void Late_Update(float) override
    {
        if (!Is_Visible())
            return;

        const Shared<GameObject> owner = Get_Owner();
        if (owner == nullptr || !owner->Is_Visible())
            return;

        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    }

    HRESULT Render() override
    {
        if (_model == nullptr || _shader == nullptr)
            return S_OK;

        CHECK_FAILED(Bind_ShaderResources(), E_FAIL);
        CHECK_FAILED(_model->Prepare_BoneMatrixBuffers(), E_FAIL);

        const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
        for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
        {
            const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);
            const Shared<Material> material = _model->Get_Material(materialIndex);
            if (material == nullptr)
                continue;

            if (FAILED(_shader->Bind_MaterialTextures(*material)))
                continue;
            CHECK_FAILED(_shader->Bind_CBufferData(material->Get_CB()), E_FAIL);
            CHECK_FAILED(_model->Bind_PreparedBoneMatrices(_shader.get(), "g_BoneMatricesBuffer", meshIndex), E_FAIL);
            CHECK_FAILED(_shader->Begin(material->Get_PassIndex()), E_FAIL);
            _model->Render(meshIndex);
        }

        return S_OK;
    }

    Shared<ModelCom> Get_Model() const { return _model; }

    const Matrix* Get_SocketBoneMatrixPtr(const string& boneName) const
    {
        return _model != nullptr ? _model->Get_BoneMatrixPtr(boneName.c_str()) : nullptr;
    }

    Shared<GameObject> Clone(void* arg) override
    {
        auto instance = make_shared<TrailPreviewMonsterBodyObject>(*this);
        if (FAILED(instance->Initialize(arg)))
            return nullptr;
        return instance;
    }

    void Free() override
    {
        PartObject::Free();
    }

private:
    HRESULT Ready_Components()
    {
        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxAnimMesh"), _shader), E_FAIL);
        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), kBodyModelTag, _model), E_FAIL);
        if (_model != nullptr)
            _model->Set_Owner(GetSharedPtr<GameObject>());
        return S_OK;
    }

    HRESULT Bind_ShaderResources()
    {
        CHECK_FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);
        return S_OK;
    }

private:
    Shared<ShaderCom> _shader{};
    Shared<ModelCom> _model{};
};

class TrailPreviewMonsterHairObject final : public PartObject
{
public:
    TrailPreviewMonsterHairObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
        : PartObject{ device, context }
    {
    }

    TrailPreviewMonsterHairObject(const TrailPreviewMonsterHairObject& prototype)
        : PartObject{ prototype }
    {
    }

    HRESULT Initialize_Prototype() override { return S_OK; }

    HRESULT Initialize(void* arg) override
    {
        CHECK_FAILED(PartObject::Initialize(arg), E_FAIL);
        CHECK_FAILED(Ready_Components(), E_FAIL);

        Set_Name(L"TrailPreview_Monster_Hair");
        Set_Visible(false);
        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());

        if (_model != nullptr)
        {
            const int32 rootIndex = _model->Get_BoneIndex("Simon_Strand_Hair_root");
            if (rootIndex >= 0)
                _model->Set_LocalRootNode(rootIndex);
        }

        return S_OK;
    }

    void Bind_Body(const Shared<TrailPreviewMonsterBodyObject>& body)
    {
        _body = body;
    }

    void Update(float) override
    {
        if (!Is_Visible())
            return;

        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
    }

    void Late_Update(float) override
    {
        if (!Is_Visible())
            return;

        const Shared<GameObject> owner = Get_Owner();
        if (owner == nullptr || !owner->Is_Visible())
            return;

        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    }

    HRESULT Render() override
    {
        if (_model == nullptr || _shader == nullptr)
            return S_OK;

        CHECK_FAILED(Bind_ShaderResources(), E_FAIL);
        CHECK_FAILED(_model->Prepare_BoneMatrixBuffers(), E_FAIL);

        const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
        for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
        {
            const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);
            const Shared<Material> material = _model->Get_Material(materialIndex);
            if (material == nullptr)
                continue;

            if (FAILED(_shader->Bind_MaterialTextures(*material)))
                continue;
            CHECK_FAILED(_shader->Bind_CBufferData(material->Get_CB()), E_FAIL);
            CHECK_FAILED(_model->Bind_PreparedBoneMatrices(_shader.get(), "g_BoneMatricesBuffer", meshIndex), E_FAIL);
            CHECK_FAILED(_shader->Begin(material->Get_PassIndex()), E_FAIL);
            _model->Render(meshIndex);
        }

        return S_OK;
    }

    Shared<GameObject> Clone(void* arg) override
    {
        auto instance = make_shared<TrailPreviewMonsterHairObject>(*this);
        if (FAILED(instance->Initialize(arg)))
            return nullptr;
        return instance;
    }

    void Free() override
    {
        PartObject::Free();
    }

private:
    HRESULT Ready_Components()
    {
        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxAnimMesh"), _shader), E_FAIL);
        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), kHairModelTag, _model), E_FAIL);
        if (_model != nullptr)
            _model->Set_Owner(GetSharedPtr<GameObject>());
        return S_OK;
    }

    void Update_CombinedWorldMatrix(const Matrix& childMatrix) override
    {
        const Shared<TrailPreviewMonsterBodyObject> body = _body.lock();
        const Matrix socketMatrix =
            body != nullptr && body->Get_SocketBoneMatrixPtr(kHeadSocketName) != nullptr
            ? *body->Get_SocketBoneMatrixPtr(kHeadSocketName)
            : Matrix::Identity;
        const Matrix parentMatrix = body != nullptr ? body->Get_CombinedWorldMatrix() : Matrix::Identity;
        _combinedWorldMatrix = childMatrix * socketMatrix * parentMatrix;
    }

    HRESULT Bind_ShaderResources()
    {
        CHECK_FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);
        return S_OK;
    }

private:
    Weak<TrailPreviewMonsterBodyObject> _body{};
    Shared<ShaderCom> _shader{};
    Shared<ModelCom> _model{};
};

class TrailPreviewMonsterWeaponObject final : public PartObject
{
public:
    TrailPreviewMonsterWeaponObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
        : PartObject{ device, context }
    {
    }

    TrailPreviewMonsterWeaponObject(const TrailPreviewMonsterWeaponObject& prototype)
        : PartObject{ prototype }
    {
    }

    HRESULT Initialize_Prototype() override { return S_OK; }

    HRESULT Initialize(void* arg) override
    {
        CHECK_FAILED(PartObject::Initialize(arg), E_FAIL);
        CHECK_FAILED(Ready_Components(), E_FAIL);

        Set_Name(L"TrailPreview_Monster_Weapon");
        Set_Visible(false);
        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
        return S_OK;
    }

    void Bind_Body(const Shared<TrailPreviewMonsterBodyObject>& body)
    {
        _body = body;
    }

    void Set_SourceHistoryAnchors(vector<WeaponSourceHistoryAnchorDesc> anchors)
    {
        _sourceHistoryAnchors = std::move(anchors);
    }

    void Update(float) override
    {
        if (!Is_Visible())
            return;

        Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
    }

    void Late_Update(float) override
    {
        if (!Is_Visible())
            return;

        const Shared<GameObject> owner = Get_Owner();
        if (owner == nullptr || !owner->Is_Visible())
            return;

        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    }

    HRESULT Render() override
    {
        if (_model == nullptr || _shader == nullptr)
            return S_OK;

        CHECK_FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix), E_FAIL);
        CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

        const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
        for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
        {
            const uint32 materialIndex = _model->Get_MeshMaterialIndex(meshIndex);
            const Shared<Material> material = _model->Get_Material(materialIndex);
            if (material == nullptr)
                continue;

            if (FAILED(_shader->Bind_MaterialTextures(*material)))
                continue;
            CHECK_FAILED(_shader->Bind_CBufferData(material->Get_CB()), E_FAIL);
            CHECK_FAILED(_shader->Begin(material->Get_PassIndex()), E_FAIL);
            _model->Render(meshIndex);
        }

        return S_OK;
    }

    Shared<ModelCom> Get_Model() const { return _model; }
    const vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() const { return _sourceHistoryAnchors; }

    Shared<GameObject> Clone(void* arg) override
    {
        auto instance = make_shared<TrailPreviewMonsterWeaponObject>(*this);
        if (FAILED(instance->Initialize(arg)))
            return nullptr;
        return instance;
    }

    void Free() override
    {
        PartObject::Free();
    }

private:
    HRESULT Ready_Components()
    {
        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader), E_FAIL);
        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), kWeaponModelTag, _model), E_FAIL);
        if (_model != nullptr)
            _model->Set_Owner(GetSharedPtr<GameObject>());
        return S_OK;
    }

    void Update_CombinedWorldMatrix(const Matrix& childMatrix) override
    {
        const Shared<TrailPreviewMonsterBodyObject> body = _body.lock();
        const Matrix socketMatrix =
            body != nullptr && body->Get_SocketBoneMatrixPtr(kWeaponSocketName) != nullptr
            ? *body->Get_SocketBoneMatrixPtr(kWeaponSocketName)
            : Matrix::Identity;
        const Matrix parentMatrix = body != nullptr ? body->Get_CombinedWorldMatrix() : Matrix::Identity;
        const Matrix simonWeaponAttachMatrix =
            Matrix::CreateScale(kSimonWeaponAttachScaleCompensation) *
            Matrix::CreateRotationX(XMConvertToRadians(kSimonWeaponAttachOffsetDegreesX));

        _combinedWorldMatrix = simonWeaponAttachMatrix * childMatrix * socketMatrix * parentMatrix;
    }

private:
    Weak<TrailPreviewMonsterBodyObject> _body{};
    Shared<ShaderCom> _shader{};
    Shared<ModelCom> _model{};
    vector<WeaponSourceHistoryAnchorDesc> _sourceHistoryAnchors{};
};

TrailPreviewMonsterObject::TrailPreviewMonsterObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : ContainerObject{ device, context }
{
}

TrailPreviewMonsterObject::TrailPreviewMonsterObject(const TrailPreviewMonsterObject& prototype)
    : ContainerObject{ prototype }
{
}

TrailPreviewMonsterObject::~TrailPreviewMonsterObject()
{
    Free();
}

HRESULT TrailPreviewMonsterObject::Initialize(void* arg)
{
    ContainerObjectDesc desc{};
    CHECK_FAILED(ContainerObject::Initialize(arg != nullptr ? arg : &desc), E_FAIL);
    CHECK_FAILED(Ready_PreviewParts(), E_FAIL);
    CHECK_FAILED(Restore_VisualPartsFromPrefab(), E_FAIL);

    Set_Name(L"TrailPreview_Monster");
    Set_SourcePrefabName(kSourcePrefabName);
    Set_PreviewVisible(false);

    if (const Shared<ModelCom> model = Get_BodyModel())
        model->Refresh_AnimStateMappingFromOwnerContext();

    return S_OK;
}

void TrailPreviewMonsterObject::BeginPlay()
{
    ContainerObject::BeginPlay();
}

void TrailPreviewMonsterObject::Update(float timeDelta)
{
    if (!Is_Visible())
    {
        Clear_TrailSample();
        return;
    }

    ContainerObject::Update(timeDelta);
    Update_TrailPoints();
}

void TrailPreviewMonsterObject::Late_Update(float timeDelta)
{
    if (!Is_Visible())
        return;

    ContainerObject::Late_Update(timeDelta);
    Draw_TrailAnchorMarkers();
}

Shared<ModelCom> TrailPreviewMonsterObject::Get_BodyModel() const
{
    return _body != nullptr ? _body->Get_Model() : nullptr;
}

void TrailPreviewMonsterObject::Set_PreviewVisible(bool visible)
{
    Set_Visible(visible);

    for (const auto& [partTag, partObject] : _partObjects)
    {
        if (partObject != nullptr)
            partObject->Set_Visible(visible);
    }

    if (!visible)
        Clear_TrailSample();
}

bool TrailPreviewMonsterObject::Has_TrailSample() const
{
    return Is_Visible() && _hasTrailSample;
}

bool TrailPreviewMonsterObject::Try_GetTrailSample(EffectTrailSample& outSample) const
{
    if (!Has_TrailSample())
        return false;

    outSample.baseWorldPosition = _trailBaseWorldPosition;
    outSample.tipWorldPosition = _trailTipWorldPosition;
    outSample.baseWorldRotation = _trailBaseWorldRotation;
    outSample.tipWorldRotation = _trailTipWorldRotation;
    return true;
}

bool TrailPreviewMonsterObject::Try_GetSourcePointSample(EffectSourcePointSample& outSample) const
{
    if (!Has_TrailSample())
        return false;

    outSample.worldPosition = _sourcePointWorldPosition;
    outSample.worldRotation = _sourcePointWorldRotation;
    return true;
}

HRESULT TrailPreviewMonsterObject::Ready_PreviewParts()
{
    PartObject::PARTOBJECT_DESC partDesc{};
    partDesc.parentMatrix = &_transformCom->Get_WorldMatrix();

    _body = make_shared<TrailPreviewMonsterBodyObject>(_device, _context);
    CHECK_NULL(_body, E_FAIL);
    CHECK_FAILED(_body->Initialize(&partDesc), E_FAIL);
    _body->Set_Owner(GetSharedPtr<GameObject>());
    _partObjects.emplace(kPartBody, _body);
    _partObjectUpdateOrder.push_back(_body);

    _hair = make_shared<TrailPreviewMonsterHairObject>(_device, _context);
    CHECK_NULL(_hair, E_FAIL);
    CHECK_FAILED(_hair->Initialize(&partDesc), E_FAIL);
    _hair->Bind_Body(_body);
    _hair->Set_Owner(GetSharedPtr<GameObject>());
    _partObjects.emplace(kPartHair, _hair);
    _partObjectUpdateOrder.push_back(_hair);

    WeaponPrefabLoader::LoadedWeaponPrefab loadedPrefab{};
    if (!WeaponPrefabLoader::Load(Constants::Prefab::Simon_Weapon, loadedPrefab, "Mon_Simon_Weapon"))
        return E_FAIL;

    _weapon = make_shared<TrailPreviewMonsterWeaponObject>(_device, _context);
    CHECK_NULL(_weapon, E_FAIL);
    CHECK_FAILED(_weapon->Initialize(&partDesc), E_FAIL);
    _weapon->Bind_Body(_body);
    _weapon->Set_Owner(GetSharedPtr<GameObject>());
    _weapon->From_Json(loadedPrefab.rootJson);
    _weapon->Set_SourceHistoryAnchors(Read_SourceHistoryAnchors(loadedPrefab.rootJson));
    _partObjects.emplace(kPartWeapon, _weapon);
    _partObjectUpdateOrder.push_back(_weapon);
    Load_TrailAnchorAsset(loadedPrefab.modelGuid);

    return S_OK;
}

HRESULT TrailPreviewMonsterObject::Restore_VisualPartsFromPrefab()
{
    const filesystem::path prefabPath =
        filesystem::path(GAME->Get_AssetRoot()) / L"Data" / L"json" / L"Prefabs" / L"Monster" / L"MB_Simon.prefab.json";
    const json prefabRoot = GAME->Load_JsonFile(prefabPath);
    if (!prefabRoot.is_object())
    {
        LOG_ERROR("[TrailPreview] failed to load monster visual fixture prefab. path={}", prefabPath.string());
        return E_FAIL;
    }

    const auto partsIter = prefabRoot.find("parts");
    if (partsIter == prefabRoot.end() || !partsIter->is_array())
    {
        LOG_ERROR("[TrailPreview] monster visual fixture prefab has no parts array. path={}", prefabPath.string());
        return E_FAIL;
    }

    uint32 restoredCount = 0;
    for (const json& partNode : *partsIter)
    {
        if (SUCCEEDED(Restore_VisualPartFromPrefab(partNode)))
            ++restoredCount;
    }

    if (restoredCount < 2)
        LOG_WARN("[TrailPreview] restored {}/2 monster visual parts from MB_Simon prefab.", restoredCount);

    return S_OK;
}

HRESULT TrailPreviewMonsterObject::Restore_VisualPartFromPrefab(const json& partNode)
{
    if (!partNode.is_object())
        return E_FAIL;

    const string partTagString = partNode.value("part_tag", string{});
    const wstring partTag = String::ToWString(partTagString);
    if (!Is_RestorableMonsterVisualPart(partTag))
        return E_FAIL;

    const auto objectIter = partNode.find("object");
    if (objectIter == partNode.end() || !objectIter->is_object())
        return E_FAIL;

    PartObject* partObject = Get_PartObject(partTag);
    if (partObject == nullptr)
        return E_FAIL;

    partObject->From_Json(*objectIter);
    return S_OK;
}

void TrailPreviewMonsterObject::Load_TrailAnchorAsset(const string& modelGuid)
{
    _trailAnchorAsset = {};
    _hasTrailAnchorAsset = false;

    if (modelGuid.empty())
    {
        LOG_WARN("[TrailPreview] monster weapon model guid is empty. fallback preview anchors will be used.");
        return;
    }

    const fs::path anchorPath = TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(modelGuid);
    if (!TrailAnchor_Parser::Load_FromFile(anchorPath.wstring(), _trailAnchorAsset))
    {
        LOG_WARN("[TrailPreview] failed to load monster weapon trail anchor. path={}", anchorPath.string());
        return;
    }

    if (!Is_ValidTrailAnchorPoint(_trailAnchorAsset.base) ||
        !Is_ValidTrailAnchorPoint(_trailAnchorAsset.tip))
    {
        LOG_WARN("[TrailPreview] monster weapon trail anchor is incomplete. modelGuid={}", modelGuid);
        _trailAnchorAsset = {};
        return;
    }

    _hasTrailAnchorAsset = true;
}

void TrailPreviewMonsterObject::Clear_TrailSample()
{
    _trailBaseWorldPosition = {};
    _trailTipWorldPosition = {};
    _sourcePointWorldPosition = {};
    _trailBaseWorldRotation = Quat::Identity;
    _trailTipWorldRotation = Quat::Identity;
    _sourcePointWorldRotation = Quat::Identity;
    _hasTrailSample = false;
}

void TrailPreviewMonsterObject::Update_TrailPoints()
{
    if (EDITOR == nullptr || _weapon == nullptr)
    {
        Clear_TrailSample();
        return;
    }

    const Vec3 baseLocalOffset = EDITOR->Get_TrailPreviewBaseLocalOffset();
    const Vec3 tipLocalOffset = EDITOR->Get_TrailPreviewTipLocalOffset();
    const Vec3 sourceLocalOffset = EDITOR->Get_SourceHistorySpriteTrailPreviewSourceLocalOffset();

    const bool hasBase =
        (_hasTrailAnchorAsset && Try_ResolveTrailAnchorPoint(_trailAnchorAsset.base, baseLocalOffset, _trailBaseWorldPosition, _trailBaseWorldRotation)) ||
        Try_ResolveFallbackLocalPoint(kFallbackBaseLocalOffset, baseLocalOffset, _trailBaseWorldPosition, _trailBaseWorldRotation);
    const bool hasTip =
        (_hasTrailAnchorAsset && Try_ResolveTrailAnchorPoint(_trailAnchorAsset.tip, tipLocalOffset, _trailTipWorldPosition, _trailTipWorldRotation)) ||
        Try_ResolveFallbackLocalPoint(kFallbackTipLocalOffset, tipLocalOffset, _trailTipWorldPosition, _trailTipWorldRotation);
    const bool hasSource =
        Try_ResolveSourceAnchorPoint(sourceLocalOffset, _sourcePointWorldPosition, _sourcePointWorldRotation) ||
        (_hasTrailAnchorAsset && Try_ResolveTrailAnchorPoint(_trailAnchorAsset.tip, sourceLocalOffset, _sourcePointWorldPosition, _sourcePointWorldRotation)) ||
        Try_ResolveFallbackLocalPoint(kFallbackSourceLocalOffset, sourceLocalOffset, _sourcePointWorldPosition, _sourcePointWorldRotation);

    _hasTrailSample = hasBase && hasTip && hasSource;

    if (!_hasTrailAnchorAsset && !_anchorFallbackWarningLogged)
    {
        LOG_WARN("[TrailPreview] monster preview uses fallback local anchors because trail anchor asset is unavailable.");
        _anchorFallbackWarningLogged = true;
    }
}

bool TrailPreviewMonsterObject::Try_ResolveWeaponLocalPoint(
    const Vec3& localPosition,
    Vec3& outWorldPosition,
    Quat& outWorldRotation) const
{
    if (_weapon == nullptr)
        return false;

    outWorldPosition = Vec3::Transform(localPosition, _weapon->Get_CombinedWorldMatrix());
    outWorldRotation = Resolve_NormalizedRotation(_weapon->Get_CombinedWorldMatrix());
    return true;
}

bool TrailPreviewMonsterObject::Try_ResolveTrailAnchorPoint(
    const TrailAnchorPointDesc& anchor,
    const Vec3& localOffset,
    Vec3& outWorldPosition,
    Quat& outWorldRotation) const
{
    if (_weapon == nullptr || anchor.boneName.empty())
        return false;

    const Shared<ModelCom> weaponModel = _weapon->Get_Model();
    if (weaponModel == nullptr)
        return false;

    const int32 boneIndex = weaponModel->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = weaponModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix anchorLocalMatrix = weaponModel->Get_ModelType() == ModelType::NonAnim
                                     ? Matrix::Identity
                                     : *boneMatrix;
    const Matrix anchorWorldMatrix = anchorLocalMatrix * _weapon->Get_CombinedWorldMatrix();
    outWorldPosition = Vec3::Transform(anchor.localPosition + localOffset, anchorWorldMatrix);
    outWorldRotation = Resolve_NormalizedRotation(anchorWorldMatrix);
    return true;
}

bool TrailPreviewMonsterObject::Try_ResolveSourceAnchorPoint(
    const Vec3& localOffset,
    Vec3& outWorldPosition,
    Quat& outWorldRotation) const
{
    if (_weapon == nullptr)
        return false;

    const vector<WeaponSourceHistoryAnchorDesc>& anchors = _weapon->Get_SourceHistoryAnchors();
    if (anchors.empty())
        return false;

    const WeaponSourceHistoryAnchorDesc& anchor = anchors.front();
    TrailAnchorPointDesc sourceAnchor{};
    sourceAnchor.boneName = anchor.boneName;
    sourceAnchor.localPosition = anchor.localPosition;
    return Try_ResolveTrailAnchorPoint(sourceAnchor, localOffset, outWorldPosition, outWorldRotation);
}

bool TrailPreviewMonsterObject::Try_ResolveFallbackLocalPoint(
    const Vec3& localPosition,
    const Vec3& localOffset,
    Vec3& outWorldPosition,
    Quat& outWorldRotation) const
{
    return Try_ResolveWeaponLocalPoint(localPosition + localOffset, outWorldPosition, outWorldRotation);
}

void TrailPreviewMonsterObject::Draw_TrailAnchorMarkers() const
{
    if (!Has_TrailSample())
        return;

    if (EDITOR != nullptr && !EDITOR->Is_TrailPreviewDebugRenderEnabled())
        return;

    Draw_TrailAnchorMarker(_trailBaseWorldPosition, Color(0.2f, 0.9f, 1.f, 1.f));
    Draw_TrailAnchorMarker(_trailTipWorldPosition, Color(1.f, 0.72f, 0.18f, 1.f));
    Draw_TrailAnchorMarker(_sourcePointWorldPosition, Color(0.95f, 0.3f, 1.f, 1.f));

    DebugLineDesc lineDesc{};
    lineDesc.start = _trailBaseWorldPosition;
    lineDesc.end = _trailTipWorldPosition;
    lineDesc.style.color = Color(0.95f, 1.f, 0.25f, 1.f);
    lineDesc.style.depthEnabled = false;
    GAME->Draw_DebugLine(lineDesc);
}

void TrailPreviewMonsterObject::Draw_TrailAnchorMarker(const Vec3& worldPosition, const Color& color) const
{
    DebugSphereDesc sphereDesc{};
    sphereDesc.center = worldPosition;
    sphereDesc.radius = kTrailAnchorMarkerRadius;
    sphereDesc.style.color = color;
    sphereDesc.style.depthEnabled = false;
    GAME->Draw_DebugSphere(sphereDesc);
}

Shared<TrailPreviewMonsterObject> TrailPreviewMonsterObject::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<TrailPreviewMonsterObject>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : TrailPreviewMonsterObject");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TrailPreviewMonsterObject::Clone(void* arg)
{
    auto instance = make_shared<TrailPreviewMonsterObject>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : TrailPreviewMonsterObject");
        MSG_BOX("Failed to Clone : TrailPreviewMonsterObject");
        return nullptr;
    }

    return instance;
}

void TrailPreviewMonsterObject::Free()
{
    ContainerObject::Free();
}

NS_END
