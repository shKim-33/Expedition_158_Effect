#include "pch.h"
#include "EffectAssetRuntimeLoader.h"

#include "EffectAssetRuntimeLoader_Support.h"
#include "GameInstance.h"

NS_BEGIN(Client)

using namespace EffectAssetRuntimeLoad::Assets;
using namespace EffectAssetRuntimeLoad::Emitters;
using namespace EffectAssetRuntimeLoad::Json;

namespace
{
    EffectHistoryBudgetRuntimeDesc Read_HistoryBudget(const json& root)
    {
        EffectHistoryBudgetRuntimeDesc budget{};
        const auto budgetIter = root.find("historyBudget");
        if (budgetIter == root.end() || !budgetIter->is_object())
            return budget;

        const json& data = *budgetIter;
        Read_Enum(data, "preset", budget.preset);
        budget.preserveLength = true;
        budget.sharedSourceHistory = true;
        budget.trailDensityScale = max(0.f, Read_Float(data, "trailDensityScale", budget.trailDensityScale));
        budget.spriteStampDensityScale = 1.f;
        budget.ribbonDensityScale = max(0.f, Read_Float(data, "ribbonDensityScale", budget.ribbonDensityScale));
        budget.distortionDensityScale = max(0.f, Read_Float(data, "distortionDensityScale", budget.distortionDensityScale));
        Read_Enum(data, "updateRate", budget.updateRate);
        return budget;
    }

    vector<EffectHistorySourceGroupBudgetDesc> Read_HistorySourceGroups(const json& root)
    {
        vector<EffectHistorySourceGroupBudgetDesc> groups{};
        const auto groupsIter = root.find("historySourceGroups");
        if (groupsIter == root.end() || !groupsIter->is_array())
            return groups;

        for (const json& node : *groupsIter)
        {
            if (!node.is_object())
                continue;

            EffectHistorySourceGroupBudgetDesc group{};
            Read_Enum(node, "kind", group.kind);
            group.stableId = Read_String(node, "stableId", group.stableId);
            group.confirmed = Read_Bool(node, "confirmed", group.confirmed);
            group.sourceSampleCount = Read_UInt(node, "sourceSampleCount", group.sourceSampleCount);
            group.sampleLifetime = max(0.f, Read_Float(node, "sampleLifetime", group.sampleLifetime));
            group.sampleSpacing = max(0.f, Read_Float(node, "sampleSpacing", group.sampleSpacing));
            group.sampleInterval = max(0.f, Read_Float(node, "sampleInterval", group.sampleInterval));
            group.curveSubdivision = Read_UInt(node, "curveSubdivision", group.curveSubdivision);
            group.smoothTangent = Read_Bool(node, "smoothTangent", group.smoothTangent);
            if (group.kind != EffectHistorySourceGroupKind::None && !group.stableId.empty())
                groups.push_back(group);
        }

        return groups;
    }

    uint32 Estimate_SourceHistorySpriteTrailSourceSamples(const ComputeSourceHistorySpriteTrailEmitterDesc& desc)
    {
        return max(2u, static_cast<uint32>(ceilf(desc.sampleLifetime / max(0.001f, desc.sampleSpacing))) + 2u);
    }

    void Assign_HistoryBudgetEffective(EffectEmitterDefinition& emitter, const EffectHistoryBudgetEffectiveInput& input)
    {
        emitter.effectiveHistoryBudget.family = input.family;
        emitter.effectiveHistoryBudget.sourceSampleCount = input.sourceSampleCount;
        emitter.effectiveHistoryBudget.renderSegmentCount = input.renderSegmentCount;
        emitter.effectiveHistoryBudget.spriteStampCount = input.spriteStampCount;
    }

    uint32 Resolve_HistoryBudgetCurveSubdivision(
        const EffectHistoryBudgetRuntimeDesc& budget,
        const EffectEmitterHistoryBudgetRuntimeDesc& usage,
        EffectHistoryBudgetFamily family,
        uint32 sourceCount,
        uint32 authoredSubdivision)
    {
        if (budget.preset == EffectHistoryBudgetPreset::Full || sourceCount == 0u)
            return authoredSubdivision;

        const uint32 effectiveAuthoredSubdivision = max(1u, authoredSubdivision);
        const EffectHistoryBudgetEffectiveDesc target = Compute_EffectHistoryBudgetEffective(
            budget,
            usage,
            EffectHistoryBudgetEffectiveInput{
                family,
                sourceCount,
                sourceCount * effectiveAuthoredSubdivision,
                0u
            }
        );
        const uint32 desiredSubdivision = max(1u, (target.renderSegmentCount + sourceCount - 1u) / sourceCount);
        const uint32 cappedSubdivision = min(effectiveAuthoredSubdivision, desiredSubdivision);
        return authoredSubdivision == 0u ? 0u : cappedSubdivision;
    }

    void Apply_SafeHistoryBudgetDensityCap(EffectDefinition& definition, EffectEmitterDefinition& emitter)
    {
        if (auto* trailDesc = get_if<ComputeTrailEmitterDesc>(&emitter.concreteDesc))
        {
            trailDesc->curveSubdivision = Resolve_HistoryBudgetCurveSubdivision(
                definition.historyBudget,
                emitter.historyBudget,
                EffectHistoryBudgetFamily::Trail,
                trailDesc->historyCount,
                trailDesc->curveSubdivision
            );
            return;
        }

        if (auto* ribbonDesc = get_if<ComputeRibbonEmitterDesc>(&emitter.concreteDesc))
        {
            ribbonDesc->curveSubdivision = Resolve_HistoryBudgetCurveSubdivision(
                definition.historyBudget,
                emitter.historyBudget,
                EffectHistoryBudgetFamily::Ribbon,
                ribbonDesc->maxSampleCount,
                ribbonDesc->curveSubdivision
            );
            return;
        }

        if (auto* spriteTrailDesc = get_if<ComputeSourceHistorySpriteTrailEmitterDesc>(&emitter.concreteDesc))
        {
            const uint32 estimatedSourceSampleCount = Estimate_SourceHistorySpriteTrailSourceSamples(*spriteTrailDesc);
            spriteTrailDesc->curveSubdivision = Resolve_HistoryBudgetCurveSubdivision(
                definition.historyBudget,
                emitter.historyBudget,
                EffectHistoryBudgetFamily::SourceHistorySpriteTrail,
                estimatedSourceSampleCount,
                spriteTrailDesc->curveSubdivision
            );
        }
    }

    void Refresh_HistoryBudgetEffective(EffectEmitterDefinition& emitter)
    {
        if (const auto* trailDesc = get_if<ComputeTrailEmitterDesc>(&emitter.concreteDesc))
        {
            Assign_HistoryBudgetEffective(
                emitter,
                EffectHistoryBudgetEffectiveInput{
                    EffectHistoryBudgetFamily::Trail,
                    trailDesc->historyCount,
                    trailDesc->historyCount * max(1u, trailDesc->curveSubdivision),
                    0u
                }
            );
            return;
        }

        if (const auto* ribbonDesc = get_if<ComputeRibbonEmitterDesc>(&emitter.concreteDesc))
        {
            Assign_HistoryBudgetEffective(
                emitter,
                EffectHistoryBudgetEffectiveInput{
                    EffectHistoryBudgetFamily::Ribbon,
                    ribbonDesc->maxSampleCount,
                    ribbonDesc->maxSampleCount * max(1u, ribbonDesc->curveSubdivision),
                    0u
                }
            );
            return;
        }

        if (const auto* spriteTrailDesc = get_if<ComputeSourceHistorySpriteTrailEmitterDesc>(&emitter.concreteDesc))
        {
            const uint32 estimatedSourceSampleCount = Estimate_SourceHistorySpriteTrailSourceSamples(*spriteTrailDesc);
            Assign_HistoryBudgetEffective(
                emitter,
                EffectHistoryBudgetEffectiveInput{
                    EffectHistoryBudgetFamily::SourceHistorySpriteTrail,
                    estimatedSourceSampleCount,
                    estimatedSourceSampleCount * max(1u, spriteTrailDesc->curveSubdivision),
                    spriteTrailDesc->maxStampCount
                }
            );
        }
    }

    const char* Resolve_TrailSourceGroupId()
    {
        return "TrailProvider";
    }

    string Resolve_SourcePointSourceGroupId(EffectSourceHistoryRibbonSourceMode sourceMode, uint32 sourceEmitterId)
    {
        if (sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
            return format("ParticleEmitter:{}", sourceEmitterId);

        return "SelfRoot";
    }

    void Apply_SourceGroupBudget(EffectDefinition& definition, EffectEmitterDefinition& emitter)
    {
        if (auto* trailDesc = get_if<ComputeTrailEmitterDesc>(&emitter.concreteDesc))
        {
            const EffectHistorySourceGroupBudgetDesc* group = Find_EffectHistorySourceGroupBudget(
                definition.historySourceGroups,
                EffectHistorySourceGroupKind::TrailPairHistory,
                Resolve_TrailSourceGroupId()
            );
            if (group == nullptr)
                return;

            if (group->sourceSampleCount > 0u)
                trailDesc->historyCount = max(2u, group->sourceSampleCount);
            if (group->sampleSpacing > 0.f)
                trailDesc->sampleSpacing = max(0.001f, group->sampleSpacing);
            if (group->curveSubdivision > 0u)
                trailDesc->curveSubdivision = group->curveSubdivision;
            trailDesc->smoothTangent = group->smoothTangent;
            return;
        }

        if (auto* ribbonDesc = get_if<ComputeRibbonEmitterDesc>(&emitter.concreteDesc))
        {
            const string sourceGroupId = Resolve_SourcePointSourceGroupId(ribbonDesc->sourceMode, ribbonDesc->sourceEmitterId);
            const EffectHistorySourceGroupBudgetDesc* group = Find_EffectHistorySourceGroupBudget(
                definition.historySourceGroups,
                EffectHistorySourceGroupKind::SourcePointHistory,
                sourceGroupId
            );
            if (group == nullptr)
                return;

            if (group->sourceSampleCount > 0u)
                ribbonDesc->maxSampleCount = max(2u, group->sourceSampleCount);
            if (group->sampleLifetime > 0.f)
                ribbonDesc->sampleLifetime = max(0.0001f, group->sampleLifetime);
            if (group->sampleSpacing > 0.f)
                ribbonDesc->sampleSpacing = max(0.001f, group->sampleSpacing);
            ribbonDesc->sampleInterval = max(0.f, group->sampleInterval);
            if (group->curveSubdivision > 0u)
                ribbonDesc->curveSubdivision = group->curveSubdivision;
            ribbonDesc->smoothTangent = group->smoothTangent;
            return;
        }

        if (auto* spriteTrailDesc = get_if<ComputeSourceHistorySpriteTrailEmitterDesc>(&emitter.concreteDesc))
        {
            const string sourceGroupId = Resolve_SourcePointSourceGroupId(spriteTrailDesc->sourceMode, spriteTrailDesc->sourceEmitterId);
            const EffectHistorySourceGroupBudgetDesc* group = Find_EffectHistorySourceGroupBudget(
                definition.historySourceGroups,
                EffectHistorySourceGroupKind::SourcePointHistory,
                sourceGroupId
            );
            if (group == nullptr)
                return;

            if (group->sampleLifetime > 0.f)
                spriteTrailDesc->sampleLifetime = max(0.0001f, group->sampleLifetime);
            if (group->sampleSpacing > 0.f)
                spriteTrailDesc->sampleSpacing = max(0.001f, group->sampleSpacing);
            if (group->curveSubdivision > 0u)
                spriteTrailDesc->curveSubdivision = group->curveSubdivision;
            spriteTrailDesc->smoothTangent = group->smoothTangent;
        }
    }
}

Shared<const EffectDefinition> EffectAssetRuntimeLoader::Load_Definition(const wstring& effectAssetPath)
{
    json root{};
    if (!Load_JsonRoot(effectAssetPath, root))
        return nullptr;

    if (!root.is_object())
    {
        LOG_ERROR("Root is not object. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    const auto emittersIter = root.find("emitters");
    if (emittersIter == root.end() || !emittersIter->is_array())
    {
        LOG_ERROR("Emitters array is missing. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    auto definition = make_shared<EffectDefinition>();
    definition->name = Read_String(root, "name");
    if (definition->name.empty())
        definition->name = String::ToString(fs::path(effectAssetPath).stem().wstring());
    definition->historyBudget = Read_HistoryBudget(root);
    definition->historySourceGroups = Read_HistorySourceGroups(root);

    for (const json& emitter : *emittersIter)
    {
        if (!emitter.is_object())
        {
            LOG_WARN("Skipped invalid emitter node. effect='{}'", definition->name);
            continue;
        }

        if (!Read_Bool(emitter, "enabled", true))
            continue;

        const string rendererType = Resolve_RendererType(emitter);
        const string rendererTypeLower = String::ToLowerCopy(rendererType);

        EffectEmitterDefinition emitterDefinition{};
        bool converted = false;

        if (rendererTypeLower == "sprite")
            converted = Build_EmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "trail")
            converted = Build_TrailEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "sourcehistoryribbon" || rendererTypeLower == "ribbon")
            converted = Build_SourceHistoryRibbonEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "sourcehistoryspritetrail")
            converted = Build_SourceHistorySpriteTrailEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "beam")
            converted = Build_BeamEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "mesh")
            converted = Build_MeshEmitterDefinition(emitter, emitterDefinition);
        else
        {
            LOG_INFO(
                "Skipped unsupported renderer. effect='{}', emitter='{}', rendererType='{}'",
                definition->name,
                Read_String(emitter, "name"),
                rendererType
            );
            continue;
        }

        if (!converted)
        {
            LOG_WARN(
                "EffectAssetRuntimeLoader skipped emitter conversion failure. effect='{}', emitter='{}'",
                definition->name,
                Read_String(emitter, "name")
            );
            continue;
        }

        Apply_SourceGroupBudget(*definition, emitterDefinition);
        Apply_SafeHistoryBudgetDensityCap(*definition, emitterDefinition);
        Refresh_HistoryBudgetEffective(emitterDefinition);
        definition->emitters.push_back(emitterDefinition);
    }

    if (definition->emitters.empty())
    {
        LOG_ERROR("No runtime emitter converted. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    return definition;
}

Shared<const EffectDefinition> EffectAssetRuntimeLoader::Load_DefinitionByName(const wstring& effectName)
{
    string effectAssetGuid{};
    if (!Find_EffectAssetGuidByName(effectName, effectAssetGuid))
        return nullptr;

    wstring resolvedEffectAssetPath{};
    if (!Resolve_EffectAssetPathByGuid(effectAssetGuid, resolvedEffectAssetPath))
        return nullptr;

    return Load_Definition(resolvedEffectAssetPath);
}

namespace EffectAssetRuntimeLoad::Assets
{
    bool Try_MakeResourceRelativePath(
        const fs::path& fullPath,
        const char* logContext,
        const string& assetKey,
        wstring& outRelativePath)
    {
        outRelativePath.clear();

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
        {
            LOG_ERROR(
                "EffectAssetRuntimeLoader {} failed: asset root is empty. key='{}'",
                logContext,
                assetKey
            );
            return false;
        }

        const fs::path assetRootPath = fs::path(assetRoot).lexically_normal();
        const fs::path normalizedFullPath = fullPath.lexically_normal();

        error_code errorCode{};
        const fs::path relativePath = fs::relative(normalizedFullPath, assetRootPath, errorCode).lexically_normal();
        if (errorCode || relativePath.empty() || relativePath.native().starts_with(L".."))
        {
            LOG_ERROR(
                "EffectAssetRuntimeLoader {} failed: resolved path is outside asset root. key='{}', path='{}', root='{}'",
                logContext,
                assetKey,
                To_LogPath(normalizedFullPath),
                To_LogPath(assetRootPath)
            );
            return false;
        }

        outRelativePath = relativePath.wstring();
        return true;
    }

    bool Resolve_EffectAssetPathByGuid(const string& effectAssetGuid, wstring& outEffectAssetPath)
    {
        outEffectAssetPath.clear();

        if (effectAssetGuid.empty())
        {
            LOG_ERROR("Empty guid");
            return false;
        }

        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(effectAssetGuid);
        if (nullptr == assetMeta)
        {
            LOG_WARN("Asset not found. guid='{}'", effectAssetGuid);
            return false;
        }

        if (assetMeta->type != "Effect")
        {
            LOG_ERROR("Asset type mismatch. guid='{}', type='{}'", effectAssetGuid, assetMeta->type);
            return false;
        }

        const wstring resolvedPath = GAME->Resolve_AssetPath(effectAssetGuid);
        if (resolvedPath.empty())
        {
            LOG_WARN("Resolved path is empty. guid='{}'", effectAssetGuid);
            return false;
        }

        return Try_MakeResourceRelativePath(fs::path(resolvedPath), "guid resolve", effectAssetGuid, outEffectAssetPath);
    }

    string Normalize_EffectNameKey(const wstring& effectName)
    {
        const string effectJsonSuffix = ".effect.json";
        string key = String::ToLowerCopy(String::ToString(fs::path(effectName).filename().wstring()));
        if (key.ends_with(effectJsonSuffix))
            key.erase(key.size() - effectJsonSuffix.size());
        else
            key = fs::path(key).stem().generic_string();

        return key;
    }

    bool Is_EffectDefinitionAssetPath(const wstring& relativePath)
    {
        const string lowerPath = String::ToLowerCopy(fs::path(relativePath).generic_string());
        return lowerPath.starts_with("effects/assets/") && lowerPath.ends_with(".effect.json");
    }

    bool Find_EffectAssetGuidByName(const wstring& effectName, string& outEffectAssetGuid)
    {
        outEffectAssetGuid.clear();

        const string effectNameKey = Normalize_EffectNameKey(effectName);
        if (effectNameKey.empty())
        {
            LOG_ERROR("Empty effect name");
            return false;
        }

        AssetMeta matchedAsset{};
        bool hasMatch = false;
        const vector<AssetMeta> effectAssets = GAME->Get_AssetsByType("Effect");
        for (const AssetMeta& assetMeta : effectAssets)
        {
            if (!Is_EffectDefinitionAssetPath(assetMeta.relativePath))
                continue;

            const string candidateKey = Normalize_EffectNameKey(fs::path(assetMeta.relativePath).filename().wstring());
            if (candidateKey != effectNameKey)
                continue;

            if (hasMatch)
            {
                LOG_ERROR(
                    "Duplicate effect asset name. name='{}', first='{}', duplicate='{}'",
                    String::ToString(effectName),
                    To_LogPath(matchedAsset.relativePath),
                    To_LogPath(assetMeta.relativePath)
                );
                return false;
            }

            matchedAsset = assetMeta;
            hasMatch = true;
        }

        if (!hasMatch)
        {
            LOG_WARN("Effect asset name not found. name='{}'", String::ToString(effectName));
            return false;
        }

        if (matchedAsset.guid.empty())
        {
            LOG_ERROR(
                "Effect asset guid is empty. name='{}', path='{}'",
                String::ToString(effectName),
                To_LogPath(matchedAsset.relativePath)
            );
            return false;
        }

        outEffectAssetGuid = matchedAsset.guid;
        return true;
    }

    bool Load_JsonRoot(const wstring& effectAssetPath, json& outRoot)
    {
        if (effectAssetPath.empty())
        {
            LOG_ERROR("Empty asset path");
            return false;
        }

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
        {
            LOG_ERROR("Asset root is empty");
            return false;
        }

        const fs::path fullPath = (fs::path(assetRoot) / effectAssetPath).lexically_normal();
        if (!fs::exists(fullPath))
        {
            LOG_WARN("File not found. path='{}'", To_LogPath(fullPath));
            return false;
        }

        try
        {
            ifstream file(fullPath);
            if (!file.is_open())
            {
                LOG_ERROR("Open failed. path='{}'", To_LogPath(fullPath));
                return false;
            }

            file >> outRoot;
            return true;
        }
        catch (const exception& exception)
        {
            LOG_ERROR(
                "Json parse failed. path='{}', error='{}'",
                To_LogPath(fullPath),
                exception.what()
            );
            return false;
        }
    }
}

NS_END
