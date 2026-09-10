#include "Content_Browser.h"

#include <shellapi.h>
#include "EffectEditorInstance.h"
#include "EffectMaterial_View.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "Notification_Manager.h"
#include "Texture.h"
#include "TextureDisplayInfoResolver.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr auto kEffectTexturePayloadType = "CONTENT_BROWSER_EFFECT_TEXTURE";
    constexpr auto kEffectModelPayloadType = "CONTENT_BROWSER_EFFECT_MODEL";

    bool Is_TexturePayloadFile(const fs::path& filePath)
    {
        const string extension = String::ToLowerCopy(filePath.extension().string());
        return extension == ".png" || extension == ".dds";
    }

    bool Is_ModelPayloadFile(const fs::path& filePath)
    {
        return String::ToLowerCopy(filePath.extension().string()) == ".model";
    }

    ImTextureID ToImTextureId(const Shared<Texture>& texture)
    {
        if (nullptr == texture)
            return static_cast<ImTextureID>(0);

        const auto& srvs = texture->Get_SRVs();
        if (srvs.empty() || nullptr == srvs[0])
            return static_cast<ImTextureID>(0);

        return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srvs[0].Get()));
    }

    void DrawTileLabel(const string& text, float maxWidth)
    {
        if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
        {
            ImGui::TextUnformatted(text.c_str());
            return;
        }

        constexpr auto ellipsis = "...";
        const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
        if (maxWidth <= ellipsisWidth)
        {
            ImGui::TextUnformatted(ellipsis);
            return;
        }

        size_t low = 0;
        size_t high = text.size();
        while (low < high)
        {
            const size_t mid = (low + high + 1) / 2;
            const string candidate = text.substr(0, mid) + ellipsis;
            if (ImGui::CalcTextSize(candidate.c_str()).x <= maxWidth)
                low = mid;
            else
                high = mid - 1;
        }

        const string label = text.substr(0, low) + ellipsis;
        ImGui::TextUnformatted(label.c_str());
    }
}

Content_Browser::Content_Browser()
    : Editor_Window{ L"Content Browser", ICON_FA_FOLDER_OPEN }
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (!assetRoot.empty())
        Set_ResourceRoot(fs::path(assetRoot));

    Load_Icons();
}

Content_Browser::~Content_Browser()
{
    Free();
}

void Content_Browser::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    if (_pendingResourceRefresh && !_resourceRoot.empty() && fs::exists(_resourceRoot))
    {
        Refresh_Resources();
        _pendingResourceRefresh = false;
    }
}

void Content_Browser::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        if (!_isFocused)
            _selectedFilePath.clear();

        if (_resourceRoot.empty())
        {
            const wstring assetRoot = GAME->Get_AssetRoot();
            if (!assetRoot.empty())
                Set_ResourceRoot(fs::path(assetRoot));
        }

        if (_needsInitialRefresh && _rootFolder.fullPath.empty() && !_resourceRoot.empty() && fs::exists(_resourceRoot))
        {
            Refresh_Resources();
            Load_Settings();
            _needsInitialRefresh = false;
        }

        Handle_SideButtonEvent();
        Handle_ThumbnailZoom();

        ImGui::BeginChild("FolderTree", ImVec2(_leftPanelWidth, 0.f), true);
        {
            if (ImGui::InputTextWithHint("##Search", "Search folders...", _searchBuffer, IM_ARRAYSIZE(_searchBuffer)))
            {
                if (strlen(_searchBuffer) > 0)
                {
                    const string searchLower = ToLowerCopy(_searchBuffer);

                    FolderNode* firstMatch = Get_FirstMatchingFolder(_rootFolder, searchLower);
                    if (firstMatch && _currentFolder != firstMatch)
                    {
                        _currentFolder = firstMatch;
                        Expand_PathTo(firstMatch->fullPath);
                    }
                }
            }

            ImGui::Separator();

            ImGui::BeginChild("FolderTreeScroll", ImVec2(0, 0), false);
            if (_rootFolder.fullPath.empty())
                ImGui::TextDisabled("Resource folder is not available yet.");
            else
                Draw_FolderTree(_rootFolder);
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        const float availHeight = ImGui::GetContentRegionAvail().y;
        if (availHeight > 0.f)
            ImGui::InvisibleButton("vsplitter", ImVec2(4.f, availHeight));

        if (ImGui::IsItemActive())
            _leftPanelWidth = max(180.f, _leftPanelWidth + ImGui::GetIO().MouseDelta.x);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        ImGui::SameLine();

        ImGui::BeginChild("AssetView", ImVec2(0.f, 0.f), true);
        {
            if (_currentFolder)
            {
                ImGui::BeginDisabled(!Can_GoParentFolder());
                if (ImGui::ArrowButton("##ParentFolder", ImGuiDir_Up))
                    Go_ParentFolder();
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(!Can_GoBack());
                if (ImGui::ArrowButton("##BackFolder", ImGuiDir_Left))
                    Go_BackFolder();
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(!Can_GoForward());
                if (ImGui::ArrowButton("##ForwardFolder", ImGuiDir_Right))
                    Go_ForwardFolder();
                ImGui::EndDisabled();

                ImGui::SameLine();

                const string pathStr = String::ToString(_currentFolder->fullPath);
                char pathBuffer[512] = {};
                strncpy_s(pathBuffer, pathStr.c_str(), _TRUNCATE);

                float pathWidth = min(500.f, ImGui::GetContentRegionAvail().x - 220.f);
                if (pathWidth < 150.f)
                    pathWidth = 150.f;

                ImGui::SetNextItemWidth(pathWidth);
                ImGui::InputText("##CurrentPath", pathBuffer, IM_ARRAYSIZE(pathBuffer), ImGuiInputTextFlags_ReadOnly);

                const float refreshWidth = 120.f;
                const float settingsWidth = 28.f;
                const float controlSpacing = 8.f;
                const bool isLargeTextureFolder = Is_LargeTextureFolder();
                const float statusWidth = isLargeTextureFolder ? 170.f : 0.f;

                const float remainingWidth = ImGui::GetContentRegionAvail().x;
                const float searchSlotWidth = remainingWidth - refreshWidth - settingsWidth - statusWidth - controlSpacing * 3.f;

                if (searchSlotWidth > 120.f)
                {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(min(220.f, searchSlotWidth));
                    ImGui::InputTextWithHint("##AssetNameSearch", "Search files...", _assetSearchBuffer, IM_ARRAYSIZE(_assetSearchBuffer));
                }

                ImGui::SameLine();

                const float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                                     refreshWidth - settingsWidth - statusWidth - controlSpacing * 2.f;
                if (rightX > ImGui::GetCursorPosX())
                    ImGui::SetCursorPosX(rightX);

                if (isLargeTextureFolder)
                {
                    ImGui::TextDisabled("Large texture folder: %d/frame", Resolve_CurrentThumbnailLoadBudget());
                    ImGui::SameLine();
                }

                if (ImGui::Button(ICON_FA_SLIDERS "##ContentBrowserThumbnailSettings", ImVec2(settingsWidth, 24.f)))
                    ImGui::OpenPopup("ContentBrowserThumbnailSettings");

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Thumbnail settings");

                Draw_ThumbnailSettingsPopup();

                ImGui::SameLine();

                if (ImGui::Button(ICON_FA_ROTATE_RIGHT " 새로고침", ImVec2(refreshWidth, 24.f)))
                    Refresh_AssetState();

                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("새로고침");

                ImGui::Separator();

                ImGui::BeginChild("AssetViewScroll", ImVec2(0, 0), false);
                Draw_AssetView();
                ImGui::EndChild();
            }
            else
                ImGui::TextDisabled("No folder selected.");
        }
        ImGui::EndChild();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Content_Browser::Request_RefreshResources()
{
    _pendingResourceRefresh = true;
}

void Content_Browser::Refresh_Resources()
{
    const auto refreshBegin = chrono::steady_clock::now();
    _thumbnailCache.clear();
    _noThumbnailPaths.clear();
    _gridItemCache.clear();
    _gridItemCacheFolder = nullptr;
    Refresh_BrowserResources(
        [](const wstring& filePath)
        {
            return GAME->Find_AssetGUID(filePath);
        }
    );

    const auto elapsedMs = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - refreshBegin).count();
    if (elapsedMs >= 200)
    {
        LOG_INFO(
            "[ContentBrowserTiming] Refresh_Resources elapsed={}ms root={} folders={} files={}",
            elapsedMs,
            String::ToString(_resourceRoot.wstring()),
            static_cast<uint32>(_rootFolder.subFolders.size()),
            static_cast<uint32>(_rootFolder.files.size())
        );
    }
}

void Content_Browser::Refresh_AssetState()
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
    {
        NOTIFY("새로고침 실패");
        return;
    }

    const fs::path missingMetaRootPath = fs::path(assetRoot) / L"Effects";
    if (SUCCEEDED(GAME->Initialize_AssetManager(assetRoot, true, missingMetaRootPath.wstring())))
    {
        Refresh_Resources();
        NOTIFY("새로고침");
    }
    else
        NOTIFY("새로고침 실패");
}

void Content_Browser::Refresh_CurrentFolder()
{
    _gridItemCache.clear();
    _gridItemCacheFolder = nullptr;
    ContentBrowserCore::Refresh_CurrentFolder(
        [](const wstring& filePath)
        {
            return GAME->Find_AssetGUID(filePath);
        }
    );
}

void Content_Browser::Load_Icons()
{
    _iconFiles.assign(static_cast<size_t>(IconType::End), nullptr);

    fs::path iconRoot = _resourceRoot;
    if (iconRoot.empty())
    {
        const wstring assetRoot = GAME->Get_AssetRoot();
        if (!assetRoot.empty())
            iconRoot = fs::path(assetRoot);
    }

    if (iconRoot.empty())
        return;

    iconRoot /= L"Textures";
    iconRoot /= L"UI";
    iconRoot /= L"Common";
    iconRoot /= L"FileType";

    const auto load_texture = [&](const wchar_t* fileName) -> Shared<Texture>
    {
        const fs::path filePath = iconRoot / fileName;
        if (!fs::exists(filePath))
            return nullptr;

        return Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), filePath.c_str(), 1);
    };

    _iconFiles[static_cast<size_t>(IconType::Folder)] = load_texture(L"Folder_Icon.png");

    const Shared<Texture> dataTableIcon = load_texture(L"DataTable.png");
    _iconFiles[static_cast<size_t>(IconType::Csv)] = dataTableIcon;
    _iconFiles[static_cast<size_t>(IconType::Xlsx)] = dataTableIcon;

    _iconFiles[static_cast<size_t>(IconType::Json)] = load_texture(L"JSON.png");
    _iconFiles[static_cast<size_t>(IconType::Python)] = load_texture(L"Python.png");
}

void Content_Browser::Draw_FolderTree(FolderNode& node)
{
    if (strlen(_searchBuffer) > 0 && _currentFolder != &node)
    {
        const string searchLower = ToLowerCopy(_searchBuffer);

        if (node.fullPath != _rootFolder.fullPath && !IsFolderMatchingSearch(node, searchLower))
            return;

        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (_currentFolder == &node)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (node.subFolders.empty())
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (_expandedFolders.contains(node.fullPath))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        _expandedFolders.erase(node.fullPath);
    }

    const string id = "##" + String::ToString(node.fullPath);
    const bool isOpen = ImGui::TreeNodeEx(id.c_str(), flags);

    Draw_FolderContextMenu(node, "FolderTreeContext");

    const bool isClicked = ImGui::IsItemClicked();
    ImGui::SameLine();

    if (const ImTextureID iconId = Get_IconTextureId(IconType::Folder))
    {
        ImGui::Image(iconId, ImVec2(16.f, 16.f));
        ImGui::SameLine(0.f, 6.f);
    }

    const string folderName = String::ToString(node.name);
    ImGui::TextUnformatted(folderName.c_str());

    if (isClicked)
        Open_Folder(&node);

    if (isOpen)
    {
        for (auto& child : node.subFolders)
            Draw_FolderTree(child);

        ImGui::TreePop();
    }
}

void Content_Browser::Draw_AssetView()
{
    if (!_currentFolder)
        return;

    _thumbnailLoadsThisFrame = 0;
    _thumbnailLoadBudgetThisFrame = Resolve_CurrentThumbnailLoadBudget();

    const string assetSearchLower = ToLowerCopy(_assetSearchBuffer);
    const bool shouldRebuildGridItems =
        _gridItemCacheFolder != _currentFolder ||
        _gridItemCacheSubFolderCount != _currentFolder->subFolders.size() ||
        _gridItemCacheFileCount != _currentFolder->files.size() ||
        _gridItemCacheSearchLower != assetSearchLower;

    if (shouldRebuildGridItems)
    {
        Build_GridItems(
            *_currentFolder,
            assetSearchLower,
            [](const FolderNode& folder, const string& searchLower)
            {
                if (searchLower.empty())
                    return true;

                const string folderNameLower = ToLowerCopy(String::ToString(folder.name));
                return folderNameLower.find(searchLower) != string::npos;
            },
            [](const FileEntry& fileEntry, const string& searchLower)
            {
                if (searchLower.empty())
                    return true;

                const fs::path path(fileEntry.filePath);
                const string fileNameLower = ToLowerCopy(String::ToString(path.filename().wstring()));
                const string pureNameLower = ToLowerCopy(String::ToString(path.stem().wstring()));
                return pureNameLower.find(searchLower) != string::npos ||
                       fileNameLower.find(searchLower) != string::npos;
            },
            _gridItemCache
        );

        _gridItemCacheFolder = _currentFolder;
        _gridItemCacheSubFolderCount = _currentFolder->subFolders.size();
        _gridItemCacheFileCount = _currentFolder->files.size();
        _gridItemCacheSearchLower = assetSearchLower;
    }

    const GridLayout gridLayout = Calculate_GridLayout(
        ImGui::GetContentRegionAvail().x,
        _thumbnailSize,
        ImGui::GetTextLineHeightWithSpacing(),
        _gridItemCache.size()
    );
    const GridVisibleRange visibleRange = Calculate_GridVisibleRange(
        gridLayout,
        ImGui::GetScrollY(),
        ImGui::GetWindowHeight(),
        _gridItemCache.size(),
        1
    );

    const auto resolveGridItem = [&](const GridItemRef& item) -> pair<FolderNode*, const FileEntry*>
    {
        if (item.type == ContentBrowserGridItemType::Folder)
        {
            if (item.index >= _currentFolder->subFolders.size())
                return { nullptr, nullptr };

            return { &_currentFolder->subFolders[item.index], nullptr };
        }

        if (item.index >= _currentFolder->files.size())
            return { nullptr, nullptr };

        return { nullptr, &_currentFolder->files[item.index] };
    };

    const auto drawFileTile = [&](const FileEntry& fileEntry)
    {
        const fs::path path(fileEntry.filePath);
        const string fileName = String::ToString(path.filename().wstring());
        const string pureName = String::ToString(path.stem().wstring());

        ImGui::PushID(fileName.c_str());

        const bool isTileVisible = ImGui::IsRectVisible(
            ImVec2(_thumbnailSize, _thumbnailSize + ImGui::GetTextLineHeightWithSpacing() * 2.f)
        );
        Shared<Texture> thumbnailTexture = Get_FileThumbnail(
            fileEntry,
            Should_RequestThumbnail(fileEntry, isTileVisible, _thumbnailLoadsThisFrame, _thumbnailLoadBudgetThisFrame)
        );
        if (thumbnailTexture)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
            ImGui::ImageButton(
                fileName.c_str(),
                ToImTextureId(thumbnailTexture),
                ImVec2(_thumbnailSize, _thumbnailSize)
            );
            ImGui::PopStyleVar();
        }
        else
        {
            const ImTextureID iconId = Get_IconTextureId(Resolve_IconType(path));
            if (iconId)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
                ImGui::ImageButton(fileName.c_str(), iconId, ImVec2(_thumbnailSize, _thumbnailSize));
                ImGui::PopStyleVar();
            }
            else
                ImGui::Button(pureName.c_str(), ImVec2(_thumbnailSize, _thumbnailSize));
        }

        if (Is_EffectMaterialPresetFile(path))
        {
            if (ImGui::BeginDragDropSource())
            {
                const wstring payloadPath = fileEntry.filePath;
                ImGui::SetDragDropPayload(
                    "CONTENT_BROWSER_EFFECT_MATERIAL",
                    payloadPath.c_str(),
                    (payloadPath.size() + 1) * sizeof(wchar_t)
                );

                ImGui::TextUnformatted("Effect Material");
                ImGui::Separator();
                ImGui::Text("%s", fileName.c_str());

                ImGui::EndDragDropSource();
            }
        }
        else if (Is_ModelPayloadFile(path))
        {
            if (ImGui::BeginDragDropSource())
            {
                const wstring payloadPath = fileEntry.filePath;
                ImGui::SetDragDropPayload(
                    kEffectModelPayloadType,
                    payloadPath.c_str(),
                    (payloadPath.size() + 1) * sizeof(wchar_t)
                );

                ImGui::TextUnformatted("Effect Model");
                ImGui::Separator();
                ImGui::Text("%s", fileName.c_str());

                ImGui::EndDragDropSource();
            }
        }
        else if (Is_TexturePayloadFile(path))
        {
            if (ImGui::BeginDragDropSource())
            {
                const wstring payloadPath = fileEntry.filePath;
                ImGui::SetDragDropPayload(
                    kEffectTexturePayloadType,
                    payloadPath.c_str(),
                    (payloadPath.size() + 1) * sizeof(wchar_t)
                );

                ImGui::TextUnformatted("Effect Texture");
                ImGui::Separator();
                ImGui::Text("%s", fileName.c_str());

                ImGui::EndDragDropSource();
            }
        }

        const bool isFocused = ImGui::IsItemFocused();
        const bool isSelected = _selectedFilePath == fileEntry.filePath;

        if (isSelected)
        {
            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRect(itemMin, itemMax, IM_COL32(70, 130, 210, 255), 0.f, 0, 2.f);
        }

        if (isFocused && !isSelected)
            _selectedFilePath = fileEntry.filePath;

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            _selectedFilePath = fileEntry.filePath;

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            Open_File(fileEntry);

        Draw_FileContextMenu(fileEntry.filePath, "FileItemContext");

        const bool isTextureFile = Is_TexturePayloadFile(path);
        const string displayName = isTextureFile ? fileName : pureName;
        DrawTileLabel(displayName, gridLayout.tileWidth - 8.f);
        if (ImGui::IsItemHovered())
        {
            if (isTextureFile)
            {
                string textureId{};
                if (TextureDisplayInfoResolver::TryFindTextureIdByFilePath(path, textureId))
                {
                    const string tooltip =
                        fileName +
                        "\n" +
                        TextureDisplayInfoResolver::MakeTextureTooltip(textureId);
                    ImGui::SetTooltip("%s", tooltip.c_str());
                }
                else
                {
                    const string tooltip = fileName + "\nId: (not registered in DT_Texture)";
                    ImGui::SetTooltip("%s", tooltip.c_str());
                }
            }
            else
                ImGui::SetTooltip("%s", fileName.c_str());
        }

        ImGui::PopID();
    };

    if (visibleRange.hasItems)
    {
        const ImVec2 startCursorPos = ImGui::GetCursorPos();

        for (int rowIndex = visibleRange.firstRow; rowIndex <= visibleRange.lastRow; ++rowIndex)
        {
            const float rowY = startCursorPos.y + static_cast<float>(rowIndex) * gridLayout.tileHeight;

            for (int columnIndex = 0; columnIndex < gridLayout.columnCount; ++columnIndex)
            {
                const size_t itemIndex = static_cast<size_t>(rowIndex * gridLayout.columnCount + columnIndex);
                if (itemIndex > visibleRange.lastItem || itemIndex >= _gridItemCache.size())
                    break;

                ImGui::SetCursorPos(ImVec2(startCursorPos.x + static_cast<float>(columnIndex) * gridLayout.tileWidth, rowY));
                ImGui::BeginGroup();

                const auto [folder, file] = resolveGridItem(_gridItemCache[itemIndex]);
                if (folder)
                    Draw_FolderTile(*folder);
                else if (file)
                    drawFileTile(*file);

                ImGui::EndGroup();
            }
        }

        ImGui::SetCursorPos(ImVec2(startCursorPos.x, startCursorPos.y + static_cast<float>(gridLayout.rowCount) * gridLayout.tileHeight));
        ImGui::Dummy(ImVec2(1.f, 1.f));
    }
}

void Content_Browser::Draw_ThumbnailSettingsPopup()
{
    if (!ImGui::BeginPopup("ContentBrowserThumbnailSettings"))
        return;

    bool changed = false;
    changed |= ImGui::Checkbox("Texture thumbnails", &_thumbnailPolicy.textureThumbnails);
    changed |= ImGui::Checkbox("Show DDS thumbnails", &_thumbnailPolicy.showDdsThumbnails);
    changed |= ImGui::Checkbox("Adaptive large folders", &_thumbnailPolicy.adaptiveLargeFolders);

    changed |= ImGui::InputInt("Large folder threshold", &_thumbnailPolicy.largeFolderThreshold, 10, 100);
    changed |= ImGui::InputInt("Thumbnail load budget", &_thumbnailPolicy.thumbnailLoadBudget, 1, 4);
    changed |= ImGui::InputInt("Large folder budget", &_thumbnailPolicy.largeFolderThumbnailLoadBudget, 1, 4);

    _thumbnailPolicy.largeFolderThreshold = clamp(_thumbnailPolicy.largeFolderThreshold, 1, 10000);
    _thumbnailPolicy.thumbnailLoadBudget = clamp(_thumbnailPolicy.thumbnailLoadBudget, 1, 64);
    _thumbnailPolicy.largeFolderThumbnailLoadBudget = clamp(_thumbnailPolicy.largeFolderThumbnailLoadBudget, 0, 64);

    if (changed)
        Save_Settings();

    ImGui::EndPopup();
}

void Content_Browser::Draw_FolderTile(FolderNode& folder)
{
    ImGui::PushID(String::ToString(folder.fullPath).c_str());

    const ImTextureID iconId = Get_IconTextureId(IconType::Folder);
    if (iconId)
        ImGui::Image(iconId, ImVec2(_thumbnailSize, _thumbnailSize));
    else
        ImGui::Button("[Folder]", ImVec2(_thumbnailSize, _thumbnailSize));

    const bool isFocused = ImGui::IsItemFocused();
    const bool isSelected = _selectedFilePath == folder.fullPath;

    if (isSelected)
    {
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRect(itemMin, itemMax, IM_COL32(70, 130, 210, 255), 0.f, 0, 2.f);
    }

    if (isFocused && !isSelected)
        _selectedFilePath = folder.fullPath;

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        Open_Folder(&folder);

    Draw_FolderContextMenu(folder, "FolderTileContext");

    const string folderName = String::ToString(folder.name);
    DrawTileLabel(folderName, _thumbnailSize + 32.f);

    ImGui::PopID();
}

void Content_Browser::Open_Folder(FolderNode* folder)
{
    if (!folder)
        return;

    _currentFolder = folder;
    _selectedFilePath.clear();

    Expand_PathTo(folder->fullPath);

    if (!_suppressHistoryRecord)
        Record_FolderHistory(folder->fullPath);

    Save_Settings();
}

void Content_Browser::Open_File(const FileEntry& fileEntry)
{
    if (nullptr == EDITOR)
        return;

    const fs::path filePath = fileEntry.filePath;
    if (!Is_EffectMaterialPresetFile(filePath))
        return;

    string assetType = {};
    if (!fileEntry.guid.empty())
    {
        if (const AssetMeta* assetMeta = GAME->Find_AssetByGUID(fileEntry.guid))
            assetType = assetMeta->type;
    }

    if (!assetType.empty() && assetType != "EffectMaterial")
        return;

    const Shared<Editor_Window> materialWindow = EDITOR->Get_Window(L"Effect Material");
    const Shared<EffectMaterial_View> materialView = dynamic_pointer_cast<EffectMaterial_View>(materialWindow);
    if (nullptr == materialView)
        return;

    materialView->Open_Preset(fileEntry.filePath);
}

void Content_Browser::Draw_FolderContextMenu(const FolderNode& folder, const string& popupId)
{
    if (!ImGui::BeginPopupContextItem(popupId.c_str()))
        return;

    if (ImGui::MenuItem("탐색기에서 열기"))
    {
        const wstring absPath = fs::absolute(folder.fullPath).wstring();

        ShellExecuteW(
            nullptr,
            L"open",
            absPath.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL
        );
    }

    ImGui::EndPopup();
}

void Content_Browser::Draw_FileContextMenu(const wstring& filePath, const string& popupId)
{
    if (!ImGui::BeginPopupContextItem(popupId.c_str()))
        return;

    if (ImGui::MenuItem("탐색기에서 열기"))
    {
        const wstring absPath = fs::absolute(filePath).wstring();
        const wstring args = L"/select,\"" + absPath + L"\"";

        ShellExecuteW(
            nullptr,
            L"open",
            L"explorer.exe",
            args.c_str(),
            nullptr,
            SW_SHOWNORMAL
        );
    }

    ImGui::EndPopup();
}

Shared<Texture> Content_Browser::Get_FileThumbnail(const FileEntry& fileEntry, bool allowLoad)
{
    if (fileEntry.filePath.empty())
        return nullptr;

    const fs::path sourcePath(fileEntry.filePath);
    if (Is_TextureThumbnailCandidate(sourcePath))
    {
        if (!_thumbnailPolicy.textureThumbnails)
            return nullptr;

        if (Is_DdsFile(sourcePath) && !_thumbnailPolicy.showDdsThumbnails)
            return nullptr;
    }

    const auto cacheIt = _thumbnailCache.find(fileEntry.filePath);
    if (cacheIt != _thumbnailCache.end())
        return cacheIt->second;

    if (_noThumbnailPaths.contains(fileEntry.filePath))
        return nullptr;

    if (!allowLoad)
        return nullptr;

    if (_thumbnailLoadsThisFrame >= _thumbnailLoadBudgetThisFrame)
        return nullptr;

    fs::path thumbnailPath{};
    if (!fileEntry.guid.empty())
    {
        thumbnailPath = _resourceRoot / L"Thumbnails" / String::ToWString(fileEntry.guid + ".png");
        if (!fs::exists(thumbnailPath))
            thumbnailPath.clear();
    }

    if (thumbnailPath.empty())
    {
        if (!Is_ThumbnailFileExtension(sourcePath))
        {
            _noThumbnailPaths.insert(fileEntry.filePath);
            return nullptr;
        }

        thumbnailPath = sourcePath;
    }

    ++_thumbnailLoadsThisFrame;

    Shared<Texture> thumbnailTexture =
        Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), thumbnailPath.c_str(), 1);

    if (nullptr == thumbnailTexture)
    {
        _noThumbnailPaths.insert(fileEntry.filePath);
        return nullptr;
    }

    _thumbnailCache[fileEntry.filePath] = thumbnailTexture;
    return thumbnailTexture;
}

bool Content_Browser::Is_EffectMaterialPresetFile(const fs::path& filePath)
{
    return String::ToLowerCopy(filePath.generic_string()).ends_with(".effectmaterial.json");
}

bool Content_Browser::Is_LargeTextureFolder() const
{
    return ContentBrowserCore::Is_LargeTextureFolder(_currentFolder, _thumbnailPolicy);
}

int Content_Browser::Resolve_CurrentThumbnailLoadBudget() const
{
    return Resolve_ThumbnailLoadBudget(_currentFolder, _thumbnailPolicy);
}

void Content_Browser::Save_Settings() const
{
    Save_CurrentPathSetting(kContentBrowserSettingsPath, "effectEditor");
    Save_ThumbnailPolicySetting(kContentBrowserSettingsPath, "effectEditor", _thumbnailPolicy);
}

void Content_Browser::Load_Settings()
{
    Load_CurrentPathSetting(kContentBrowserSettingsPath, "effectEditor");
    Load_ThumbnailPolicySetting(kContentBrowserSettingsPath, "effectEditor", _thumbnailPolicy);
}

Content_Browser::IconType Content_Browser::Resolve_IconType(const fs::path& filePath) const
{
    const string extension = ToLowerCopy(String::ToString(filePath.extension().wstring()));

    if (extension == ".csv")
        return IconType::Csv;
    if (extension == ".json")
        return IconType::Json;
    if (extension == ".py")
        return IconType::Python;
    if (extension == ".xlsx")
        return IconType::Xlsx;

    return IconType::Json;
}

ImTextureID Content_Browser::Get_IconTextureId(IconType iconType) const
{
    const size_t iconIndex = static_cast<size_t>(iconType);
    if (iconIndex >= _iconFiles.size())
        return static_cast<ImTextureID>(0);

    return ToImTextureId(_iconFiles[iconIndex]);
}

bool Content_Browser::Can_GoParentFolder() const
{
    if (nullptr == _currentFolder || _rootFolder.fullPath.empty())
        return false;

    const fs::path currentPath(_currentFolder->fullPath);
    const fs::path rootPath(_rootFolder.fullPath);
    if (currentPath.empty() || currentPath == rootPath)
        return false;

    const fs::path parentPath = currentPath.parent_path();
    return !parentPath.empty() && parentPath != currentPath;
}

void Content_Browser::Go_ParentFolder()
{
    if (!Can_GoParentFolder())
        return;

    const fs::path parentPath = fs::path(_currentFolder->fullPath).parent_path();
    FolderNode* target = Find_FolderNode(_rootFolder, parentPath.wstring());
    if (!target)
        return;

    Open_Folder(target);
}

void Content_Browser::Go_BackFolder()
{
    if (!Can_GoBack())
        return;

    --_folderHistoryIndex;

    FolderNode* target = Find_FolderNode(_rootFolder, _folderHistory[_folderHistoryIndex]);
    if (!target)
        return;

    _suppressHistoryRecord = true;
    Open_Folder(target);
    _suppressHistoryRecord = false;
}

void Content_Browser::Go_ForwardFolder()
{
    if (!Can_GoForward())
        return;

    ++_folderHistoryIndex;

    FolderNode* target = Find_FolderNode(_rootFolder, _folderHistory[_folderHistoryIndex]);
    if (!target)
        return;

    _suppressHistoryRecord = true;
    Open_Folder(target);
    _suppressHistoryRecord = false;
}

void Content_Browser::Handle_SideButtonEvent()
{
    const bool isBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const bool isBrowserHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    if (!(isBrowserFocused || isBrowserHovered))
        return;

    if (ImGui::IsMouseClicked(3))
        Go_BackFolder();

    if (ImGui::IsMouseClicked(4))
        Go_ForwardFolder();
}

void Content_Browser::Handle_ThumbnailZoom()
{
    const bool isBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (!isBrowserFocused)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl || io.MouseWheel == 0.f)
        return;

    _thumbnailSize += io.MouseWheel * 8.f;
    _thumbnailSize = clamp(_thumbnailSize, 32.f, 128.f);
}

Shared<Content_Browser> Content_Browser::Create()
{
    return make_shared<Content_Browser>();
}

void Content_Browser::Free()
{
    __super::Free();
}

NS_END
