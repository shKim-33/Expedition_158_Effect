#include "pch.h"
#include "CommandHistory.h"

#include "EffectEditorInstance.h"
#include "ICommand.h"
#include "Notification_Manager.h"

NS_BEGIN(EffectEditor)

void CommandHistory::Execute(const Shared<ICommand>& command)
{
    if (!command)
        return;

    command->Execute();
    _undoStack.push_back(command);
    _redoStack.clear();

    if (_undoStack.size() > kMaxHistory)
        _undoStack.erase(_undoStack.begin());
}

void CommandHistory::Undo()
{
    if (_undoStack.empty())
        return;

    const Shared<ICommand> command = _undoStack.back();
    _undoStack.pop_back();
    command->Undo();
    _redoStack.push_back(command);

    if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(
            NotifyType::Info,
            "Undo : {}",
            command->Get_Description()
        );
    }
}

void CommandHistory::Redo()
{
    if (_redoStack.empty())
        return;

    const Shared<ICommand> command = _redoStack.back();
    _redoStack.pop_back();
    command->Redo();
    _undoStack.push_back(command);

    if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(
            NotifyType::Info,
            "Redo: {}",
            command->Get_Description()
        );
    }
}

void CommandHistory::Clear()
{
    _undoStack.clear();
    _redoStack.clear();
}

Unique<CommandHistory> CommandHistory::Create()
{
    return make_unique<CommandHistory>();
}

NS_END
