#include "TrailAnchor_Asset.h"
#include "GameInstance.h"

namespace
{
json Vec3_ToJson(const Vec3& value)
{
    return json::array({ value.x, value.y, value.z });
}

bool Json_ToVec3(const json& value, Vec3& outVec3)
{
    if (!value.is_array() || value.size() != 3)
        return false;

    outVec3.x = value[0].get<float>();
    outVec3.y = value[1].get<float>();
    outVec3.z = value[2].get<float>();
    return true;
}
}

NS_BEGIN(Client)

fs::path TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(const string& modelGuid)
{
    // notify와 비슷하게 model guid 단위로 저장해서 무기 모델별 anchor를 독립 관리한다.
    return fs::path(GAME->Get_AssetRoot()) /
           L"Data" / L"json" / L"TrailAnchors" /
           String::ToWString(modelGuid + ".trailanchors.json");
}

bool TrailAnchor_Parser::Load_FromFile(const wstring& filePath, TrailAnchorAsset& outAsset)
{
    outAsset = {};

    if (!fs::exists(filePath))
        return false;

    std::ifstream input(filePath);
    if (!input.is_open())
        return false;

    json root{};
    input >> root;

    outAsset.version = root.value("version", 1);
    outAsset.modelGuid = root.value("model_guid", "");

    if (root.contains("base") && root["base"].is_object())
    {
        const json& baseJson = root["base"];
        outAsset.base.boneName = baseJson.value("bone_name", "");
        if (baseJson.contains("local_position"))
            (void)Json_ToVec3(baseJson["local_position"], outAsset.base.localPosition);
    }

    if (root.contains("tip") && root["tip"].is_object())
    {
        const json& tipJson = root["tip"];
        outAsset.tip.boneName = tipJson.value("bone_name", "");
        if (tipJson.contains("local_position"))
            (void)Json_ToVec3(tipJson["local_position"], outAsset.tip.localPosition);
    }

    return true;
}

bool TrailAnchor_Parser::Save_ToFile(const wstring& filePath, const TrailAnchorAsset& asset)
{
    fs::create_directories(fs::path(filePath).parent_path());

    json root{};
    root["version"] = asset.version;
    root["model_guid"] = asset.modelGuid;
    root["base"] = {
        { "bone_name", asset.base.boneName },
        { "local_position", Vec3_ToJson(asset.base.localPosition) }
    };
    root["tip"] = {
        { "bone_name", asset.tip.boneName },
        { "local_position", Vec3_ToJson(asset.tip.localPosition) }
    };

    std::ofstream output(filePath);
    if (!output.is_open())
        return false;

    output << root.dump(4);
    return true;
}

NS_END
