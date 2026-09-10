#include "Profile_View.h"

#include "ClientInstance.h"
#include "EffectInstancePool.h"

#include <psapi.h>

#pragma comment(lib, "psapi.lib")

NS_BEGIN(EffectEditor)

Profile_View* Profile_View::activeView = nullptr;

struct Profile_View::ProfilerRuntime
{
    struct ActiveScope
    {
        const char* name = nullptr;
        clock::time_point beginTime = {};
    };

    struct ScopeStatsInternal
    {
        ScopeStats ui = {};
        deque<double> window = {};
        double windowSum = 0.0;
        uint64 lastTouchedFrame = 0;
    };

    vector<ActiveScope> scopeStack = {};
    unordered_map<string, ScopeStatsInternal> scopes = {};

    uint64 frameIndex = 0;
    int avgWindow = 120;
    bool capture = true;

    bool externalFrameTime = false;
    double externalFrameMs = 0.0;
    bool timingSampleFrame = false;

    deque<double> frameWindow = {};
    double frameWindowSum = 0.0;
};

namespace
{
    constexpr auto kProfileSnapshotTag = "EffectEditorProfileSnapshot";

    const char* Get_Profile_Level_Name()
    {
#if ENGINE_PROFILE_LEVEL == ENGINE_PROFILE_LEVEL_OFF
        return "Off";
#elif ENGINE_PROFILE_LEVEL == ENGINE_PROFILE_LEVEL_BASIC
        return "Basic";
#elif ENGINE_PROFILE_LEVEL == ENGINE_PROFILE_LEVEL_DETAIL
        return "Detail";
#else
        return "Custom";
#endif
    }

    bool Str_IContains(const string& haystack, const char* needle)
    {
        if (nullptr == needle || '\0' == needle[0])
            return true;

        auto to_lower = [](unsigned char ch) { return static_cast<char>(tolower(ch)); };

        string lowerHaystack = {};
        lowerHaystack.reserve(haystack.size());
        for (const char ch : haystack)
            lowerHaystack.push_back(to_lower(static_cast<unsigned char>(ch)));

        string lowerNeedle = {};
        lowerNeedle.reserve(strlen(needle));
        for (const char* cursor = needle; '\0' != *cursor; ++cursor)
            lowerNeedle.push_back(to_lower(static_cast<unsigned char>(*cursor)));

        return string::npos != lowerHaystack.find(lowerNeedle);
    }

    bool Should_Refresh_Sample(
        const Profile_View::clock::time_point now,
        const Profile_View::clock::time_point lastSampleTime,
        const bool hasSample,
        const float intervalSec)
    {
        if (!hasSample)
            return true;

        if (intervalSec <= 0.f)
            return true;

        const float elapsedSec = static_cast<float>(
            chrono::duration<double>(now - lastSampleTime).count());
        return elapsedSec >= intervalSec;
    }

    double Bytes_To_Mb(const size_t bytes)
    {
        return static_cast<double>(bytes) / (1024.0 * 1024.0);
    }

    void Log_Profile_Snapshot_Line(const spdlog::source_loc& sourceLoc, const string& message)
    {
        GAME->Log_FileOnly(
            sourceLoc,
            spdlog::level::info,
            fmt::format("[{}] {}", kProfileSnapshotTag, message)
        );
    }
}

Profile_View::Scope::Scope(const char* scopeName)
{
#if ENGINE_PROFILE_LEVEL > ENGINE_PROFILE_LEVEL_OFF
    if (nullptr == scopeName || '\0' == scopeName[0])
        return;

    Begin_Scope(scopeName);
    _active = true;
#else
    UNREFERENCED_PARAMETER(scopeName);
#endif
}

Profile_View::Scope::~Scope()
{
#if ENGINE_PROFILE_LEVEL > ENGINE_PROFILE_LEVEL_OFF
    if (_active)
        End_Scope();
#endif
}

Profile_View::Profile_View()
    : Editor_Window{ L"Profile", ICON_FA_GAUGE }
{
    _runtime = make_unique<ProfilerRuntime>();
    _prevFrame = clock::now();
    activeView = this;
}

Profile_View::~Profile_View()
{
    if (activeView == this)
        activeView = nullptr;
}

void Profile_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        Draw_Toolbar();
        ImGui::Separator();
        Draw_Frame_Info();
        ImGui::Separator();
        Draw_Scopes();
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_BASIC
        Refresh_Memory_Samples();
        ImGui::Separator();
        Draw_Memory();
#endif
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Profile_View::Frame_Begin()
{
    if (nullptr == activeView || nullptr == activeView->_runtime)
        return;

    auto& runtime = *activeView->_runtime;
    const clock::time_point now = clock::now();
    runtime.frameIndex++;
    runtime.avgWindow = max(1, activeView->_avgWindow);
    runtime.timingSampleFrame = Should_Refresh_Sample(
        now,
        activeView->_lastTimingSampleTime,
        activeView->_hasTimingSample,
        ENGINE_PROFILE_TIMING_SAMPLE_INTERVAL_SEC
    );
    if (runtime.timingSampleFrame)
    {
        activeView->_lastTimingSampleTime = now;
        activeView->_hasTimingSample = true;
    }
    runtime.capture = activeView->_capture && activeView->Is_Open() && runtime.timingSampleFrame;
}

void Profile_View::Frame_End()
{
    if (nullptr == activeView || nullptr == activeView->_runtime)
        return;

    auto& runtime = *activeView->_runtime;
    const clock::time_point now = clock::now();

    double frameMs = 0.0;
    if (runtime.externalFrameTime)
    {
        frameMs = runtime.externalFrameMs;
        runtime.externalFrameTime = false;
        activeView->_prevFrame = now;
    }
    else
    {
        frameMs = chrono::duration<double, milli>(now - activeView->_prevFrame).count();
        activeView->_prevFrame = now;
    }

    if (runtime.timingSampleFrame)
    {
        activeView->_frameMsLast = frameMs;
        activeView->_fpsLast = frameMs > 0.0001 ? 1000.0 / frameMs : 0.0;

        runtime.frameWindow.push_back(frameMs);
        runtime.frameWindowSum += frameMs;

        while (static_cast<int>(runtime.frameWindow.size()) > runtime.avgWindow)
        {
            runtime.frameWindowSum -= runtime.frameWindow.front();
            runtime.frameWindow.pop_front();
        }

        activeView->_frameMsAvg = runtime.frameWindow.empty()
                                  ? 0.0
                                  : runtime.frameWindowSum / static_cast<double>(runtime.frameWindow.size());
        activeView->_fpsAvg = activeView->_frameMsAvg > 0.0001 ? 1000.0 / activeView->_frameMsAvg : 0.0;
    }

    vector<DebugProfileSample> debugSamples = {};
    Debug_Profile::Consume_Samples(debugSamples);

    if (runtime.capture)
    {
        unordered_map<string, double> debugSampleSums = {};
        for (const DebugProfileSample& sample : debugSamples)
            debugSampleSums[sample.name] += sample.elapsedMs;

        for (const auto& [scopeName, elapsedMs] : debugSampleSums)
            activeView->Record_Scope_Sample(scopeName, elapsedMs);
    }

    activeView->Prune_Dead_Scopes();
}

void Profile_View::Begin_Scope(const char* scopeName)
{
    if (nullptr == activeView || nullptr == activeView->_runtime)
        return;

    auto& runtime = *activeView->_runtime;

    if (!runtime.capture)
        return;

    ProfilerRuntime::ActiveScope activeScope = {};
    activeScope.name = scopeName;
    activeScope.beginTime = clock::now();
    runtime.scopeStack.push_back(activeScope);
}

void Profile_View::End_Scope()
{
    if (nullptr == activeView || nullptr == activeView->_runtime)
        return;

    auto& runtime = *activeView->_runtime;

    if (!runtime.capture || runtime.scopeStack.empty())
        return;

    const clock::time_point endTime = clock::now();
    const ProfilerRuntime::ActiveScope activeScope = runtime.scopeStack.back();
    runtime.scopeStack.pop_back();

    auto& entry = runtime.scopes[activeScope.name];
    auto& stats = entry.ui;

    const double elapsedMs = chrono::duration<double, milli>(endTime - activeScope.beginTime).count();

    if (0 == stats.samples)
    {
        stats.name = activeScope.name;
        stats.minMs = elapsedMs;
        stats.maxMs = elapsedMs;
        stats.isEnabled = true;
    }

    entry.lastTouchedFrame = runtime.frameIndex;

    if (!stats.isEnabled)
        return;

    stats.lastMs = elapsedMs;
    stats.minMs = min(stats.minMs, elapsedMs);
    stats.maxMs = max(stats.maxMs, elapsedMs);
    stats.samples++;

    entry.window.push_back(elapsedMs);
    entry.windowSum += elapsedMs;

    while (static_cast<int>(entry.window.size()) > runtime.avgWindow)
    {
        entry.windowSum -= entry.window.front();
        entry.window.pop_front();
    }

    stats.avgMs = entry.window.empty()
                  ? 0.0
                  : entry.windowSum / static_cast<double>(entry.window.size());
}

void Profile_View::Set_Frame_Time_External(double frameMs)
{
    if (nullptr == activeView || nullptr == activeView->_runtime)
        return;

    auto& runtime = *activeView->_runtime;
    runtime.externalFrameTime = true;
    runtime.externalFrameMs = frameMs;
}

void Profile_View::Set_RenderCallCount_External(uint32 renderCallCount)
{
    if (nullptr == activeView)
        return;

    activeView->_renderCallCount = renderCallCount;
}

void Profile_View::Draw_Toolbar()
{
    if (ImGui::Checkbox("Capture", &_capture))
    {
        if (_runtime)
            _runtime->capture = _capture;
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset"))
        Reset_Stats();

    ImGui::SameLine();
    if (ImGui::Button("Log Snapshot"))
        Log_Snapshot();

    ImGui::SameLine();
    ImGui::Checkbox("Sort by cost", &_sortByCost);

    ImGui::SameLine();
    ImGui::Checkbox("Show disabled", &_showDisabled);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputInt("AvgWindow", &_avgWindow);
    _avgWindow = max(1, min(_avgWindow, 1000));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.f);
    ImGui::InputText("Filter", _filter, IM_ARRAYSIZE(_filter));
}

void Profile_View::Draw_Frame_Info()
{
    ImGui::Text("Profile Level: %s (%d)", Get_Profile_Level_Name(), ENGINE_PROFILE_LEVEL);
    ImGui::Text("FPS (Last / Avg): %d / %d", static_cast<int>(_fpsLast), static_cast<int>(_fpsAvg));
    ImGui::Text("Frame ms (Last / Avg): %.2f / %.2f", static_cast<float>(_frameMsLast), static_cast<float>(_frameMsAvg));
    ImGui::Text("Render Calls: %u", _renderCallCount);
}

void Profile_View::Draw_Scopes()
{
    if (nullptr == _runtime)
        return;

    const vector<ScopeRow> rows = Build_Scope_Rows();

    if (ImGui::BeginTable("##ProfileScopes", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 36.f);
        ImGui::TableSetupColumn("Scope");
        ImGui::TableSetupColumn("Last (ms)");
        ImGui::TableSetupColumn("Avg (ms)");
        ImGui::TableSetupColumn("Min (ms)");
        ImGui::TableSetupColumn("Max (ms)");
        ImGui::TableHeadersRow();

        for (const ScopeRow& row : rows)
        {
            const ScopeStats& stats = *row.stats;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(stats.name.c_str());
            bool isEnabled = stats.isEnabled;
            if (ImGui::Checkbox("##enabled", &isEnabled))
            {
                auto iter = _runtime->scopes.find(stats.name);
                if (iter != _runtime->scopes.end())
                    iter->second.ui.isEnabled = isEnabled;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(stats.name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", static_cast<float>(stats.lastMs));

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.3f", static_cast<float>(stats.avgMs));

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.3f", static_cast<float>(stats.minMs));

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.3f", static_cast<float>(stats.maxMs));
        }

        ImGui::EndTable();
    }
}

void Profile_View::Draw_Memory()
{
    ImGui::Text("Memory");

    if (ImGui::BeginTable("MemoryTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Peak");
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Working Set");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%zu MB", _processMemory.workingSetMb);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%zu MB", _peakWorkingSetMb);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Private");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%zu MB", _processMemory.privateBytesMb);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%zu MB", _peakPrivateBytesMb);

        ImGui::EndTable();
    }

    ImGui::Separator();
    Draw_Resource_Memory();
}

void Profile_View::Draw_Resource_Memory()
{
    ImGui::Text("Resource Summary");

    if (!_resourceMemory.hasData)
    {
        ImGui::TextDisabled("Pending");
        return;
    }

    const DebugResourceMemorySummary& summary = _resourceMemory.engine;

    if (ImGui::BeginTable("ResourceMemoryTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Count");
        ImGui::TableSetupColumn("Estimated");
        ImGui::TableSetupColumn("Detail");
        ImGui::TableHeadersRow();

        auto drawRow = [](const char* category, const string& count, const string& estimated, const string& detail)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(category);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(count.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(estimated.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(detail.c_str());
        };

        drawRow(
            "Textures",
            fmt::format("{} prototypes", summary.texturePrototypeCount),
            fmt::format("{:.1f} MB", Bytes_To_Mb(summary.textureEstimatedBytes)),
            fmt::format("{} SRVs incl. materials", summary.textureSrvCount)
        );

        drawRow(
            "Models",
            fmt::format("{} prototypes", summary.modelPrototypeCount),
            fmt::format("{:.1f} MB", Bytes_To_Mb(summary.modelEstimatedBytes)),
            fmt::format(
                "{} meshes / {} materials / {} bones / {} anims",
                summary.modelMeshCount,
                summary.modelMaterialCount,
                summary.modelBoneCount,
                summary.modelAnimationCount
            )
        );

        drawRow(
            "Standalone Buffers",
            fmt::format("{} prototypes", summary.standaloneBufferPrototypeCount),
            fmt::format("{:.1f} MB", Bytes_To_Mb(summary.standaloneBufferEstimatedBytes)),
            "VIBuffer prototypes"
        );

        drawRow(
            "Objects",
            fmt::format("{} objects", summary.currentLevelObjectCount),
            "N/A",
            fmt::format("{} components", summary.currentLevelComponentCount)
        );

        drawRow(
            "Effect Pool",
            fmt::format("{} entries", _resourceMemory.effectPoolEntryCount),
            "N/A",
            fmt::format(
                "{} active / {} idle",
                _resourceMemory.effectPoolActiveCount,
                _resourceMemory.effectPoolIdleCount
            )
        );

        ImGui::EndTable();
    }
}

void Profile_View::Reset_Stats()
{
    if (nullptr == _runtime)
        return;

    for (auto& [_, entry] : _runtime->scopes)
    {
        ScopeStats& stats = entry.ui;
        stats.lastMs = 0.0;
        stats.avgMs = 0.0;
        stats.minMs = 0.0;
        stats.maxMs = 0.0;
        stats.samples = 0;

        entry.window.clear();
        entry.windowSum = 0.0;
        entry.lastTouchedFrame = _runtime->frameIndex;
    }

    _runtime->frameWindow.clear();
    _runtime->frameWindowSum = 0.0;
    _frameMsLast = 0.0;
    _frameMsAvg = 0.0;
    _fpsLast = 0.0;
    _fpsAvg = 0.0;
    _processMemory = {};
    _resourceMemory = {};
    _hasTimingSample = false;
    _hasProcessMemorySample = false;
    _hasResourceMemorySample = false;
    _peakWorkingSetMb = 0;
    _peakPrivateBytesMb = 0;
}

void Profile_View::Prune_Dead_Scopes()
{
    if (nullptr == _runtime)
        return;

    const uint64 keepFrames = static_cast<uint64>(max(60, _runtime->avgWindow * 10));

    for (auto iter = _runtime->scopes.begin(); iter != _runtime->scopes.end();)
    {
        const uint64 age = _runtime->frameIndex - iter->second.lastTouchedFrame;
        if (age > keepFrames)
            iter = _runtime->scopes.erase(iter);
        else
            ++iter;
    }
}

vector<Profile_View::ScopeRow> Profile_View::Build_Scope_Rows() const
{
    vector<ScopeRow> rows = {};
    if (nullptr == _runtime)
        return rows;

    rows.reserve(_runtime->scopes.size());

    for (const auto& [_, entry] : _runtime->scopes)
    {
        const ScopeStats& stats = entry.ui;

        if (!_showDisabled && !stats.isEnabled)
            continue;

        if (!Str_IContains(stats.name, _filter))
            continue;

        ScopeRow row = {};
        row.stats = &stats;
        row.sortKey = _sortByCost ? stats.avgMs : 0.0;
        rows.push_back(row);
    }

    if (_sortByCost)
    {
        ranges::sort(
            rows,
            [](const ScopeRow& lhs, const ScopeRow& rhs)
            {
                return lhs.sortKey > rhs.sortKey;
            }
        );
    }
    else
    {
        ranges::sort(
            rows,
            [](const ScopeRow& lhs, const ScopeRow& rhs)
            {
                return lhs.stats->name < rhs.stats->name;
            }
        );
    }

    if (static_cast<int>(rows.size()) > _maxScopeRows)
        rows.resize(static_cast<size_t>(_maxScopeRows));

    return rows;
}

void Profile_View::Log_Snapshot() const
{
    const spdlog::source_loc sourceLoc{ __FILE__, __LINE__, SPDLOG_FUNCTION };
    const vector<ScopeRow> rows = Build_Scope_Rows();
    const string filterText = '\0' == _filter[0] ? string("<none>") : string(_filter);

    Log_Profile_Snapshot_Line(sourceLoc, "----- begin -----");
    Log_Profile_Snapshot_Line(
        sourceLoc,
        fmt::format(
            "frame profile_level={}({}) fps_last={} fps_avg={} frame_ms_last={:.2f} frame_ms_avg={:.2f} render_calls={}",
            Get_Profile_Level_Name(),
            ENGINE_PROFILE_LEVEL,
            static_cast<int>(_fpsLast),
            static_cast<int>(_fpsAvg),
            _frameMsLast,
            _frameMsAvg,
            _renderCallCount
        )
    );
    Log_Profile_Snapshot_Line(
        sourceLoc,
        fmt::format(
            "toolbar capture={} sort_by_cost={} show_disabled={} avg_window={} filter=\"{}\" row_limit={}",
            _capture,
            _sortByCost,
            _showDisabled,
            _avgWindow,
            filterText,
            _maxScopeRows
        )
    );

    if (rows.empty())
        Log_Profile_Snapshot_Line(sourceLoc, "scopes empty_under_current_filters=true");
    else
    {
        Log_Profile_Snapshot_Line(sourceLoc, fmt::format("scopes visible_count={}", rows.size()));
        for (size_t index = 0; index < rows.size(); ++index)
        {
            const ScopeStats& stats = *rows[index].stats;
            Log_Profile_Snapshot_Line(
                sourceLoc,
                fmt::format(
                    "scope[{}] name=\"{}\" enabled={} last_ms={:.3f} avg_ms={:.3f} min_ms={:.3f} max_ms={:.3f} samples={}",
                    index,
                    stats.name,
                    stats.isEnabled,
                    stats.lastMs,
                    stats.avgMs,
                    stats.minMs,
                    stats.maxMs,
                    stats.samples
                )
            );
        }
    }

    if (_hasProcessMemorySample)
    {
        Log_Profile_Snapshot_Line(
            sourceLoc,
            fmt::format(
                "memory working_set_mb={} peak_working_set_mb={} private_mb={} peak_private_mb={}",
                _processMemory.workingSetMb,
                _peakWorkingSetMb,
                _processMemory.privateBytesMb,
                _peakPrivateBytesMb
            )
        );
    }
    else
        Log_Profile_Snapshot_Line(sourceLoc, "memory pending=true");

    if (_hasResourceMemorySample && _resourceMemory.hasData)
    {
        const DebugResourceMemorySummary& summary = _resourceMemory.engine;
        Log_Profile_Snapshot_Line(
            sourceLoc,
            fmt::format(
                "resource textures={} textures_mb={:.1f} texture_srvs={} "
                "models={} models_mb={:.1f} model_meshes={} model_materials={} model_bones={} model_anims={} "
                "standalone_buffers={} standalone_buffers_mb={:.1f} objects={} components={} "
                "effect_pool_entries={} effect_pool_active={} effect_pool_idle={}",
                summary.texturePrototypeCount,
                Bytes_To_Mb(summary.textureEstimatedBytes),
                summary.textureSrvCount,
                summary.modelPrototypeCount,
                Bytes_To_Mb(summary.modelEstimatedBytes),
                summary.modelMeshCount,
                summary.modelMaterialCount,
                summary.modelBoneCount,
                summary.modelAnimationCount,
                summary.standaloneBufferPrototypeCount,
                Bytes_To_Mb(summary.standaloneBufferEstimatedBytes),
                summary.currentLevelObjectCount,
                summary.currentLevelComponentCount,
                _resourceMemory.effectPoolEntryCount,
                _resourceMemory.effectPoolActiveCount,
                _resourceMemory.effectPoolIdleCount
            )
        );
    }
    else if (_hasResourceMemorySample)
        Log_Profile_Snapshot_Line(sourceLoc, "resource pending=false has_data=false");
    else
        Log_Profile_Snapshot_Line(sourceLoc, "resource pending=true");

    Log_Profile_Snapshot_Line(sourceLoc, "----- end -----");
}

void Profile_View::Record_Scope_Sample(const string& scopeName, double elapsedMs)
{
    if (nullptr == _runtime || scopeName.empty())
        return;

    auto& runtime = *_runtime;

    if (!runtime.capture)
        return;

    auto& entry = runtime.scopes[scopeName];
    auto& stats = entry.ui;

    if (0 == stats.samples)
    {
        stats.name = scopeName;
        stats.minMs = elapsedMs;
        stats.maxMs = elapsedMs;
        stats.isEnabled = true;
    }

    entry.lastTouchedFrame = runtime.frameIndex;

    if (!stats.isEnabled)
        return;

    stats.lastMs = elapsedMs;
    stats.minMs = min(stats.minMs, elapsedMs);
    stats.maxMs = max(stats.maxMs, elapsedMs);
    stats.samples++;

    entry.window.push_back(elapsedMs);
    entry.windowSum += elapsedMs;

    while (static_cast<int>(entry.window.size()) > runtime.avgWindow)
    {
        entry.windowSum -= entry.window.front();
        entry.window.pop_front();
    }

    stats.avgMs = entry.window.empty()
                  ? 0.0
                  : entry.windowSum / static_cast<double>(entry.window.size());
}

void Profile_View::Refresh_Memory_Samples()
{
    const clock::time_point now = clock::now();

    if (Should_Refresh_Sample(
        now,
        _lastProcessMemorySampleTime,
        _hasProcessMemorySample,
        ENGINE_PROFILE_PROCESS_MEMORY_SAMPLE_INTERVAL_SEC
    ))
    {
        _processMemory = Get_Process_Memory();
        _peakWorkingSetMb = max(_peakWorkingSetMb, _processMemory.workingSetMb);
        _peakPrivateBytesMb = max(_peakPrivateBytesMb, _processMemory.privateBytesMb);
        _lastProcessMemorySampleTime = now;
        _hasProcessMemorySample = true;
    }

    if (Should_Refresh_Sample(
        now,
        _lastResourceMemorySampleTime,
        _hasResourceMemorySample,
        ENGINE_PROFILE_RESOURCE_MEMORY_SAMPLE_INTERVAL_SEC
    ))
    {
        _resourceMemory = Get_Resource_Memory();
        _lastResourceMemorySampleTime = now;
        _hasResourceMemorySample = true;
    }
}

Profile_View::ProcessMemory Profile_View::Get_Process_Memory() const
{
    PROCESS_MEMORY_COUNTERS_EX processMemoryCounters = {};
    GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&processMemoryCounters),
        sizeof(processMemoryCounters)
    );

    ProcessMemory result = {};
    result.workingSetMb = processMemoryCounters.WorkingSetSize / (1024 * 1024);
    result.privateBytesMb = processMemoryCounters.PrivateUsage / (1024 * 1024);
    return result;
}

Profile_View::ResourceMemory Profile_View::Get_Resource_Memory() const
{
    ResourceMemory result{};
    result.hasData = GAME->Build_DebugResourceMemorySummary(result.engine);

    const Shared<ClientInstance>& client = ClientInstance::GetInstance();
    if (client && client->Get_EffectInstancePool())
    {
        const EffectInstancePool::DebugSummary poolSummary =
            client->Get_EffectInstancePool()->Get_DebugSummary();
        result.effectPoolEntryCount = poolSummary.entryCount;
        result.effectPoolActiveCount = poolSummary.activeCount;
        result.effectPoolIdleCount = poolSummary.idleCount;
    }

    return result;
}

Shared<Profile_View> Profile_View::Create()
{
    return make_shared<Profile_View>();
}

NS_END
