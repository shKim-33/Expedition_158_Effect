#include "Hierarchy_View.h"

#include "Client_Defines.h"
#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"

NS_BEGIN(EffectEditor)

Hierarchy_View::Hierarchy_View()
    : Editor_Window{ L"Hierarchy", ICON_FA_SITEMAP }
{
}

Hierarchy_View::~Hierarchy_View()
{
    Free();
}

HRESULT Hierarchy_View::Initialize()
{
    return Editor_Window::Initialize();
}

void Hierarchy_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    const Editor_Context* context = EDITOR->Get_EditorContext();
    if (context == nullptr)
        return;

    const uint32 selectionRevision = context->Get_SelectionRevision();
    if (_cachedSelectionRevision != selectionRevision)
        _cachedSelectionRevision = selectionRevision;

    const uint32 currentLevelIndex = GAME->Current_LevelIndex();
    if (_cachedLevelIndex != currentLevelIndex)
    {
        _cachedLevelIndex = currentLevelIndex;
        _dirtyHierarchyCache = true;
    }

    const uint64 objectRevision = GAME->Get_CurrentLevelObjectRevision();
    if (_cachedObjectRevision != objectRevision)
    {
        _cachedObjectRevision = objectRevision;
        _dirtyHierarchyCache = true;
    }

    if (_dirtyHierarchyCache)
        Rebuild_Cache();
}

void Hierarchy_View::Pre_Render()
{
    Editor_Window::Pre_Render();
}

void Hierarchy_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        Editor_Context* context = EDITOR->Get_EditorContext();
        if (context == nullptr)
        {
            ImGui::TextDisabled("Editor Context Missing");
            Set_Open(isOpen);
            ImGui::End();
            return;
        }

        const Shared<GameObject> selectedObject = context->Get_Selection().selectedObject;
        Handle_Shortcuts();

        Draw_SearchBar();

        if (_groupedObjects.empty())
            ImGui::TextDisabled("No objects in current level");
        else if (!_searchFilter.empty())
        {
            for (const auto& [_, objects] : _groupedObjects)
            {
                uint32 objectIndex = 0;

                for (const auto& gameObject : objects)
                {
                    if (gameObject == nullptr)
                    {
                        ++objectIndex;
                        continue;
                    }

                    if (!Pass_SearchFilter(gameObject, objectIndex))
                    {
                        ++objectIndex;
                        continue;
                    }

                    const bool isSelected = selectedObject == gameObject;
                    const string label = Build_HierarchyLabel(gameObject, objectIndex);

                    ImGui::PushID(gameObject.get());
                    if (ImGui::Selectable(label.c_str(), isSelected))
                        context->Set_SelectObject(gameObject);

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        context->Set_SelectObject(gameObject);
                        context->Request_FocusObject(gameObject);
                        EDITOR->Request_WindowFocus(EditorViewportTarget::Scene);
                    }
                    ImGui::PopID();

                    ++objectIndex;
                }
            }
        }
        else
        {
            for (const auto& [layerTag, objects] : _groupedObjects)
            {
                const string layerName = String::ToString(layerTag);

                if (ImGui::TreeNodeEx(layerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    uint32 objectIndex = 0;

                    for (const auto& gameObject : objects)
                    {
                        if (gameObject == nullptr)
                        {
                            ++objectIndex;
                            continue;
                        }

                        const bool isSelected = selectedObject == gameObject;
                        const string label = Build_HierarchyLabel(gameObject, objectIndex);

                        ImGui::PushID(gameObject.get());
                        if (ImGui::Selectable(label.c_str(), isSelected))
                            context->Set_SelectObject(gameObject);

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            context->Set_SelectObject(gameObject);
                            context->Request_FocusObject(gameObject);
                            EDITOR->Request_WindowFocus(EditorViewportTarget::Scene);
                        }
                        ImGui::PopID();

                        ++objectIndex;
                    }

                    ImGui::TreePop();
                }
            }
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Hierarchy_View::Draw_SearchBar()
{
    ImGui::InputTextWithHint("##SearchHierarchy", "Search...", _searchBuffer, IM_ARRAYSIZE(_searchBuffer));
    _searchFilter = _searchBuffer;

    ImGui::Separator();

    const uint32 totalCount = Get_TotalObjectCount();
    uint32 filteredCount = totalCount;

    if (!_searchFilter.empty())
    {
        filteredCount = 0;

        for (const auto& [_, objects] : _groupedObjects)
        {
            uint32 objectIndex = 0;

            for (const auto& gameObject : objects)
            {
                if (gameObject != nullptr && Pass_SearchFilter(gameObject, objectIndex))
                    ++filteredCount;

                ++objectIndex;
            }
        }
    }

    ImGui::Text("Objects: %u / %u", filteredCount, totalCount);
    ImGui::Separator();
}

void Hierarchy_View::Rebuild_Cache()
{
    _groupedObjects.clear();

    const auto append_level_objects =
        [this](uint32 levelIndex, const char* levelLabel)
    {
        GAME->Visit_LevelObjects(
            levelIndex,
            [this, levelLabel](const wstring& layerTag, const Shared<GameObject>& gameObject)
            {
                if (gameObject == nullptr)
                    return;

                const wstring groupedLayerTag =
                    String::ToWString(format("[{}] {}", levelLabel, String::ToString(layerTag)));

                _groupedObjects[groupedLayerTag].push_back(gameObject);
            }
        );
    };

    append_level_objects(ETOI(LevelType::Static), "Static");

    const uint32 currentLevelIndex = GAME->Current_LevelIndex();
    if (currentLevelIndex != ETOI(LevelType::Static))
        append_level_objects(currentLevelIndex, "Current");

    _dirtyHierarchyCache = false;
}

void Hierarchy_View::Handle_Shortcuts()
{
    if (!_isFocused)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        EDITOR->Copy_SelectedObject();
        return;
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        EDITOR->Paste_CopiedObject();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        EDITOR->Delete_SelectedObject();
}

bool Hierarchy_View::Pass_SearchFilter(const Shared<GameObject>& gameObject, uint32 objectIndex) const
{
    if (_searchFilter.empty())
        return true;

    const string label = Build_HierarchyLabel(gameObject, objectIndex);
    return label.find(_searchFilter) != string::npos;
}

uint32 Hierarchy_View::Get_TotalObjectCount() const
{
    uint32 totalCount = 0;

    for (const auto& [_, objects] : _groupedObjects)
    {
        for (const auto& gameObject : objects)
        {
            if (gameObject != nullptr)
                ++totalCount;
        }
    }

    return totalCount;
}

string Hierarchy_View::Build_HierarchyLabel(const Shared<GameObject>& gameObject, uint32 objectIndex) const
{
    if (gameObject == nullptr)
        return "<Null>";

    const string objectName = String::ToString(gameObject->Get_Name());
    if (!objectName.empty())
        return objectName + " [" + to_string(objectIndex) + "]";

    return "GameObject [" + to_string(objectIndex) + "]";
}

Shared<Hierarchy_View> Hierarchy_View::Create()
{
    return make_shared<Hierarchy_View>();
}

void Hierarchy_View::Free()
{
    _groupedObjects.clear();
    Editor_Window::Free();
}

NS_END
