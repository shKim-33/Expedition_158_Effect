#include "BattleEffectPrewarmCollector.h"

#include "ClientInstance.h"
#include "GameInstance.h"
#include "GradientSkill.h"
#include "Helper_String.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "SkillScript.h"
#include "SkillTreeCom.h"
#include "Spawn_Prefab.h"

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

NS_BEGIN(Client)

namespace
{
    constexpr wchar_t kDefaultLayerTag[] = L"Layer_Effect";

    bool Is_EffectNotifyType(const string& typeName)
    {
        return typeName == "AN_PlayEffect" ||
            typeName == "AN_PlayEffectBattleDir" ||
            typeName == "ANS_PlayEffect" ||
            typeName == "ANS_Trail" ||
            typeName == "ANS_SourceHistoryEffect" ||
            typeName == "AN_SpawnLineGroundEffect";
    }

    void Add_Request(vector<EffectPrewarmRequest>& outRequests, const wstring& effectName, const wstring& layerTag, uint32 count)
    {
        if (effectName.empty() || count == 0)
            return;

        const wstring resolvedLayerTag = layerTag.empty() ? kDefaultLayerTag : layerTag;

        auto iter = find_if(
            outRequests.begin(),
            outRequests.end(),
            [&](const EffectPrewarmRequest& request)
            {
                return request.effectName == effectName && request.layerTag == resolvedLayerTag;
            });

        if (iter != outRequests.end())
        {
            iter->targetCount += count;
            return;
        }

        EffectPrewarmRequest request{};
        request.effectName = effectName;
        request.layerTag = resolvedLayerTag;
        request.targetCount = count;
        outRequests.push_back(request);
    }

    void Merge_RequestMax(vector<EffectPrewarmRequest>& requests)
    {
        vector<EffectPrewarmRequest> mergedRequests{};

        for (const EffectPrewarmRequest& request : requests)
        {
            if (request.effectName.empty() || request.targetCount == 0)
                continue;

            const wstring resolvedLayerTag = request.layerTag.empty() ? kDefaultLayerTag : request.layerTag;

            auto iter = find_if(
                mergedRequests.begin(),
                mergedRequests.end(),
                [&](const EffectPrewarmRequest& merged)
                {
                    return merged.effectName == request.effectName && merged.layerTag == resolvedLayerTag;
                });

            if (iter != mergedRequests.end())
            {
                iter->targetCount = max(iter->targetCount, request.targetCount);
                continue;
            }

            EffectPrewarmRequest merged{};
            merged.effectName = request.effectName;
            merged.layerTag = resolvedLayerTag;
            merged.targetCount = max(1u, request.targetCount);
            mergedRequests.push_back(merged);
        }

        requests = std::move(mergedRequests);
    }

    const map<string, vector<fs::path>>& Get_NotifyFileCache()
    {
        static map<string, vector<fs::path>> notifyFileCache{};
        static bool isInitialized = false;

        if (isInitialized)
            return notifyFileCache;

        isInitialized = true;

        const fs::path notifyRoot = fs::path(GAME->Get_AssetRoot()) / L"Data" / L"json" / L"AnimNotifies";
        if (!fs::exists(notifyRoot))
            return notifyFileCache;

        try
        {
            for (const fs::directory_entry& entry : fs::recursive_directory_iterator(notifyRoot))
            {
                if (!entry.is_regular_file())
                    continue;

                const fs::path path = entry.path();
                if (path.extension() != L".json")
                    continue;

                const wstring fileName = path.filename().wstring();
                constexpr wchar_t notifySuffix[] = L".notify.json";
                if (fileName.size() <= wcslen(notifySuffix))
                    continue;
                if (!fileName.ends_with(notifySuffix))
                    continue;

                const wstring animationKey = fileName.substr(0, fileName.size() - wcslen(notifySuffix));
                notifyFileCache[String::ToString(animationKey)].push_back(path);
            }
        }
        catch (const fs::filesystem_error&)
        {
            notifyFileCache.clear();
        }

        return notifyFileCache;
    }

    void Collect_FromNotifyEntry(const json& notifyEntry, vector<EffectPrewarmRequest>& outRequests)
    {
        const string typeName = notifyEntry.value("type_name", string{});
        if (!Is_EffectNotifyType(typeName))
            return;

        if (!notifyEntry.contains("payload") || !notifyEntry["payload"].is_object())
            return;

        const json& payload = notifyEntry["payload"];
        const string effectName = payload.value("_effectName", string{});
        if (effectName.empty())
            return;

        const string layerTag = payload.value("_layerTag", string{});
        Add_Request(
            outRequests,
            String::ToWString(effectName),
            layerTag.empty() ? kDefaultLayerTag : String::ToWString(layerTag),
            1);
    }

    void Collect_FromNotifyArray(const json& animationJson, const char* arrayKey, vector<EffectPrewarmRequest>& outRequests)
    {
        if (!animationJson.contains(arrayKey) || !animationJson[arrayKey].is_array())
            return;

        for (const json& notifyEntry : animationJson[arrayKey])
            Collect_FromNotifyEntry(notifyEntry, outRequests);
    }

    void Collect_FromNotifyFile(const fs::path& notifyPath, vector<EffectPrewarmRequest>& outRequests)
    {
        ifstream file(notifyPath);
        if (!file.is_open())
            return;

        json root{};
        try
        {
            file >> root;
        }
        catch (const json::exception&)
        {
            return;
        }

        if (!root.contains("animations") || !root["animations"].is_array())
            return;

        for (const json& animationJson : root["animations"])
        {
            Collect_FromNotifyArray(animationJson, "notifies", outRequests);
            Collect_FromNotifyArray(animationJson, "notify_states", outRequests);
        }
    }

    void Collect_FromAnimationKey(const string& animationKey, vector<EffectPrewarmRequest>& outRequests)
    {
        if (animationKey.empty())
            return;

        const map<string, vector<fs::path>>& notifyFileCache = Get_NotifyFileCache();
        const auto iter = notifyFileCache.find(animationKey);
        if (iter == notifyFileCache.end())
            return;

        for (const fs::path& notifyPath : iter->second)
            Collect_FromNotifyFile(notifyPath, outRequests);
    }

    void Collect_FromSkill(const Skill_Data& skill, const BattleEffectPrewarmContext& context, vector<EffectPrewarmRequest>& outRequests)
    {
        for (const BattleActionPhase& phase : skill.AnimPhases)
            Collect_FromAnimationKey(phase.animationKey, outRequests);

        if (skill.scriptClassName.empty())
            return;

        const Shared<SkillScript> script = SkillScript::Create_SkillScriptByClass(skill.scriptClassName);
        if (script != nullptr)
            script->Collect_PrewarmEffects(skill, context, outRequests);
    }

    vector<string> Resolve_PlayerSkillKeys(HeroId heroId)
    {
        const Shared<Player_WorldBase> worldHero = CLIENT->Find_Hero(heroId);
        const Shared<PlayerStatCom> worldStat = worldHero ? worldHero->Get_Component<PlayerStatCom>() : nullptr;
        const Shared<SkillTreeCom> worldSkillTreeCom = worldHero ? worldHero->Get_Component<SkillTreeCom>() : nullptr;

        vector<string> skillKeys = worldStat ? worldStat->Get_SkillKeys() : vector<string>{};
        if (skillKeys.empty() && worldSkillTreeCom)
            skillKeys = worldSkillTreeCom->Get_UnlockedSkillIds();

        const char* gradientSkillKey = GradientSkill::Resolve_SkillKey(heroId);
        if (gradientSkillKey[0] != '\0' &&
            CLIENT->Get_Skill(gradientSkillKey) != nullptr &&
            find(skillKeys.begin(), skillKeys.end(), gradientSkillKey) == skillKeys.end())
        {
            skillKeys.push_back(gradientSkillKey);
        }

        return skillKeys;
    }

    vector<string> Load_MonsterPrefabSkillKeys(const string& prefabName)
    {
        vector<string> skillKeys{};
        if (prefabName.empty())
            return skillKeys;

        const fs::path prefabPath = Spawn_Prefab::Resolve_PrefabPath(prefabName);
        if (!fs::exists(prefabPath))
            return skillKeys;

        ifstream file(prefabPath);
        if (!file.is_open())
            return skillKeys;

        json root{};
        try
        {
            file >> root;
        }
        catch (const json::exception&)
        {
            return skillKeys;
        }

        if (!root.contains("components") || !root["components"].is_array())
            return skillKeys;

        for (const json& componentJson : root["components"])
        {
            if (!componentJson.is_object())
                continue;

            const string className = componentJson.value("class_name", string{});
            if (className != "MonsterStatCom" && className != "CombatStatCom")
                continue;

            if (!componentJson.contains("_skillKeys") || !componentJson["_skillKeys"].is_array())
                continue;

            for (const json& skillKeyJson : componentJson["_skillKeys"])
            {
                if (skillKeyJson.is_string())
                    skillKeys.push_back(skillKeyJson.get<string>());
            }

            return skillKeys;
        }

        return skillKeys;
    }

    void Collect_FromSkillKeys(const vector<string>& skillKeys, const BattleEffectPrewarmContext& context, vector<EffectPrewarmRequest>& outRequests)
    {
        for (const string& skillKey : skillKeys)
        {
            if (skillKey.empty())
                continue;

            const Skill_Data* skill = CLIENT->Get_Skill(skillKey);
            if (skill == nullptr)
                continue;

            Collect_FromSkill(*skill, context, outRequests);
        }
    }

}

vector<EffectPrewarmRequest> BattleEffectPrewarmCollector::Collect_ForBattleSetup(const vector<HeroId>& heroIds, const vector<BattleMonsterSpawnDesc>& monsterSpawnDescs)
{
    BattleEffectPrewarmContext context{};
    context.currentHeroCount = static_cast<uint32>(heroIds.size());
    context.currentEnemyCount = static_cast<uint32>(monsterSpawnDescs.size());

    vector<EffectPrewarmRequest> requests{};

    for (HeroId heroId : heroIds)
        Collect_FromSkillKeys(Resolve_PlayerSkillKeys(heroId), context, requests);

    for (const BattleMonsterSpawnDesc& spawnDesc : monsterSpawnDescs)
        Collect_FromSkillKeys(Load_MonsterPrefabSkillKeys(spawnDesc.prefabName), context, requests);

    Merge_RequestMax(requests);
    return requests;
}

NS_END
