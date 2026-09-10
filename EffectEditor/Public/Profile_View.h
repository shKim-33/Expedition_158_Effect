#pragma once

#include "Debug_Profile.h"
#include "Editor_Window.h"
#include "GameInstance.h"

NS_BEGIN(EffectEditor)

class Profile_View final : public Editor_Window
{
public:
    using clock = chrono::steady_clock;

    struct ScopeStats
    {
        string name{};
        double lastMs{ 0.0 };
        double avgMs{ 0.0 };
        double minMs{ 0.0 };
        double maxMs{ 0.0 };
        uint64 samples{ 0 };
        bool isEnabled{ true };
    };

    class Scope final
    {
    public:
        explicit Scope(const char* scopeName);
        ~Scope();

    private:
        bool _active{ false };
    };

public:
    Profile_View();
    ~Profile_View() override;

public:
    void Render() override;

public:
    static void Frame_Begin();
    static void Frame_End();
    static void Begin_Scope(const char* scopeName);
    static void End_Scope();
    static void Set_Frame_Time_External(double frameMs);
    static void Set_RenderCallCount_External(uint32 renderCallCount);

private: //## Types::Profiling
    struct ScopeRow
    {
        const ScopeStats* stats{ nullptr };
        double sortKey{ 0.0 };
    };

    struct ProfilerRuntime;

    struct ProcessMemory
    {
        size_t workingSetMb{ 0 };
        size_t privateBytesMb{ 0 };
    };

    struct ResourceMemory
    {
        DebugResourceMemorySummary engine{};
        uint32 effectPoolEntryCount{ 0 };
        uint32 effectPoolActiveCount{ 0 };
        uint32 effectPoolIdleCount{ 0 };
        bool hasData{ false };
    };

private: //## Static::Profiling
    static Profile_View* activeView;

private: //## Data::Profiling
    Unique<ProfilerRuntime> _runtime{ nullptr };
    clock::time_point _prevFrame{ clock::now() };

    bool _capture{ true };
    bool _sortByCost{ true };
    bool _showDisabled{ false };
    int _avgWindow{ 120 };
    int _maxScopeRows{ 64 };
    char _filter[128]{};

    double _frameMsLast{ 0.0 };
    double _frameMsAvg{ 0.0 };
    double _fpsLast{ 0.0 };
    double _fpsAvg{ 0.0 };
    uint32 _renderCallCount{ 0 };
    ProcessMemory _processMemory{};
    ResourceMemory _resourceMemory{};
    clock::time_point _lastTimingSampleTime{};
    clock::time_point _lastProcessMemorySampleTime{};
    clock::time_point _lastResourceMemorySampleTime{};
    bool _hasTimingSample{ false };
    bool _hasProcessMemorySample{ false };
    bool _hasResourceMemorySample{ false };
    size_t _peakWorkingSetMb{ 0 };
    size_t _peakPrivateBytesMb{ 0 };

private: //## Helper::Profiling
    void Draw_Toolbar();
    void Draw_Frame_Info();
    void Draw_Scopes();
    void Draw_Memory();
    void Draw_Resource_Memory();
    void Reset_Stats();
    void Prune_Dead_Scopes();
    vector<ScopeRow> Build_Scope_Rows() const;
    void Log_Snapshot() const;
    void Record_Scope_Sample(const string& scopeName, double elapsedMs);
    void Refresh_Memory_Samples();
    ProcessMemory Get_Process_Memory() const;
    ResourceMemory Get_Resource_Memory() const;

public:
    static Shared<Profile_View> Create();
};

NS_END
