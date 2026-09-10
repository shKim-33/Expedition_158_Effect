#pragma once

#include "Editor_Window.h"

NS_BEGIN(EffectEditor)

class Hierarchy_View final : public Editor_Window
{
public:
    Hierarchy_View();
    ~Hierarchy_View() override;

public:
    HRESULT Initialize() override;
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;

private: //## Data::HierarchyCache
    map<wstring, vector<Shared<GameObject>>> _groupedObjects{};
    char _searchBuffer[128]{};
    string _searchFilter{};
    uint32 _cachedSelectionRevision{ 0 };
    uint32 _cachedLevelIndex{ UINT32_MAX };
    uint64 _cachedObjectRevision{ UINT64_MAX };
    bool _dirtyHierarchyCache{ true };

private: //## Helper::HierarchyView
    void Draw_SearchBar();
    void Rebuild_Cache();
    void Handle_Shortcuts();
    bool Pass_SearchFilter(const Shared<GameObject>& gameObject, uint32 objectIndex) const;
    uint32 Get_TotalObjectCount() const;
    string Build_HierarchyLabel(const Shared<GameObject>& gameObject, uint32 objectIndex) const;

public:
    static Shared<Hierarchy_View> Create();
    void Free() override;
};

NS_END
