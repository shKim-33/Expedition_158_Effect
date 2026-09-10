#pragma once

#include "ICommand.h"

NS_BEGIN(EffectEditor)

class Action_Command final : public ICommand
{
public:
    Action_Command(function<void()> onUndo, function<void()> onRedo, string desc = "Action");
    ~Action_Command() override = default;

public:
    void Execute() override;
    void Undo() override;
    void Redo() override;

    string Get_Description() const override { return _desc; }

private: //## Data::Command
    function<void()> _onUndo{};
    function<void()> _onRedo{};
    string _desc{};

public:
    static Shared<Action_Command> Create(function<void()> onUndo, function<void()> onRedo, string desc = "Action");
    void Free() override;
};

NS_END
