#include "EffectMaterialPresetReader.h"

#include "GameInstance.h"

namespace EffectEditor::EffectMaterialPresetReader
{
bool Read(const fs::path& filePath, EffectMaterialInstanceData& outMaterial)
{
    outMaterial = EffectMaterialInstanceData{};

    if (!fs::exists(filePath))
        return false;

    ifstream file{ filePath };
    if (!file.is_open())
        return false;

    json root{};
    try
    {
        file >> root;
    }
    catch (...)
    {
        return false;
    }

    const auto read_string = [&](const char* key, const string& defaultValue) -> string
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;

        return iter->get<string>();
    };

    const auto read_float = [&](const char* key, float defaultValue) -> float
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_number())
            return defaultValue;

        return iter->get<float>();
    };

    const auto read_uint = [&](const char* key, uint32 defaultValue) -> uint32
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_number_unsigned())
            return defaultValue;

        return iter->get<uint32>();
    };

    const auto read_bool = [&](const char* key, bool defaultValue) -> bool
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_boolean())
            return defaultValue;

        return iter->get<bool>();
    };

    const auto read_uv_tiling_mode = [&](const char* key, EffectTextureUVTilingMode defaultValue) -> EffectTextureUVTilingMode
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;

        const optional<EffectTextureUVTilingMode> parsed = magic_enum::enum_cast<EffectTextureUVTilingMode>(iter->get<string>());
        return parsed.value_or(defaultValue);
    };

    const auto resolve_compatible_uv_tiling_mode = [](const EffectMaterialUVAxisPolicy& policy) -> EffectTextureUVTilingMode
    {
        return policy.uPolicy == policy.vPolicy ? policy.uPolicy : EffectTextureUVTilingMode::Wrap;
    };

    const auto read_uv_axis_policy = [&](
        const char* key,
        EffectTextureUVTilingMode legacyMode,
        EffectMaterialUVAxisPolicy) -> EffectMaterialUVAxisPolicy
    {
        EffectMaterialUVAxisPolicy policy{ legacyMode, legacyMode };
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_object())
            return policy;

        const auto read_policy_axis = [](const json& node, const char* axisKey, EffectTextureUVTilingMode fallbackValue)
        {
            const auto axisIter = node.find(axisKey);
            if (axisIter == node.end() || !axisIter->is_string())
                return fallbackValue;

            const optional<EffectTextureUVTilingMode> parsed = magic_enum::enum_cast<EffectTextureUVTilingMode>(axisIter->get<string>());
            return parsed.value_or(fallbackValue);
        };

        policy.uPolicy = read_policy_axis(*iter, "uPolicy", policy.uPolicy);
        policy.vPolicy = read_policy_axis(*iter, "vPolicy", policy.vPolicy);
        return policy;
    };

    const auto read_blend_mode = [&](const char* key, EffectMaterialBlendMode defaultValue) -> EffectMaterialBlendMode
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;

        const optional<EffectMaterialBlendMode> parsed = magic_enum::enum_cast<EffectMaterialBlendMode>(iter->get<string>());
        return parsed.value_or(defaultValue);
    };

    const auto read_material_family = [&](const char* key, EffectMaterialFamily defaultValue) -> EffectMaterialFamily
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;

        const optional<EffectMaterialFamily> parsed = magic_enum::enum_cast<EffectMaterialFamily>(iter->get<string>());
        return parsed.value_or(defaultValue);
    };

    const auto read_distortion_shape_mode = [&](const char* key, EffectDistortionShapeMode defaultValue) -> EffectDistortionShapeMode
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;

        const optional<EffectDistortionShapeMode> parsed = magic_enum::enum_cast<EffectDistortionShapeMode>(iter->get<string>());
        return parsed.value_or(defaultValue);
    };

    const auto read_uv_rotation = [&](const char* key, EffectMaterialUVRotation defaultValue) -> EffectMaterialUVRotation
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;

        const optional<EffectMaterialUVRotation> parsed = magic_enum::enum_cast<EffectMaterialUVRotation>(iter->get<string>());
        return parsed.value_or(defaultValue);
    };

    const auto read_additive_color_source = [](
        const json& node,
        const char* key,
        EffectMaterialAdditiveColorSource defaultValue) -> EffectMaterialAdditiveColorSource
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return defaultValue;

        const optional<EffectMaterialAdditiveColorSource> parsed =
            magic_enum::enum_cast<EffectMaterialAdditiveColorSource>(iter->get<string>());
        return parsed.value_or(defaultValue);
    };

    const auto read_additive_amount_source = [](
        const json& node,
        const char* key,
        EffectMaterialAdditiveAmountSource defaultValue) -> EffectMaterialAdditiveAmountSource
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return defaultValue;

        const optional<EffectMaterialAdditiveAmountSource> parsed =
            magic_enum::enum_cast<EffectMaterialAdditiveAmountSource>(iter->get<string>());
        return parsed.value_or(defaultValue);
    };

    const auto read_additive_coverage_policy = [](
        const json& node,
        const char* key,
        EffectMaterialAdditiveCoveragePolicy defaultValue) -> EffectMaterialAdditiveCoveragePolicy
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return defaultValue;

        const optional<EffectMaterialAdditiveCoveragePolicy> parsed =
            magic_enum::enum_cast<EffectMaterialAdditiveCoveragePolicy>(iter->get<string>());
        return parsed.value_or(defaultValue);
    };

    const auto read_vec2 = [&](const char* key, const Vec2& defaultValue) -> Vec2
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_array() || iter->size() < 2)
            return defaultValue;

        return Vec2{
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y
        };
    };

    const auto read_vec3 = [&](const char* key, const Vec3& defaultValue) -> Vec3
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_array() || iter->size() < 3)
            return defaultValue;

        return Vec3{
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y,
            iter->at(2).is_number() ? iter->at(2).get<float>() : defaultValue.z
        };
    };

    const auto read_color_node = [](const json& node, const Color& defaultValue) -> Color
    {
        if (node.is_object())
        {
            return Color(
                node.value("r", defaultValue.x),
                node.value("g", defaultValue.y),
                node.value("b", defaultValue.z),
                node.value("a", defaultValue.w)
            );
        }

        if (node.is_array() && node.size() >= 4)
        {
            return Color(
                node.at(0).is_number() ? node.at(0).get<float>() : defaultValue.x,
                node.at(1).is_number() ? node.at(1).get<float>() : defaultValue.y,
                node.at(2).is_number() ? node.at(2).get<float>() : defaultValue.z,
                node.at(3).is_number() ? node.at(3).get<float>() : defaultValue.w
            );
        }

        return defaultValue;
    };

    const auto read_color = [&](const char* key, const Color& defaultValue) -> Color
    {
        const auto iter = root.find(key);
        return iter == root.end() ? defaultValue : read_color_node(*iter, defaultValue);
    };

    const auto resolve_texture_path_by_path = [](const string& texturePath) -> wstring
    {
        if (texturePath.empty())
            return {};

        fs::path candidatePath = String::ToWString(texturePath);
        if (candidatePath.is_relative() && GAME != nullptr)
            candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

        candidatePath = candidatePath.lexically_normal();
        return fs::exists(candidatePath) ? candidatePath.wstring() : wstring{};
    };

    const auto migrate_legacy_texture_id = [](const string& textureId, string& outGuid, string& outPath) -> bool
    {
        if (textureId == "Texture_Effect_DefaultTexture")
        {
            outGuid.clear();
            outPath = "Effects/Textures/Shared/DefaultTexture.dds";
            return true;
        }

        if (textureId == "Texture_Effect_Clouds4")
        {
            outGuid.clear();
            outPath = "Effects/Textures/Shared/clouds4_T.dds";
            return true;
        }

        return false;
    };

    const auto fill_texture_guid_from_path = [&](string& textureGuid, const string& texturePath)
    {
        if (!textureGuid.empty() || GAME == nullptr)
            return;

        const wstring resolvedPath = resolve_texture_path_by_path(texturePath);
        if (resolvedPath.empty())
            return;

        textureGuid = GAME->Ensure_AssetGUID(resolvedPath, "Texture");
    };

    const auto fill_texture_path_hint_from_guid = [](const string& textureGuid, string& texturePath)
    {
        if (!texturePath.empty() || GAME == nullptr || textureGuid.empty())
            return;

        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (assetMeta == nullptr || assetMeta->type != "Texture" || assetMeta->relativePath.empty())
            return;

        texturePath = String::ToString(assetMeta->relativePath);
        ranges::replace(texturePath, '\\', '/');
    };

    const EffectMaterialInstanceData defaultMaterial{};
    EffectMaterialInstanceData material = defaultMaterial;

    material.sourcePresetName = ResolveDisplayName(filePath);
    material.materialFamily = read_material_family("materialFamily", defaultMaterial.materialFamily);
    material.mainTextureGuid = read_string("mainTextureGuid", defaultMaterial.mainTextureGuid);
    material.mainTexturePath = read_string("mainTexturePath", defaultMaterial.mainTexturePath);
    material.noiseTextureGuid = read_string("noiseTextureGuid", defaultMaterial.noiseTextureGuid);
    material.noiseTexturePath = read_string("noiseTexturePath", defaultMaterial.noiseTexturePath);
    material.maskTextureGuid = read_string("maskTextureGuid", defaultMaterial.maskTextureGuid);
    material.maskTexturePath = read_string("maskTexturePath", defaultMaterial.maskTexturePath);
    material.flowTextureGuid = read_string("flowTextureGuid", defaultMaterial.flowTextureGuid);
    material.flowTexturePath = read_string("flowTexturePath", defaultMaterial.flowTexturePath);

    if (material.mainTextureGuid.empty() && material.mainTexturePath.empty())
    {
        const string legacyMainTextureId = read_string("mainTextureId", {});
        migrate_legacy_texture_id(legacyMainTextureId, material.mainTextureGuid, material.mainTexturePath);
    }
    if (material.noiseTextureGuid.empty() && material.noiseTexturePath.empty())
    {
        const string legacyNoiseTextureId = read_string("noiseTextureId", {});
        migrate_legacy_texture_id(legacyNoiseTextureId, material.noiseTextureGuid, material.noiseTexturePath);
    }

    fill_texture_guid_from_path(material.mainTextureGuid, material.mainTexturePath);
    fill_texture_guid_from_path(material.noiseTextureGuid, material.noiseTexturePath);
    fill_texture_guid_from_path(material.maskTextureGuid, material.maskTexturePath);
    fill_texture_guid_from_path(material.flowTextureGuid, material.flowTexturePath);
    fill_texture_path_hint_from_guid(material.mainTextureGuid, material.mainTexturePath);
    fill_texture_path_hint_from_guid(material.noiseTextureGuid, material.noiseTexturePath);
    fill_texture_path_hint_from_guid(material.maskTextureGuid, material.maskTexturePath);
    fill_texture_path_hint_from_guid(material.flowTextureGuid, material.flowTexturePath);

    material.tint = read_color("tint", defaultMaterial.tint);
    material.intensity = read_float("intensity", defaultMaterial.intensity);
    material.opacityPower = read_float("opacityPower", defaultMaterial.opacityPower);
    material.alphaMultiplier = read_float("alphaMultiplier", defaultMaterial.alphaMultiplier);
    material.noiseStrength = read_float("noiseStrength", defaultMaterial.noiseStrength);
    material.alphaCutoff = read_float("alphaCutoff", defaultMaterial.alphaCutoff);
    material.alphaErosion = read_float("alphaErosion", defaultMaterial.alphaErosion);
    material.noiseSource = read_string("noiseSource", defaultMaterial.noiseSource);
    material.maskSource = read_string("maskSource", defaultMaterial.maskSource);
    material.noiseInvert = root.value("noiseInvert", defaultMaterial.noiseInvert);
    material.maskInvert = root.value("maskInvert", defaultMaterial.maskInvert);
    material.mainUVScale = read_vec2("mainUVScale", defaultMaterial.mainUVScale);
    material.mainUVOffset = read_vec2("mainUVOffset", defaultMaterial.mainUVOffset);
    material.mainUVScrollSpeed = read_vec2("mainUVScrollSpeed", defaultMaterial.mainUVScrollSpeed);
    material.mainUVTilingMode = read_uv_tiling_mode("mainUVTilingMode", defaultMaterial.mainUVTilingMode);
    material.mainUVPolicy = read_uv_axis_policy("mainUVPolicy", material.mainUVTilingMode, defaultMaterial.mainUVPolicy);
    material.mainUVTilingMode = resolve_compatible_uv_tiling_mode(material.mainUVPolicy);
    material.mainUVRotation = read_uv_rotation("mainUVRotation", defaultMaterial.mainUVRotation);
    material.noiseUVScale = read_vec2("noiseUVScale", defaultMaterial.noiseUVScale);
    material.noiseUVOffset = read_vec2("noiseUVOffset", defaultMaterial.noiseUVOffset);
    material.noiseUVScrollSpeed = read_vec2("noiseUVScrollSpeed", defaultMaterial.noiseUVScrollSpeed);
    material.noiseUVTilingMode = read_uv_tiling_mode("noiseUVTilingMode", defaultMaterial.noiseUVTilingMode);
    material.noiseUVPolicy = read_uv_axis_policy("noiseUVPolicy", material.noiseUVTilingMode, defaultMaterial.noiseUVPolicy);
    material.noiseUVTilingMode = resolve_compatible_uv_tiling_mode(material.noiseUVPolicy);
    material.noiseUVRotation = read_uv_rotation("noiseUVRotation", defaultMaterial.noiseUVRotation);
    material.maskUVScale = read_vec2("maskUVScale", defaultMaterial.maskUVScale);
    material.maskUVOffset = read_vec2("maskUVOffset", defaultMaterial.maskUVOffset);
    material.maskUVScrollSpeed = read_vec2("maskUVScrollSpeed", defaultMaterial.maskUVScrollSpeed);
    material.maskUVTilingMode = read_uv_tiling_mode("maskUVTilingMode", defaultMaterial.maskUVTilingMode);
    material.maskUVPolicy = read_uv_axis_policy("maskUVPolicy", material.maskUVTilingMode, defaultMaterial.maskUVPolicy);
    material.maskUVTilingMode = resolve_compatible_uv_tiling_mode(material.maskUVPolicy);
    material.maskUVRotation = read_uv_rotation("maskUVRotation", defaultMaterial.maskUVRotation);
    material.flowUVScale = read_vec2("flowUVScale", defaultMaterial.flowUVScale);
    material.flowUVOffset = read_vec2("flowUVOffset", defaultMaterial.flowUVOffset);
    material.flowUVScrollSpeed = read_vec2("flowUVScrollSpeed", defaultMaterial.flowUVScrollSpeed);
    material.flowUVTilingMode = read_uv_tiling_mode("flowUVTilingMode", defaultMaterial.flowUVTilingMode);
    material.flowUVPolicy = read_uv_axis_policy("flowUVPolicy", material.flowUVTilingMode, defaultMaterial.flowUVPolicy);
    material.flowUVTilingMode = resolve_compatible_uv_tiling_mode(material.flowUVPolicy);
    material.flowUVRotation = read_uv_rotation("flowUVRotation", defaultMaterial.flowUVRotation);
    material.blendMode = read_blend_mode("blendMode", defaultMaterial.blendMode);
    material.opacitySource = read_string("opacitySource", defaultMaterial.opacitySource);

    if (const auto coreEmissiveIter = root.find("coreEmissive"); coreEmissiveIter != root.end() && coreEmissiveIter->is_object())
    {
        const json& coreEmissive = *coreEmissiveIter;
        material.coreEmissive.enabled = coreEmissive.value("enabled", defaultMaterial.coreEmissive.enabled);
        if (const auto coreColorIter = coreEmissive.find("coreColor"); coreColorIter != coreEmissive.end())
            material.coreEmissive.coreColor = read_color_node(*coreColorIter, defaultMaterial.coreEmissive.coreColor);
        if (const auto corePowerIter = coreEmissive.find("corePower"); corePowerIter != coreEmissive.end() && corePowerIter->is_number())
            material.coreEmissive.corePower = corePowerIter->get<float>();
        if (const auto coreIntensityIter = coreEmissive.find("coreIntensity"); coreIntensityIter != coreEmissive.end() && coreIntensityIter->is_number())
            material.coreEmissive.coreIntensity = coreIntensityIter->get<float>();
        if (const auto outerPowerIter = coreEmissive.find("outerPower"); outerPowerIter != coreEmissive.end() && outerPowerIter->is_number())
            material.coreEmissive.outerPower = outerPowerIter->get<float>();
        if (const auto outerIntensityIter = coreEmissive.find("outerIntensity"); outerIntensityIter != coreEmissive.end() && outerIntensityIter->is_number())
            material.coreEmissive.outerIntensity = outerIntensityIter->get<float>();
    }

    material.refractionIntensity = read_float("refractionIntensity", defaultMaterial.refractionIntensity);
    material.refractionPresence = read_float("refractionPresence", defaultMaterial.refractionPresence);
    material.distortionShapeMode = read_distortion_shape_mode("distortionShapeMode", defaultMaterial.distortionShapeMode);
    const auto read_air_sheath_interpretation = [&](const char* key, EffectAirSheathMapInterpretation defaultValue)
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;
        return magic_enum::enum_cast<EffectAirSheathMapInterpretation>(iter->get<string>()).value_or(defaultValue);
    };
    const auto read_air_sheath_scalar_source = [&](const char* key, EffectAirSheathMapScalarSource defaultValue)
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;
        return magic_enum::enum_cast<EffectAirSheathMapScalarSource>(iter->get<string>()).value_or(defaultValue);
    };
    const auto read_air_sheath_vector_space = [&](const char* key, EffectAirSheathMapVectorSpace defaultValue)
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;
        return magic_enum::enum_cast<EffectAirSheathMapVectorSpace>(iter->get<string>()).value_or(defaultValue);
    };
    const auto read_air_sheath_composition = [&](const char* key, EffectAirSheathMapComposition defaultValue)
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;
        return magic_enum::enum_cast<EffectAirSheathMapComposition>(iter->get<string>()).value_or(defaultValue);
    };
    material.airSheathMapInterpretation = read_air_sheath_interpretation("airSheathMapInterpretation", defaultMaterial.airSheathMapInterpretation);
    material.airSheathMapXSource = read_air_sheath_scalar_source("airSheathMapXSource", defaultMaterial.airSheathMapXSource);
    material.airSheathMapYSource = read_air_sheath_scalar_source("airSheathMapYSource", defaultMaterial.airSheathMapYSource);
    material.airSheathMapVectorSpace = read_air_sheath_vector_space("airSheathMapVectorSpace", defaultMaterial.airSheathMapVectorSpace);
    material.airSheathMapComposition = read_air_sheath_composition("airSheathMapComposition", defaultMaterial.airSheathMapComposition);
    material.airSheathMapInfluence = read_float("airSheathMapInfluence", defaultMaterial.airSheathMapInfluence);
    material.distortionShapeRadius = read_float("distortionShapeRadius", defaultMaterial.distortionShapeRadius);
    material.distortionShapeThickness = read_float("distortionShapeThickness", defaultMaterial.distortionShapeThickness);
    material.distortionShapeSoftness = read_float("distortionShapeSoftness", defaultMaterial.distortionShapeSoftness);
    material.glassAlpha = read_float("glassAlpha", defaultMaterial.glassAlpha);
    material.glassAlphaPower = read_float("glassAlphaPower", defaultMaterial.glassAlphaPower);
    material.glassNormalStrength = read_float("glassNormalStrength", defaultMaterial.glassNormalStrength);
    material.glassRimColor = read_color("glassRimColor", defaultMaterial.glassRimColor);
    material.glassRimIntensity = read_float("glassRimIntensity", defaultMaterial.glassRimIntensity);
    material.glassRimPower = read_float("glassRimPower", defaultMaterial.glassRimPower);
    material.glassLightDirection = read_vec3("glassLightDirection", defaultMaterial.glassLightDirection);
    material.glassLightColor = read_color("glassLightColor", defaultMaterial.glassLightColor);
    material.glassLightIntensity = read_float("glassLightIntensity", defaultMaterial.glassLightIntensity);
    material.glassSpecularPower = read_float("glassSpecularPower", defaultMaterial.glassSpecularPower);
    material.glassSpecularSoftness = read_float("glassSpecularSoftness", defaultMaterial.glassSpecularSoftness);
    material.glassMainInfluence = read_float("glassMainInfluence", defaultMaterial.glassMainInfluence);
    material.glassNoiseBreakup = read_float("glassNoiseBreakup", defaultMaterial.glassNoiseBreakup);
    material.glassMaskStrength = read_float("glassMaskStrength", defaultMaterial.glassMaskStrength);

    if (const auto additiveIter = root.find("additive"); additiveIter != root.end() && additiveIter->is_object())
    {
        const json& additive = *additiveIter;
        material.additive.colorSource = read_additive_color_source(additive, "colorSource", defaultMaterial.additive.colorSource);
        material.additive.amountSource = read_additive_amount_source(additive, "amountSource", defaultMaterial.additive.amountSource);
        material.additive.coveragePolicy = read_additive_coverage_policy(additive, "coveragePolicy", defaultMaterial.additive.coveragePolicy);

        if (const auto intensityScaleIter = additive.find("intensityScale"); intensityScaleIter != additive.end() && intensityScaleIter->is_number())
            material.additive.intensityScale = intensityScaleIter->get<float>();
        if (const auto blackNeutralIter = additive.find("blackNeutral"); blackNeutralIter != additive.end() && blackNeutralIter->is_boolean())
            material.additive.blackNeutral = blackNeutralIter->get<bool>();
        if (const auto emissiveColorIter = additive.find("emissiveColor"); emissiveColorIter != additive.end())
            material.additive.emissiveColor = read_color_node(*emissiveColorIter, defaultMaterial.additive.emissiveColor);
        if (const auto constantColorIter = additive.find("constantColor"); constantColorIter != additive.end())
            material.additive.constantColor = read_color_node(*constantColorIter, defaultMaterial.additive.constantColor);
    }
    material.twoSided = read_bool("twoSided", defaultMaterial.twoSided);
    material.subUVRows = max(1u, read_uint("subUVRows", defaultMaterial.subUVRows));
    material.subUVCols = max(1u, read_uint("subUVCols", defaultMaterial.subUVCols));

    outMaterial = material;
    return true;
}

string ResolveDisplayName(const fs::path& filePath)
{
    string fileName = String::ToString(filePath.filename().wstring());
    constexpr auto suffix = ".effectmaterial.json";
    if (fileName.ends_with(suffix))
        fileName.erase(fileName.size() - strlen(suffix));

    return fileName;
}
}
