#pragma once

NS_BEGIN(Engine)
class ICommand;
NS_END

NS_BEGIN(EffectEditor)

class CommandHistory final
{
public:
    CommandHistory() = default;
    ~CommandHistory() = default;

public:
    void Execute(const Shared<ICommand>& command);
    void Undo();
    void Redo();

    void Clear();

public: //## Accessors
    bool CanUndo() const { return !_undoStack.empty(); }
    bool CanRedo() const { return !_redoStack.empty(); }

private: //## Static::History
    static constexpr size_t kMaxHistory{ 100 };

private: //## Data::History
    vector<Shared<ICommand>> _undoStack{};
    vector<Shared<ICommand>> _redoStack{};

public:
    static Unique<CommandHistory> Create();
};

NS_END
