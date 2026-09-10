#include "pch.h"
#include "EffectAssetRuntimeLoader_Support.h"

NS_BEGIN(Client)

namespace EffectAssetRuntimeLoad::Json
{
    string To_LogPath(const fs::path& path)
    {
        return String::ToString(path.wstring());
    }

    string Read_String(const json& node, const char* key, const string& fallbackValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return fallbackValue;

        return iter->get<string>();
    }

    bool Read_Bool(const json& node, const char* key, bool fallbackValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_boolean())
            return fallbackValue;

        return iter->get<bool>();
    }

    uint32 Read_UInt(const json& node, const char* key, uint32 fallbackValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number_unsigned())
            return fallbackValue;

        return iter->get<uint32>();
    }

    int32 Read_Int(const json& node, const char* key, int32 fallbackValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number_integer())
            return fallbackValue;

        return iter->get<int32>();
    }

    float Read_Float(const json& node, const char* key, float fallbackValue)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number())
            return fallbackValue;

        return iter->get<float>();
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

    void Read_Vec4(const json& node, Vec4& outValue)
    {
        if (!node.is_array() || node.size() < 4 ||
            !node[0].is_number() || !node[1].is_number() || !node[2].is_number() || !node[3].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
        outValue.z = node[2].get<float>();
        outValue.w = node[3].get<float>();
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

    EffectTextureUVTilingMode Resolve_CompatibleUVTilingMode(const EffectMaterialUVAxisPolicy& policy)
    {
        return policy.uPolicy == policy.vPolicy ? policy.uPolicy : EffectTextureUVTilingMode::Wrap;
    }

    void Read_UVAxisPolicy(
        const json& node,
        const char* key,
        EffectTextureUVTilingMode legacyMode,
        EffectMaterialUVAxisPolicy& outPolicy,
        EffectTextureUVTilingMode& outCompatibleMode)
    {
        outPolicy.uPolicy = legacyMode;
        outPolicy.vPolicy = legacyMode;

        const auto policyIter = node.find(key);
        if (policyIter != node.end() && policyIter->is_object())
        {
            Read_Enum(*policyIter, "uPolicy", outPolicy.uPolicy);
            Read_Enum(*policyIter, "vPolicy", outPolicy.vPolicy);
        }

        outCompatibleMode = Resolve_CompatibleUVTilingMode(outPolicy);
    }

    EffectSortPolicy Read_SortPolicy(const json& node, EffectSortPolicy fallbackValue)
    {
        string token = Read_String(node, "sortPolicy");
        if (!token.empty())
        {
            const optional<EffectSortPolicy> parsed = magic_enum::enum_cast<EffectSortPolicy>(token);
            if (parsed.has_value())
                return parsed.value();
        }

        token = Read_String(node, "sortMode");
        if (token == "ViewProjDepth" || token == "DistanceToView")
            return EffectSortPolicy::EmitterDepth;
        if (token == "None")
            return EffectSortPolicy::None;

        return fallbackValue;
    }

    void Apply_SortPayload(const json& data, EffectRequiredSortRuntimeDesc& sortDesc)
    {
        sortDesc.sortPolicy = Read_SortPolicy(data, sortDesc.sortPolicy);
        sortDesc.sortLayer = Read_Int(data, "sortLayer", sortDesc.sortLayer);
        sortDesc.artistSortBias = Read_Float(data, "sortBias", sortDesc.artistSortBias);
    }

    PointParticleRandomSeedRuntimeDesc Read_RandomSeedDesc(const json& moduleData)
    {
        const auto randomSeedIter = moduleData.find("randomSeed");
        if (randomSeedIter == moduleData.end() || !randomSeedIter->is_object())
            return {};

        const string mode = Read_String(*randomSeedIter, "mode");
        uint32 manualSeed = 0u;
        if (const auto seedIter = randomSeedIter->find("manualSeed"); seedIter != randomSeedIter->end())
        {
            if (seedIter->is_number_unsigned())
                manualSeed = seedIter->get<uint32>();
            else if (seedIter->is_number_integer())
                manualSeed = static_cast<uint32>(max<int64_t>(0, seedIter->get<int64_t>()));
        }

        return PointParticleRandomSeedRuntimeDesc{
            .manualSeedEnabled = mode == "Manual",
            .seed = mode == "Manual" ? manualSeed : 0u,
            .useInstanceSeed = Read_Bool(*randomSeedIter, "useInstanceSeed", false)
        };
    }

    PointParticleRandomSeedRuntimeDesc Read_DistributionRandomSeedDesc(const json& distribution, const json* legacyModuleData)
    {
        if (distribution.is_object())
        {
            const auto payloadIter = distribution.find("payload");
            if (payloadIter != distribution.end() && payloadIter->is_object())
            {
                const auto randomSeedIter = payloadIter->find("randomSeed");
                if (randomSeedIter != payloadIter->end() && randomSeedIter->is_object())
                    return Read_RandomSeedDesc(*payloadIter);
            }
        }

        return legacyModuleData != nullptr ? Read_RandomSeedDesc(*legacyModuleData) : PointParticleRandomSeedRuntimeDesc{};
    }
}

NS_END
