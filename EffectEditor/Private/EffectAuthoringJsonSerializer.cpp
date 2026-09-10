#include "EffectAuthoringJsonSerializer.h"

#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"

NS_BEGIN(EffectEditor)

namespace
{
    namespace Codec
    {
        json To_Json(const EffectMaterialInstanceData& data);
        void From_Json(const json& root, EffectMaterialInstanceData& outData);

        //## Codec::Common

        template <typename EnumType>
        string To_EnumToken(EnumType value)
        {
            return string(magic_enum::enum_name(value));
        }

        template <typename EnumType>
        bool Try_ReadEnumToken(const json& node, EnumType& outValue)
        {
            if (!node.is_string())
                return false;

            const optional<EnumType> parsed = magic_enum::enum_cast<EnumType>(node.get<string>());
            if (!parsed.has_value())
                return false;

            outValue = parsed.value();
            return true;
        }

        uint32 To_VelocityOverLifeApplyChannelMask(VelocityOverLifeApplyChannel channel)
        {
            switch (channel)
            {
            case VelocityOverLifeApplyChannel::InitialVelocity:
                return kVelocityOverLifeApplyChannelInitialVelocityMask;
            case VelocityOverLifeApplyChannel::InitialRadialVelocity:
                return kVelocityOverLifeApplyChannelInitialRadialVelocityMask;
            case VelocityOverLifeApplyChannel::VelocityCone:
                return kVelocityOverLifeApplyChannelVelocityConeMask;
            case VelocityOverLifeApplyChannel::SourceMotionVelocity:
                return kVelocityOverLifeApplyChannelSourceMotionVelocityMask;
            case VelocityOverLifeApplyChannel::AccelerationIntegratedVelocity:
                return kVelocityOverLifeApplyChannelAccelerationIntegratedVelocityMask;
            default:
                return 0u;
            }
        }

        json To_JsonVelocityOverLifeApplyChannels(uint32 mask)
        {
            json channels = json::array();
            const VelocityOverLifeApplyChannel candidates[] = {
                VelocityOverLifeApplyChannel::InitialVelocity,
                VelocityOverLifeApplyChannel::InitialRadialVelocity,
                VelocityOverLifeApplyChannel::VelocityCone,
                VelocityOverLifeApplyChannel::SourceMotionVelocity,
                VelocityOverLifeApplyChannel::AccelerationIntegratedVelocity,
            };

            for (const VelocityOverLifeApplyChannel channel : candidates)
            {
                if ((mask & To_VelocityOverLifeApplyChannelMask(channel)) != 0u)
                    channels.push_back(To_EnumToken(channel));
            }

            return channels;
        }

        uint32 Read_VelocityOverLifeApplyChannelMask(const json& root)
        {
            const auto iter = root.find("applyChannels");
            if (iter == root.end() || !iter->is_array())
                return kVelocityOverLifeApplyChannelDefaultMask;

            uint32 mask = 0u;
            for (const json& channelNode : *iter)
            {
                VelocityOverLifeApplyChannel channel{};
                if (Try_ReadEnumToken(channelNode, channel))
                    mask |= To_VelocityOverLifeApplyChannelMask(channel);
            }

            return mask != 0u ? mask : kVelocityOverLifeApplyChannelDefaultMask;
        }

        EffectTextureUVTilingMode Resolve_CompatibleUVTilingMode(const EffectMaterialUVAxisPolicy& policy)
        {
            return policy.uPolicy == policy.vPolicy ? policy.uPolicy : EffectTextureUVTilingMode::Wrap;
        }

        json To_Json(const EffectMaterialUVAxisPolicy& data)
        {
            json root = json::object();
            root["uPolicy"] = To_EnumToken(data.uPolicy);
            root["vPolicy"] = To_EnumToken(data.vPolicy);
            return root;
        }

        void Read_UVAxisPolicyField(
            const json& root,
            const char* key,
            EffectTextureUVTilingMode legacyMode,
            EffectMaterialUVAxisPolicy& outPolicy,
            EffectTextureUVTilingMode& outCompatibleMode)
        {
            outPolicy.uPolicy = legacyMode;
            outPolicy.vPolicy = legacyMode;

            const auto policyIter = root.find(key);
            if (policyIter != root.end() && policyIter->is_object())
            {
                const json& policy = *policyIter;
                if (const auto uPolicyIter = policy.find("uPolicy"); uPolicyIter != policy.end())
                    Try_ReadEnumToken(*uPolicyIter, outPolicy.uPolicy);
                if (const auto vPolicyIter = policy.find("vPolicy"); vPolicyIter != policy.end())
                    Try_ReadEnumToken(*vPolicyIter, outPolicy.vPolicy);
            }

            outCompatibleMode = Resolve_CompatibleUVTilingMode(outPolicy);
        }

        template <typename T>
        void Read_NumberField(const json& node, const char* key, T& outValue)
        {
            const auto iter = node.find(key);
            if (iter == node.end() || !iter->is_number())
                return;

            outValue = iter->get<T>();
        }

        EmitterSortPolicy Resolve_SortPolicyFromLegacyMode(EmitterSortMode sortMode)
        {
            switch (sortMode)
            {
            case EmitterSortMode::ViewProjDepth:
            case EmitterSortMode::DistanceToView:
                return EmitterSortPolicy::EmitterDepth;
            case EmitterSortMode::None:
            case EmitterSortMode::AgeOldestFirst:
            case EmitterSortMode::AgeNewestFirst:
            default:
                return EmitterSortPolicy::None;
            }
        }

        void Read_BoolField(const json& node, const char* key, bool& outValue)
        {
            const auto iter = node.find(key);
            if (iter == node.end() || !iter->is_boolean())
                return;

            outValue = iter->get<bool>();
        }

        SourceHistorySpriteTrailArrivalMode Resolve_SourceHistorySpriteTrailArrivalMode(const json& root)
        {
            SourceHistorySpriteTrailArrivalMode arrivalMode = SourceHistorySpriteTrailArrivalMode::KillOnArrive;
            if (const auto arrivalModeIter = root.find("arrivalMode"); arrivalModeIter != root.end() &&
                                                                       Try_ReadEnumToken(*arrivalModeIter, arrivalMode))
                return arrivalMode;

            bool killOnArrive = true;
            bool clampAtEnd = true;
            Read_BoolField(root, "killOnArrive", killOnArrive);
            Read_BoolField(root, "clampAtEnd", clampAtEnd);
            if (!killOnArrive && clampAtEnd)
                return SourceHistorySpriteTrailArrivalMode::ClampAtEnd;

            return SourceHistorySpriteTrailArrivalMode::KillOnArrive;
        }

        json To_Json(const DistributionRandomSeedData& data)
        {
            json root = json::object();
            root["mode"] = To_EnumToken(data.mode);
            root["manualSeed"] = data.manualSeed;
            root["useInstanceSeed"] = data.useInstanceSeed;
            return root;
        }

        void From_Json(const json& root, DistributionRandomSeedData& outData)
        {
            outData = DistributionRandomSeedData{};
            if (!root.is_object())
                return;

            const auto modeIter = root.find("mode");
            if (modeIter != root.end())
                Try_ReadEnumToken(*modeIter, outData.mode);
            Read_NumberField(root, "manualSeed", outData.manualSeed);
            Read_BoolField(root, "useInstanceSeed", outData.useInstanceSeed);
        }

        void Write_RandomSeedField(json& root, const DistributionRandomSeedData& data)
        {
            root["randomSeed"] = To_Json(data);
        }

        void Read_RandomSeedField(const json& root, DistributionRandomSeedData& outData)
        {
            outData = DistributionRandomSeedData{};
            const auto iter = root.find("randomSeed");
            if (iter != root.end())
                From_Json(*iter, outData);
        }

        bool Has_RandomSeedField(const json& root)
        {
            const auto iter = root.find("randomSeed");
            return iter != root.end() && iter->is_object();
        }

        bool Is_DefaultRandomSeed(const DistributionRandomSeedData& data)
        {
            return data.mode == DistributionRandomSeedMode::Default &&
                   data.manualSeed == 0u &&
                   !data.useInstanceSeed;
        }

        void Apply_LegacyRandomSeedToUniform(FloatDistributionData& data, const DistributionRandomSeedData& legacySeed)
        {
            if (auto* uniform = get_if<UniformFloatDistributionData>(&data.payload);
                data.mode == DistributionMode::Uniform && uniform != nullptr && Is_DefaultRandomSeed(uniform->randomSeed))
                uniform->randomSeed = legacySeed;
        }

        void Apply_LegacyRandomSeedToUniform(Vector2DistributionData& data, const DistributionRandomSeedData& legacySeed)
        {
            if (auto* uniform = get_if<UniformVector2DistributionData>(&data.payload);
                data.mode == DistributionMode::Uniform && uniform != nullptr && Is_DefaultRandomSeed(uniform->randomSeed))
                uniform->randomSeed = legacySeed;
        }

        void Apply_LegacyRandomSeedToUniform(Vector3DistributionData& data, const DistributionRandomSeedData& legacySeed)
        {
            if (auto* uniform = get_if<UniformVector3DistributionData>(&data.payload);
                data.mode == DistributionMode::Uniform && uniform != nullptr && Is_DefaultRandomSeed(uniform->randomSeed))
                uniform->randomSeed = legacySeed;
        }

        void Apply_LegacyRandomSeedToUniform(ColorDistributionData& data, const DistributionRandomSeedData& legacySeed)
        {
            if (auto* uniform = get_if<UniformColorDistributionData>(&data.payload);
                data.mode == DistributionMode::Uniform && uniform != nullptr && Is_DefaultRandomSeed(uniform->randomSeed))
                uniform->randomSeed = legacySeed;
        }

        void Read_StringField(const json& node, const char* key, string& outValue)
        {
            const auto iter = node.find(key);
            if (iter == node.end() || !iter->is_string())
                return;

            outValue = iter->get<string>();
        }

        bool Try_MigrateLegacyTextureId(const string& textureId, string& outGuid, string& outPath)
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
        }

        wstring Resolve_TexturePathByGuid(const string& textureGuid)
        {
            if (GAME == nullptr || textureGuid.empty())
                return {};

            const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
            if (assetMeta == nullptr || assetMeta->type != "Texture")
                return {};

            const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
            return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
        }

        wstring Resolve_TexturePathByPath(const string& texturePath)
        {
            if (texturePath.empty())
                return {};

            fs::path candidatePath = String::ToWString(texturePath);
            if (candidatePath.is_relative() && GAME != nullptr)
                candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

            candidatePath = candidatePath.lexically_normal();
            return fs::exists(candidatePath) ? candidatePath.wstring() : wstring{};
        }

        string Make_TextureGuidForSave(const string& textureGuid, const string& texturePath)
        {
            if (!textureGuid.empty())
                return textureGuid;

            if (GAME == nullptr)
                return {};

            const wstring resolvedPath = Resolve_TexturePathByPath(texturePath);
            return resolvedPath.empty() ? string{} : GAME->Ensure_AssetGUID(resolvedPath, "Texture");
        }

        void Fill_TextureGuidFromPath(string& textureGuid, const string& texturePath)
        {
            if (!textureGuid.empty() || GAME == nullptr)
                return;

            const wstring resolvedPath = Resolve_TexturePathByPath(texturePath);
            if (resolvedPath.empty())
                return;

            textureGuid = GAME->Ensure_AssetGUID(resolvedPath, "Texture");
        }

        void Fill_TexturePathHintFromGuid(const string& textureGuid, string& texturePath)
        {
            if (!texturePath.empty())
                return;

            const wstring resolvedPath = Resolve_TexturePathByGuid(textureGuid);
            if (resolvedPath.empty() || GAME == nullptr)
                return;

            const fs::path assetRoot = fs::path(GAME->Get_AssetRoot()).lexically_normal();
            const fs::path relativePath = fs::path(resolvedPath).lexically_normal().lexically_relative(assetRoot);
            if (relativePath.empty() || relativePath.native().starts_with(L".."))
                return;

            texturePath = String::ToString(relativePath.wstring());
            ranges::replace(texturePath, '\\', '/');
        }

        //## Codec::PrimitiveValue

        json To_Json(const Vec2& value)
        {
            return json::array({ value.x, value.y });
        }

        json To_Json(const Vec3& value)
        {
            return json::array({ value.x, value.y, value.z });
        }

        json To_Json(const Color& value)
        {
            return json::array({ value.x, value.y, value.z, value.w });
        }

        void Read_Vec2(const json& node, Vec2& outValue)
        {
            if (!node.is_array() || node.size() < 2 || !node[0].is_number() || !node[1].is_number())
                return;

            outValue.x = node[0].get<float>();
            outValue.y = node[1].get<float>();
        }

        void Read_Vec3(const json& node, Vec3& outValue)
        {
            if (!node.is_array() || node.size() < 3 || !node[0].is_number() || !node[1].is_number() || !node[2].is_number())
                return;

            outValue.x = node[0].get<float>();
            outValue.y = node[1].get<float>();
            outValue.z = node[2].get<float>();
        }

        void Read_Color(const json& node, Color& outValue)
        {
            if (!node.is_array() || node.size() < 4 ||
                !node[0].is_number() || !node[1].is_number() || !node[2].is_number() || !node[3].is_number())
                return;

            outValue.x = node[0].get<float>();
            outValue.y = node[1].get<float>();
            outValue.z = node[2].get<float>();
            outValue.w = node[3].get<float>();
        }

        bool Is_SameColor(const Color& lhs, const Color& rhs)
        {
            return
                lhs.x == rhs.x &&
                lhs.y == rhs.y &&
                lhs.z == rhs.z &&
                lhs.w == rhs.w;
        }

        bool Is_DefaultAdditiveContribution(const EffectMaterialAdditiveContributionData& data)
        {
            const EffectMaterialAdditiveContributionData defaultData{};
            return
                data.colorSource == defaultData.colorSource &&
                data.amountSource == defaultData.amountSource &&
                data.coveragePolicy == defaultData.coveragePolicy &&
                data.intensityScale == defaultData.intensityScale &&
                data.blackNeutral == defaultData.blackNeutral &&
                Is_SameColor(data.emissiveColor, defaultData.emissiveColor) &&
                Is_SameColor(data.constantColor, defaultData.constantColor);
        }

        bool Is_DefaultCoreEmissive(const EffectMaterialCoreEmissiveData& data)
        {
            const EffectMaterialCoreEmissiveData defaultData{};
            return
                data.enabled == defaultData.enabled &&
                Is_SameColor(data.coreColor, defaultData.coreColor) &&
                data.corePower == defaultData.corePower &&
                data.coreIntensity == defaultData.coreIntensity &&
                data.outerPower == defaultData.outerPower &&
                data.outerIntensity == defaultData.outerIntensity;
        }

        json To_Json(const EffectMaterialCoreEmissiveData& data)
        {
            json root = json::object();
            root["enabled"] = data.enabled;
            root["coreColor"] = To_Json(data.coreColor);
            root["corePower"] = data.corePower;
            root["coreIntensity"] = data.coreIntensity;
            root["outerPower"] = data.outerPower;
            root["outerIntensity"] = data.outerIntensity;
            return root;
        }

        void From_Json(const json& root, EffectMaterialCoreEmissiveData& outData)
        {
            if (!root.is_object())
                return;

            Read_BoolField(root, "enabled", outData.enabled);
            if (const auto coreColorIter = root.find("coreColor"); coreColorIter != root.end())
                Read_Color(*coreColorIter, outData.coreColor);
            Read_NumberField(root, "corePower", outData.corePower);
            Read_NumberField(root, "coreIntensity", outData.coreIntensity);
            Read_NumberField(root, "outerPower", outData.outerPower);
            Read_NumberField(root, "outerIntensity", outData.outerIntensity);
        }

        json To_Json(const EffectMaterialAdditiveContributionData& data)
        {
            json root = json::object();
            root["colorSource"] = To_EnumToken(data.colorSource);
            root["amountSource"] = To_EnumToken(data.amountSource);
            root["coveragePolicy"] = To_EnumToken(data.coveragePolicy);
            root["intensityScale"] = data.intensityScale;
            root["blackNeutral"] = data.blackNeutral;
            root["emissiveColor"] = To_Json(data.emissiveColor);
            root["constantColor"] = To_Json(data.constantColor);
            return root;
        }

        void From_Json(const json& root, EffectMaterialAdditiveContributionData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto colorSourceIter = root.find("colorSource"); colorSourceIter != root.end())
                Try_ReadEnumToken(*colorSourceIter, outData.colorSource);
            if (const auto amountSourceIter = root.find("amountSource"); amountSourceIter != root.end())
                Try_ReadEnumToken(*amountSourceIter, outData.amountSource);
            if (const auto coveragePolicyIter = root.find("coveragePolicy"); coveragePolicyIter != root.end())
                Try_ReadEnumToken(*coveragePolicyIter, outData.coveragePolicy);

            Read_NumberField(root, "intensityScale", outData.intensityScale);
            Read_BoolField(root, "blackNeutral", outData.blackNeutral);

            if (const auto emissiveColorIter = root.find("emissiveColor"); emissiveColorIter != root.end())
                Read_Color(*emissiveColorIter, outData.emissiveColor);
            if (const auto constantColorIter = root.find("constantColor"); constantColorIter != root.end())
                Read_Color(*constantColorIter, outData.constantColor);
        }

        json To_Json(const ParticleSystemAuthoringData& data)
        {
            json root = json::object();
            root["autoDeactivate"] = data.autoDeactivate;
            return root;
        }

        void From_Json(const json& root, ParticleSystemAuthoringData& outData)
        {
            if (!root.is_object())
                return;

            Read_BoolField(root, "autoDeactivate", outData.autoDeactivate);
        }

        json To_Json(const TrailTypeData& data)
        {
            json root = json::object();
            root["width"] = data.width;
            root["segmentLifetime"] = data.segmentLifetime;
            root["historyCount"] = data.historyCount;
            root["uvTiling"] = data.uvTiling;
            root["maxTrailLength"] = data.maxTrailLength;
            root["tailFadeLength"] = data.tailFadeLength;
            root["autoLifeFade"] = data.autoLifeFade;
            root["curveQuality"] = To_EnumToken(data.curveQuality);
            root["sampleSpacing"] = data.sampleSpacing;
            root["curveSubdivision"] = data.curveSubdivision;
            root["smoothTangent"] = data.smoothTangent;
            root["sideFade"] = data.sideFade;
            return root;
        }

        void From_Json(const json& root, TrailTypeData& outData)
        {
            if (!root.is_object())
                return;

            Read_NumberField(root, "width", outData.width);
            Read_NumberField(root, "segmentLifetime", outData.segmentLifetime);
            Read_NumberField(root, "historyCount", outData.historyCount);
            Read_NumberField(root, "uvTiling", outData.uvTiling);
            Read_NumberField(root, "maxTrailLength", outData.maxTrailLength);
            Read_NumberField(root, "tailFadeLength", outData.tailFadeLength);
            Read_BoolField(root, "autoLifeFade", outData.autoLifeFade);
            const auto curveQualityIter = root.find("curveQuality");
            if (curveQualityIter != root.end())
                Try_ReadEnumToken(*curveQualityIter, outData.curveQuality);
            Read_NumberField(root, "sampleSpacing", outData.sampleSpacing);
            Read_NumberField(root, "curveSubdivision", outData.curveSubdivision);
            Read_BoolField(root, "smoothTangent", outData.smoothTangent);
            Read_NumberField(root, "sideFade", outData.sideFade);
        }

        void From_Json(const json& root, BeamTypeData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto endpointModeIter = root.find("endpointMode"); endpointModeIter != root.end())
                Try_ReadEnumToken(*endpointModeIter, outData.endpointMode);
            if (const auto localStartIter = root.find("localStart"); localStartIter != root.end())
                Read_Vec3(*localStartIter, outData.localStart);
            if (const auto localEndIter = root.find("localEnd"); localEndIter != root.end())
                Read_Vec3(*localEndIter, outData.localEnd);
            if (const auto localDirectionIter = root.find("localDirection"); localDirectionIter != root.end())
                Read_Vec3(*localDirectionIter, outData.localDirection);
            Read_NumberField(root, "length", outData.length);
            Read_NumberField(root, "segmentCount", outData.segmentCount);
            Read_NumberField(root, "noiseAmplitude", outData.noiseAmplitude);
            Read_NumberField(root, "seed", outData.seed);
            Read_NumberField(root, "stripCount", outData.stripCount);
            Read_NumberField(root, "endSpreadRadius", outData.endSpreadRadius);
            Read_NumberField(root, "lengthVariance", outData.lengthVariance);
            if (const auto branchPresetIter = root.find("branchPreset"); branchPresetIter != root.end())
                Try_ReadEnumToken(*branchPresetIter, outData.branchPreset);
            Read_BoolField(root, "branchEnabled", outData.branchEnabled);
            Read_NumberField(root, "branchCount", outData.branchCount);
            Read_NumberField(root, "branchChance", outData.branchChance);
            Read_NumberField(root, "branchSegmentCount", outData.branchSegmentCount);
            Read_NumberField(root, "branchLength", outData.branchLength);
            Read_NumberField(root, "branchLengthVariance", outData.branchLengthVariance);
            Read_NumberField(root, "branchStartMin", outData.branchStartMin);
            Read_NumberField(root, "branchStartMax", outData.branchStartMax);
            Read_NumberField(root, "branchSpreadRadius", outData.branchSpreadRadius);
            if (const auto endSpreadIter = root.find("branchEndSpreadRadius"); endSpreadIter != root.end())
                Read_NumberField(root, "branchEndSpreadRadius", outData.branchEndSpreadRadius);
            else
                outData.branchEndSpreadRadius = outData.branchSpreadRadius;
            Read_NumberField(root, "branchOutwardAmount", outData.branchOutwardAmount);
            Read_NumberField(root, "branchCurveAmount", outData.branchCurveAmount);
            Read_NumberField(root, "branchDownLength", outData.branchDownLength);
            Read_NumberField(root, "branchEntangleRadius", outData.branchEntangleRadius);
            Read_NumberField(root, "branchEntangleAdvance", outData.branchEntangleAdvance);
            Read_NumberField(root, "branchCrackLength", outData.branchCrackLength);
            Read_NumberField(root, "branchCrackSpreadRadius", outData.branchCrackSpreadRadius);
            Read_NumberField(root, "branchWidthScale", outData.branchWidthScale);
            Read_NumberField(root, "branchSeedOffset", outData.branchSeedOffset);
            Read_NumberField(root, "baseWidth", outData.baseWidth);
            Read_NumberField(root, "tilingDistance", outData.tilingDistance);

            outData.length = max(0.f, outData.length);
            outData.segmentCount = max(1u, outData.segmentCount);
            outData.noiseAmplitude = max(0.f, outData.noiseAmplitude);
            outData.stripCount = max(1u, outData.stripCount);
            outData.endSpreadRadius = max(0.f, outData.endSpreadRadius);
            outData.lengthVariance = max(0.f, outData.lengthVariance);
            outData.branchChance = clamp(outData.branchChance, 0.f, 1.f);
            outData.branchSegmentCount = max(1u, outData.branchSegmentCount);
            outData.branchLength = max(0.f, outData.branchLength);
            outData.branchLengthVariance = max(0.f, outData.branchLengthVariance);
            outData.branchStartMin = clamp(outData.branchStartMin, 0.f, 1.f);
            outData.branchStartMax = clamp(outData.branchStartMax, 0.f, 1.f);
            if (outData.branchStartMin > outData.branchStartMax)
                swap(outData.branchStartMin, outData.branchStartMax);
            outData.branchSpreadRadius = max(0.f, outData.branchSpreadRadius);
            outData.branchEndSpreadRadius = max(0.f, outData.branchEndSpreadRadius);
            outData.branchOutwardAmount = max(0.f, outData.branchOutwardAmount);
            outData.branchCurveAmount = max(0.f, outData.branchCurveAmount);
            outData.branchDownLength = max(0.f, outData.branchDownLength);
            outData.branchEntangleRadius = max(0.f, outData.branchEntangleRadius);
            outData.branchEntangleAdvance = max(0.f, outData.branchEntangleAdvance);
            outData.branchCrackLength = max(0.f, outData.branchCrackLength);
            outData.branchCrackSpreadRadius = max(0.f, outData.branchCrackSpreadRadius);
            outData.branchWidthScale = max(0.f, outData.branchWidthScale);
            outData.baseWidth = max(0.001f, outData.baseWidth);
            outData.tilingDistance = max(0.f, outData.tilingDistance);
        }

        bool Nearly_Equal(float lhs, float rhs)
        {
            return fabsf(lhs - rhs) <= 0.0001f;
        }

        bool Is_DefaultHistoryBudget(const HistoryBudgetData& data)
        {
            const HistoryBudgetData defaults{};
            return data.preset == defaults.preset && data.sourceGroups.empty();
        }

        bool Is_DefaultEmitterHistoryBudget(const EmitterHistoryBudgetData& data)
        {
            const EmitterHistoryBudgetData defaults{};
            return data.priority == defaults.priority &&
                   data.densityBias == defaults.densityBias &&
                   Nearly_Equal(data.lengthScale, defaults.lengthScale);
        }

        json To_Json(const HistoryBudgetData& data)
        {
            json root = json::object();
            root["preset"] = To_EnumToken(data.preset);
            return root;
        }

        void From_Json(const json& root, HistoryBudgetData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto presetIter = root.find("preset"); presetIter != root.end())
                Try_ReadEnumToken(*presetIter, outData.preset);
            Read_BoolField(root, "preserveLength", outData.preserveLength);
            Read_BoolField(root, "sharedSourceHistory", outData.sharedSourceHistory);
            outData.sharedSourceHistory = true;
            Read_NumberField(root, "trailDensityScale", outData.trailDensityScale);
            Read_NumberField(root, "spriteStampDensityScale", outData.spriteStampDensityScale);
            Read_NumberField(root, "ribbonDensityScale", outData.ribbonDensityScale);
            Read_NumberField(root, "distortionDensityScale", outData.distortionDensityScale);
            if (const auto updateRateIter = root.find("updateRate"); updateRateIter != root.end())
                Try_ReadEnumToken(*updateRateIter, outData.updateRate);

            outData.trailDensityScale = max(0.f, outData.trailDensityScale);
            outData.spriteStampDensityScale = max(0.f, outData.spriteStampDensityScale);
            outData.ribbonDensityScale = max(0.f, outData.ribbonDensityScale);
            outData.distortionDensityScale = max(0.f, outData.distortionDensityScale);
        }

        json To_Json(const HistorySourceGroupBudgetData& data)
        {
            json root = json::object();
            root["kind"] = To_EnumToken(data.kind);
            root["stableId"] = data.stableId;
            root["confirmed"] = data.confirmed;
            root["sourceSampleCount"] = data.sourceSampleCount;
            root["sampleSpacing"] = data.sampleSpacing;
            root["curveSubdivision"] = data.curveSubdivision;
            root["smoothTangent"] = data.smoothTangent;

            if (data.kind == EffectHistorySourceGroupKind::SourcePointHistory)
            {
                if (data.sampleLifetime > 0.f)
                    root["sampleLifetime"] = data.sampleLifetime;
                if (data.sampleInterval > 0.f)
                    root["sampleInterval"] = data.sampleInterval;
            }

            return root;
        }

        void From_Json(const json& root, HistorySourceGroupBudgetData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto kindIter = root.find("kind"); kindIter != root.end())
                Try_ReadEnumToken(*kindIter, outData.kind);
            outData.stableId = root.value("stableId", outData.stableId);
            Read_BoolField(root, "confirmed", outData.confirmed);
            Read_NumberField(root, "sourceSampleCount", outData.sourceSampleCount);
            Read_NumberField(root, "sampleLifetime", outData.sampleLifetime);
            Read_NumberField(root, "sampleSpacing", outData.sampleSpacing);
            Read_NumberField(root, "sampleInterval", outData.sampleInterval);
            Read_NumberField(root, "curveSubdivision", outData.curveSubdivision);
            Read_BoolField(root, "smoothTangent", outData.smoothTangent);

            outData.sourceSampleCount = max(0u, outData.sourceSampleCount);
            outData.sampleLifetime = max(0.f, outData.sampleLifetime);
            outData.sampleSpacing = max(0.f, outData.sampleSpacing);
            outData.sampleInterval = max(0.f, outData.sampleInterval);
            outData.curveSubdivision = max(0u, outData.curveSubdivision);
        }

        json To_Json(const EmitterHistoryBudgetData& data)
        {
            json root = json::object();
            root["priority"] = To_EnumToken(data.priority);
            root["densityBias"] = To_EnumToken(data.densityBias);
            root["lengthScale"] = data.lengthScale;
            return root;
        }

        void From_Json(const json& root, EmitterHistoryBudgetData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto priorityIter = root.find("priority"); priorityIter != root.end())
                Try_ReadEnumToken(*priorityIter, outData.priority);
            if (const auto densityBiasIter = root.find("densityBias"); densityBiasIter != root.end())
                Try_ReadEnumToken(*densityBiasIter, outData.densityBias);
            Read_NumberField(root, "lengthScale", outData.lengthScale);
            outData.lengthScale = max(0.f, outData.lengthScale);
        }

        RibbonCurveQuality Infer_RibbonCurveQuality(float sampleSpacing, float sampleInterval)
        {
            if (!Nearly_Equal(sampleInterval, 0.f))
                return RibbonCurveQuality::Custom;

            if (Nearly_Equal(sampleSpacing, 0.08f))
                return RibbonCurveQuality::Basic;
            if (Nearly_Equal(sampleSpacing, 0.04f))
                return RibbonCurveQuality::Smooth;
            if (Nearly_Equal(sampleSpacing, 0.02f))
                return RibbonCurveQuality::HighQuality;

            return RibbonCurveQuality::Custom;
        }

        uint32 Resolve_SourceHistoryCurveSubdivisionPreset(RibbonCurveQuality quality)
        {
            switch (quality)
            {
            case RibbonCurveQuality::Smooth:
                return 8u;
            case RibbonCurveQuality::HighQuality:
                return 12u;
            case RibbonCurveQuality::Basic:
            case RibbonCurveQuality::Custom:
            default:
                return 4u;
            }
        }

        json To_Json(const RibbonTypeData& data)
        {
            json root = json::object();
            root["sourceMode"] = To_EnumToken(data.sourceMode);
            root["sourceEmitterId"] = data.sourceEmitterId;
            root["followerLaneCount"] = data.followerLaneCount;
            root["curveQuality"] = To_EnumToken(data.curveQuality);
            root["maxSampleCount"] = data.maxSampleCount;
            root["sampleSpacing"] = data.sampleSpacing;
            root["curveSubdivision"] = data.curveSubdivision;
            root["smoothTangent"] = data.smoothTangent;
            root["sampleInterval"] = data.sampleInterval;
            root["maxLength"] = data.maxLength;
            root["tailFadeLength"] = data.tailFadeLength;
            root["laneSpawnFadeInEnabled"] = data.laneSpawnFadeInEnabled;
            root["laneSpawnFadeInDuration"] = data.laneSpawnFadeInDuration;
            root["baseWidth"] = data.baseWidth;
            root["tilingDistance"] = data.tilingDistance;
            root["autoLifeFade"] = data.autoLifeFade;
            root["tailCollapseOnIdle"] = data.tailCollapseOnIdle;
            root["tailCollapseSpeed"] = data.tailCollapseSpeed;
            root["useManualRoll"] = data.useManualRoll;
            root["manualRollDegrees"] = data.manualRollDegrees;
            return root;
        }

        json To_Json(const SourceHistorySpriteTrailTypeData& data)
        {
            json root = json::object();
            root["sourceMode"] = To_EnumToken(data.sourceMode);
            root["sourceEmitterId"] = data.sourceEmitterId;
            root["followerLaneCount"] = data.followerLaneCount;
            root["sampleLifetime"] = data.sampleLifetime;
            root["curveQuality"] = To_EnumToken(data.curveQuality);
            root["sampleSpacing"] = data.sampleSpacing;
            root["curveSubdivision"] = data.curveSubdivision;
            root["smoothTangent"] = data.smoothTangent;
            root["maxLength"] = data.maxLength;
            root["stampSpawnMode"] = To_EnumToken(data.stampSpawnMode);
            root["stampSpacing"] = data.stampSpacing;
            root["stampInterval"] = data.stampInterval;
            root["maxStampCount"] = data.maxStampCount;
            root["cardLength"] = data.cardLength;
            root["cardWidth"] = data.cardWidth;
            root["flipU"] = data.flipU;
            root["flipV"] = data.flipV;
            root["rotationOffsetDegrees"] = data.rotationOffsetDegrees;
            root["spawnJitter"] = data.spawnJitter;
            return root;
        }

        json To_Json(const BeamTypeData& data)
        {
            json root = json::object();
            root["endpointMode"] = To_EnumToken(data.endpointMode);
            root["localStart"] = To_Json(data.localStart);
            root["localEnd"] = To_Json(data.localEnd);
            root["localDirection"] = To_Json(data.localDirection);
            root["length"] = data.length;
            root["segmentCount"] = data.segmentCount;
            root["noiseAmplitude"] = data.noiseAmplitude;
            root["seed"] = data.seed;
            root["stripCount"] = data.stripCount;
            root["endSpreadRadius"] = data.endSpreadRadius;
            root["lengthVariance"] = data.lengthVariance;
            root["branchPreset"] = To_EnumToken(data.branchPreset);
            root["branchEnabled"] = data.branchEnabled;
            root["branchCount"] = data.branchCount;
            root["branchChance"] = data.branchChance;
            root["branchSegmentCount"] = data.branchSegmentCount;
            root["branchLength"] = data.branchLength;
            root["branchLengthVariance"] = data.branchLengthVariance;
            root["branchStartMin"] = data.branchStartMin;
            root["branchStartMax"] = data.branchStartMax;
            root["branchSpreadRadius"] = data.branchSpreadRadius;
            root["branchEndSpreadRadius"] = data.branchEndSpreadRadius;
            root["branchOutwardAmount"] = data.branchOutwardAmount;
            root["branchCurveAmount"] = data.branchCurveAmount;
            root["branchDownLength"] = data.branchDownLength;
            root["branchEntangleRadius"] = data.branchEntangleRadius;
            root["branchEntangleAdvance"] = data.branchEntangleAdvance;
            root["branchCrackLength"] = data.branchCrackLength;
            root["branchCrackSpreadRadius"] = data.branchCrackSpreadRadius;
            root["branchWidthScale"] = data.branchWidthScale;
            root["branchSeedOffset"] = data.branchSeedOffset;
            root["baseWidth"] = data.baseWidth;
            root["tilingDistance"] = data.tilingDistance;
            return root;
        }

        void From_Json(const json& root, RibbonTypeData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto sourceModeIter = root.find("sourceMode"); sourceModeIter != root.end())
                Try_ReadEnumToken(*sourceModeIter, outData.sourceMode);
            Read_NumberField(root, "sourceEmitterId", outData.sourceEmitterId);
            Read_NumberField(root, "followerLaneCount", outData.followerLaneCount);
            if (outData.sourceMode == SourceHistoryRibbonSourceMode::SourceEmitter)
            {
                outData.sourceMode = outData.sourceEmitterId != 0u
                                     ? SourceHistoryRibbonSourceMode::ParticleEmitter
                                     : SourceHistoryRibbonSourceMode::SelfRoot;
            }
            if (outData.sourceMode == SourceHistoryRibbonSourceMode::SelfRoot)
                outData.sourceEmitterId = 0u;
            outData.followerLaneCount = max(1u, outData.followerLaneCount);
            Read_NumberField(root, "maxSampleCount", outData.maxSampleCount);
            Read_NumberField(root, "sampleSpacing", outData.sampleSpacing);
            const bool hasCurveSubdivision = root.find("curveSubdivision") != root.end();
            Read_NumberField(root, "curveSubdivision", outData.curveSubdivision);
            Read_BoolField(root, "smoothTangent", outData.smoothTangent);
            Read_NumberField(root, "sampleInterval", outData.sampleInterval);
            const auto curveQualityIter = root.find("curveQuality");
            if (curveQualityIter != root.end())
                Try_ReadEnumToken(*curveQualityIter, outData.curveQuality);
            else
                outData.curveQuality = Infer_RibbonCurveQuality(outData.sampleSpacing, outData.sampleInterval);
            if (!hasCurveSubdivision && outData.curveQuality != RibbonCurveQuality::Custom)
                outData.curveSubdivision = Resolve_SourceHistoryCurveSubdivisionPreset(outData.curveQuality);
            Read_NumberField(root, "maxLength", outData.maxLength);
            Read_NumberField(root, "tailFadeLength", outData.tailFadeLength);
            Read_BoolField(root, "laneSpawnFadeInEnabled", outData.laneSpawnFadeInEnabled);
            Read_NumberField(root, "laneSpawnFadeInDuration", outData.laneSpawnFadeInDuration);
            outData.laneSpawnFadeInDuration = max(0.f, outData.laneSpawnFadeInDuration);
            Read_NumberField(root, "baseWidth", outData.baseWidth);
            Read_NumberField(root, "tilingDistance", outData.tilingDistance);
            Read_BoolField(root, "autoLifeFade", outData.autoLifeFade);
            Read_BoolField(root, "tailCollapseOnIdle", outData.tailCollapseOnIdle);
            Read_NumberField(root, "tailCollapseSpeed", outData.tailCollapseSpeed);
            outData.tailCollapseSpeed = max(0.f, outData.tailCollapseSpeed);
            Read_BoolField(root, "useManualRoll", outData.useManualRoll);
            Read_NumberField(root, "manualRollDegrees", outData.manualRollDegrees);
        }

        void From_Json(const json& root, SourceHistorySpriteTrailTypeData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto sourceModeIter = root.find("sourceMode"); sourceModeIter != root.end())
                Try_ReadEnumToken(*sourceModeIter, outData.sourceMode);
            Read_NumberField(root, "sourceEmitterId", outData.sourceEmitterId);
            Read_NumberField(root, "followerLaneCount", outData.followerLaneCount);
            if (outData.sourceMode == SourceHistoryRibbonSourceMode::SourceEmitter)
            {
                outData.sourceMode = outData.sourceEmitterId != 0u
                                     ? SourceHistoryRibbonSourceMode::ParticleEmitter
                                     : SourceHistoryRibbonSourceMode::SelfRoot;
            }
            if (outData.sourceMode == SourceHistoryRibbonSourceMode::SelfRoot)
                outData.sourceEmitterId = 0u;
            outData.followerLaneCount = max(1u, outData.followerLaneCount);

            Read_NumberField(root, "sampleLifetime", outData.sampleLifetime);
            outData.sampleLifetime = max(0.0001f, outData.sampleLifetime);
            Read_NumberField(root, "sampleSpacing", outData.sampleSpacing);
            outData.sampleSpacing = max(0.001f, outData.sampleSpacing);
            const auto curveQualityIter = root.find("curveQuality");
            if (curveQualityIter != root.end())
                Try_ReadEnumToken(*curveQualityIter, outData.curveQuality);
            else
                outData.curveQuality = Infer_RibbonCurveQuality(outData.sampleSpacing, 0.f);
            const bool hasCurveSubdivision = root.find("curveSubdivision") != root.end();
            Read_NumberField(root, "curveSubdivision", outData.curveSubdivision);
            Read_BoolField(root, "smoothTangent", outData.smoothTangent);
            if (!hasCurveSubdivision && outData.curveQuality != RibbonCurveQuality::Custom)
                outData.curveSubdivision = Resolve_SourceHistoryCurveSubdivisionPreset(outData.curveQuality);
            Read_NumberField(root, "maxLength", outData.maxLength);
            outData.maxLength = max(0.f, outData.maxLength);
            if (const auto stampSpawnModeIter = root.find("stampSpawnMode"); stampSpawnModeIter != root.end())
                Try_ReadEnumToken(*stampSpawnModeIter, outData.stampSpawnMode);
            Read_NumberField(root, "stampSpacing", outData.stampSpacing);
            outData.stampSpacing = max(0.001f, outData.stampSpacing);
            Read_NumberField(root, "stampInterval", outData.stampInterval);
            outData.stampInterval = max(0.001f, outData.stampInterval);
            Read_NumberField(root, "maxStampCount", outData.maxStampCount);
            outData.maxStampCount = max(1u, outData.maxStampCount);
            Read_NumberField(root, "cardLength", outData.cardLength);
            outData.cardLength = max(0.001f, outData.cardLength);
            Read_NumberField(root, "cardWidth", outData.cardWidth);
            outData.cardWidth = max(0.001f, outData.cardWidth);
            if (const auto directionModeIter = root.find("directionMode"); directionModeIter != root.end())
                Try_ReadEnumToken(*directionModeIter, outData.directionMode);
            if (const auto textureAxisIter = root.find("textureAxis"); textureAxisIter != root.end())
                Try_ReadEnumToken(*textureAxisIter, outData.textureAxis);
            Read_BoolField(root, "flipU", outData.flipU);
            Read_BoolField(root, "flipV", outData.flipV);
            Read_NumberField(root, "rotationOffsetDegrees", outData.rotationOffsetDegrees);
            Read_NumberField(root, "spawnJitter", outData.spawnJitter);
            outData.spawnJitter = max(0.f, outData.spawnJitter);
        }

        json To_Json(const MeshTypeData& data)
        {
            json root = json::object();
            root["modelGuid"] = data.modelGuid;
            root["modelPath"] = data.modelPath;
            root["assignedEffectMaterialGuid"] = data.assignedEffectMaterialGuid;
            root["assignedEffectMaterialPath"] = data.assignedEffectMaterialPath;
            root["hasAssignedMaterialInstance"] = data.hasAssignedMaterialInstance;
            if (data.hasAssignedMaterialInstance)
                root["assignedMaterial"] = To_Json(data.assignedMaterial);
            root["previewScale"] = To_Json(data.previewScale);
            root["useModelMaterials"] = data.useModelMaterials;
            return root;
        }

        void From_Json(const json& root, MeshTypeData& outData)
        {
            if (!root.is_object())
                return;

            Read_StringField(root, "modelGuid", outData.modelGuid);
            Read_StringField(root, "modelPath", outData.modelPath);
            Read_StringField(root, "assignedEffectMaterialGuid", outData.assignedEffectMaterialGuid);
            Read_StringField(root, "assignedEffectMaterialPath", outData.assignedEffectMaterialPath);
            Read_BoolField(root, "hasAssignedMaterialInstance", outData.hasAssignedMaterialInstance);
            if (const auto assignedMaterialIter = root.find("assignedMaterial");
                assignedMaterialIter != root.end() && assignedMaterialIter->is_object())
            {
                From_Json(*assignedMaterialIter, outData.assignedMaterial);
                outData.hasAssignedMaterialInstance = true;
            }
            if (const auto previewScaleIter = root.find("previewScale"); previewScaleIter != root.end())
                Read_Vec3(*previewScaleIter, outData.previewScale);
            Read_BoolField(root, "useModelMaterials", outData.useModelMaterials);
        }

        json To_Json(const AuthoringTypeData& data)
        {
            json root = json::object();
            root["kind"] = To_EnumToken(data.kind);

            switch (data.kind)
            {
            case AuthoringTypeDataKind::Trail:
            {
                const TrailTypeData* trailData = get_if<TrailTypeData>(&data.payload);
                root["data"] = To_Json(trailData != nullptr ? *trailData : TrailTypeData{});
                break;
            }
            case AuthoringTypeDataKind::Mesh:
            {
                const MeshTypeData* meshData = get_if<MeshTypeData>(&data.payload);
                root["data"] = To_Json(meshData != nullptr ? *meshData : MeshTypeData{});
                break;
            }
            case AuthoringTypeDataKind::Ribbon:
            {
                const RibbonTypeData* sourceHistoryRibbonData = get_if<RibbonTypeData>(&data.payload);
                root["data"] = To_Json(sourceHistoryRibbonData != nullptr ? *sourceHistoryRibbonData : RibbonTypeData{});
                break;
            }
            case AuthoringTypeDataKind::SourceHistorySpriteTrail:
            {
                const SourceHistorySpriteTrailTypeData* sourceHistorySpriteTrailData = get_if<SourceHistorySpriteTrailTypeData>(&data.payload);
                root["data"] = To_Json(sourceHistorySpriteTrailData != nullptr ? *sourceHistorySpriteTrailData : SourceHistorySpriteTrailTypeData{});
                break;
            }
            case AuthoringTypeDataKind::Beam:
            {
                const BeamTypeData* beamData = get_if<BeamTypeData>(&data.payload);
                root["data"] = To_Json(beamData != nullptr ? *beamData : BeamTypeData{});
                break;
            }
            case AuthoringTypeDataKind::None:
            default:
                break;
            }

            return root;
        }

        void From_Json(const json& root, AuthoringTypeData& outData)
        {
            if (!root.is_object())
                return;

            AuthoringTypeDataKind kind = AuthoringTypeDataKind::None;
            const auto kindIter = root.find("kind");

            if (kindIter == root.end() || !Try_ReadEnumToken(*kindIter, kind))
                return;

            switch (kind)
            {
            case AuthoringTypeDataKind::Trail:
            {
                TrailTypeData data{};
                const auto dataIter = root.find("data");
                if (dataIter != root.end())
                    From_Json(*dataIter, data);

                outData.kind = AuthoringTypeDataKind::Trail;
                outData.payload = data;
                break;
            }
            case AuthoringTypeDataKind::Mesh:
            {
                MeshTypeData data{};
                const auto dataIter = root.find("data");
                if (dataIter != root.end())
                    From_Json(*dataIter, data);

                outData.kind = AuthoringTypeDataKind::Mesh;
                outData.payload = data;
                break;
            }
            case AuthoringTypeDataKind::Ribbon:
            {
                RibbonTypeData data{};
                const auto dataIter = root.find("data");
                if (dataIter != root.end())
                    From_Json(*dataIter, data);

                outData.kind = AuthoringTypeDataKind::Ribbon;
                outData.payload = data;
                break;
            }
            case AuthoringTypeDataKind::SourceHistorySpriteTrail:
            {
                SourceHistorySpriteTrailTypeData data{};
                const auto dataIter = root.find("data");
                if (dataIter != root.end())
                    From_Json(*dataIter, data);

                outData.kind = AuthoringTypeDataKind::SourceHistorySpriteTrail;
                outData.payload = data;
                break;
            }
            case AuthoringTypeDataKind::Beam:
            {
                BeamTypeData data{};
                const auto dataIter = root.find("data");
                if (dataIter != root.end())
                    From_Json(*dataIter, data);

                outData.kind = AuthoringTypeDataKind::Beam;
                outData.payload = data;
                break;
            }
            case AuthoringTypeDataKind::None:
            default:
                outData.kind = AuthoringTypeDataKind::None;
                outData.payload = EmptyTypeData{};
                break;
            }
        }

        json To_Json(const EffectMaterialInstanceData& data)
        {
            json root = json::object();
            root["sourcePresetName"] = data.sourcePresetName;
            root["materialFamily"] = To_EnumToken(data.materialFamily);
            root["mainTextureGuid"] = Make_TextureGuidForSave(data.mainTextureGuid, data.mainTexturePath);
            root["mainTexturePath"] = data.mainTexturePath;
            root["noiseTextureGuid"] = Make_TextureGuidForSave(data.noiseTextureGuid, data.noiseTexturePath);
            root["noiseTexturePath"] = data.noiseTexturePath;
            root["maskTextureGuid"] = Make_TextureGuidForSave(data.maskTextureGuid, data.maskTexturePath);
            root["maskTexturePath"] = data.maskTexturePath;
            root["flowTextureGuid"] = Make_TextureGuidForSave(data.flowTextureGuid, data.flowTexturePath);
            root["flowTexturePath"] = data.flowTexturePath;
            root["tint"] = To_Json(data.tint);
            root["intensity"] = data.intensity;
            root["opacityPower"] = data.opacityPower;
            root["alphaMultiplier"] = data.alphaMultiplier;
            root["noiseStrength"] = data.noiseStrength;
            root["alphaCutoff"] = data.alphaCutoff;
            root["alphaErosion"] = data.alphaErosion;
            root["noiseSource"] = data.noiseSource;
            root["maskSource"] = data.maskSource;
            root["noiseInvert"] = data.noiseInvert;
            root["maskInvert"] = data.maskInvert;
            root["mainUVScale"] = To_Json(data.mainUVScale);
            root["mainUVOffset"] = To_Json(data.mainUVOffset);
            root["mainUVScrollSpeed"] = To_Json(data.mainUVScrollSpeed);
            root["mainUVTilingMode"] = To_EnumToken(Resolve_CompatibleUVTilingMode(data.mainUVPolicy));
            root["mainUVPolicy"] = To_Json(data.mainUVPolicy);
            root["mainUVRotation"] = To_EnumToken(data.mainUVRotation);
            root["noiseUVScale"] = To_Json(data.noiseUVScale);
            root["noiseUVOffset"] = To_Json(data.noiseUVOffset);
            root["noiseUVScrollSpeed"] = To_Json(data.noiseUVScrollSpeed);
            root["noiseUVTilingMode"] = To_EnumToken(Resolve_CompatibleUVTilingMode(data.noiseUVPolicy));
            root["noiseUVPolicy"] = To_Json(data.noiseUVPolicy);
            root["noiseUVRotation"] = To_EnumToken(data.noiseUVRotation);
            root["maskUVScale"] = To_Json(data.maskUVScale);
            root["maskUVOffset"] = To_Json(data.maskUVOffset);
            root["maskUVScrollSpeed"] = To_Json(data.maskUVScrollSpeed);
            root["maskUVTilingMode"] = To_EnumToken(Resolve_CompatibleUVTilingMode(data.maskUVPolicy));
            root["maskUVPolicy"] = To_Json(data.maskUVPolicy);
            root["maskUVRotation"] = To_EnumToken(data.maskUVRotation);
            root["flowUVScale"] = To_Json(data.flowUVScale);
            root["flowUVOffset"] = To_Json(data.flowUVOffset);
            root["flowUVScrollSpeed"] = To_Json(data.flowUVScrollSpeed);
            root["flowUVTilingMode"] = To_EnumToken(Resolve_CompatibleUVTilingMode(data.flowUVPolicy));
            root["flowUVPolicy"] = To_Json(data.flowUVPolicy);
            root["flowUVRotation"] = To_EnumToken(data.flowUVRotation);
            root["blendMode"] = To_EnumToken(data.blendMode);
            root["opacitySource"] = data.opacitySource;
            if (data.blendMode == EffectMaterialBlendMode::Additive || !Is_DefaultAdditiveContribution(data.additive))
                root["additive"] = To_Json(data.additive);
            if (!Is_DefaultCoreEmissive(data.coreEmissive))
                root["coreEmissive"] = To_Json(data.coreEmissive);
            root["refractionIntensity"] = data.refractionIntensity;
            root["refractionPresence"] = data.refractionPresence;
            root["distortionShapeMode"] = To_EnumToken(data.distortionShapeMode);
            root["airSheathMapInterpretation"] = To_EnumToken(data.airSheathMapInterpretation);
            root["airSheathMapXSource"] = To_EnumToken(data.airSheathMapXSource);
            root["airSheathMapYSource"] = To_EnumToken(data.airSheathMapYSource);
            root["airSheathMapVectorSpace"] = To_EnumToken(data.airSheathMapVectorSpace);
            root["airSheathMapComposition"] = To_EnumToken(data.airSheathMapComposition);
            root["airSheathMapInfluence"] = data.airSheathMapInfluence;
            root["distortionShapeRadius"] = data.distortionShapeRadius;
            root["distortionShapeThickness"] = data.distortionShapeThickness;
            root["distortionShapeSoftness"] = data.distortionShapeSoftness;
            root["glassAlpha"] = data.glassAlpha;
            root["glassAlphaPower"] = data.glassAlphaPower;
            root["glassNormalStrength"] = data.glassNormalStrength;
            root["glassRimColor"] = To_Json(data.glassRimColor);
            root["glassRimIntensity"] = data.glassRimIntensity;
            root["glassRimPower"] = data.glassRimPower;
            root["glassLightDirection"] = To_Json(data.glassLightDirection);
            root["glassLightColor"] = To_Json(data.glassLightColor);
            root["glassLightIntensity"] = data.glassLightIntensity;
            root["glassSpecularPower"] = data.glassSpecularPower;
            root["glassSpecularSoftness"] = data.glassSpecularSoftness;
            root["glassMainInfluence"] = data.glassMainInfluence;
            root["glassNoiseBreakup"] = data.glassNoiseBreakup;
            root["glassMaskStrength"] = data.glassMaskStrength;
            root["twoSided"] = data.twoSided;
            root["subUVRows"] = data.subUVRows;
            root["subUVCols"] = data.subUVCols;
            return root;
        }

        void From_Json(const json& root, EffectMaterialInstanceData& outData)
        {
            if (!root.is_object())
                return;

            Read_StringField(root, "sourcePresetName", outData.sourcePresetName);
            if (const auto materialFamilyIter = root.find("materialFamily"); materialFamilyIter != root.end())
                Try_ReadEnumToken(*materialFamilyIter, outData.materialFamily);
            Read_StringField(root, "mainTextureGuid", outData.mainTextureGuid);
            Read_StringField(root, "mainTexturePath", outData.mainTexturePath);
            Read_StringField(root, "noiseTextureGuid", outData.noiseTextureGuid);
            Read_StringField(root, "noiseTexturePath", outData.noiseTexturePath);
            Read_StringField(root, "maskTextureGuid", outData.maskTextureGuid);
            Read_StringField(root, "maskTexturePath", outData.maskTexturePath);
            Read_StringField(root, "flowTextureGuid", outData.flowTextureGuid);
            Read_StringField(root, "flowTexturePath", outData.flowTexturePath);
            if (outData.mainTextureGuid.empty() && outData.mainTexturePath.empty())
            {
                string legacyMainTextureId{};
                Read_StringField(root, "mainTextureId", legacyMainTextureId);
                Try_MigrateLegacyTextureId(legacyMainTextureId, outData.mainTextureGuid, outData.mainTexturePath);
            }
            if (outData.noiseTextureGuid.empty() && outData.noiseTexturePath.empty())
            {
                string legacyNoiseTextureId{};
                Read_StringField(root, "noiseTextureId", legacyNoiseTextureId);
                Try_MigrateLegacyTextureId(legacyNoiseTextureId, outData.noiseTextureGuid, outData.noiseTexturePath);
            }
            Fill_TextureGuidFromPath(outData.mainTextureGuid, outData.mainTexturePath);
            Fill_TextureGuidFromPath(outData.noiseTextureGuid, outData.noiseTexturePath);
            Fill_TextureGuidFromPath(outData.maskTextureGuid, outData.maskTexturePath);
            Fill_TextureGuidFromPath(outData.flowTextureGuid, outData.flowTexturePath);
            Fill_TexturePathHintFromGuid(outData.mainTextureGuid, outData.mainTexturePath);
            Fill_TexturePathHintFromGuid(outData.noiseTextureGuid, outData.noiseTexturePath);
            Fill_TexturePathHintFromGuid(outData.maskTextureGuid, outData.maskTexturePath);
            Fill_TexturePathHintFromGuid(outData.flowTextureGuid, outData.flowTexturePath);
            Read_NumberField(root, "intensity", outData.intensity);
            Read_NumberField(root, "opacityPower", outData.opacityPower);
            Read_NumberField(root, "alphaMultiplier", outData.alphaMultiplier);
            Read_NumberField(root, "noiseStrength", outData.noiseStrength);
            Read_NumberField(root, "alphaCutoff", outData.alphaCutoff);
            Read_NumberField(root, "alphaErosion", outData.alphaErosion);
            Read_StringField(root, "noiseSource", outData.noiseSource);
            Read_StringField(root, "maskSource", outData.maskSource);
            Read_BoolField(root, "noiseInvert", outData.noiseInvert);
            Read_BoolField(root, "maskInvert", outData.maskInvert);
            if (const auto blendModeIter = root.find("blendMode"); blendModeIter != root.end())
                Try_ReadEnumToken(*blendModeIter, outData.blendMode);
            Read_StringField(root, "opacitySource", outData.opacitySource);
            if (const auto additiveIter = root.find("additive"); additiveIter != root.end())
                From_Json(*additiveIter, outData.additive);
            if (const auto coreEmissiveIter = root.find("coreEmissive"); coreEmissiveIter != root.end())
                From_Json(*coreEmissiveIter, outData.coreEmissive);
            Read_BoolField(root, "twoSided", outData.twoSided);
            Read_NumberField(root, "subUVRows", outData.subUVRows);
            Read_NumberField(root, "subUVCols", outData.subUVCols);

            if (const auto tintIter = root.find("tint"); tintIter != root.end())
                Read_Color(*tintIter, outData.tint);

            if (const auto mainUVScaleIter = root.find("mainUVScale"); mainUVScaleIter != root.end())
                Read_Vec2(*mainUVScaleIter, outData.mainUVScale);

            if (const auto mainUVOffsetIter = root.find("mainUVOffset"); mainUVOffsetIter != root.end())
                Read_Vec2(*mainUVOffsetIter, outData.mainUVOffset);

            if (const auto mainUVScrollSpeedIter = root.find("mainUVScrollSpeed"); mainUVScrollSpeedIter != root.end())
                Read_Vec2(*mainUVScrollSpeedIter, outData.mainUVScrollSpeed);

            if (const auto mainUVTilingModeIter = root.find("mainUVTilingMode"); mainUVTilingModeIter != root.end())
                Try_ReadEnumToken(*mainUVTilingModeIter, outData.mainUVTilingMode);
            Read_UVAxisPolicyField(root, "mainUVPolicy", outData.mainUVTilingMode, outData.mainUVPolicy, outData.mainUVTilingMode);
            if (const auto mainUVRotationIter = root.find("mainUVRotation"); mainUVRotationIter != root.end())
                Try_ReadEnumToken(*mainUVRotationIter, outData.mainUVRotation);

            if (const auto noiseUVScaleIter = root.find("noiseUVScale"); noiseUVScaleIter != root.end())
                Read_Vec2(*noiseUVScaleIter, outData.noiseUVScale);

            if (const auto noiseUVOffsetIter = root.find("noiseUVOffset"); noiseUVOffsetIter != root.end())
                Read_Vec2(*noiseUVOffsetIter, outData.noiseUVOffset);

            if (const auto noiseUVScrollSpeedIter = root.find("noiseUVScrollSpeed"); noiseUVScrollSpeedIter != root.end())
                Read_Vec2(*noiseUVScrollSpeedIter, outData.noiseUVScrollSpeed);

            if (const auto noiseUVTilingModeIter = root.find("noiseUVTilingMode"); noiseUVTilingModeIter != root.end())
                Try_ReadEnumToken(*noiseUVTilingModeIter, outData.noiseUVTilingMode);
            Read_UVAxisPolicyField(root, "noiseUVPolicy", outData.noiseUVTilingMode, outData.noiseUVPolicy, outData.noiseUVTilingMode);
            if (const auto noiseUVRotationIter = root.find("noiseUVRotation"); noiseUVRotationIter != root.end())
                Try_ReadEnumToken(*noiseUVRotationIter, outData.noiseUVRotation);

            if (const auto maskUVScaleIter = root.find("maskUVScale"); maskUVScaleIter != root.end())
                Read_Vec2(*maskUVScaleIter, outData.maskUVScale);

            if (const auto maskUVOffsetIter = root.find("maskUVOffset"); maskUVOffsetIter != root.end())
                Read_Vec2(*maskUVOffsetIter, outData.maskUVOffset);

            if (const auto maskUVScrollSpeedIter = root.find("maskUVScrollSpeed"); maskUVScrollSpeedIter != root.end())
                Read_Vec2(*maskUVScrollSpeedIter, outData.maskUVScrollSpeed);

            if (const auto maskUVTilingModeIter = root.find("maskUVTilingMode"); maskUVTilingModeIter != root.end())
                Try_ReadEnumToken(*maskUVTilingModeIter, outData.maskUVTilingMode);
            Read_UVAxisPolicyField(root, "maskUVPolicy", outData.maskUVTilingMode, outData.maskUVPolicy, outData.maskUVTilingMode);
            if (const auto maskUVRotationIter = root.find("maskUVRotation"); maskUVRotationIter != root.end())
                Try_ReadEnumToken(*maskUVRotationIter, outData.maskUVRotation);

            if (const auto flowUVScaleIter = root.find("flowUVScale"); flowUVScaleIter != root.end())
                Read_Vec2(*flowUVScaleIter, outData.flowUVScale);

            if (const auto flowUVOffsetIter = root.find("flowUVOffset"); flowUVOffsetIter != root.end())
                Read_Vec2(*flowUVOffsetIter, outData.flowUVOffset);

            if (const auto flowUVScrollSpeedIter = root.find("flowUVScrollSpeed"); flowUVScrollSpeedIter != root.end())
                Read_Vec2(*flowUVScrollSpeedIter, outData.flowUVScrollSpeed);

            if (const auto flowUVTilingModeIter = root.find("flowUVTilingMode"); flowUVTilingModeIter != root.end())
                Try_ReadEnumToken(*flowUVTilingModeIter, outData.flowUVTilingMode);
            Read_UVAxisPolicyField(root, "flowUVPolicy", outData.flowUVTilingMode, outData.flowUVPolicy, outData.flowUVTilingMode);
            if (const auto flowUVRotationIter = root.find("flowUVRotation"); flowUVRotationIter != root.end())
                Try_ReadEnumToken(*flowUVRotationIter, outData.flowUVRotation);

            Read_NumberField(root, "refractionIntensity", outData.refractionIntensity);
            Read_NumberField(root, "refractionPresence", outData.refractionPresence);
            if (const auto shapeModeIter = root.find("distortionShapeMode"); shapeModeIter != root.end())
                Try_ReadEnumToken(*shapeModeIter, outData.distortionShapeMode);
            if (const auto iter = root.find("airSheathMapInterpretation"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.airSheathMapInterpretation);
            if (const auto iter = root.find("airSheathMapXSource"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.airSheathMapXSource);
            if (const auto iter = root.find("airSheathMapYSource"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.airSheathMapYSource);
            if (const auto iter = root.find("airSheathMapVectorSpace"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.airSheathMapVectorSpace);
            if (const auto iter = root.find("airSheathMapComposition"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.airSheathMapComposition);
            Read_NumberField(root, "airSheathMapInfluence", outData.airSheathMapInfluence);
            Read_NumberField(root, "distortionShapeRadius", outData.distortionShapeRadius);
            Read_NumberField(root, "distortionShapeThickness", outData.distortionShapeThickness);
            Read_NumberField(root, "distortionShapeSoftness", outData.distortionShapeSoftness);
            Read_NumberField(root, "glassAlpha", outData.glassAlpha);
            Read_NumberField(root, "glassAlphaPower", outData.glassAlphaPower);
            Read_NumberField(root, "glassNormalStrength", outData.glassNormalStrength);
            if (const auto glassRimColorIter = root.find("glassRimColor"); glassRimColorIter != root.end())
                Read_Color(*glassRimColorIter, outData.glassRimColor);
            Read_NumberField(root, "glassRimIntensity", outData.glassRimIntensity);
            Read_NumberField(root, "glassRimPower", outData.glassRimPower);
            if (const auto glassLightDirectionIter = root.find("glassLightDirection"); glassLightDirectionIter != root.end())
                Read_Vec3(*glassLightDirectionIter, outData.glassLightDirection);
            if (const auto glassLightColorIter = root.find("glassLightColor"); glassLightColorIter != root.end())
                Read_Color(*glassLightColorIter, outData.glassLightColor);
            Read_NumberField(root, "glassLightIntensity", outData.glassLightIntensity);
            Read_NumberField(root, "glassSpecularPower", outData.glassSpecularPower);
            Read_NumberField(root, "glassSpecularSoftness", outData.glassSpecularSoftness);
            Read_NumberField(root, "glassMainInfluence", outData.glassMainInfluence);
            Read_NumberField(root, "glassNoiseBreakup", outData.glassNoiseBreakup);
            Read_NumberField(root, "glassMaskStrength", outData.glassMaskStrength);
        }

        json To_Json(const FloatCurveKeyData& data)
        {
            json root = json::object();
            root["time"] = data.time;
            root["value"] = data.value;
            root["arriveTangent"] = data.arriveTangent;
            root["leaveTangent"] = data.leaveTangent;
            root["interpolationMode"] = To_EnumToken(data.interpolationMode);
            return root;
        }

        void From_Json(const json& root, FloatCurveKeyData& outData)
        {
            if (!root.is_object())
                return;

            Read_NumberField(root, "time", outData.time);
            Read_NumberField(root, "value", outData.value);
            Read_NumberField(root, "arriveTangent", outData.arriveTangent);
            Read_NumberField(root, "leaveTangent", outData.leaveTangent);

            const auto interpolationIter = root.find("interpolationMode");
            if (interpolationIter != root.end())
                Try_ReadEnumToken(*interpolationIter, outData.interpolationMode);
        }

        json To_Json(const Vector2CurveKeyData& data)
        {
            json root = json::object();
            root["time"] = data.time;
            root["value"] = To_Json(data.value);
            root["arriveTangent"] = To_Json(data.arriveTangent);
            root["leaveTangent"] = To_Json(data.leaveTangent);
            root["interpolationMode"] = To_EnumToken(data.interpolationMode);
            return root;
        }

        void From_Json(const json& root, Vector2CurveKeyData& outData)
        {
            if (!root.is_object())
                return;

            Read_NumberField(root, "time", outData.time);

            const auto valueIter = root.find("value");
            if (valueIter != root.end())
                Read_Vec2(*valueIter, outData.value);

            const auto arriveTangentIter = root.find("arriveTangent");
            if (arriveTangentIter != root.end())
                Read_Vec2(*arriveTangentIter, outData.arriveTangent);

            const auto leaveTangentIter = root.find("leaveTangent");
            if (leaveTangentIter != root.end())
                Read_Vec2(*leaveTangentIter, outData.leaveTangent);

            const auto interpolationIter = root.find("interpolationMode");
            if (interpolationIter != root.end())
                Try_ReadEnumToken(*interpolationIter, outData.interpolationMode);
        }

        json To_Json(const Vector3CurveKeyData& data)
        {
            json root = json::object();
            root["time"] = data.time;
            root["value"] = To_Json(data.value);
            root["arriveTangent"] = To_Json(data.arriveTangent);
            root["leaveTangent"] = To_Json(data.leaveTangent);
            root["interpolationMode"] = To_EnumToken(data.interpolationMode);
            return root;
        }

        void From_Json(const json& root, Vector3CurveKeyData& outData)
        {
            if (!root.is_object())
                return;

            Read_NumberField(root, "time", outData.time);

            const auto valueIter = root.find("value");
            if (valueIter != root.end())
                Read_Vec3(*valueIter, outData.value);

            const auto arriveTangentIter = root.find("arriveTangent");
            if (arriveTangentIter != root.end())
                Read_Vec3(*arriveTangentIter, outData.arriveTangent);

            const auto leaveTangentIter = root.find("leaveTangent");
            if (leaveTangentIter != root.end())
                Read_Vec3(*leaveTangentIter, outData.leaveTangent);

            const auto interpolationIter = root.find("interpolationMode");
            if (interpolationIter != root.end())
                Try_ReadEnumToken(*interpolationIter, outData.interpolationMode);
        }

        json To_ColorRgbJson(const Color& value)
        {
            return json::array({ value.x, value.y, value.z });
        }

        void Read_ColorRgb(const json& node, Color& outValue)
        {
            if (!node.is_array() || node.size() < 3 || !node[0].is_number() || !node[1].is_number() || !node[2].is_number())
                return;

            outValue.x = node[0].get<float>();
            outValue.y = node[1].get<float>();
            outValue.z = node[2].get<float>();
        }

        json To_Json(const ColorCurveKeyData& data)
        {
            json root = json::object();
            root["time"] = data.time;
            root["value"] = To_ColorRgbJson(data.value);
            root["arriveTangent"] = To_ColorRgbJson(data.arriveTangent);
            root["leaveTangent"] = To_ColorRgbJson(data.leaveTangent);
            root["interpolationMode"] = To_EnumToken(data.interpolationMode);
            return root;
        }

        void From_Json(const json& root, ColorCurveKeyData& outData)
        {
            if (!root.is_object())
                return;

            Read_NumberField(root, "time", outData.time);

            const auto valueIter = root.find("value");
            if (valueIter != root.end())
                Read_ColorRgb(*valueIter, outData.value);

            const auto arriveTangentIter = root.find("arriveTangent");
            if (arriveTangentIter != root.end())
                Read_ColorRgb(*arriveTangentIter, outData.arriveTangent);

            const auto leaveTangentIter = root.find("leaveTangent");
            if (leaveTangentIter != root.end())
                Read_ColorRgb(*leaveTangentIter, outData.leaveTangent);

            const auto interpolationIter = root.find("interpolationMode");
            if (interpolationIter != root.end())
                Try_ReadEnumToken(*interpolationIter, outData.interpolationMode);
        }

        float Evaluate_FloatCurveSegmentLinear(const FloatCurveKeyData& leftKey, const FloatCurveKeyData& rightKey, float x)
        {
            const float width = max(0.0001f, rightKey.time - leftKey.time);
            const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
            return lerp(leftKey.value, rightKey.value, t);
        }

        float Evaluate_FloatCurveSegmentAutoClamped(const FloatCurveKeyData& leftKey, const FloatCurveKeyData& rightKey, float x)
        {
            const float width = max(0.0001f, rightKey.time - leftKey.time);
            const float t = clamp((x - leftKey.time) / width, 0.f, 1.f);
            const float m0 = leftKey.leaveTangent * width;
            const float m1 = rightKey.arriveTangent * width;
            const float t2 = t * t;
            const float t3 = t2 * t;

            return (2.f * t3 - 3.f * t2 + 1.f) * leftKey.value +
                   (t3 - 2.f * t2 + t) * m0 +
                   (-2.f * t3 + 3.f * t2) * rightKey.value +
                   (t3 - t2) * m1;
        }

        float Evaluate_FloatConstantCurve(const ConstantCurveFloatDistributionData& curve, float x, float fallbackValue)
        {
            if (curve.keys.empty())
                return fallbackValue;

            if (curve.keys.size() == 1)
                return curve.keys.front().value;

            const float clampedX = clamp(x, 0.f, 1.f);
            for (size_t index = 1; index < curve.keys.size(); ++index)
            {
                const FloatCurveKeyData& leftKey = curve.keys[index - 1];
                const FloatCurveKeyData& rightKey = curve.keys[index];
                if (clampedX > rightKey.time)
                    continue;

                if (leftKey.interpolationMode == FloatCurveInterpolationMode::Constant)
                    return leftKey.value;

                if (leftKey.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
                    return Evaluate_FloatCurveSegmentAutoClamped(leftKey, rightKey, clampedX);

                return Evaluate_FloatCurveSegmentLinear(leftKey, rightKey, clampedX);
            }

            return curve.keys.back().value;
        }

        Vec2 Lerp_Vec2(const Vec2& left, const Vec2& right, float t)
        {
            return Vec2{ lerp(left.x, right.x, t), lerp(left.y, right.y, t) };
        }

        //## Codec::Distribution

        json To_Json(const FloatDistributionData& data)
        {
            json root = json::object();
            root["mode"] = To_EnumToken(data.mode);

            json payload = json::object();
            if (const auto* constantData = get_if<ConstantFloatDistributionData>(&data.payload))
                payload["value"] = constantData->value;
            else if (const auto* uniformData = get_if<UniformFloatDistributionData>(&data.payload))
            {
                payload["minValue"] = uniformData->minValue;
                payload["maxValue"] = uniformData->maxValue;
                Write_RandomSeedField(payload, uniformData->randomSeed);
            }
            else if (const auto* constantCurveData = get_if<ConstantCurveFloatDistributionData>(&data.payload))
            {
                payload["keys"] = json::array();
                for (const FloatCurveKeyData& key : constantCurveData->keys)
                    payload["keys"].push_back(To_Json(key));
            }

            root["payload"] = move(payload);
            return root;
        }

        void From_Json(const json& root, FloatDistributionData& outData)
        {
            if (!root.is_object())
                return;

            DistributionMode parsedMode = outData.mode;
            const auto modeIter = root.find("mode");
            if (modeIter != root.end() && Try_ReadEnumToken(*modeIter, parsedMode))
            {
                if (parsedMode == DistributionMode::Constant ||
                    parsedMode == DistributionMode::Uniform ||
                    parsedMode == DistributionMode::ConstantCurve)
                    outData.mode = parsedMode;
            }

            const auto payloadIter = root.find("payload");
            if (payloadIter == root.end() || !payloadIter->is_object())
                return;

            switch (outData.mode)
            {
            case DistributionMode::Constant:
            {
                ConstantFloatDistributionData payload{};
                if (const auto* existingPayload = get_if<ConstantFloatDistributionData>(&outData.payload))
                    payload = *existingPayload;
                Read_NumberField(*payloadIter, "value", payload.value);
                outData.payload = payload;
                break;
            }
            case DistributionMode::Uniform:
            {
                UniformFloatDistributionData payload{};
                if (const auto* existingPayload = get_if<UniformFloatDistributionData>(&outData.payload))
                    payload = *existingPayload;
                Read_NumberField(*payloadIter, "minValue", payload.minValue);
                Read_NumberField(*payloadIter, "maxValue", payload.maxValue);
                Read_RandomSeedField(*payloadIter, payload.randomSeed);
                outData.payload = payload;
                break;
            }
            case DistributionMode::ConstantCurve:
            {
                ConstantCurveFloatDistributionData payload{};
                const auto keysIter = payloadIter->find("keys");
                if (keysIter != payloadIter->end() && keysIter->is_array())
                {
                    payload.keys.clear();
                    for (const json& keyNode : *keysIter)
                    {
                        FloatCurveKeyData key{};
                        From_Json(keyNode, key);
                        payload.keys.push_back(key);
                    }
                }

                if (payload.keys.empty())
                {
                    if (const auto* existingPayload = get_if<ConstantCurveFloatDistributionData>(&outData.payload))
                        payload.keys = existingPayload->keys;
                }

                outData.payload = payload;
                break;
            }
            default:
                break;
            }
        }

        json To_Json(const Vector2DistributionData& data)
        {
            json root = json::object();
            root["mode"] = To_EnumToken(data.mode);

            json payload = json::object();
            if (const auto* constantData = get_if<ConstantVector2DistributionData>(&data.payload))
                payload["value"] = To_Json(constantData->value);
            else if (const auto* uniformData = get_if<UniformVector2DistributionData>(&data.payload))
            {
                payload["minValue"] = To_Json(uniformData->minValue);
                payload["maxValue"] = To_Json(uniformData->maxValue);
                Write_RandomSeedField(payload, uniformData->randomSeed);
            }
            else if (const auto* constantCurveData = get_if<ConstantCurveVector2DistributionData>(&data.payload))
            {
                payload["keys"] = json::array();
                for (const Vector2CurveKeyData& key : constantCurveData->keys)
                    payload["keys"].push_back(To_Json(key));
            }

            root["payload"] = move(payload);
            return root;
        }

        void From_Json(const json& root, Vector2DistributionData& outData)
        {
            if (!root.is_object())
                return;

            DistributionMode parsedMode = outData.mode;
            const auto modeIter = root.find("mode");
            if (modeIter != root.end() && Try_ReadEnumToken(*modeIter, parsedMode))
            {
                if (parsedMode == DistributionMode::Constant ||
                    parsedMode == DistributionMode::Uniform ||
                    parsedMode == DistributionMode::ConstantCurve)
                    outData.mode = parsedMode;
            }

            const auto payloadIter = root.find("payload");
            if (payloadIter == root.end() || !payloadIter->is_object())
                return;

            switch (outData.mode)
            {
            case DistributionMode::Constant:
            {
                ConstantVector2DistributionData payload{};
                if (const auto* existingPayload = get_if<ConstantVector2DistributionData>(&outData.payload))
                    payload = *existingPayload;
                const auto valueIter = payloadIter->find("value");
                if (valueIter != payloadIter->end())
                    Read_Vec2(*valueIter, payload.value);
                outData.payload = payload;
                break;
            }
            case DistributionMode::Uniform:
            {
                UniformVector2DistributionData payload{};
                if (const auto* existingPayload = get_if<UniformVector2DistributionData>(&outData.payload))
                    payload = *existingPayload;
                const auto minIter = payloadIter->find("minValue");
                if (minIter != payloadIter->end())
                    Read_Vec2(*minIter, payload.minValue);
                const auto maxIter = payloadIter->find("maxValue");
                if (maxIter != payloadIter->end())
                    Read_Vec2(*maxIter, payload.maxValue);
                Read_RandomSeedField(*payloadIter, payload.randomSeed);
                outData.payload = payload;
                break;
            }
            case DistributionMode::ConstantCurve:
            {
                ConstantCurveVector2DistributionData payload{};
                const auto keysIter = payloadIter->find("keys");
                if (keysIter != payloadIter->end() && keysIter->is_array())
                {
                    payload.keys.clear();
                    for (const json& keyNode : *keysIter)
                    {
                        Vector2CurveKeyData key{};
                        From_Json(keyNode, key);
                        payload.keys.push_back(key);
                    }
                }

                if (payload.keys.empty())
                {
                    if (const auto* existingPayload = get_if<ConstantCurveVector2DistributionData>(&outData.payload))
                        payload.keys = existingPayload->keys;
                }

                outData.payload = payload;
                break;
            }
            default:
                break;
            }
        }

        json To_Json(const Vector3DistributionData& data)
        {
            json root = json::object();
            root["mode"] = To_EnumToken(data.mode);

            json payload = json::object();
            if (const auto* constantData = get_if<ConstantVector3DistributionData>(&data.payload))
                payload["value"] = To_Json(constantData->value);
            else if (const auto* uniformData = get_if<UniformVector3DistributionData>(&data.payload))
            {
                payload["minValue"] = To_Json(uniformData->minValue);
                payload["maxValue"] = To_Json(uniformData->maxValue);
                Write_RandomSeedField(payload, uniformData->randomSeed);
            }
            else if (const auto* constantCurveData = get_if<ConstantCurveVector3DistributionData>(&data.payload))
            {
                payload["keys"] = json::array();
                for (const Vector3CurveKeyData& key : constantCurveData->keys)
                    payload["keys"].push_back(To_Json(key));
            }

            root["payload"] = move(payload);
            return root;
        }

        void From_Json(const json& root, Vector3DistributionData& outData)
        {
            if (!root.is_object())
                return;

            DistributionMode parsedMode = outData.mode;
            const auto modeIter = root.find("mode");
            if (modeIter != root.end() && Try_ReadEnumToken(*modeIter, parsedMode))
            {
                if (parsedMode == DistributionMode::Constant ||
                    parsedMode == DistributionMode::Uniform ||
                    parsedMode == DistributionMode::ConstantCurve)
                    outData.mode = parsedMode;
            }

            const auto payloadIter = root.find("payload");
            if (payloadIter == root.end() || !payloadIter->is_object())
                return;

            switch (outData.mode)
            {
            case DistributionMode::Constant:
            {
                ConstantVector3DistributionData payload{};
                if (const auto* existingPayload = get_if<ConstantVector3DistributionData>(&outData.payload))
                    payload = *existingPayload;
                const auto valueIter = payloadIter->find("value");
                if (valueIter != payloadIter->end())
                    Read_Vec3(*valueIter, payload.value);
                outData.payload = payload;
                break;
            }
            case DistributionMode::Uniform:
            {
                UniformVector3DistributionData payload{};
                if (const auto* existingPayload = get_if<UniformVector3DistributionData>(&outData.payload))
                    payload = *existingPayload;
                const auto minIter = payloadIter->find("minValue");
                if (minIter != payloadIter->end())
                    Read_Vec3(*minIter, payload.minValue);
                const auto maxIter = payloadIter->find("maxValue");
                if (maxIter != payloadIter->end())
                    Read_Vec3(*maxIter, payload.maxValue);
                Read_RandomSeedField(*payloadIter, payload.randomSeed);
                outData.payload = payload;
                break;
            }
            case DistributionMode::ConstantCurve:
            {
                ConstantCurveVector3DistributionData payload{};
                const auto keysIter = payloadIter->find("keys");
                if (keysIter != payloadIter->end() && keysIter->is_array())
                {
                    payload.keys.clear();
                    for (const json& keyNode : *keysIter)
                    {
                        Vector3CurveKeyData key{};
                        From_Json(keyNode, key);
                        payload.keys.push_back(key);
                    }
                }

                if (payload.keys.empty())
                {
                    if (const auto* existingPayload = get_if<ConstantCurveVector3DistributionData>(&outData.payload))
                        payload.keys = existingPayload->keys;
                }

                outData.payload = payload;
                break;
            }
            default:
                break;
            }
        }

        json To_Json(const ColorDistributionData& data)
        {
            json root = json::object();
            root["mode"] = To_EnumToken(data.mode);

            json payload = json::object();
            if (const auto* constantData = get_if<ConstantColorDistributionData>(&data.payload))
                payload["value"] = To_Json(constantData->value);
            else if (const auto* uniformData = get_if<UniformColorDistributionData>(&data.payload))
            {
                payload["minValue"] = To_Json(uniformData->minValue);
                payload["maxValue"] = To_Json(uniformData->maxValue);
                Write_RandomSeedField(payload, uniformData->randomSeed);
            }
            else if (const auto* constantCurveData = get_if<ConstantCurveColorDistributionData>(&data.payload))
            {
                payload["keys"] = json::array();
                for (const ColorCurveKeyData& key : constantCurveData->keys)
                    payload["keys"].push_back(To_Json(key));
            }

            root["payload"] = move(payload);
            return root;
        }

        void From_Json(const json& root, ColorDistributionData& outData)
        {
            if (!root.is_object())
                return;

            DistributionMode parsedMode = outData.mode;
            const auto modeIter = root.find("mode");
            if (modeIter != root.end() && Try_ReadEnumToken(*modeIter, parsedMode))
            {
                if (parsedMode == DistributionMode::Constant ||
                    parsedMode == DistributionMode::Uniform ||
                    parsedMode == DistributionMode::ConstantCurve)
                    outData.mode = parsedMode;
            }

            const auto payloadIter = root.find("payload");
            if (payloadIter == root.end() || !payloadIter->is_object())
                return;

            switch (outData.mode)
            {
            case DistributionMode::Constant:
            {
                ConstantColorDistributionData payload{};
                if (const auto* existingPayload = get_if<ConstantColorDistributionData>(&outData.payload))
                    payload = *existingPayload;
                const auto valueIter = payloadIter->find("value");
                if (valueIter != payloadIter->end())
                    Read_Color(*valueIter, payload.value);
                outData.payload = payload;
                break;
            }
            case DistributionMode::Uniform:
            {
                UniformColorDistributionData payload{};
                if (const auto* existingPayload = get_if<UniformColorDistributionData>(&outData.payload))
                    payload = *existingPayload;
                const auto minIter = payloadIter->find("minValue");
                if (minIter != payloadIter->end())
                    Read_Color(*minIter, payload.minValue);
                const auto maxIter = payloadIter->find("maxValue");
                if (maxIter != payloadIter->end())
                    Read_Color(*maxIter, payload.maxValue);
                Read_RandomSeedField(*payloadIter, payload.randomSeed);
                outData.payload = payload;
                break;
            }
            case DistributionMode::ConstantCurve:
            {
                ConstantCurveColorDistributionData payload{};
                const auto keysIter = payloadIter->find("keys");
                if (keysIter != payloadIter->end() && keysIter->is_array())
                {
                    payload.keys.clear();
                    for (const json& keyNode : *keysIter)
                    {
                        ColorCurveKeyData key{};
                        From_Json(keyNode, key);
                        payload.keys.push_back(key);
                    }
                }

                if (payload.keys.empty())
                {
                    if (const auto* existingPayload = get_if<ConstantCurveColorDistributionData>(&outData.payload))
                        payload.keys = existingPayload->keys;
                }

                outData.payload = payload;
                break;
            }
            default:
                break;
            }
        }

        json To_Json(const RequiredModuleData& data)
        {
            json root = json::object();
            root["rendererType"] = data.rendererType;
            root["material"] = To_Json(data.material);
            root["emitterOrigin"] = To_Json(data.emitterOrigin);
            root["emitterRotationDegrees"] = To_Json(data.emitterRotationDegrees);
            root["screenAlignment"] = To_EnumToken(data.screenAlignment);
            root["directionalAlignmentMode"] = To_EnumToken(data.directionalAlignmentMode);
            root["spriteTextureAxis"] = To_EnumToken(data.spriteTextureAxis);
            root["spriteRollOffsetDegrees"] = data.spriteRollOffsetDegrees;
            root["minCameraBlendDistance"] = data.minCameraBlendDistance;
            root["maxCameraBlendDistance"] = data.maxCameraBlendDistance;
            root["useLocalSpace"] = data.useLocalSpace;
            root["sortPolicy"] = To_EnumToken(data.sortPolicy);
            root["sortLayer"] = data.sortLayer;
            root["sortBias"] = data.sortBias;
            root["cameraMotionBlurAmount"] = data.cameraMotionBlurAmount;
            root["clearExistingParticlesOnInit"] = data.clearExistingParticlesOnInit;
            root["loopCount"] = data.loopCount;
            root["duration"] = data.duration;
            root["durationLow"] = data.durationLow;
            root["useDurationRange"] = data.useDurationRange;
            root["recalculateDurationEachLoop"] = data.recalculateDurationEachLoop;
            root["delay"] = data.delay;
            root["delayLow"] = data.delayLow;
            root["useDelayRange"] = data.useDelayRange;
            root["delayFirstLoopOnly"] = data.delayFirstLoopOnly;
            root["killOnDeactivate"] = data.killOnDeactivate;
            root["killOnCompleted"] = data.killOnCompleted;
            root["useMaxDrawCount"] = data.useMaxDrawCount;
            root["maxDrawCount"] = data.maxDrawCount;
            return root;
        }

        void From_Json(const json& root, RequiredModuleData& outData)
        {
            if (!root.is_object())
                return;

            Read_StringField(root, "rendererType", outData.rendererType);
            const auto materialIter = root.find("material");
            if (materialIter != root.end())
                From_Json(*materialIter, outData.material);
            const auto originIter = root.find("emitterOrigin");
            if (originIter != root.end())
                Read_Vec3(*originIter, outData.emitterOrigin);
            const auto rotationIter = root.find("emitterRotationDegrees");
            if (rotationIter != root.end())
                Read_Vec3(*rotationIter, outData.emitterRotationDegrees);

            const auto screenAlignmentIter = root.find("screenAlignment");
            if (screenAlignmentIter != root.end())
                Try_ReadEnumToken(*screenAlignmentIter, outData.screenAlignment);
            const auto directionalAlignmentIter = root.find("directionalAlignmentMode");
            if (directionalAlignmentIter != root.end())
                Try_ReadEnumToken(*directionalAlignmentIter, outData.directionalAlignmentMode);
            const auto spriteTextureAxisIter = root.find("spriteTextureAxis");
            if (spriteTextureAxisIter != root.end())
                Try_ReadEnumToken(*spriteTextureAxisIter, outData.spriteTextureAxis);
            const auto sortPolicyIter = root.find("sortPolicy");
            if (sortPolicyIter != root.end())
                Try_ReadEnumToken(*sortPolicyIter, outData.sortPolicy);
            else
            {
                const auto sortModeIter = root.find("sortMode");
                if (sortModeIter != root.end() && Try_ReadEnumToken(*sortModeIter, outData.sortMode))
                    outData.sortPolicy = Resolve_SortPolicyFromLegacyMode(outData.sortMode);
            }

            Read_NumberField(root, "spriteRollOffsetDegrees", outData.spriteRollOffsetDegrees);
            Read_NumberField(root, "minCameraBlendDistance", outData.minCameraBlendDistance);
            Read_NumberField(root, "maxCameraBlendDistance", outData.maxCameraBlendDistance);
            Read_BoolField(root, "useLocalSpace", outData.useLocalSpace);
            Read_NumberField(root, "sortLayer", outData.sortLayer);
            Read_NumberField(root, "sortBias", outData.sortBias);
            Read_NumberField(root, "cameraMotionBlurAmount", outData.cameraMotionBlurAmount);
            Read_BoolField(root, "clearExistingParticlesOnInit", outData.clearExistingParticlesOnInit);
            Read_NumberField(root, "loopCount", outData.loopCount);
            Read_NumberField(root, "duration", outData.duration);
            Read_NumberField(root, "durationLow", outData.durationLow);
            Read_BoolField(root, "useDurationRange", outData.useDurationRange);
            Read_BoolField(root, "recalculateDurationEachLoop", outData.recalculateDurationEachLoop);
            Read_NumberField(root, "delay", outData.delay);
            Read_NumberField(root, "delayLow", outData.delayLow);
            Read_BoolField(root, "useDelayRange", outData.useDelayRange);
            Read_BoolField(root, "delayFirstLoopOnly", outData.delayFirstLoopOnly);
            Read_BoolField(root, "killOnDeactivate", outData.killOnDeactivate);
            Read_BoolField(root, "killOnCompleted", outData.killOnCompleted);
            Read_BoolField(root, "useMaxDrawCount", outData.useMaxDrawCount);
            Read_NumberField(root, "maxDrawCount", outData.maxDrawCount);
        }

        json To_Json(const SpawnModuleData& data)
        {
            json root = json::object();
            root["processSpawnRate"] = data.processSpawnRate;
            root["spawnRate"] = To_Json(data.spawnRate);
            root["spawnRateScale"] = To_Json(data.spawnRateScale);
            root["processBurstList"] = data.processBurstList;
            root["burstList"] = json::array();
            for (const SpawnModuleData::ParticleBurstData& burst : data.burstList)
            {
                json burstNode = json::object();
                burstNode["time"] = burst.time;
                burstNode["count"] = burst.count;
                root["burstList"].push_back(move(burstNode));
            }
            root["burstScale"] = To_Json(data.burstScale);
            root["maxParticleCount"] = data.maxParticleCount;
            return root;
        }

        void From_Json(const json& root, SpawnModuleData& outData)
        {
            if (!root.is_object())
                return;

            Read_BoolField(root, "processSpawnRate", outData.processSpawnRate);
            const auto spawnRateIter = root.find("spawnRate");
            if (spawnRateIter != root.end())
                From_Json(*spawnRateIter, outData.spawnRate);
            const auto spawnRateScaleIter = root.find("spawnRateScale");
            if (spawnRateScaleIter != root.end())
                From_Json(*spawnRateScaleIter, outData.spawnRateScale);
            Read_BoolField(root, "processBurstList", outData.processBurstList);
            const auto burstScaleIter = root.find("burstScale");
            if (burstScaleIter != root.end())
                From_Json(*burstScaleIter, outData.burstScale);
            Read_NumberField(root, "maxParticleCount", outData.maxParticleCount);

            const auto burstListIter = root.find("burstList");
            if (burstListIter != root.end() && burstListIter->is_array())
            {
                outData.burstList.clear();
                for (const json& burstNode : *burstListIter)
                {
                    if (!burstNode.is_object())
                        continue;

                    SpawnModuleData::ParticleBurstData burst{};
                    Read_NumberField(burstNode, "time", burst.time);
                    Read_NumberField(burstNode, "count", burst.count);
                    outData.burstList.push_back(burst);
                }
            }
        }

        json To_Json(const LifetimeModuleData& data)
        {
            json root = json::object();
            root["lifeTime"] = To_Json(data.lifeTime);
            return root;
        }

        void From_Json(const json& root, LifetimeModuleData& outData)
        {
            const auto iter = root.find("lifeTime");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.lifeTime);
            if (root.is_object())
            {
                DistributionRandomSeedData legacySeed{};
                Read_RandomSeedField(root, legacySeed);
                Apply_LegacyRandomSeedToUniform(outData.lifeTime, legacySeed);
            }
        }

        json To_Json(const InitialLocationModuleData& data)
        {
            json root = json::object();
            root["location"] = To_Json(data.location);
            return root;
        }

        void From_Json(const json& root, InitialLocationModuleData& outData)
        {
            const auto iter = root.find("location");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.location);
            if (root.is_object())
            {
                DistributionRandomSeedData legacySeed{};
                Read_RandomSeedField(root, legacySeed);
                Apply_LegacyRandomSeedToUniform(outData.location, legacySeed);
            }
        }

        json To_Json(const RibbonOrientationModuleData& data)
        {
            json root = json::object();
            root["spreadBasis"] = To_EnumToken(data.spreadBasis);
            root["spreadAngleDegrees"] = data.spreadAngleDegrees;
            return root;
        }

        void From_Json(const json& root, RibbonOrientationModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto spreadBasisIter = root.find("spreadBasis");
            if (spreadBasisIter != root.end())
                Try_ReadEnumToken(*spreadBasisIter, outData.spreadBasis);
            Read_NumberField(root, "spreadAngleDegrees", outData.spreadAngleDegrees);
        }

        json To_Json(const SphereLocationModuleData& data)
        {
            json root = json::object();
            root["offset"] = To_Json(data.offset);
            root["radius"] = data.radius;
            root["spawnMode"] = To_EnumToken(data.spawnMode);
            root["placementMode"] = To_EnumToken(data.placementMode);
            Write_RandomSeedField(root, data.randomSeed);
            return root;
        }

        void From_Json(const json& root, SphereLocationModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto offsetIter = root.find("offset");
            if (offsetIter != root.end())
                Read_Vec3(*offsetIter, outData.offset);
            Read_NumberField(root, "radius", outData.radius);
            const auto spawnModeIter = root.find("spawnMode");
            if (spawnModeIter != root.end())
                Try_ReadEnumToken(*spawnModeIter, outData.spawnMode);
            const auto placementModeIter = root.find("placementMode");
            if (placementModeIter != root.end())
                Try_ReadEnumToken(*placementModeIter, outData.placementMode);
            Read_RandomSeedField(root, outData.randomSeed);
        }

        void Read_FloatDistributionOrLegacyRangeField(
            const json& root,
            const char* distributionKey,
            const char* legacyRangeKey,
            FloatDistributionData& outData)
        {
            if (const auto distributionIter = root.find(distributionKey);
                distributionIter != root.end() && distributionIter->is_object())
            {
                From_Json(*distributionIter, outData);
                return;
            }

            if (const auto legacyIter = root.find(legacyRangeKey); legacyIter != root.end())
            {
                Vec2 legacyRange{};
                Read_Vec2(*legacyIter, legacyRange);
                outData = legacyRange.x == legacyRange.y
                          ? FloatDistributionData::Make_Constant(legacyRange.x)
                          : FloatDistributionData::Make_Uniform(legacyRange.x, legacyRange.y);
            }
        }

        json To_Json(const PlaneRadialLocationModuleData& data)
        {
            json root = json::object();
            root["plane"] = To_EnumToken(data.plane);
            root["shape"] = To_EnumToken(data.shape);
            root["placementMode"] = To_EnumToken(data.placementMode);
            root["offset"] = To_Json(data.offset);
            root["uDistribution"] = To_Json(data.uDistribution);
            root["vDistribution"] = To_Json(data.vDistribution);
            root["radiusDistribution"] = To_Json(data.radiusDistribution);
            root["angleDegreesDistribution"] = To_Json(data.angleDegreesDistribution);
            root["thickness"] = data.thickness;
            return root;
        }

        void From_Json(const json& root, PlaneRadialLocationModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto planeIter = root.find("plane");
            if (planeIter != root.end())
                Try_ReadEnumToken(*planeIter, outData.plane);
            const auto shapeIter = root.find("shape");
            if (shapeIter != root.end())
                Try_ReadEnumToken(*shapeIter, outData.shape);
            const auto placementModeIter = root.find("placementMode");
            if (placementModeIter != root.end())
                Try_ReadEnumToken(*placementModeIter, outData.placementMode);

            const auto offsetIter = root.find("offset");
            if (offsetIter != root.end())
                Read_Vec3(*offsetIter, outData.offset);
            Read_FloatDistributionOrLegacyRangeField(root, "uDistribution", "rangeU", outData.uDistribution);
            Read_FloatDistributionOrLegacyRangeField(root, "vDistribution", "rangeV", outData.vDistribution);
            Read_FloatDistributionOrLegacyRangeField(root, "radiusDistribution", "radiusRange", outData.radiusDistribution);
            Read_FloatDistributionOrLegacyRangeField(root, "angleDegreesDistribution", "angleDegreesRange", outData.angleDegreesDistribution);
            Read_NumberField(root, "thickness", outData.thickness);
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.uDistribution, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.vDistribution, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.radiusDistribution, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.angleDegreesDistribution, legacySeed);
        }

        json To_Json(const CylinderLocationModuleData& data)
        {
            json root = json::object();
            root["axis"] = To_EnumToken(data.axis);
            root["spawnMode"] = To_EnumToken(data.spawnMode);
            root["placementMode"] = To_EnumToken(data.placementMode);
            root["offset"] = To_Json(data.offset);
            root["radiusDistribution"] = To_Json(data.radiusDistribution);
            root["heightDistribution"] = To_Json(data.heightDistribution);
            root["angleDegreesDistribution"] = To_Json(data.angleDegreesDistribution);
            return root;
        }

        void From_Json(const json& root, CylinderLocationModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto axisIter = root.find("axis");
            if (axisIter != root.end())
                Try_ReadEnumToken(*axisIter, outData.axis);
            const auto spawnModeIter = root.find("spawnMode");
            if (spawnModeIter != root.end())
                Try_ReadEnumToken(*spawnModeIter, outData.spawnMode);
            const auto placementModeIter = root.find("placementMode");
            if (placementModeIter != root.end())
                Try_ReadEnumToken(*placementModeIter, outData.placementMode);

            const auto offsetIter = root.find("offset");
            if (offsetIter != root.end())
                Read_Vec3(*offsetIter, outData.offset);
            Read_FloatDistributionOrLegacyRangeField(root, "radiusDistribution", "radiusRange", outData.radiusDistribution);
            Read_FloatDistributionOrLegacyRangeField(root, "heightDistribution", "heightRange", outData.heightDistribution);
            Read_FloatDistributionOrLegacyRangeField(root, "angleDegreesDistribution", "angleDegreesRange", outData.angleDegreesDistribution);
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.radiusDistribution, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.heightDistribution, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.angleDegreesDistribution, legacySeed);
        }

        json To_Json(const PlaneRadialOrientationModuleData& data)
        {
            json root = json::object();
            root["targetKind"] = To_EnumToken(data.targetKind);
            root["orientationMode"] = To_EnumToken(data.orientationMode);
            root["meshForwardAxis"] = To_EnumToken(data.meshForwardAxis);
            root["meshUpAxis"] = To_EnumToken(data.meshUpAxis);
            root["tiltDegrees"] = data.tiltDegrees;
            root["rollOffsetDegrees"] = data.rollOffsetDegrees;
            return root;
        }

        void From_Json(const json& root, PlaneRadialOrientationModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto targetKindIter = root.find("targetKind");
            if (targetKindIter != root.end())
                Try_ReadEnumToken(*targetKindIter, outData.targetKind);
            const auto orientationModeIter = root.find("orientationMode");
            if (orientationModeIter != root.end())
                Try_ReadEnumToken(*orientationModeIter, outData.orientationMode);
            const auto meshForwardAxisIter = root.find("meshForwardAxis");
            if (meshForwardAxisIter != root.end())
                Try_ReadEnumToken(*meshForwardAxisIter, outData.meshForwardAxis);
            const auto meshUpAxisIter = root.find("meshUpAxis");
            if (meshUpAxisIter != root.end())
                Try_ReadEnumToken(*meshUpAxisIter, outData.meshUpAxis);
            Read_NumberField(root, "tiltDegrees", outData.tiltDegrees);
            Read_NumberField(root, "rollOffsetDegrees", outData.rollOffsetDegrees);
        }

        json To_Json(const CylinderOrientationModuleData& data)
        {
            json root = json::object();
            root["targetKind"] = To_EnumToken(data.targetKind);
            root["orientationMode"] = To_EnumToken(data.orientationMode);
            root["followOrbitOverLife"] = data.followOrbitOverLife;
            root["meshForwardAxis"] = To_EnumToken(data.meshForwardAxis);
            root["meshUpAxis"] = To_EnumToken(data.meshUpAxis);
            root["tiltDegrees"] = data.tiltDegrees;
            root["rollOffsetDegrees"] = data.rollOffsetDegrees;
            return root;
        }

        void From_Json(const json& root, CylinderOrientationModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto targetKindIter = root.find("targetKind");
            if (targetKindIter != root.end())
                Try_ReadEnumToken(*targetKindIter, outData.targetKind);
            const auto orientationModeIter = root.find("orientationMode");
            if (orientationModeIter != root.end())
                Try_ReadEnumToken(*orientationModeIter, outData.orientationMode);
            Read_BoolField(root, "followOrbitOverLife", outData.followOrbitOverLife);
            const auto meshForwardAxisIter = root.find("meshForwardAxis");
            if (meshForwardAxisIter != root.end())
                Try_ReadEnumToken(*meshForwardAxisIter, outData.meshForwardAxis);
            const auto meshUpAxisIter = root.find("meshUpAxis");
            if (meshUpAxisIter != root.end())
                Try_ReadEnumToken(*meshUpAxisIter, outData.meshUpAxis);
            Read_NumberField(root, "tiltDegrees", outData.tiltDegrees);
            Read_NumberField(root, "rollOffsetDegrees", outData.rollOffsetDegrees);
        }

        json To_Json(const SphereRadialOrientationModuleData& data)
        {
            json root = json::object();
            root["orientationMode"] = To_EnumToken(data.orientationMode);
            root["meshForwardAxis"] = To_EnumToken(data.meshForwardAxis);
            root["meshUpAxis"] = To_EnumToken(data.meshUpAxis);
            root["tiltDegrees"] = data.tiltDegrees;
            root["rollOffsetDegrees"] = data.rollOffsetDegrees;
            return root;
        }

        void From_Json(const json& root, SphereRadialOrientationModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto orientationModeIter = root.find("orientationMode");
            if (orientationModeIter != root.end())
                Try_ReadEnumToken(*orientationModeIter, outData.orientationMode);
            const auto meshForwardAxisIter = root.find("meshForwardAxis");
            if (meshForwardAxisIter != root.end())
                Try_ReadEnumToken(*meshForwardAxisIter, outData.meshForwardAxis);
            const auto meshUpAxisIter = root.find("meshUpAxis");
            if (meshUpAxisIter != root.end())
                Try_ReadEnumToken(*meshUpAxisIter, outData.meshUpAxis);
            Read_NumberField(root, "tiltDegrees", outData.tiltDegrees);
            Read_NumberField(root, "rollOffsetDegrees", outData.rollOffsetDegrees);
        }

        json To_Json(const InitialSizeModuleData& data)
        {
            json root = json::object();
            root["size"] = To_Json(data.size);
            return root;
        }

        void From_Json(const json& root, InitialSizeModuleData& outData)
        {
            const auto iter = root.find("size");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.size);
        }

        json To_Json(const InitialMeshSizeModuleData& data)
        {
            json root = json::object();
            root["size"] = To_Json(data.size);
            return root;
        }

        void From_Json(const json& root, InitialMeshSizeModuleData& outData)
        {
            const auto iter = root.find("size");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.size);
        }

        json To_Json(const InitialVelocityModuleData& data)
        {
            json root = json::object();
            root["velocity"] = To_Json(data.velocity);
            root["inWorldSpace"] = data.inWorldSpace;
            return root;
        }

        void From_Json(const json& root, InitialVelocityModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto velocityIter = root.find("velocity");
            if (velocityIter != root.end())
                From_Json(*velocityIter, outData.velocity);
            Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.velocity, legacySeed);
        }

        json To_Json(const InitialRadialVelocityModuleData& data)
        {
            json root = json::object();
            root["speed"] = To_Json(data.speed);
            root["radialPivot"] = To_Json(data.radialPivot);
            root["centerDirectionMode"] = To_EnumToken(data.centerDirectionMode);
            root["inWorldSpace"] = data.inWorldSpace;
            return root;
        }

        void From_Json(const json& root, InitialRadialVelocityModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto speedIter = root.find("speed");
            if (speedIter != root.end())
                From_Json(*speedIter, outData.speed);
            const auto radialPivotIter = root.find("radialPivot");
            if (radialPivotIter != root.end())
                Read_Vec3(*radialPivotIter, outData.radialPivot);
            const auto centerDirectionModeIter = root.find("centerDirectionMode");
            if (centerDirectionModeIter != root.end())
                Try_ReadEnumToken(*centerDirectionModeIter, outData.centerDirectionMode);
            Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.speed, legacySeed);
        }

        json To_Json(const VelocityConeModuleData& data)
        {
            json root = json::object();
            root["axis"] = To_Json(data.axis);
            root["angleDegrees"] = data.angleDegrees;
            root["speed"] = To_Json(data.speed);
            root["inWorldSpace"] = data.inWorldSpace;
            return root;
        }

        void From_Json(const json& root, VelocityConeModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto axisIter = root.find("axis");
            if (axisIter != root.end())
                Read_Vec3(*axisIter, outData.axis);
            Read_NumberField(root, "angleDegrees", outData.angleDegrees);
            const auto speedIter = root.find("speed");
            if (speedIter != root.end())
                From_Json(*speedIter, outData.speed);
            Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.speed, legacySeed);
        }

        json To_Json(const SourceMotionVelocityModuleData& data)
        {
            json root = json::object();
            root["directionMode"] = To_EnumToken(data.directionMode);
            root["speed"] = To_Json(data.speed);
            root["sourceSpeedScale"] = data.sourceSpeedScale;
            root["spreadAngleDegrees"] = data.spreadAngleDegrees;
            return root;
        }

        void From_Json(const json& root, SourceMotionVelocityModuleData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto directionModeIter = root.find("directionMode"); directionModeIter != root.end())
                Try_ReadEnumToken(*directionModeIter, outData.directionMode);
            if (outData.directionMode == SourceMotionVelocityDirectionMode::InheritSourceVelocity)
                outData.directionMode = SourceMotionVelocityDirectionMode::SourceVelocityDirection;
            if (const auto speedIter = root.find("speed"); speedIter != root.end())
                From_Json(*speedIter, outData.speed);
            Read_NumberField(root, "sourceSpeedScale", outData.sourceSpeedScale);
            Read_NumberField(root, "spreadAngleDegrees", outData.spreadAngleDegrees);
            DistributionRandomSeedData legacySeed{};
            Read_RandomSeedField(root, legacySeed);
            Apply_LegacyRandomSeedToUniform(outData.speed, legacySeed);
        }

        json To_Json(const AccelerationModuleData& data)
        {
            json root = json::object();
            root["acceleration"] = To_Json(data.acceleration);
            root["timeBasis"] = To_EnumToken(data.timeBasis);
            root["inWorldSpace"] = data.inWorldSpace;
            return root;
        }

        void From_Json(const json& root, AccelerationModuleData& outData)
        {
            const auto iter = root.find("acceleration");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.acceleration);
            if (root.is_object())
            {
                if (const auto timeBasisIter = root.find("timeBasis"); timeBasisIter != root.end())
                    Try_ReadEnumToken(*timeBasisIter, outData.timeBasis);
                Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
                DistributionRandomSeedData legacySeed{};
                Read_RandomSeedField(root, legacySeed);
                Apply_LegacyRandomSeedToUniform(outData.acceleration, legacySeed);
            }
        }

        json To_Json(const DragModuleData& data)
        {
            json root = json::object();
            root["drag"] = To_Json(data.drag);
            return root;
        }

        void From_Json(const json& root, DragModuleData& outData)
        {
            const auto iter = root.find("drag");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.drag);
            if (root.is_object())
            {
                DistributionRandomSeedData legacySeed{};
                Read_RandomSeedField(root, legacySeed);
                Apply_LegacyRandomSeedToUniform(outData.drag, legacySeed);
            }
        }

        json To_Json(const VelocityOverLifeModuleData& data)
        {
            json root = json::object();
            root["scaleOverLife"] = To_Json(data.scaleOverLife);
            root["applyChannels"] = To_JsonVelocityOverLifeApplyChannels(data.applyChannelMask);
            return root;
        }

        void From_Json(const json& root, VelocityOverLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto scaleOverLifeIter = root.find("scaleOverLife");
            if (scaleOverLifeIter != root.end())
                From_Json(*scaleOverLifeIter, outData.scaleOverLife);
            else
            {
                float scaleStart{ 1.f };
                float scaleEnd{ 1.f };
                Read_NumberField(root, "scaleStart", scaleStart);
                Read_NumberField(root, "scaleEnd", scaleEnd);
                outData.scaleOverLife = FloatDistributionData::Make_ConstantCurve(scaleStart, scaleEnd);
            }

            outData.applyChannelMask = Read_VelocityOverLifeApplyChannelMask(root);
        }

        json To_Json(const OrbitOverLifeModuleData& data)
        {
            json root = json::object();
            root["pivotMode"] = To_EnumToken(data.pivotMode);
            root["plane"] = To_EnumToken(data.plane);
            root["angleDegreesOverLife"] = To_Json(data.angleDegreesOverLife);
            root["radiusScaleOverLife"] = To_Json(data.radiusScaleOverLife);
            root["orientationMode"] = To_EnumToken(data.orientationMode);
            return root;
        }

        void From_Json(const json& root, OrbitOverLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto pivotModeIter = root.find("pivotMode");
            if (pivotModeIter != root.end())
                Try_ReadEnumToken(*pivotModeIter, outData.pivotMode);
            const auto planeIter = root.find("plane");
            if (planeIter != root.end())
                Try_ReadEnumToken(*planeIter, outData.plane);
            const auto angleIter = root.find("angleDegreesOverLife");
            if (angleIter != root.end())
                From_Json(*angleIter, outData.angleDegreesOverLife);
            const auto radiusIter = root.find("radiusScaleOverLife");
            if (radiusIter != root.end())
                From_Json(*radiusIter, outData.radiusScaleOverLife);
            const auto orientationIter = root.find("orientationMode");
            if (orientationIter != root.end())
                Try_ReadEnumToken(*orientationIter, outData.orientationMode);
        }

        json To_Json(const InitialRotationModuleData& data)
        {
            json root = json::object();
            root["rotationDegrees"] = To_Json(data.rotationDegrees);
            return root;
        }

        void From_Json(const json& root, InitialRotationModuleData& outData)
        {
            const auto iter = root.find("rotationDegrees");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.rotationDegrees);
            if (root.is_object())
            {
                DistributionRandomSeedData legacySeed{};
                Read_RandomSeedField(root, legacySeed);
                Apply_LegacyRandomSeedToUniform(outData.rotationDegrees, legacySeed);
            }
        }

        json To_Json(const RotationOverLifeModuleData& data)
        {
            json root = json::object();
            root["rotationOverLife"] = To_Json(data.rotationOverLife);
            return root;
        }

        void From_Json(const json& root, RotationOverLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto rotationOverLifeIter = root.find("rotationOverLife");
            if (rotationOverLifeIter != root.end())
                From_Json(*rotationOverLifeIter, outData.rotationOverLife);
            else
            {
                float rotationStartDegrees{ 0.f };
                float rotationEndDegrees{ 0.f };
                Read_NumberField(root, "rotationStartDegrees", rotationStartDegrees);
                Read_NumberField(root, "rotationEndDegrees", rotationEndDegrees);
                outData.rotationOverLife = FloatDistributionData::Make_ConstantCurve(rotationStartDegrees, rotationEndDegrees);
            }
        }

        json To_Json(const SpriteTiltModuleData& data)
        {
            json root = json::object();
            root["tiltDegrees"] = To_Json(data.tiltDegrees);
            return root;
        }

        void From_Json(const json& root, SpriteTiltModuleData& outData)
        {
            const auto iter = root.find("tiltDegrees");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.tiltDegrees);
        }

        json To_Json(const SpriteTiltOverLifeModuleData& data)
        {
            json root = json::object();
            root["tiltOverLife"] = To_Json(data.tiltOverLife);
            return root;
        }

        void From_Json(const json& root, SpriteTiltOverLifeModuleData& outData)
        {
            const auto iter = root.find("tiltOverLife");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.tiltOverLife);
        }

        json To_Json(const InitialRotationRateModuleData& data)
        {
            json root = json::object();
            root["rotationRateDegrees"] = To_Json(data.rotationRateDegrees);
            return root;
        }

        void From_Json(const json& root, InitialRotationRateModuleData& outData)
        {
            const auto iter = root.find("rotationRateDegrees");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.rotationRateDegrees);
            if (root.is_object())
            {
                DistributionRandomSeedData legacySeed{};
                Read_RandomSeedField(root, legacySeed);
                Apply_LegacyRandomSeedToUniform(outData.rotationRateDegrees, legacySeed);
            }
        }

        json To_Json(const RotationRateScaleByLifeModuleData& data)
        {
            json root = json::object();
            root["scaleOverLife"] = To_Json(data.scaleOverLife);
            return root;
        }

        void From_Json(const json& root, RotationRateScaleByLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto scaleOverLifeIter = root.find("scaleOverLife");
            if (scaleOverLifeIter != root.end())
                From_Json(*scaleOverLifeIter, outData.scaleOverLife);
            else
            {
                float scaleStart{ 1.f };
                float scaleEnd{ 1.f };
                Read_NumberField(root, "scaleStart", scaleStart);
                Read_NumberField(root, "scaleEnd", scaleEnd);
                outData.scaleOverLife = FloatDistributionData::Make_ConstantCurve(scaleStart, scaleEnd);
            }
        }

        json To_Json(const InitialMeshRotationModuleData& data)
        {
            json root = json::object();
            root["rotationDegrees"] = To_Json(data.rotationDegrees);
            return root;
        }

        void From_Json(const json& root, InitialMeshRotationModuleData& outData)
        {
            const auto iter = root.find("rotationDegrees");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.rotationDegrees);
        }

        json To_Json(const MeshRotationOverLifeModuleData& data)
        {
            json root = json::object();
            root["rotationOverLife"] = To_Json(data.rotationOverLife);
            return root;
        }

        void From_Json(const json& root, MeshRotationOverLifeModuleData& outData)
        {
            const auto iter = root.find("rotationOverLife");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.rotationOverLife);
        }

        json To_Json(const MeshDirectionAlignOverLifeModuleData& data)
        {
            json root = json::object();
            root["targetMode"] = To_EnumToken(data.targetMode);
            root["space"] = To_EnumToken(data.space);
            root["target"] = To_Json(data.target);
            root["meshForwardAxis"] = To_EnumToken(data.meshForwardAxis);
            root["meshUpAxis"] = To_EnumToken(data.meshUpAxis);
            root["alignmentProgress"] = To_Json(data.alignmentProgress);
            root["randomDelay"] = To_Json(data.randomDelay);
            root["randomWeightScale"] = To_Json(data.randomWeightScale);
            root["blendMode"] = To_EnumToken(data.blendMode);
            return root;
        }

        void From_Json(const json& root, MeshDirectionAlignOverLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto iter = root.find("targetMode"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.targetMode);
            if (const auto iter = root.find("space"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.space);
            if (const auto iter = root.find("target"); iter != root.end())
                Read_Vec3(*iter, outData.target);
            if (const auto iter = root.find("meshForwardAxis"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.meshForwardAxis);
            if (const auto iter = root.find("meshUpAxis"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.meshUpAxis);
            if (const auto iter = root.find("alignmentProgress"); iter != root.end())
                From_Json(*iter, outData.alignmentProgress);
            if (const auto iter = root.find("randomDelay"); iter != root.end())
                From_Json(*iter, outData.randomDelay);
            if (const auto iter = root.find("randomWeightScale"); iter != root.end())
                From_Json(*iter, outData.randomWeightScale);
            if (const auto iter = root.find("blendMode"); iter != root.end())
                Try_ReadEnumToken(*iter, outData.blendMode);
        }

        json To_Json(const InitialMeshRotationRateModuleData& data)
        {
            json root = json::object();
            root["rotationRateDegrees"] = To_Json(data.rotationRateDegrees);
            root["inWorldSpace"] = data.inWorldSpace;
            return root;
        }

        void From_Json(const json& root, InitialMeshRotationRateModuleData& outData)
        {
            const auto iter = root.find("rotationRateDegrees");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.rotationRateDegrees);
            Read_BoolField(root, "inWorldSpace", outData.inWorldSpace);
        }

        json To_Json(const MeshRotationRateScaleByLifeModuleData& data)
        {
            json root = json::object();
            root["scaleOverLife"] = To_Json(data.scaleOverLife);
            return root;
        }

        void From_Json(const json& root, MeshRotationRateScaleByLifeModuleData& outData)
        {
            const auto iter = root.find("scaleOverLife");
            if (root.is_object() && iter != root.end())
                From_Json(*iter, outData.scaleOverLife);
        }

        json To_Json(const InitialColorModuleData& data)
        {
            json root = json::object();
            root["color"] = To_Json(data.color);
            root["alpha"] = To_Json(data.alpha);
            return root;
        }

        void From_Json(const json& root, InitialColorModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto colorIter = root.find("color");
            if (colorIter != root.end())
                From_Json(*colorIter, outData.color);
            const auto alphaIter = root.find("alpha");
            if (alphaIter != root.end())
                From_Json(*alphaIter, outData.alpha);
        }

        json To_Json(const ColorOverLifeModuleData& data)
        {
            json root = json::object();
            root["colorOverLife"] = To_Json(data.colorOverLife);
            root["alphaOverLife"] = To_Json(data.alphaOverLife);
            return root;
        }

        void From_Json(const json& root, ColorOverLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto colorIter = root.find("colorOverLife");
            if (colorIter != root.end())
                From_Json(*colorIter, outData.colorOverLife);
            const auto alphaIter = root.find("alphaOverLife");
            if (alphaIter != root.end())
                From_Json(*alphaIter, outData.alphaOverLife);
        }

        json To_Json(const SubUVFrameOverLifeModuleData& data)
        {
            json root = json::object();
            root["frameIndex"] = To_Json(data.frameIndex);
            root["startFrame"] = data.startFrame;
            root["endFrame"] = data.endFrame;
            root["loop"] = data.loop;
            root["playbackMode"] = To_EnumToken(data.playbackMode);
            root["framesPerSecond"] = data.framesPerSecond;
            root["randomStartPhase"] = data.randomStartPhase;
            Write_RandomSeedField(root, data.randomSeed);
            root["perStripSubUVVariation"] = data.perStripSubUVVariation;
            return root;
        }

        void From_Json(const json& root, SubUVFrameOverLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto frameIndexIter = root.find("frameIndex");
            if (frameIndexIter != root.end())
                From_Json(*frameIndexIter, outData.frameIndex);
            Read_NumberField(root, "startFrame", outData.startFrame);
            Read_NumberField(root, "endFrame", outData.endFrame);
            Read_BoolField(root, "loop", outData.loop);
            const auto playbackModeIter = root.find("playbackMode");
            if (playbackModeIter != root.end())
                Try_ReadEnumToken(*playbackModeIter, outData.playbackMode);
            Read_NumberField(root, "framesPerSecond", outData.framesPerSecond);
            Read_BoolField(root, "randomStartPhase", outData.randomStartPhase);
            Read_RandomSeedField(root, outData.randomSeed);
            Read_BoolField(root, "perStripSubUVVariation", outData.perStripSubUVVariation);
        }

        //## Codec::ModuleData

        json To_Json(const SizeByLifeModuleData& data)
        {
            json root = json::object();
            root["scaleOverLife"] = To_Json(data.scaleOverLife);
            root["multiplyX"] = data.multiplyX;
            root["multiplyY"] = data.multiplyY;
            root["axisLock"] = To_EnumToken(data.axisLock);
            return root;
        }

        void From_Json(const json& root, SizeByLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            Read_BoolField(root, "multiplyX", outData.multiplyX);
            Read_BoolField(root, "multiplyY", outData.multiplyY);

            const auto axisLockIter = root.find("axisLock");
            if (axisLockIter != root.end())
                Try_ReadEnumToken(*axisLockIter, outData.axisLock);

            const auto scaleOverLifeIter = root.find("scaleOverLife");
            if (scaleOverLifeIter != root.end())
                From_Json(*scaleOverLifeIter, outData.scaleOverLife);
            else
            {
                float multiplyXStart{ 1.f };
                float multiplyXEnd{ 1.f };
                float multiplyYStart{ 1.f };
                float multiplyYEnd{ 1.f };
                Read_NumberField(root, "multiplyXStart", multiplyXStart);
                Read_NumberField(root, "multiplyXEnd", multiplyXEnd);
                Read_NumberField(root, "multiplyYStart", multiplyYStart);
                Read_NumberField(root, "multiplyYEnd", multiplyYEnd);

                const auto responseCurveIter = root.find("responseCurve");
                if (responseCurveIter == root.end())
                {
                    outData.scaleOverLife = Vector2DistributionData::Make_ConstantCurve(
                        Vec2{ multiplyXStart, multiplyYStart },
                        Vec2{ multiplyXEnd, multiplyYEnd }
                    );
                }
                else
                {
                    FloatDistributionData responseCurve{ FloatDistributionData::Make_ConstantCurve(0.f, 1.f) };
                    From_Json(*responseCurveIter, responseCurve);

                    ConstantCurveVector2DistributionData bakedCurve{};
                    bakedCurve.keys.clear();

                    const auto* curve = get_if<ConstantCurveFloatDistributionData>(&responseCurve.payload);
                    if (responseCurve.mode == DistributionMode::ConstantCurve && curve != nullptr && !curve->keys.empty())
                    {
                        for (const FloatCurveKeyData& responseKey : curve->keys)
                        {
                            const float ratio = Evaluate_FloatConstantCurve(*curve, responseKey.time, responseKey.value);
                            bakedCurve.keys.push_back(
                                Vector2CurveKeyData{
                                    responseKey.time,
                                    Lerp_Vec2(Vec2{ multiplyXStart, multiplyYStart }, Vec2{ multiplyXEnd, multiplyYEnd }, ratio),
                                    Vec2{},
                                    Vec2{},
                                    responseKey.interpolationMode
                                }
                            );
                        }
                    }

                    if (bakedCurve.keys.empty())
                    {
                        bakedCurve.keys = get<ConstantCurveVector2DistributionData>(
                            Vector2DistributionData::Make_ConstantCurve(
                                Vec2{ multiplyXStart, multiplyYStart },
                                Vec2{ multiplyXEnd, multiplyYEnd }
                            ).payload
                        ).keys;
                    }

                    outData.scaleOverLife.mode = DistributionMode::ConstantCurve;
                    outData.scaleOverLife.payload = bakedCurve;
                }
            }
        }

        FloatDistributionData Extract_FloatChannel(const Vector2DistributionData& data, bool useY)
        {
            if (const auto* constant = get_if<ConstantVector2DistributionData>(&data.payload))
                return FloatDistributionData::Make_Constant(useY ? constant->value.y : constant->value.x);

            if (const auto* uniform = get_if<UniformVector2DistributionData>(&data.payload))
            {
                return FloatDistributionData::Make_Uniform(
                    useY ? uniform->minValue.y : uniform->minValue.x,
                    useY ? uniform->maxValue.y : uniform->maxValue.x
                );
            }

            if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&data.payload))
            {
                ConstantCurveFloatDistributionData floatCurve{};
                floatCurve.keys.clear();
                floatCurve.keys.reserve(curve->keys.size());
                for (const Vector2CurveKeyData& key : curve->keys)
                {
                    floatCurve.keys.push_back(
                        FloatCurveKeyData{
                            key.time,
                            useY ? key.value.y : key.value.x,
                            useY ? key.arriveTangent.y : key.arriveTangent.x,
                            useY ? key.leaveTangent.y : key.leaveTangent.x,
                            key.interpolationMode
                        }
                    );
                }

                FloatDistributionData outData{};
                outData.mode = DistributionMode::ConstantCurve;
                outData.payload = floatCurve;
                return outData;
            }

            return FloatDistributionData::Make_Constant(0.f);
        }

        void Apply_LegacyBeamEnvelope(const json& legacyEnvelope, BeamEnvelopeOverLifeModuleData& outData)
        {
            Vector2DistributionData legacyData{ Vector2DistributionData::Make_ConstantCurve(Vec2{ 1.f, 1.f }) };
            From_Json(legacyEnvelope, legacyData);
            outData.startRatioOverLife = FloatDistributionData::Make_ConstantCurve(0.f);
            outData.endRatioOverLife = Extract_FloatChannel(legacyData, false);
            outData.widthScaleOverLife = Extract_FloatChannel(legacyData, true);
        }

        json To_Json(const BeamEnvelopeOverLifeModuleData& data)
        {
            json root = json::object();
            root["startRatioOverLife"] = To_Json(data.startRatioOverLife);
            root["endRatioOverLife"] = To_Json(data.endRatioOverLife);
            root["widthScaleOverLife"] = To_Json(data.widthScaleOverLife);
            return root;
        }

        void From_Json(const json& root, BeamEnvelopeOverLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto legacyEnvelopeIter = root.find("envelopeOverLife");
            if (legacyEnvelopeIter != root.end())
                Apply_LegacyBeamEnvelope(*legacyEnvelopeIter, outData);

            const auto startRatioIter = root.find("startRatioOverLife");
            if (startRatioIter != root.end())
                From_Json(*startRatioIter, outData.startRatioOverLife);

            const auto endRatioIter = root.find("endRatioOverLife");
            if (endRatioIter != root.end())
                From_Json(*endRatioIter, outData.endRatioOverLife);

            const auto widthScaleIter = root.find("widthScaleOverLife");
            if (widthScaleIter != root.end())
                From_Json(*widthScaleIter, outData.widthScaleOverLife);
        }

        json To_Json(const MeshSizeByLifeModuleData& data)
        {
            json root = json::object();
            root["scaleOverLife"] = To_Json(data.scaleOverLife);
            root["multiplyX"] = data.multiplyX;
            root["multiplyY"] = data.multiplyY;
            root["multiplyZ"] = data.multiplyZ;
            return root;
        }

        void From_Json(const json& root, MeshSizeByLifeModuleData& outData)
        {
            if (!root.is_object())
                return;

            Read_BoolField(root, "multiplyX", outData.multiplyX);
            Read_BoolField(root, "multiplyY", outData.multiplyY);
            Read_BoolField(root, "multiplyZ", outData.multiplyZ);

            const auto scaleOverLifeIter = root.find("scaleOverLife");
            if (scaleOverLifeIter != root.end())
                From_Json(*scaleOverLifeIter, outData.scaleOverLife);
        }

        json To_Json(const SpawnPerUnitModuleData& data)
        {
            json root = json::object();
            root["spawnPerUnit"] = To_Json(data.spawnPerUnit);
            root["unitScalar"] = data.unitScalar;
            root["movementTolerance"] = data.movementTolerance;
            root["maxFrameDistance"] = data.maxFrameDistance;
            return root;
        }

        void From_Json(const json& root, SpawnPerUnitModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto spawnPerUnitIter = root.find("spawnPerUnit");
            if (spawnPerUnitIter != root.end())
                From_Json(*spawnPerUnitIter, outData.spawnPerUnit);

            Read_NumberField(root, "unitScalar", outData.unitScalar);
            Read_NumberField(root, "movementTolerance", outData.movementTolerance);
            Read_NumberField(root, "maxFrameDistance", outData.maxFrameDistance);

            if (spawnPerUnitIter == root.end())
            {
                float legacyUnitDistance = 0.f;
                Read_NumberField(root, "unitDistance", legacyUnitDistance);
                if (legacyUnitDistance > 0.f)
                    outData.spawnPerUnit = FloatDistributionData::Make_Constant(max(0.0001f, outData.unitScalar) / legacyUnitDistance);
            }
        }

        json To_Json(const SourceHistorySpriteTrailPathFollowModuleData& data)
        {
            json root = json::object();
            root["direction"] = To_EnumToken(data.direction);
            root["speed"] = To_Json(data.speed);
            root["startDelay"] = To_Json(data.startDelay);
            root["arrivalMode"] = To_EnumToken(data.arrivalMode);
            return root;
        }

        void From_Json(const json& root, SourceHistorySpriteTrailPathFollowModuleData& outData)
        {
            if (!root.is_object())
                return;

            if (const auto directionIter = root.find("direction"); directionIter != root.end())
                Try_ReadEnumToken(*directionIter, outData.direction);
            if (const auto speedIter = root.find("speed"); speedIter != root.end())
                From_Json(*speedIter, outData.speed);
            if (const auto startDelayIter = root.find("startDelay"); startDelayIter != root.end())
                From_Json(*startDelayIter, outData.startDelay);
            outData.arrivalMode = Resolve_SourceHistorySpriteTrailArrivalMode(root);
        }

        json To_Json(const SourceHistorySpriteTrailPathReplayModuleData& data)
        {
            json root = json::object();
            root["delayTime"] = data.delayTime;
            root["replayMode"] = To_EnumToken(data.replayMode);
            root["speedScale"] = data.speedScale;
            root["drainDuration"] = data.drainDuration;
            root["drainCurve"] = To_Json(data.drainCurve);
            root["startMode"] = To_EnumToken(data.startMode);
            root["arrivalMode"] = To_EnumToken(data.arrivalMode);
            return root;
        }

        void From_Json(const json& root, SourceHistorySpriteTrailPathReplayModuleData& outData)
        {
            if (!root.is_object())
                return;

            Read_NumberField(root, "delayTime", outData.delayTime);
            if (const auto replayModeIter = root.find("replayMode"); replayModeIter != root.end())
                Try_ReadEnumToken(*replayModeIter, outData.replayMode);
            Read_NumberField(root, "speedScale", outData.speedScale);
            Read_NumberField(root, "drainDuration", outData.drainDuration);
            if (const auto drainCurveIter = root.find("drainCurve"); drainCurveIter != root.end())
                From_Json(*drainCurveIter, outData.drainCurve);
            if (const auto startModeIter = root.find("startMode"); startModeIter != root.end())
                Try_ReadEnumToken(*startModeIter, outData.startMode);
            outData.arrivalMode = Resolve_SourceHistorySpriteTrailArrivalMode(root);

            outData.delayTime = max(0.f, outData.delayTime);
            outData.speedScale = max(0.f, outData.speedScale);
            outData.drainDuration = max(0.0001f, outData.drainDuration);
        }

        json To_Json(const MaterialScalarModulatorData& data)
        {
            json root = json::object();
            root["enabled"] = data.enabled;
            root["targetField"] = To_EnumToken(data.targetField);
            root["operation"] = To_EnumToken(data.operation);
            root["timeSource"] = To_EnumToken(data.timeSource);
            root["distribution"] = To_Json(data.distribution);
            return root;
        }

        void From_Json(const json& root, MaterialScalarModulatorData& outData)
        {
            if (!root.is_object())
                return;

            Read_BoolField(root, "enabled", outData.enabled);

            const auto targetIter = root.find("targetField");
            if (targetIter != root.end())
                Try_ReadEnumToken(*targetIter, outData.targetField);

            const auto operationIter = root.find("operation");
            if (operationIter != root.end())
                Try_ReadEnumToken(*operationIter, outData.operation);

            const auto timeSourceIter = root.find("timeSource");
            if (timeSourceIter != root.end())
                Try_ReadEnumToken(*timeSourceIter, outData.timeSource);

            const auto distributionIter = root.find("distribution");
            if (distributionIter != root.end())
                From_Json(*distributionIter, outData.distribution);
        }

        json To_Json(const MaterialCoreColorRgbModulatorData& data)
        {
            json root = json::object();
            root["enabled"] = data.enabled;
            root["timeSource"] = To_EnumToken(data.timeSource);
            root["distribution"] = To_Json(data.distribution);
            return root;
        }

        void From_Json(const json& root, MaterialCoreColorRgbModulatorData& outData)
        {
            if (!root.is_object())
                return;

            Read_BoolField(root, "enabled", outData.enabled);

            const auto timeSourceIter = root.find("timeSource");
            if (timeSourceIter != root.end())
                Try_ReadEnumToken(*timeSourceIter, outData.timeSource);

            const auto distributionIter = root.find("distribution");
            if (distributionIter != root.end())
                From_Json(*distributionIter, outData.distribution);
        }

        json To_Json(const MaterialVec2ModulatorData& data)
        {
            json root = json::object();
            root["enabled"] = data.enabled;
            root["targetField"] = To_EnumToken(data.targetField);
            root["timeSource"] = To_EnumToken(data.timeSource);
            root["distribution"] = To_Json(data.distribution);
            return root;
        }

        void From_Json(const json& root, MaterialVec2ModulatorData& outData)
        {
            if (!root.is_object())
                return;

            Read_BoolField(root, "enabled", outData.enabled);

            const auto targetIter = root.find("targetField");
            if (targetIter != root.end())
                Try_ReadEnumToken(*targetIter, outData.targetField);

            const auto timeSourceIter = root.find("timeSource");
            if (timeSourceIter != root.end())
                Try_ReadEnumToken(*timeSourceIter, outData.timeSource);

            const auto distributionIter = root.find("distribution");
            if (distributionIter != root.end())
                From_Json(*distributionIter, outData.distribution);
        }

        json To_Json(const MaterialScalarModulationModuleData& data)
        {
            json root = json::object();
            root["modulators"] = json::array();
            for (const MaterialScalarModulatorData& modulator : data.modulators)
                root["modulators"].push_back(To_Json(modulator));

            root["coreColorRgbModulators"] = json::array();
            for (const MaterialCoreColorRgbModulatorData& modulator : data.coreColorRgbModulators)
                root["coreColorRgbModulators"].push_back(To_Json(modulator));

            root["vec2Modulators"] = json::array();
            for (const MaterialVec2ModulatorData& modulator : data.vec2Modulators)
                root["vec2Modulators"].push_back(To_Json(modulator));
            return root;
        }

        void From_Json(const json& root, MaterialScalarModulationModuleData& outData)
        {
            if (!root.is_object())
                return;

            const auto modulatorsIter = root.find("modulators");
            if (modulatorsIter != root.end() && modulatorsIter->is_array())
            {
                outData.modulators.clear();
                for (const json& modulatorNode : *modulatorsIter)
                {
                    MaterialScalarModulatorData modulator{};
                    From_Json(modulatorNode, modulator);
                    outData.modulators.push_back(modulator);
                }
            }

            const auto coreColorRgbModulatorsIter = root.find("coreColorRgbModulators");
            if (coreColorRgbModulatorsIter != root.end() && coreColorRgbModulatorsIter->is_array())
            {
                outData.coreColorRgbModulators.clear();
                for (const json& modulatorNode : *coreColorRgbModulatorsIter)
                {
                    MaterialCoreColorRgbModulatorData modulator{};
                    From_Json(modulatorNode, modulator);
                    outData.coreColorRgbModulators.push_back(modulator);
                }
            }

            const auto vec2ModulatorsIter = root.find("vec2Modulators");
            if (vec2ModulatorsIter != root.end() && vec2ModulatorsIter->is_array())
            {
                outData.vec2Modulators.clear();
                for (const json& modulatorNode : *vec2ModulatorsIter)
                {
                    MaterialVec2ModulatorData modulator{};
                    From_Json(modulatorNode, modulator);
                    outData.vec2Modulators.push_back(modulator);
                }
            }
        }
    }
}

json EffectAuthoringJsonSerializer::To_Json(const EffectAuthoringDocument& document)
{
    json root = json::object();
    root["version"] = kEffectAuthoringJsonVersion;
    root["name"] = document.name;
    root["particleSystem"] = Codec::To_Json(document.particleSystemData);
    if (!Codec::Is_DefaultHistoryBudget(document.historyBudget))
        root["historyBudget"] = Codec::To_Json(document.historyBudget);
    if (!document.historyBudget.sourceGroups.empty())
    {
        root["historySourceGroups"] = json::array();
        for (const HistorySourceGroupBudgetData& group : document.historyBudget.sourceGroups)
            root["historySourceGroups"].push_back(Codec::To_Json(group));
    }
    root["emitters"] = json::array();

    for (const AuthoringEmitter& emitter : document.emitters)
        root["emitters"].push_back(To_Json(emitter));

    return root;
}

bool EffectAuthoringJsonSerializer::From_Json(const json& root, EffectAuthoringDocument& outDocument)
{
    if (!root.is_object())
        return false;

    EffectAuthoringDocument document{};
    document.name = root.value("name", string{});

    const auto particleSystemIter = root.find("particleSystem");
    if (particleSystemIter != root.end())
        Codec::From_Json(*particleSystemIter, document.particleSystemData);

    const auto historyBudgetIter = root.find("historyBudget");
    if (historyBudgetIter != root.end())
        Codec::From_Json(*historyBudgetIter, document.historyBudget);

    const auto historySourceGroupsIter = root.find("historySourceGroups");
    if (historySourceGroupsIter != root.end() && historySourceGroupsIter->is_array())
    {
        for (const json& groupNode : *historySourceGroupsIter)
        {
            HistorySourceGroupBudgetData group{};
            Codec::From_Json(groupNode, group);
            if (group.kind != EffectHistorySourceGroupKind::None && !group.stableId.empty())
                document.historyBudget.sourceGroups.push_back(group);
        }
    }

    const auto emittersIter = root.find("emitters");
    if (emittersIter == root.end() || !emittersIter->is_array())
        return false;

    for (const json& emitterNode : *emittersIter)
    {
        AuthoringEmitter emitter{};
        if (From_Json(emitterNode, emitter))
            document.emitters.push_back(move(emitter));
    }

    outDocument = move(document);
    return true;
}

uint32 EffectAuthoringJsonSerializer::Find_NextAuthoringId(const EffectAuthoringDocument& document)
{
    return Find_NextAuthoringId(document.emitters);
}

json EffectAuthoringJsonSerializer::To_Json(const AuthoringModule& module)
{
    json root = json::object();
    root["id"] = module.id;
    root["type"] = Codec::To_EnumToken(module.type);
    root["enabled"] = module.enabled;

    switch (module.type)
    {
    case AuthoringModuleType::Required:
        root["data"] = Codec::To_Json(get<RequiredModuleData>(module.data));
        break;
    case AuthoringModuleType::Spawn:
        root["data"] = Codec::To_Json(get<SpawnModuleData>(module.data));
        break;
    case AuthoringModuleType::Lifetime:
        root["data"] = Codec::To_Json(get<LifetimeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialLocation:
        root["data"] = Codec::To_Json(get<InitialLocationModuleData>(module.data));
        break;
    case AuthoringModuleType::SphereLocation:
        root["data"] = Codec::To_Json(get<SphereLocationModuleData>(module.data));
        break;
    case AuthoringModuleType::PlaneRadialLocation:
        root["data"] = Codec::To_Json(get<PlaneRadialLocationModuleData>(module.data));
        break;
    case AuthoringModuleType::CylinderLocation:
        root["data"] = Codec::To_Json(get<CylinderLocationModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialSize:
        root["data"] = Codec::To_Json(get<InitialSizeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialMeshSize:
        root["data"] = Codec::To_Json(get<InitialMeshSizeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialVelocity:
        root["data"] = Codec::To_Json(get<InitialVelocityModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialRadialVelocity:
        root["data"] = Codec::To_Json(get<InitialRadialVelocityModuleData>(module.data));
        break;
    case AuthoringModuleType::VelocityCone:
        root["data"] = Codec::To_Json(get<VelocityConeModuleData>(module.data));
        break;
    case AuthoringModuleType::SourceMotionVelocity:
        root["data"] = Codec::To_Json(get<SourceMotionVelocityModuleData>(module.data));
        break;
    case AuthoringModuleType::Acceleration:
        root["data"] = Codec::To_Json(get<AccelerationModuleData>(module.data));
        break;
    case AuthoringModuleType::Drag:
        root["data"] = Codec::To_Json(get<DragModuleData>(module.data));
        break;
    case AuthoringModuleType::VelocityOverLife:
        root["data"] = Codec::To_Json(get<VelocityOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::OrbitOverLife:
        root["data"] = Codec::To_Json(get<OrbitOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialRotation:
        root["data"] = Codec::To_Json(get<InitialRotationModuleData>(module.data));
        break;
    case AuthoringModuleType::SphereRadialOrientation:
        root["data"] = Codec::To_Json(get<SphereRadialOrientationModuleData>(module.data));
        break;
    case AuthoringModuleType::PlaneRadialOrientation:
        root["data"] = Codec::To_Json(get<PlaneRadialOrientationModuleData>(module.data));
        break;
    case AuthoringModuleType::CylinderOrientation:
        root["data"] = Codec::To_Json(get<CylinderOrientationModuleData>(module.data));
        break;
    case AuthoringModuleType::RotationOverLife:
        root["data"] = Codec::To_Json(get<RotationOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::SpriteTilt:
        root["data"] = Codec::To_Json(get<SpriteTiltModuleData>(module.data));
        break;
    case AuthoringModuleType::SpriteTiltOverLife:
        root["data"] = Codec::To_Json(get<SpriteTiltOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialRotationRate:
        root["data"] = Codec::To_Json(get<InitialRotationRateModuleData>(module.data));
        break;
    case AuthoringModuleType::RotationRateScaleByLife:
        root["data"] = Codec::To_Json(get<RotationRateScaleByLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialMeshRotation:
        root["data"] = Codec::To_Json(get<InitialMeshRotationModuleData>(module.data));
        break;
    case AuthoringModuleType::MeshRotationOverLife:
        root["data"] = Codec::To_Json(get<MeshRotationOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::MeshDirectionAlignOverLife:
        root["data"] = Codec::To_Json(get<MeshDirectionAlignOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialMeshRotationRate:
        root["data"] = Codec::To_Json(get<InitialMeshRotationRateModuleData>(module.data));
        break;
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        root["data"] = Codec::To_Json(get<MeshRotationRateScaleByLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::InitialColor:
        root["data"] = Codec::To_Json(get<InitialColorModuleData>(module.data));
        break;
    case AuthoringModuleType::ColorOverLife:
        root["data"] = Codec::To_Json(get<ColorOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::SubUVFrameOverLife:
        root["data"] = Codec::To_Json(get<SubUVFrameOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::SizeByLife:
        root["data"] = Codec::To_Json(get<SizeByLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::BeamEnvelopeOverLife:
        root["data"] = Codec::To_Json(get<BeamEnvelopeOverLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::MeshSizeByLife:
        root["data"] = Codec::To_Json(get<MeshSizeByLifeModuleData>(module.data));
        break;
    case AuthoringModuleType::SpawnPerUnit:
        root["data"] = Codec::To_Json(get<SpawnPerUnitModuleData>(module.data));
        break;
    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        root["data"] = Codec::To_Json(get<SourceHistorySpriteTrailPathFollowModuleData>(module.data));
        break;
    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        root["data"] = Codec::To_Json(get<SourceHistorySpriteTrailPathReplayModuleData>(module.data));
        break;
    case AuthoringModuleType::RibbonOrientation:
        root["data"] = Codec::To_Json(get<RibbonOrientationModuleData>(module.data));
        break;
    case AuthoringModuleType::MaterialScalarModulation:
        root["data"] = Codec::To_Json(get<MaterialScalarModulationModuleData>(module.data));
        break;
    }

    return root;
}

json EffectAuthoringJsonSerializer::To_Json(const AuthoringEmitter& emitter)
{
    json root = json::object();
    root["id"] = emitter.id;
    root["name"] = emitter.name;
    root["rendererType"] = Authoring::Resolve_RendererType(emitter.typeData);
    root["renderLayerOverride"] = Codec::To_EnumToken(emitter.renderLayerOverride);
    root["enabled"] = emitter.enabled;
    if (!Codec::Is_DefaultEmitterHistoryBudget(emitter.historyBudget))
        root["historyBudget"] = Codec::To_Json(emitter.historyBudget);
    if (emitter.typeData.kind != AuthoringTypeDataKind::None)
        root["typeData"] = Codec::To_Json(emitter.typeData);
    root["modules"] = json::array();

    for (const AuthoringModule& module : emitter.modules)
        root["modules"].push_back(To_Json(module));

    return root;
}

bool EffectAuthoringJsonSerializer::From_Json(const json& root, AuthoringModule& outModule)
{
    if (!root.is_object())
        return false;

    AuthoringModuleType moduleType = AuthoringModuleType::Required;
    const auto typeIter = root.find("type");
    if (typeIter == root.end() || !Codec::Try_ReadEnumToken(*typeIter, moduleType))
        return false;

    AuthoringModule module{};
    module.type = moduleType;
    module.data = Make_DefaultModuleDataForRestore(moduleType);
    module.id = root.value("id", module.id);
    module.enabled = root.value("enabled", module.enabled);

    const auto dataIter = root.find("data");
    if (dataIter != root.end())
    {
        switch (module.type)
        {
        case AuthoringModuleType::Required:
            Codec::From_Json(*dataIter, get<RequiredModuleData>(module.data));
            break;
        case AuthoringModuleType::Spawn:
            Codec::From_Json(*dataIter, get<SpawnModuleData>(module.data));
            break;
        case AuthoringModuleType::Lifetime:
            Codec::From_Json(*dataIter, get<LifetimeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialLocation:
            Codec::From_Json(*dataIter, get<InitialLocationModuleData>(module.data));
            break;
        case AuthoringModuleType::SphereLocation:
            Codec::From_Json(*dataIter, get<SphereLocationModuleData>(module.data));
            break;
        case AuthoringModuleType::PlaneRadialLocation:
            Codec::From_Json(*dataIter, get<PlaneRadialLocationModuleData>(module.data));
            break;
        case AuthoringModuleType::CylinderLocation:
            Codec::From_Json(*dataIter, get<CylinderLocationModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialSize:
            Codec::From_Json(*dataIter, get<InitialSizeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialMeshSize:
            Codec::From_Json(*dataIter, get<InitialMeshSizeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialVelocity:
            Codec::From_Json(*dataIter, get<InitialVelocityModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialRadialVelocity:
            Codec::From_Json(*dataIter, get<InitialRadialVelocityModuleData>(module.data));
            break;
        case AuthoringModuleType::VelocityCone:
            Codec::From_Json(*dataIter, get<VelocityConeModuleData>(module.data));
            break;
        case AuthoringModuleType::SourceMotionVelocity:
            Codec::From_Json(*dataIter, get<SourceMotionVelocityModuleData>(module.data));
            break;
        case AuthoringModuleType::Acceleration:
            Codec::From_Json(*dataIter, get<AccelerationModuleData>(module.data));
            break;
        case AuthoringModuleType::Drag:
            Codec::From_Json(*dataIter, get<DragModuleData>(module.data));
            break;
        case AuthoringModuleType::VelocityOverLife:
            Codec::From_Json(*dataIter, get<VelocityOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::OrbitOverLife:
            Codec::From_Json(*dataIter, get<OrbitOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialRotation:
            Codec::From_Json(*dataIter, get<InitialRotationModuleData>(module.data));
            break;
        case AuthoringModuleType::SphereRadialOrientation:
            Codec::From_Json(*dataIter, get<SphereRadialOrientationModuleData>(module.data));
            break;
        case AuthoringModuleType::PlaneRadialOrientation:
            Codec::From_Json(*dataIter, get<PlaneRadialOrientationModuleData>(module.data));
            break;
        case AuthoringModuleType::CylinderOrientation:
            Codec::From_Json(*dataIter, get<CylinderOrientationModuleData>(module.data));
            break;
        case AuthoringModuleType::RotationOverLife:
            Codec::From_Json(*dataIter, get<RotationOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::SpriteTilt:
            Codec::From_Json(*dataIter, get<SpriteTiltModuleData>(module.data));
            break;
        case AuthoringModuleType::SpriteTiltOverLife:
            Codec::From_Json(*dataIter, get<SpriteTiltOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialRotationRate:
            Codec::From_Json(*dataIter, get<InitialRotationRateModuleData>(module.data));
            break;
        case AuthoringModuleType::RotationRateScaleByLife:
            Codec::From_Json(*dataIter, get<RotationRateScaleByLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialMeshRotation:
            Codec::From_Json(*dataIter, get<InitialMeshRotationModuleData>(module.data));
            break;
        case AuthoringModuleType::MeshRotationOverLife:
            Codec::From_Json(*dataIter, get<MeshRotationOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::MeshDirectionAlignOverLife:
            Codec::From_Json(*dataIter, get<MeshDirectionAlignOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialMeshRotationRate:
            Codec::From_Json(*dataIter, get<InitialMeshRotationRateModuleData>(module.data));
            break;
        case AuthoringModuleType::MeshRotationRateScaleByLife:
            Codec::From_Json(*dataIter, get<MeshRotationRateScaleByLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::InitialColor:
            Codec::From_Json(*dataIter, get<InitialColorModuleData>(module.data));
            break;
        case AuthoringModuleType::ColorOverLife:
            Codec::From_Json(*dataIter, get<ColorOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::SubUVFrameOverLife:
            Codec::From_Json(*dataIter, get<SubUVFrameOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::SizeByLife:
            Codec::From_Json(*dataIter, get<SizeByLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::BeamEnvelopeOverLife:
            Codec::From_Json(*dataIter, get<BeamEnvelopeOverLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::MeshSizeByLife:
            Codec::From_Json(*dataIter, get<MeshSizeByLifeModuleData>(module.data));
            break;
        case AuthoringModuleType::SpawnPerUnit:
            Codec::From_Json(*dataIter, get<SpawnPerUnitModuleData>(module.data));
            break;
        case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
            Codec::From_Json(*dataIter, get<SourceHistorySpriteTrailPathFollowModuleData>(module.data));
            break;
        case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
            Codec::From_Json(*dataIter, get<SourceHistorySpriteTrailPathReplayModuleData>(module.data));
            break;
        case AuthoringModuleType::RibbonOrientation:
            Codec::From_Json(*dataIter, get<RibbonOrientationModuleData>(module.data));
            break;
        case AuthoringModuleType::MaterialScalarModulation:
            Codec::From_Json(*dataIter, get<MaterialScalarModulationModuleData>(module.data));
            break;
        }
    }

    outModule = move(module);
    return true;
}

bool EffectAuthoringJsonSerializer::From_Json(const json& root, AuthoringEmitter& outEmitter)
{
    if (!root.is_object())
        return false;

    AuthoringEmitter emitter{};
    emitter.id = root.value("id", 0u);
    emitter.name = root.value("name", string{});
    emitter.rendererType = root.value("rendererType", emitter.rendererType);
    if (const auto renderLayerOverrideIter = root.find("renderLayerOverride"); renderLayerOverrideIter != root.end())
        Codec::Try_ReadEnumToken(*renderLayerOverrideIter, emitter.renderLayerOverride);
    emitter.enabled = root.value("enabled", emitter.enabled);
    if (const auto historyBudgetIter = root.find("historyBudget"); historyBudgetIter != root.end())
        Codec::From_Json(*historyBudgetIter, emitter.historyBudget);
    emitter.previewDirty = true;

    const auto typeDataIter = root.find("typeData");
    if (typeDataIter != root.end())
    {
        Codec::From_Json(*typeDataIter, emitter.typeData);
        emitter.rendererType = Authoring::Resolve_RendererType(emitter.typeData);
    }

    const auto modulesIter = root.find("modules");
    if (modulesIter != root.end() && modulesIter->is_array())
    {
        for (const json& moduleNode : *modulesIter)
        {
            AuthoringModule module{};
            if (From_Json(moduleNode, module))
                emitter.modules.push_back(move(module));
        }
    }

    outEmitter = move(emitter);
    return true;
}

uint32 EffectAuthoringJsonSerializer::Find_NextAuthoringId(const vector<AuthoringEmitter>& emitters)
{
    uint32 maxId = 0;
    for (const AuthoringEmitter& emitter : emitters)
    {
        maxId = max(maxId, emitter.id);
        for (const AuthoringModule& module : emitter.modules)
            maxId = max(maxId, module.id);
    }

    return maxId + 1;
}

AuthoringModuleData EffectAuthoringJsonSerializer::Make_DefaultModuleDataForRestore(AuthoringModuleType type)
{
    switch (type)
    {
    case AuthoringModuleType::Required:
        return RequiredModuleData{};
    case AuthoringModuleType::Spawn:
        return SpawnModuleData{};
    case AuthoringModuleType::Lifetime:
        return LifetimeModuleData{};
    case AuthoringModuleType::InitialLocation:
        return InitialLocationModuleData{};
    case AuthoringModuleType::SphereLocation:
        return SphereLocationModuleData{};
    case AuthoringModuleType::PlaneRadialLocation:
        return PlaneRadialLocationModuleData{};
    case AuthoringModuleType::CylinderLocation:
        return CylinderLocationModuleData{};
    case AuthoringModuleType::InitialSize:
        return InitialSizeModuleData{};
    case AuthoringModuleType::InitialMeshSize:
        return InitialMeshSizeModuleData{};
    case AuthoringModuleType::InitialVelocity:
        return InitialVelocityModuleData{};
    case AuthoringModuleType::InitialRadialVelocity:
        return InitialRadialVelocityModuleData{};
    case AuthoringModuleType::VelocityCone:
        return VelocityConeModuleData{};
    case AuthoringModuleType::SourceMotionVelocity:
        return SourceMotionVelocityModuleData{};
    case AuthoringModuleType::Acceleration:
        return AccelerationModuleData{};
    case AuthoringModuleType::Drag:
        return DragModuleData{};
    case AuthoringModuleType::VelocityOverLife:
        return VelocityOverLifeModuleData{};
    case AuthoringModuleType::OrbitOverLife:
        return OrbitOverLifeModuleData{};
    case AuthoringModuleType::InitialRotation:
        return InitialRotationModuleData{};
    case AuthoringModuleType::SphereRadialOrientation:
        return SphereRadialOrientationModuleData{};
    case AuthoringModuleType::PlaneRadialOrientation:
        return PlaneRadialOrientationModuleData{};
    case AuthoringModuleType::CylinderOrientation:
        return CylinderOrientationModuleData{};
    case AuthoringModuleType::RotationOverLife:
        return RotationOverLifeModuleData{};
    case AuthoringModuleType::SpriteTilt:
        return SpriteTiltModuleData{};
    case AuthoringModuleType::SpriteTiltOverLife:
        return SpriteTiltOverLifeModuleData{};
    case AuthoringModuleType::InitialRotationRate:
        return InitialRotationRateModuleData{};
    case AuthoringModuleType::RotationRateScaleByLife:
        return RotationRateScaleByLifeModuleData{};
    case AuthoringModuleType::InitialMeshRotation:
        return InitialMeshRotationModuleData{};
    case AuthoringModuleType::MeshRotationOverLife:
        return MeshRotationOverLifeModuleData{};
    case AuthoringModuleType::MeshDirectionAlignOverLife:
        return MeshDirectionAlignOverLifeModuleData{};
    case AuthoringModuleType::InitialMeshRotationRate:
        return InitialMeshRotationRateModuleData{};
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        return MeshRotationRateScaleByLifeModuleData{};
    case AuthoringModuleType::InitialColor:
        return InitialColorModuleData{};
    case AuthoringModuleType::ColorOverLife:
        return ColorOverLifeModuleData{};
    case AuthoringModuleType::SubUVFrameOverLife:
        return SubUVFrameOverLifeModuleData{};
    case AuthoringModuleType::SizeByLife:
        return SizeByLifeModuleData{};
    case AuthoringModuleType::BeamEnvelopeOverLife:
        return BeamEnvelopeOverLifeModuleData{};
    case AuthoringModuleType::MeshSizeByLife:
        return MeshSizeByLifeModuleData{};
    case AuthoringModuleType::SpawnPerUnit:
        return SpawnPerUnitModuleData{};
    case AuthoringModuleType::SourceHistorySpriteTrailPathFollow:
        return SourceHistorySpriteTrailPathFollowModuleData{};
    case AuthoringModuleType::SourceHistorySpriteTrailPathReplay:
        return SourceHistorySpriteTrailPathReplayModuleData{};
    case AuthoringModuleType::RibbonOrientation:
        return RibbonOrientationModuleData{};
    case AuthoringModuleType::MaterialScalarModulation:
        return MaterialScalarModulationModuleData{};
    }

    return RequiredModuleData{};
}

NS_END
