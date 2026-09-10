#pragma once

#include "Editor_Window.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class Emitter_View;

class HistoryBudget_View final : public Editor_Window
{
public:
    HistoryBudget_View();
    ~HistoryBudget_View() override = default;

public:
    void Update(float timeDelta) override;
    void Render() override;
    void Open_Target(uint32 emitterId);

private:
    Shared<Emitter_View> Get_EmitterView() const;
    void Draw_SourceGroupSummary(Emitter_View& emitterView);

private:
    optional<uint32> _pendingFocusEmitterId{};
    optional<uint32> _highlightEmitterId{};
    string _selectedSourceGroupId{};
    float _highlightTimer{ 0.f };

public:
    static Shared<HistoryBudget_View> Create();
};

NS_END
