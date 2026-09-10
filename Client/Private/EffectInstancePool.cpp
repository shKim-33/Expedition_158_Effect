#include "EffectInstancePool.h"

#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include <algorithm>

NS_BEGIN(Client)

namespace
{
    wstring Make_EffectInstanceDisplayName(
        const EffectInstancePool::AcquireDesc& desc,
        const Shared<const EffectDefinition>& definition)
    {
        if (definition && !definition->name.empty())
            return String::ToWString(definition->name);

        wstring displayName = desc.effectName;
        const size_t slashPos = displayName.find_last_of(L"\\/");
        if (slashPos != wstring::npos)
            displayName = displayName.substr(slashPos + 1);

        constexpr wstring_view effectSuffix = L".effect.json";
        if (displayName.ends_with(effectSuffix))
            displayName.erase(displayName.size() - effectSuffix.size());
        else if (constexpr wstring_view shortEffectSuffix = L".effect"; displayName.ends_with(shortEffectSuffix))
            displayName.erase(displayName.size() - shortEffectSuffix.size());

        return displayName.empty() ? L"EffectInstance" : displayName;
    }
}

HRESULT EffectInstancePool::Acquire(const AcquireDesc& desc, AttachedHandle& outHandle)
{
    Shared<const EffectDefinition> definition{};
    Shared<EffectInstance> instance{};
    CHECK_FAILED(Acquire_Instance(desc, definition, instance), E_FAIL);
    CHECK_FAILED(Prepare_AcquiredInstance(desc, definition, instance), E_FAIL);

    Populate_Handle(desc, definition, instance, outHandle);

    return S_OK;
}

HRESULT EffectInstancePool::Reload_Attached(const AcquireDesc& desc, AttachedHandle& handle)
{
    if (handle.effectObject == nullptr)
        return E_FAIL;

    PooledInstance* entry = Find_EntryByObject(handle.effectObject);
    if (nullptr == entry || nullptr == entry->instance)
        return E_FAIL;

    const Shared<const EffectDefinition> definition = desc.definition ? desc.definition : EffectAssetRuntimeLoader::Load_DefinitionByName(desc.effectName);
    CHECK_NULL(definition, E_FAIL);

    CHECK_FAILED(entry->instance->Reload_Definition(definition), E_FAIL);

    entry->effectName = desc.effectName;
    entry->definition = definition;
    entry->layerLevelIndex = desc.layerLevelIndex;
    entry->layerTag = desc.layerTag;
    entry->inUse = true;
    entry->autoReleaseOnFinished = desc.autoReleaseOnFinished;

    CHECK_FAILED(Prepare_AcquiredInstance(desc, definition, entry->instance), E_FAIL);
    Populate_Handle(desc, definition, entry->instance, handle);


    return S_OK;
}

void EffectInstancePool::Release(const AttachedHandle& handle)
{
    if (handle.effectObject == nullptr)
        return;

    PooledInstance* entry = Find_EntryByObject(handle.effectObject);
    if (nullptr == entry || nullptr == entry->instance)
    {
        LOG_WARN("[EffectPool] release skipped. entry not found. key={}", String::ToString(handle.effectName));
        return;
    }

    entry->instance->Stop();
    entry->inUse = false;
}

void EffectInstancePool::Release_Finished()
{
    for (PooledInstance& entry : _entries)
    {
        if (!entry.inUse || nullptr == entry.instance)
            continue;
        if (!entry.autoReleaseOnFinished)
            continue;

        if (!entry.instance->Is_Finished())
            continue;

        entry.instance->Stop();
        entry.inUse = false;
    }
}

void EffectInstancePool::Clear()
{
    const size_t entryCount = _entries.size();

    for (PooledInstance& entry : _entries)
    {
        if (entry.instance)
            entry.instance->Stop();
    }

    _entries.clear();

    if (entryCount > 0)
    {
    }
}

void EffectInstancePool::Discard_Inactive(const wstring& effectName)
{
    uint32 discardedCount = 0;

    erase_if(
        _entries,
        [&](const PooledInstance& entry)
        {
            if (entry.inUse)
                return false;
            if (entry.effectName != effectName)
                return false;

            if (entry.instance)
            {
                entry.instance->Stop();
                GAME->Remove_GameObject(entry.layerLevelIndex, static_pointer_cast<GameObject>(entry.instance));
            }

            ++discardedCount;
            return true;
        }
    );

    if (discardedCount > 0)
    {
    }
    else
    {
    }
}

uint32 EffectInstancePool::Discard_AllInactive()
{
    uint32 discardedCount = 0;

    erase_if(
        _entries,
        [&](const PooledInstance& entry)
        {
            if (entry.inUse)
                return false;

            if (entry.instance)
            {
                entry.instance->Stop();
                GAME->Remove_GameObject(entry.layerLevelIndex, static_pointer_cast<GameObject>(entry.instance));
            }

            ++discardedCount;
            return true;
        }
    );

    return discardedCount;
}

uint32 EffectInstancePool::Discard_All()
{
    uint32 discardedCount = 0;

    for (PooledInstance& entry : _entries)
    {
        if (entry.instance)
        {
            entry.instance->Stop();
            GAME->Remove_GameObject(entry.layerLevelIndex, static_pointer_cast<GameObject>(entry.instance));
        }

        ++discardedCount;
    }

    _entries.clear();
    return discardedCount;
}

EffectInstancePool::DebugSummary EffectInstancePool::Get_DebugSummary() const
{
    DebugSummary summary{};
    summary.entryCount = static_cast<uint32>(_entries.size());

    for (const PooledInstance& entry : _entries)
    {
        if (entry.inUse)
            ++summary.activeCount;
        else
            ++summary.idleCount;
    }

    return summary;
}

string EffectInstancePool::Build_DebugActiveEffectBreakdown(uint32 maxItems) const
{
    struct EffectCount
    {
        wstring effectName{};
        uint32 count = 0;
    };

    vector<EffectCount> counts{};
    for (const PooledInstance& entry : _entries)
    {
        if (!entry.inUse)
            continue;

        auto found = ranges::find_if(
            counts,
            [&entry](const EffectCount& effectCount)
            {
                return effectCount.effectName == entry.effectName;
            });

        if (found != counts.end())
        {
            ++found->count;
            continue;
        }

        counts.push_back(EffectCount{ entry.effectName, 1 });
    }

    if (counts.empty())
        return "<none>";

    ranges::sort(
        counts,
        [](const EffectCount& lhs, const EffectCount& rhs)
        {
            if (lhs.count != rhs.count)
                return lhs.count > rhs.count;
            return lhs.effectName < rhs.effectName;
        });

    const uint32 outputCount = maxItems == 0
                               ? static_cast<uint32>(counts.size())
                               : min(maxItems, static_cast<uint32>(counts.size()));

    string result{};
    for (uint32 i = 0; i < outputCount; ++i)
    {
        if (i > 0)
            result += ", ";

        result += String::ToString(counts[i].effectName);
        result += ":";
        result += to_string(counts[i].count);
    }

    if (outputCount < counts.size())
        result += ", ...";

    return result;
}

HRESULT EffectInstancePool::Prewarm(const AcquireDesc& desc, uint32 count)
{
    if (desc.effectName.empty() || desc.layerTag.empty() || count == 0)
        return E_FAIL;

    vector<AttachedHandle> handles{};
    handles.reserve(count);

    // 각 prewarm instance가 다음 frame의 첫 BeginPlay에서 다시 재생되지 않도록 미리 시작 상태를 확정한다.
    for (uint32 i = 0; i < count; ++i)
    {
        AttachedHandle handle{};
        CHECK_FAILED(Acquire(desc, handle), E_FAIL);

        if (handle.effectObject != nullptr)
            CHECK_FAILED(handle.effectObject->Try_BeginPlay(), E_FAIL);

        handles.push_back(handle);
    }

    // 모든 슬롯을 확보한 뒤 반환해야 같은 idle slot 하나만 반복 재사용하지 않는다.
    for (const AttachedHandle& handle : handles)
        Release(handle);


    return S_OK;
}

HRESULT EffectInstancePool::Prewarm_ToCount(const AcquireDesc& desc, uint32 targetCount, uint32 createBudget, PrewarmResult* outResult)
{
    if (outResult != nullptr)
    {
        *outResult = {};
        outResult->targetCount = targetCount;
    }

    if (desc.effectName.empty() || desc.layerTag.empty() || targetCount == 0)
        return E_FAIL;

    const Shared<const EffectDefinition> definition =
        desc.definition ? desc.definition : EffectAssetRuntimeLoader::Load_DefinitionByName(desc.effectName);
    CHECK_NULL(definition, E_FAIL);

    const uint32 existingCount = Count_Entries(desc, definition);
    if (outResult != nullptr)
        outResult->existingCount = existingCount;

    if (existingCount >= targetCount || createBudget == 0)
        return S_OK;

    const uint32 createCount = min(targetCount - existingCount, createBudget);
    uint32 createdCount = 0;

    for (uint32 i = 0; i < createCount; ++i)
    {
        Shared<EffectInstance> instance{};
        CHECK_FAILED(Spawn_NewInstance(desc, definition, instance), E_FAIL);

        if (instance == nullptr)
            return E_FAIL;

        if (FAILED(Prepare_AcquiredInstance(desc, definition, instance)))
        {
            GAME->Remove_GameObject(desc.layerLevelIndex, static_pointer_cast<GameObject>(instance));
            return E_FAIL;
        }

        if (FAILED(instance->Try_BeginPlay()))
        {
            GAME->Remove_GameObject(desc.layerLevelIndex, static_pointer_cast<GameObject>(instance));
            return E_FAIL;
        }

        instance->Stop();

        PooledInstance newEntry{};
        newEntry.effectName = desc.effectName;
        newEntry.definition = definition;
        newEntry.instance = instance;
        newEntry.layerLevelIndex = desc.layerLevelIndex;
        newEntry.layerTag = desc.layerTag;
        newEntry.inUse = false;
        newEntry.autoReleaseOnFinished = desc.autoReleaseOnFinished;
        _entries.push_back(newEntry);

        ++createdCount;
    }

    if (outResult != nullptr)
        outResult->createdCount = createdCount;

    return S_OK;
}

uint32 EffectInstancePool::Count_Entries(const AcquireDesc& desc, const Shared<const EffectDefinition>& definition) const
{
    uint32 count = 0;

    for (const PooledInstance& entry : _entries)
    {
        if (entry.effectName != desc.effectName)
            continue;
        if (entry.layerLevelIndex != desc.layerLevelIndex)
            continue;
        if (entry.layerTag != desc.layerTag)
            continue;
        if (desc.definition && entry.definition != definition)
            continue;
        if (entry.instance == nullptr)
            continue;
        if (entry.inUse)
            continue;

        ++count;
    }

    return count;
}

HRESULT EffectInstancePool::Acquire_Instance(
    const AcquireDesc& desc,
    Shared<const EffectDefinition>& outDefinition,
    Shared<EffectInstance>& outInstance)
{
    outDefinition = desc.definition ? desc.definition : EffectAssetRuntimeLoader::Load_DefinitionByName(desc.effectName);
    CHECK_NULL(outDefinition, E_FAIL);

    if (PooledInstance* entry = Find_AvailableEntry(desc, outDefinition))
    {
        entry->inUse = true;
        entry->autoReleaseOnFinished = desc.autoReleaseOnFinished;
        outInstance = entry->instance;

        return S_OK;
    }

    const HRESULT spawnResult = Spawn_NewInstance(desc, outDefinition, outInstance);
    if (FAILED(spawnResult))
    {
        LOG_WARN(
            "[EffectPool] spawn failed: hr={}, effect={}, level={}, layer={}",
            spawnResult,
            String::ToString(desc.effectName),
            desc.layerLevelIndex,
            String::ToString(desc.layerTag)
        );
        return E_FAIL;
    }

    PooledInstance newEntry{};
    newEntry.effectName = desc.effectName;
    newEntry.definition = outDefinition;
    newEntry.instance = outInstance;
    newEntry.layerLevelIndex = desc.layerLevelIndex;
    newEntry.layerTag = desc.layerTag;
    newEntry.inUse = true;
    newEntry.autoReleaseOnFinished = desc.autoReleaseOnFinished;
    _entries.push_back(newEntry);

    return S_OK;
}

HRESULT EffectInstancePool::Spawn_NewInstance(
    const AcquireDesc& desc,
    const Shared<const EffectDefinition>& definition,
    Shared<EffectInstance>& outInstance)
{
    EffectInstanceDesc instanceDesc{};
    instanceDesc.definition = definition;
    instanceDesc.position = desc.worldPosition;
    instanceDesc.scale = desc.scale;

    Shared<GameObject> spawnedObject{};
    const HRESULT addResult = GAME->Add_GameObject(
        ETOI(LevelType::Static),
        L"GameObject_EffectInstance",
        desc.layerLevelIndex,
        desc.layerTag,
        spawnedObject,
        &instanceDesc
    );
    if (FAILED(addResult))
    {
        LOG_WARN(
            "[EffectPool] Add_GameObject failed: hr={}, effect={}, level={}, layer={}",
            addResult,
            String::ToString(desc.effectName),
            desc.layerLevelIndex,
            String::ToString(desc.layerTag)
        );
        return E_FAIL;
    }

    outInstance = dynamic_pointer_cast<EffectInstance>(spawnedObject);
    if (outInstance == nullptr)
    {
        LOG_WARN(
            "[EffectPool] spawned object is not EffectInstance: effect={}, object={}",
            String::ToString(desc.effectName),
            static_cast<const void*>(spawnedObject.get())
        );
        return E_FAIL;
    }

    return S_OK;
}

HRESULT EffectInstancePool::Prepare_AcquiredInstance(
    const AcquireDesc& desc,
    const Shared<const EffectDefinition>& definition,
    const Shared<EffectInstance>& instance)
{
    CHECK_NULL(instance, E_FAIL);
    instance->Bind_TrailSampleProvider(desc.trailSampleProvider, desc.trailSampleGroupKey);
    instance->Bind_SourcePointSampleProvider(desc.sourcePointSampleProvider, desc.sourcePointGroupKey);
    instance->Bind_RibbonSourcePointSampleProvider(desc.ribbonSourcePointSampleProvider);
    instance->Set_Name(Make_EffectInstanceDisplayName(desc, definition));

    const Shared<TransformCom> transform = instance->Get_Transform();
    CHECK_NULL(transform, E_FAIL);
    transform->Set_WorldPosition(desc.worldPosition);
    transform->Set_WorldRotationQuaternion(desc.worldRotation);
    transform->Set_Scale(desc.scale);

    instance->Set_PlaybackSpeed(desc.playbackSpeed);
    CHECK_FAILED(instance->Reset(), E_FAIL);
    CHECK_FAILED(instance->Play(), E_FAIL);
    return S_OK;
}

void EffectInstancePool::Populate_Handle(
    const AcquireDesc& desc,
    const Shared<const EffectDefinition>& definition,
    const Shared<EffectInstance>& instance,
    AttachedHandle& outHandle) const
{
    outHandle.effectObject = static_pointer_cast<GameObject>(instance);
    outHandle.effectName = desc.effectName;
    outHandle.requiresManualRelease = false;

    if (definition == nullptr)
        return;

    for (const EffectEmitterDefinition& emitter : definition->emitters)
    {
        if (!emitter.enabled)
            continue;

        if (emitter.kind == EffectEmitterKind::Trail ||
            emitter.kind == EffectEmitterKind::Ribbon ||
            emitter.kind == EffectEmitterKind::SourceHistorySpriteTrail)
        {
            outHandle.requiresManualRelease = true;
            return;
        }
    }
}

EffectInstancePool::PooledInstance* EffectInstancePool::Find_AvailableEntry(
    const AcquireDesc& desc,
    const Shared<const EffectDefinition>& definition)
{
    for (PooledInstance& entry : _entries)
    {
        if (entry.inUse)
            continue;
        if (entry.effectName != desc.effectName)
            continue;
        if (entry.layerLevelIndex != desc.layerLevelIndex)
            continue;
        if (entry.layerTag != desc.layerTag)
            continue;
        if (desc.definition && entry.definition != definition)
            continue;
        if (nullptr == entry.instance)
            continue;
        if (entry.instance->Get_Transform() == nullptr)
            continue;

        return &entry;
    }

    return nullptr;
}

EffectInstancePool::PooledInstance* EffectInstancePool::Find_EntryByObject(const Shared<GameObject>& effectObject)
{
    for (PooledInstance& entry : _entries)
    {
        if (entry.instance == effectObject)
            return &entry;
    }

    return nullptr;
}

Unique<EffectInstancePool> EffectInstancePool::Create()
{
    auto instance = make_unique<EffectInstancePool>();

    if (nullptr == instance)
    {
        LOG_CRITICAL("Failed to Create : EffectInstancePool");
        return nullptr;
    }


    return instance;
}

void EffectInstancePool::Free()
{
    Clear();

    __super::Free();
}

NS_END
