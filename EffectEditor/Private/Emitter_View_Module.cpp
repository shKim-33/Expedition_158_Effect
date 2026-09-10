#include "Emitter_View.h"

#include "CurveEditor_View.h"
#include "EffectAuthoringModuleMetadata.h"
#include "EffectAuthoring_Types.h"
#include "EffectMaterialPresetReader.h"
#include "GameInstance.h"
#include "Helper_ImGui.h"
#include "Helper_String.h"
#include "Notification_Manager.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kDefaultEffectMaterialFileName = L"M_DefaultMaterial.effectmaterial.json";
    constexpr auto kDefaultEffectMaterialName = "M_DefaultMaterial";
    constexpr auto kDefaultEffectMaterialTexturePath = "Effects/Textures/Shared/DefaultTexture.dds";

    fs::path Resolve_DefaultEffectMaterialPath()
    {
        if (GAME == nullptr)
            return {};

        const vector<AssetMeta> materialAssets = GAME->Get_AssetsByType("EffectMaterial");
        for (const AssetMeta& assetMeta : materialAssets)
        {
            if (assetMeta.fullPath.empty())
                continue;

            if (fs::path(assetMeta.fullPath).filename() == kDefaultEffectMaterialFileName)
                return assetMeta.fullPath;
        }

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return {};

        const fs::path candidatePath =
            fs::path(assetRoot) / L"Effects" / L"Materials" / kDefaultEffectMaterialFileName;
        if (!fs::exists(candidatePath))
            return {};

        const string materialGuid = GAME->Ensure_AssetGUID(candidatePath.wstring(), "EffectMaterial");
        if (!materialGuid.empty())
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(materialGuid);
            if (!resolvedPath.empty())
                return resolvedPath;
        }

        return candidatePath;
    }
}

uint32 Emitter_View::Issue_AuthoringId()
{
    return _nextAuthoringId++;
}

AuthoringEmitter Emitter_View::Make_DefaultSpriteEmitter()
{
    AuthoringEmitter emitter{};
    emitter.id = Issue_AuthoringId();
    emitter.name = "Particle Emitter";
    emitter.enabled = true;
    emitter.previewDirty = true;

    AuthoringModule requiredModule = Make_RequiredModule();
    if (const auto* requiredData = get_if<RequiredModuleData>(&requiredModule.data))
    {
        emitter.rendererType = requiredData->rendererType;
        emitter.textureId = requiredData->material.mainTexturePath;
        emitter.resourceSummary = requiredData->material.mainTexturePath;
    }

    emitter.modules.push_back(move(requiredModule));
    emitter.modules.push_back(Make_SpawnModule());
    emitter.modules.push_back(Make_LifetimeModule());
    emitter.modules.push_back(Make_InitialLocationModule());
    emitter.modules.push_back(Make_InitialSizeModule());
    emitter.modules.push_back(Make_InitialVelocityModule());
    emitter.modules.push_back(Make_InitialRadialVelocityModule());
    emitter.modules.push_back(Make_InitialColorModule());
    emitter.modules.push_back(Make_ColorOverLifeModule());

    return emitter;
}

AuthoringEmitter Emitter_View::Clone_Emitter(const AuthoringEmitter& sourceEmitter)
{
    AuthoringEmitter clonedEmitter = sourceEmitter;
    clonedEmitter.id = Issue_AuthoringId();
    clonedEmitter.previewDirty = true;

    for (AuthoringModule& module : clonedEmitter.modules)
        module.id = Issue_AuthoringId();

    return clonedEmitter;
}

AuthoringModule Emitter_View::Clone_Module(const AuthoringModule& sourceModule)
{
    AuthoringModule clonedModule = sourceModule;
    clonedModule.id = Issue_AuthoringId();
    Apply_AuthoringModuleMetadata(clonedModule);
    return clonedModule;
}

RequiredModuleData Emitter_View::Make_DefaultRequiredModuleData()
{
    RequiredModuleData data{};
    const fs::path materialPath = Resolve_DefaultEffectMaterialPath();
    if (materialPath.empty() || !EffectMaterialPresetReader::Read(materialPath, data.material))
    {
        data.material.sourcePresetName = kDefaultEffectMaterialName;
        data.material.mainTexturePath = kDefaultEffectMaterialTexturePath;
        data.material.mainTextureGuid = GAME != nullptr
                                        ? GAME->Ensure_AssetGUID(
                                            (fs::path(GAME->Get_AssetRoot()) / String::ToWString(kDefaultEffectMaterialTexturePath)).wstring(),
                                            "Texture"
                                        )
                                        : string{};
    }

    return data;
}

AuthoringModule Emitter_View::Make_DefaultModuleByType(AuthoringModuleType type)
{
    switch (type)
    {
    case AuthoringModuleType::Required:
        return Make_RequiredModule();
    case AuthoringModuleType::Spawn:
        return Make_SpawnModule();
    case AuthoringModuleType::Lifetime:
        return Make_LifetimeModule();
    case AuthoringModuleType::InitialLocation:
        return Make_InitialLocationModule();
    case AuthoringModuleType::SphereLocation:
        return Make_SphereLocationModule();
    case AuthoringModuleType::PlaneRadialLocation:
        return Make_PlaneRadialLocationModule();
    case AuthoringModuleType::CylinderLocation:
        return Make_CylinderLocationModule();
    case AuthoringModuleType::InitialSize:
        return Make_InitialSizeModule();
    case AuthoringModuleType::InitialMeshSize:
        return Make_InitialMeshSizeModule();
    case AuthoringModuleType::InitialVelocity:
        return Make_InitialVelocityModule();
    case AuthoringModuleType::InitialRadialVelocity:
        return Make_InitialRadialVelocityModule();
    case AuthoringModuleType::VelocityCone:
        return Make_VelocityConeModule();
    case AuthoringModuleType::SourceMotionVelocity:
        return Make_SourceMotionVelocityModule();
    case AuthoringModuleType::Acceleration:
        return Make_AccelerationModule();
    case AuthoringModuleType::Drag:
        return Make_DragModule();
    case AuthoringModuleType::VelocityOverLife:
        return Make_VelocityOverLifeModule();
    case AuthoringModuleType::OrbitOverLife:
        return Make_OrbitOverLifeModule();
    case AuthoringModuleType::InitialRotation:
        return Make_InitialRotationModule();
    case AuthoringModuleType::SphereRadialOrientation:
        return Make_SphereRadialOrientationModule();
    case AuthoringModuleType::PlaneRadialOrientation:
        return Make_PlaneRadialOrientationModule();
    case AuthoringModuleType::CylinderOrientation:
        return Make_CylinderOrientationModule();
    case AuthoringModuleType::RotationOverLife:
        return Make_RotationOverLifeModule();
    case AuthoringModuleType::SpriteTilt:
        return Make_SpriteTiltModule();
    case AuthoringModuleType::SpriteTiltOverLife:
        return Make_SpriteTiltOverLifeModule();
    case AuthoringModuleType::InitialRotationRate:
        return Make_InitialRotationRateModule();
    case AuthoringModuleType::RotationRateScaleByLife:
        return Make_RotationRateScaleByLifeModule();
    case AuthoringModuleType::InitialMeshRotation:
        return Make_InitialMeshRotationModule();
    case AuthoringModuleType::MeshRotationOverLife:
        return Make_MeshRotationOverLifeModule();
    case AuthoringModuleType::MeshDirectionAlignOverLife:
        return Make_MeshDirectionAlignOverLifeModule();
    case AuthoringModuleType::InitialMeshRotationRate:
        return Make_InitialMeshRotationRateModule();
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        return Make_MeshRotationRateScaleByLifeModule();
    case AuthoringModuleType::InitialColor:
        return Make_InitialColorModule();
    case AuthoringModuleType::ColorOverLife:
        return Make_ColorOverLifeModule();
    case AuthoringModuleType::SubUVFrameOverLife:
        return Make_SubUVFrameOverLifeModule();
    case AuthoringModuleType::SizeByLife:
        return Make_SizeByLifeModule();
    case AuthoringModuleType::BeamEnvelopeOverLife:
        return Make_BeamEnvelopeOverLifeModule();
    case AuthoringModuleType::MeshSizeByLife:
        return Make_MeshSizeByLifeModule();
    case AuthoringModuleType::SpawnPerUnit:
        return Make_SpawnPerUnitModule();
    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        return Make_SourceHistorySpriteTrailPathFollowModule();
    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        return Make_SourceHistorySpriteTrailPathReplayModule();
    case AuthoringModuleType::RibbonOrientation:
        return Make_RibbonOrientationModule();
    case AuthoringModuleType::MaterialScalarModulation:
        return Make_MaterialScalarModulationModule();
    }

    return Make_RequiredModule();
}

AuthoringModule Emitter_View::Make_RequiredModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Required;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = Make_DefaultRequiredModuleData();
    return module;
}

AuthoringModule Emitter_View::Make_SpawnModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Spawn;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SpawnModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_LifetimeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Lifetime;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = LifetimeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialLocation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SphereLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SphereLocation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SphereLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_PlaneRadialLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::PlaneRadialLocation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = PlaneRadialLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_CylinderLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::CylinderLocation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = CylinderLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialSizeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialSize;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialSizeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialMeshSizeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialMeshSize;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialMeshSizeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialVelocityModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialVelocity;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialVelocityModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRadialVelocityModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRadialVelocity;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialRadialVelocityModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_VelocityConeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::VelocityCone;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = VelocityConeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SourceMotionVelocityModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SourceMotionVelocity;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SourceMotionVelocityModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_AccelerationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Acceleration;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = AccelerationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_DragModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Drag;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = DragModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_VelocityOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::VelocityOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = VelocityOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_OrbitOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::OrbitOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = OrbitOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRotationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRotation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialRotationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SphereRadialOrientationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SphereRadialOrientation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SphereRadialOrientationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_PlaneRadialOrientationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::PlaneRadialOrientation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = PlaneRadialOrientationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_CylinderOrientationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::CylinderOrientation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = CylinderOrientationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_RotationOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::RotationOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = RotationOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SpriteTiltModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SpriteTilt;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SpriteTiltModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SpriteTiltOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SpriteTiltOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SpriteTiltOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRotationRateModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRotationRate;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialRotationRateModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_RotationRateScaleByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::RotationRateScaleByLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = RotationRateScaleByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialMeshRotationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialMeshRotation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialMeshRotationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MeshRotationOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MeshRotationOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MeshRotationOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MeshDirectionAlignOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MeshDirectionAlignOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MeshDirectionAlignOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialMeshRotationRateModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialMeshRotationRate;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialMeshRotationRateModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MeshRotationRateScaleByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MeshRotationRateScaleByLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MeshRotationRateScaleByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialColorModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialColor;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialColorModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_ColorOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::ColorOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = ColorOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SubUVFrameOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SubUVFrameOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SubUVFrameOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SizeByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SizeByLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SizeByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_BeamEnvelopeOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::BeamEnvelopeOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = BeamEnvelopeOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MeshSizeByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MeshSizeByLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MeshSizeByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SpawnPerUnitModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SpawnPerUnit;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SpawnPerUnitModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SourceHistorySpriteTrailPathFollowModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SourceHistorySpriteTrailPathFollow;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SourceHistorySpriteTrailPathFollowModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SourceHistorySpriteTrailPathReplayModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SourceHistorySpriteTrailPathReplay;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SourceHistorySpriteTrailPathReplayModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_RibbonOrientationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::RibbonOrientation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = RibbonOrientationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MaterialScalarModulationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MaterialScalarModulation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MaterialScalarModulationModuleData{};
    return module;
}

bool Emitter_View::Has_ModuleType(const AuthoringEmitter& emitter, AuthoringModuleType type) const
{
    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type == type)
            return true;
    }

    return false;
}

bool Emitter_View::Is_ModuleCompatibleWithEmitter(const AuthoringEmitter& emitter, AuthoringModuleType type) const
{
    if (emitter.typeData.kind == AuthoringTypeDataKind::Ribbon)
    {
        switch (type)
        {
        case AuthoringModuleType::Required:
        case AuthoringModuleType::Lifetime:
        case AuthoringModuleType::InitialLocation:
        case AuthoringModuleType::InitialColor:
        case AuthoringModuleType::ColorOverLife:
        case AuthoringModuleType::SubUVFrameOverLife:
        case AuthoringModuleType::SizeByLife:
        case AuthoringModuleType::RibbonOrientation:
        case AuthoringModuleType::MaterialScalarModulation:
            return true;
        default:
            return false;
        }
    }

    if (emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail)
    {
        switch (type)
        {
        case AuthoringModuleType::Required:
        case AuthoringModuleType::Lifetime:
        case AuthoringModuleType::InitialSize:
        case AuthoringModuleType::InitialVelocity:
        case AuthoringModuleType::InitialRadialVelocity:
        case AuthoringModuleType::VelocityCone:
        case AuthoringModuleType::SourceMotionVelocity:
        case AuthoringModuleType::Acceleration:
        case AuthoringModuleType::Drag:
        case AuthoringModuleType::VelocityOverLife:
        case AuthoringModuleType::InitialRotation:
        case AuthoringModuleType::RotationOverLife:
        case AuthoringModuleType::SpriteTilt:
        case AuthoringModuleType::SpriteTiltOverLife:
        case AuthoringModuleType::InitialRotationRate:
        case AuthoringModuleType::RotationRateScaleByLife:
        case AuthoringModuleType::InitialColor:
        case AuthoringModuleType::ColorOverLife:
        case AuthoringModuleType::SubUVFrameOverLife:
        case AuthoringModuleType::SizeByLife:
        case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        case AuthoringModuleType::MaterialScalarModulation:
            return true;
        default:
            return false;
        }
    }

    const AuthoringModuleMetadata& metadata = Get_AuthoringModuleMetadata(type);
    if (emitter.typeData.kind == AuthoringTypeDataKind::Beam)
        return metadata.supportsBeam;

    if (emitter.typeData.kind == AuthoringTypeDataKind::Mesh)
    {
        switch (type)
        {
        case AuthoringModuleType::InitialSize:
        case AuthoringModuleType::InitialRotation:
        case AuthoringModuleType::RotationOverLife:
        case AuthoringModuleType::InitialRotationRate:
        case AuthoringModuleType::RotationRateScaleByLife:
        case AuthoringModuleType::SizeByLife:
            return false;
        default:
            break;
        }
    }

    switch (type)
    {
    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        return emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail;
    case AuthoringModuleType::SourceMotionVelocity:
        return emitter.typeData.kind == AuthoringTypeDataKind::None ||
               emitter.typeData.kind == AuthoringTypeDataKind::Mesh ||
               emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail;
    case AuthoringModuleType::CylinderLocation:
        return emitter.typeData.kind == AuthoringTypeDataKind::None ||
               emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    case AuthoringModuleType::CylinderOrientation:
        return emitter.typeData.kind == AuthoringTypeDataKind::None ||
               emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    case AuthoringModuleType::SpriteTilt:
    case AuthoringModuleType::SpriteTiltOverLife:
        return emitter.typeData.kind == AuthoringTypeDataKind::None ||
               emitter.typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail;
    case AuthoringModuleType::InitialMeshSize:
    case AuthoringModuleType::InitialMeshRotation:
    case AuthoringModuleType::MeshRotationOverLife:
    case AuthoringModuleType::MeshDirectionAlignOverLife:
    case AuthoringModuleType::InitialMeshRotationRate:
    case AuthoringModuleType::MeshRotationRateScaleByLife:
    case AuthoringModuleType::MeshSizeByLife:
    case AuthoringModuleType::SphereRadialOrientation:
        return emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    default:
        break;
    }

    if (emitter.typeData.kind == AuthoringTypeDataKind::Trail)
        return metadata.supportsTrail;

    return metadata.supportsSprite;
}

bool Emitter_View::Can_AddModuleType(const AuthoringEmitter& emitter, AuthoringModuleType type) const
{
    switch (type)
    {
    case AuthoringModuleType::Required:
    case AuthoringModuleType::Spawn:
        return false;

    case AuthoringModuleType::Lifetime:
    case AuthoringModuleType::InitialLocation:
    case AuthoringModuleType::SphereLocation:
    case AuthoringModuleType::PlaneRadialLocation:
    case AuthoringModuleType::CylinderLocation:
    case AuthoringModuleType::InitialSize:
    case AuthoringModuleType::InitialMeshSize:
    case AuthoringModuleType::InitialVelocity:
    case AuthoringModuleType::InitialRadialVelocity:
    case AuthoringModuleType::VelocityCone:
    case AuthoringModuleType::SourceMotionVelocity:
    case AuthoringModuleType::Acceleration:
    case AuthoringModuleType::Drag:
    case AuthoringModuleType::VelocityOverLife:
    case AuthoringModuleType::OrbitOverLife:
    case AuthoringModuleType::InitialRotation:
    case AuthoringModuleType::SphereRadialOrientation:
    case AuthoringModuleType::PlaneRadialOrientation:
    case AuthoringModuleType::CylinderOrientation:
    case AuthoringModuleType::RotationOverLife:
    case AuthoringModuleType::SpriteTilt:
    case AuthoringModuleType::SpriteTiltOverLife:
    case AuthoringModuleType::InitialRotationRate:
    case AuthoringModuleType::RotationRateScaleByLife:
    case AuthoringModuleType::InitialMeshRotation:
    case AuthoringModuleType::MeshRotationOverLife:
    case AuthoringModuleType::MeshDirectionAlignOverLife:
    case AuthoringModuleType::InitialMeshRotationRate:
    case AuthoringModuleType::MeshRotationRateScaleByLife:
    case AuthoringModuleType::InitialColor:
    case AuthoringModuleType::ColorOverLife:
    case AuthoringModuleType::SubUVFrameOverLife:
    case AuthoringModuleType::SizeByLife:
    case AuthoringModuleType::BeamEnvelopeOverLife:
    case AuthoringModuleType::MeshSizeByLife:
    case AuthoringModuleType::SpawnPerUnit:
    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
    case AuthoringModuleType::RibbonOrientation:
    case AuthoringModuleType::MaterialScalarModulation:
        if (type == AuthoringModuleType::VelocityOverLife)
            return Is_ModuleCompatibleWithEmitter(emitter, type);
        if (type == AuthoringModuleType::SourceHistorySpriteTrailPathFollow &&
            Has_ModuleType(emitter, AuthoringModuleType::SourceHistorySpriteTrailPathReplay))
            return false;
        if (type == AuthoringModuleType::SourceHistorySpriteTrailPathReplay &&
            Has_ModuleType(emitter, AuthoringModuleType::SourceHistorySpriteTrailPathFollow))
            return false;
        return Is_ModuleCompatibleWithEmitter(emitter, type) && !Has_ModuleType(emitter, type);

    default:
        return false;
    }
}

void Emitter_View::Add_Module(size_t emitterIndex, AuthoringModuleType type)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (!Can_AddModuleType(emitter, type))
        return;

    AuthoringModule module{};
    switch (type)
    {
    case AuthoringModuleType::Lifetime:
        module = Make_LifetimeModule();
        break;
    case AuthoringModuleType::InitialLocation:
        module = Make_InitialLocationModule();
        break;
    case AuthoringModuleType::SphereLocation:
        module = Make_SphereLocationModule();
        break;
    case AuthoringModuleType::PlaneRadialLocation:
        module = Make_PlaneRadialLocationModule();
        break;
    case AuthoringModuleType::CylinderLocation:
        module = Make_CylinderLocationModule();
        break;
    case AuthoringModuleType::InitialSize:
        module = Make_InitialSizeModule();
        break;
    case AuthoringModuleType::InitialMeshSize:
        module = Make_InitialMeshSizeModule();
        break;
    case AuthoringModuleType::InitialVelocity:
        module = Make_InitialVelocityModule();
        break;
    case AuthoringModuleType::InitialRadialVelocity:
        module = Make_InitialRadialVelocityModule();
        break;
    case AuthoringModuleType::VelocityCone:
        module = Make_VelocityConeModule();
        break;
    case AuthoringModuleType::SourceMotionVelocity:
        module = Make_SourceMotionVelocityModule();
        break;
    case AuthoringModuleType::Acceleration:
        module = Make_AccelerationModule();
        break;
    case AuthoringModuleType::Drag:
        module = Make_DragModule();
        break;
    case AuthoringModuleType::VelocityOverLife:
        module = Make_VelocityOverLifeModule();
        break;
    case AuthoringModuleType::OrbitOverLife:
        module = Make_OrbitOverLifeModule();
        break;
    case AuthoringModuleType::InitialRotation:
        module = Make_InitialRotationModule();
        break;
    case AuthoringModuleType::SphereRadialOrientation:
        module = Make_SphereRadialOrientationModule();
        break;
    case AuthoringModuleType::PlaneRadialOrientation:
        module = Make_PlaneRadialOrientationModule();
        break;
    case AuthoringModuleType::CylinderOrientation:
        module = Make_CylinderOrientationModule();
        break;
    case AuthoringModuleType::RotationOverLife:
        module = Make_RotationOverLifeModule();
        break;
    case AuthoringModuleType::SpriteTilt:
        module = Make_SpriteTiltModule();
        break;
    case AuthoringModuleType::SpriteTiltOverLife:
        module = Make_SpriteTiltOverLifeModule();
        break;
    case AuthoringModuleType::InitialRotationRate:
        module = Make_InitialRotationRateModule();
        break;
    case AuthoringModuleType::RotationRateScaleByLife:
        module = Make_RotationRateScaleByLifeModule();
        break;
    case AuthoringModuleType::InitialMeshRotation:
        module = Make_InitialMeshRotationModule();
        break;
    case AuthoringModuleType::MeshRotationOverLife:
        module = Make_MeshRotationOverLifeModule();
        break;
    case AuthoringModuleType::MeshDirectionAlignOverLife:
        module = Make_MeshDirectionAlignOverLifeModule();
        break;
    case AuthoringModuleType::InitialMeshRotationRate:
        module = Make_InitialMeshRotationRateModule();
        break;
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        module = Make_MeshRotationRateScaleByLifeModule();
        break;
    case AuthoringModuleType::InitialColor:
        module = Make_InitialColorModule();
        break;
    case AuthoringModuleType::ColorOverLife:
        module = Make_ColorOverLifeModule();
        break;
    case AuthoringModuleType::SubUVFrameOverLife:
        module = Make_SubUVFrameOverLifeModule();
        break;
    case AuthoringModuleType::SizeByLife:
        module = Make_SizeByLifeModule();
        break;
    case AuthoringModuleType::BeamEnvelopeOverLife:
        module = Make_BeamEnvelopeOverLifeModule();
        break;
    case AuthoringModuleType::MeshSizeByLife:
        module = Make_MeshSizeByLifeModule();
        break;
    case AuthoringModuleType::SpawnPerUnit:
        module = Make_SpawnPerUnitModule();
        break;
    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        module = Make_SourceHistorySpriteTrailPathFollowModule();
        break;
    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        module = Make_SourceHistorySpriteTrailPathReplayModule();
        break;
    case AuthoringModuleType::RibbonOrientation:
        module = Make_RibbonOrientationModule();
        break;
    case AuthoringModuleType::MaterialScalarModulation:
        module = Make_MaterialScalarModulationModule();
        break;
    default:
        return;
    }

    emitter.modules.push_back(std::move(module));
    emitter.previewDirty = true;
    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex = emitter.modules.size() - 1;
    Sync_SelectionContext();
    MarkDirty();
}

vector<Emitter_View::ModulePickerEntry> Emitter_View::Build_ModulePickerEntries(const AuthoringEmitter& emitter) const
{
    const auto makeEntry = [&](
        AuthoringModuleType moduleType,
        bool implemented,
        bool canAdd,
        string incompatibleText)
    {
        ModulePickerEntry entry{};
        entry.moduleType = moduleType;
        entry.displayName = Get_AuthoringModuleMetadata(moduleType).displayName;
        entry.implemented = implemented;
        entry.compatible = Is_ModuleCompatibleWithEmitter(emitter, moduleType);
        entry.alreadyAdded = Has_ModuleType(emitter, moduleType);
        entry.canAdd = canAdd;

        if (!entry.implemented)
            entry.stateText = "미구현";
        else if (!entry.compatible)
        {
            entry.stateText = incompatibleText.empty() ? "현재 TypeData와 비호환" : std::move(incompatibleText);
            entry.tooltipText = "현재 emitter TypeData에서는 이 모듈이 preview/runtime 입력으로 소비되지 않습니다.";
        }
        else if (entry.alreadyAdded)
            entry.stateText = "이미 장착됨";
        else
            entry.stateText = "추가 가능";

        return entry;
    };

    vector<ModulePickerEntry> entries{};
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Lifetime,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Lifetime),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialSize,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialSize),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialMeshSize,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialMeshSize),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialLocation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialLocation),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SphereLocation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SphereLocation),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::PlaneRadialLocation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::PlaneRadialLocation),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::CylinderLocation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::CylinderLocation),
            "Sprite/Mesh 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialVelocity,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialVelocity),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRadialVelocity,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRadialVelocity),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::VelocityCone,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::VelocityCone),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SourceMotionVelocity,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SourceMotionVelocity),
            "Sprite / Mesh / Sprite Trail TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Acceleration,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Acceleration),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Drag,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Drag),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::VelocityOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::VelocityOverLife),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::OrbitOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::OrbitOverLife),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRotation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRotation),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SphereRadialOrientation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SphereRadialOrientation),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::PlaneRadialOrientation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::PlaneRadialOrientation),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::CylinderOrientation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::CylinderOrientation),
            "Sprite/Mesh 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::RotationOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::RotationOverLife),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SpriteTilt,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SpriteTilt),
            "Sprite / Sprite Trail TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SpriteTiltOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SpriteTiltOverLife),
            "Sprite / Sprite Trail TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRotationRate,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRotationRate),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::RotationRateScaleByLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::RotationRateScaleByLife),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialMeshRotation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialMeshRotation),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MeshRotationOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MeshRotationOverLife),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialMeshRotationRate,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialMeshRotationRate),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MeshRotationRateScaleByLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MeshRotationRateScaleByLife),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialColor,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialColor),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::ColorOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::ColorOverLife),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SubUVFrameOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SubUVFrameOverLife),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SizeByLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SizeByLife),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::BeamEnvelopeOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::BeamEnvelopeOverLife),
            "Beam TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MeshSizeByLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MeshSizeByLife),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SpawnPerUnit,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SpawnPerUnit),
            ""
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SourceHistorySpriteTrailPathFollow,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SourceHistorySpriteTrailPathFollow),
            "Sprite Trail TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SourceHistorySpriteTrailPathReplay,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SourceHistorySpriteTrailPathReplay),
            "Sprite Trail TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MeshDirectionAlignOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MeshDirectionAlignOverLife),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::RibbonOrientation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::RibbonOrientation),
            "Ribbon TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MaterialScalarModulation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MaterialScalarModulation),
            ""
        )
    );

    ranges::sort(
        entries,
        [](const ModulePickerEntry& lhs, const ModulePickerEntry& rhs)
        {
            return String::ToString(lhs.displayName) < String::ToString(rhs.displayName);
        }
    );

    return entries;
}

void Emitter_View::Close_ModulePicker()
{
    _modulePickerEmitterIndex.reset();
    _modulePickerPendingOpen = false;
    _modulePickerSearchBuffer[0] = '\0';
}

void Emitter_View::Draw_ModulePickerPopup()
{
    constexpr auto modalPopupId = "새 모듈##ModuleAddPopup";

    if (_modulePickerPendingOpen)
    {
        ImGui::OpenPopup(modalPopupId);
        _modulePickerPendingOpen = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(kModulePickerModalWidth, kModulePickerModalHeight), ImGuiCond_Appearing);

    const bool isPopupOpen = ImGui::IsPopupOpen(modalPopupId);
    bool isModalOpen = true;
    if (!ImGui::BeginPopupModal(modalPopupId, &isModalOpen))
    {
        if (!isPopupOpen)
            Close_ModulePicker();
        return;
    }

    if (!_modulePickerEmitterIndex.has_value() || _modulePickerEmitterIndex.value() >= _emitters.size())
    {
        ImGui::EndPopup();
        Close_ModulePicker();
        return;
    }

    const AuthoringEmitter& emitter = _emitters[_modulePickerEmitterIndex.value()];
    const string searchLower = String::ToLowerCopy(string(_modulePickerSearchBuffer));
    const vector<ModulePickerEntry> entries = Build_ModulePickerEntries(emitter);

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##ModulePickerSearch", "모듈 검색", _modulePickerSearchBuffer, IM_ARRAYSIZE(_modulePickerSearchBuffer));
    ImGui::Spacing();

    if (ImGui::BeginChild("##ModulePickerList", ImVec2(0.f, 0.f), true))
    {
        if (ImGui::BeginTable("##ModulePickerTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthStretch, kModulePickerNameColumnStretch);
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, kModulePickerStateColumnStretch);

            for (const ModulePickerEntry& entry : entries)
            {
                const string displayName = String::ToString(entry.displayName);
                if (!searchLower.empty() &&
                    String::ToLowerCopy(displayName).find(searchLower) == string::npos)
                    continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (!entry.canAdd)
                    ImGui::BeginDisabled();

                const bool useUnsupportedPickerColor = !entry.compatible;
                if (useUnsupportedPickerColor)
                    ImGui::PushStyleColor(ImGuiCol_Text, To_ImVec4(kEmitterUnsupportedModuleTextColor));

                const bool picked = ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                const bool isModuleNameHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
                if (useUnsupportedPickerColor)
                    ImGui::PopStyleColor();

                if (isModuleNameHovered && !entry.tooltipText.empty())
                    ImGui::SetTooltip("%s", entry.tooltipText.c_str());

                if (!entry.canAdd)
                    ImGui::EndDisabled();

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", entry.stateText.c_str());

                if (!picked || !entry.canAdd || !entry.moduleType.has_value())
                    continue;

                const size_t emitterIndex = _modulePickerEmitterIndex.value();
                const AuthoringModuleType moduleType = entry.moduleType.value();
                Execute_AuthoringEdit(
                    "Add Module",
                    [this, emitterIndex, moduleType]
                    {
                        Add_Module(emitterIndex, moduleType);
                    }
                );

                ImGui::CloseCurrentPopup();
                Close_ModulePicker();
                break;
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }

    ImGui::EndPopup();

    if (!isModalOpen)
    {
        ImGui::CloseCurrentPopup();
        Close_ModulePicker();
    }
}

void Emitter_View::Open_ModulePicker(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    _modulePickerEmitterIndex = emitterIndex;
    _modulePickerPendingOpen = true;
    _modulePickerSearchBuffer[0] = '\0';
}

NS_END
