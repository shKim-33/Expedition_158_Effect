#include "Action_Command.h"

NS_BEGIN(EffectEditor)

Action_Command::Action_Command(function<void()> onUndo, function<void()> onRedo, string desc)
    : _onUndo{ move(onUndo) }
    , _onRedo{ move(onRedo) }
    , _desc{ move(desc) }
{
}

void Action_Command::Execute()
{
}

void Action_Command::Undo()
{
    if (_onUndo)
        _onUndo();
}

void Action_Command::Redo()
{
    if (_onRedo)
        _onRedo();
}

Shared<Action_Command> Action_Command::Create(function<void()> onUndo, function<void()> onRedo, string desc)
{
    return make_shared<Action_Command>(move(onUndo), move(onRedo), move(desc));
}

void Action_Command::Free()
{
    ICommand::Free();
}

NS_END
