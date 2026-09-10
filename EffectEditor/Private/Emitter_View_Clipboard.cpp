#include "Emitter_View.h"

#include "Action_Command.h"
#include "CurveEditor_View.h"
#include "Editor_Context.h"
#include "EffectAuthoringJsonSerializer.h"
#include "EffectAuthoringModuleMetadata.h"
#include "EffectAuthoring_Types.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialPresetReader.h"
#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"
#include "Helper_ImGui.h"
#include "Helper_String.h"
#include "Notification_Manager.h"

#include <ctime>
#include <iomanip>
#include <sstream>

NS_BEGIN(EffectEditor)

void Emitter_View::Copy_EmitterToClipboard(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    _clipboardKind = AuthoringClipboardKind::Emitter;
    _clipboardEmitter = _emitters[emitterIndex];
    _clipboardModule.reset();
}

void Emitter_View::Copy_ModuleToClipboard(size_t emitterIndex, size_t moduleIndex)
{
    if (emitterIndex >= _emitters.size() || moduleIndex >= _emitters[emitterIndex].modules.size())
        return;

    _clipboardKind = AuthoringClipboardKind::Module;
    _clipboardEmitter.reset();
    _clipboardModule = _emitters[emitterIndex].modules[moduleIndex];
}

bool Emitter_View::Can_PasteEmitterAfter(size_t emitterIndex, string* disabledReason) const
{
    const auto set_reason =
        [&disabledReason](const char* message)
    {
        if (disabledReason != nullptr)
            *disabledReason = message;
    };

    if (emitterIndex >= _emitters.size())
    {
        set_reason("대상 이미터가 없습니다.");
        return false;
    }

    if (_clipboardKind != AuthoringClipboardKind::Emitter || !_clipboardEmitter.has_value())
    {
        set_reason("복사된 이미터가 없습니다.");
        return false;
    }

    set_reason("");
    return true;
}

bool Emitter_View::Can_PasteEmitterToEnd(string* disabledReason) const
{
    const auto set_reason =
        [&disabledReason](const char* message)
    {
        if (disabledReason != nullptr)
            *disabledReason = message;
    };

    if (_clipboardKind != AuthoringClipboardKind::Emitter || !_clipboardEmitter.has_value())
    {
        set_reason("복사된 이미터가 없습니다.");
        return false;
    }

    set_reason("");
    return true;
}

bool Emitter_View::Can_PasteModuleInto(size_t emitterIndex, string* disabledReason) const
{
    const auto set_reason =
        [&disabledReason](const char* message)
    {
        if (disabledReason != nullptr)
            *disabledReason = message;
    };

    if (emitterIndex >= _emitters.size())
    {
        set_reason("대상 이미터가 없습니다.");
        return false;
    }

    if (_clipboardKind != AuthoringClipboardKind::Module || !_clipboardModule.has_value())
    {
        set_reason("복사된 모듈이 없습니다.");
        return false;
    }

    const AuthoringModule& clipboardModule = _clipboardModule.value();
    if (clipboardModule.type == AuthoringModuleType::Required ||
        clipboardModule.type == AuthoringModuleType::Spawn)
    {
        set_reason("이 모듈은 새로 붙여넣을 수 없습니다.");
        return false;
    }

    const AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (!Can_AddModuleType(emitter, clipboardModule.type))
    {
        if (Has_ModuleType(emitter, clipboardModule.type))
            set_reason("같은 타입 모듈이 이미 있습니다.");
        else if (!Is_ModuleCompatibleWithEmitter(emitter, clipboardModule.type))
            set_reason("현재 TypeData와 호환되지 않습니다.");
        else
            set_reason("이 모듈은 여기에 붙여넣을 수 없습니다.");
        return false;
    }

    set_reason("");
    return true;
}

bool Emitter_View::Can_PasteModuleValues(size_t emitterIndex, size_t moduleIndex, string* disabledReason) const
{
    const auto set_reason =
        [&disabledReason](const char* message)
    {
        if (disabledReason != nullptr)
            *disabledReason = message;
    };

    if (emitterIndex >= _emitters.size() || moduleIndex >= _emitters[emitterIndex].modules.size())
    {
        set_reason("대상 모듈이 없습니다.");
        return false;
    }

    if (_clipboardKind != AuthoringClipboardKind::Module || !_clipboardModule.has_value())
    {
        set_reason("복사된 모듈이 없습니다.");
        return false;
    }

    const AuthoringModule& clipboardModule = _clipboardModule.value();
    const AuthoringModule& targetModule = _emitters[emitterIndex].modules[moduleIndex];
    if (clipboardModule.type != targetModule.type)
    {
        set_reason("같은 타입 모듈에서만 값 붙여넣기가 가능합니다.");
        return false;
    }

    if (targetModule.type == AuthoringModuleType::Spawn)
    {
        set_reason("Spawn 모듈 값 붙여넣기는 이번 범위에서 지원하지 않습니다.");
        return false;
    }

    set_reason("");
    return true;
}

void Emitter_View::Paste_EmitterAfter(size_t emitterIndex)
{
    if (!Can_PasteEmitterAfter(emitterIndex))
        return;

    Close_ModulePicker();
    const size_t insertIndex = emitterIndex + 1;
    _emitters.insert(
        _emitters.begin() + static_cast<ptrdiff_t>(insertIndex),
        Clone_Emitter(_clipboardEmitter.value())
    );
    Select_Emitter(insertIndex);
    Sanitize_PreviewTransformState();
    MarkDirty();
}

void Emitter_View::Paste_EmitterToEnd()
{
    if (!Can_PasteEmitterToEnd())
        return;

    Close_ModulePicker();
    const size_t insertIndex = _emitters.size();
    _emitters.push_back(Clone_Emitter(_clipboardEmitter.value()));
    Select_Emitter(insertIndex);
    Sanitize_PreviewTransformState();
    MarkDirty();
}

void Emitter_View::Paste_ModuleInto(size_t emitterIndex)
{
    if (!Can_PasteModuleInto(emitterIndex))
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    emitter.modules.push_back(Clone_Module(_clipboardModule.value()));
    emitter.previewDirty = true;
    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex = emitter.modules.size() - 1;
    Sync_SelectionContext();
    MarkDirty();
}

void Emitter_View::Paste_ModuleValues(size_t emitterIndex, size_t moduleIndex)
{
    if (!Can_PasteModuleValues(emitterIndex, moduleIndex))
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    AuthoringModule& targetModule = emitter.modules[moduleIndex];
    targetModule.data = _clipboardModule.value().data;
    Sync_EmitterMirror(emitter);
    emitter.previewDirty = true;
    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex = moduleIndex;
    Sync_SelectionContext();
    MarkDirty();
}

void Emitter_View::Notify_ClipboardStatus(const string& message, bool warning) const
{
    if (message.empty() || EDITOR == nullptr || EDITOR->Get_Notification() == nullptr)
        return;

    EDITOR->Get_Notification()->Add_Notification_With_Type(
        warning ? NotifyType::Warning : NotifyType::Info,
        "{}",
        message
    );
}

void Emitter_View::Handle_Shortcuts()
{
    if (!_isFocused)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        if (_selectedEmitterIndex.has_value() && !_selectedTypeData && _selectedModuleIndex.has_value())
        {
            Copy_ModuleToClipboard(_selectedEmitterIndex.value(), _selectedModuleIndex.value());
            Notify_ClipboardStatus("모듈을 클립보드에 복사했습니다.");
        }
        else if (_selectedEmitterIndex.has_value() && !_selectedTypeData)
        {
            Copy_EmitterToClipboard(_selectedEmitterIndex.value());
            Notify_ClipboardStatus("이미터를 클립보드에 복사했습니다.");
        }
        else
            Notify_ClipboardStatus("이미터 또는 모듈을 선택한 상태에서 복사할 수 있습니다.", true);

        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        if (!_selectedEmitterIndex.has_value())
        {
            Notify_ClipboardStatus("삭제할 이미터, 타입 데이터 또는 모듈을 선택해야 합니다.", true);
            return;
        }

        const size_t emitterIndex = _selectedEmitterIndex.value();
        if (_selectedTypeData)
        {
            Queue_TypeDataAction(PendingTypeDataAction::Remove, emitterIndex);
            return;
        }

        if (_selectedModuleIndex.has_value())
        {
            const size_t moduleIndex = _selectedModuleIndex.value();
            if (emitterIndex >= _emitters.size() || moduleIndex >= _emitters[emitterIndex].modules.size())
            {
                Notify_ClipboardStatus("삭제할 모듈 선택이 유효하지 않습니다.", true);
                return;
            }

            const AuthoringModule& module = _emitters[emitterIndex].modules[moduleIndex];
            if (!module.removable)
            {
                Notify_ClipboardStatus("이 모듈은 삭제할 수 없습니다.", true);
                return;
            }

            Queue_ModuleAction(PendingModuleAction::Delete, emitterIndex, moduleIndex);
            return;
        }

        Queue_EmitterAction(PendingEmitterAction::Delete, emitterIndex);
        return;
    }

    if (!io.KeyCtrl || !ImGui::IsKeyPressed(ImGuiKey_V, false))
        return;

    if (!_selectedEmitterIndex.has_value() || _selectedTypeData)
    {
        Notify_ClipboardStatus("이미터 헤더 또는 모듈을 선택한 상태에서 붙여넣을 수 있습니다.", true);
        return;
    }

    const size_t emitterIndex = _selectedEmitterIndex.value();
    if (_clipboardKind == AuthoringClipboardKind::Emitter)
    {
        if (_selectedModuleIndex.has_value())
        {
            Notify_ClipboardStatus("이미터 붙여넣기는 이미터 헤더 선택 상태에서만 지원합니다.", true);
            return;
        }

        string disabledReason{};
        if (!Can_PasteEmitterAfter(emitterIndex, &disabledReason))
        {
            Notify_ClipboardStatus(disabledReason, true);
            return;
        }

        Execute_AuthoringEdit(
            "Paste Emitter",
            [this, emitterIndex]
            {
                Paste_EmitterAfter(emitterIndex);
            }
        );
        return;
    }

    if (_clipboardKind == AuthoringClipboardKind::Module)
    {
        if (_selectedModuleIndex.has_value())
        {
            const size_t moduleIndex = _selectedModuleIndex.value();
            string valuePasteReason{};
            const bool canPasteValues = Can_PasteModuleValues(emitterIndex, moduleIndex, &valuePasteReason);
            if (canPasteValues)
            {
                Execute_AuthoringEdit(
                    "Paste Module Values",
                    [this, emitterIndex, moduleIndex]
                    {
                        Paste_ModuleValues(emitterIndex, moduleIndex);
                    }
                );
                return;
            }

            string modulePasteReason{};
            const bool canPasteModule = Can_PasteModuleInto(emitterIndex, &modulePasteReason);
            if (canPasteModule)
            {
                Execute_AuthoringEdit(
                    "Paste Module",
                    [this, emitterIndex]
                    {
                        Paste_ModuleInto(emitterIndex);
                    }
                );
                return;
            }

            const AuthoringModule* targetModule =
                moduleIndex < _emitters[emitterIndex].modules.size()
                ? &_emitters[emitterIndex].modules[moduleIndex]
                : nullptr;
            const AuthoringModuleType clipboardType =
                _clipboardModule.has_value()
                ? _clipboardModule->type
                : AuthoringModuleType::Required;
            const bool prefersValuePasteReason =
                targetModule != nullptr &&
                _clipboardModule.has_value() &&
                targetModule->type == clipboardType;
            Notify_ClipboardStatus(
                prefersValuePasteReason || modulePasteReason.empty()
                ? valuePasteReason
                : modulePasteReason,
                true
            );
            return;
        }

        string disabledReason{};
        if (!Can_PasteModuleInto(emitterIndex, &disabledReason))
        {
            Notify_ClipboardStatus(disabledReason, true);
            return;
        }

        Execute_AuthoringEdit(
            "Paste Module",
            [this, emitterIndex]
            {
                Paste_ModuleInto(emitterIndex);
            }
        );
        return;
    }

    Notify_ClipboardStatus("복사된 이미터 또는 모듈이 없습니다.", true);
}

void Emitter_View::Process_PendingClipboardAction()
{
    if (_pendingClipboardAction == PendingClipboardAction::None ||
        !_pendingClipboardEmitterIndex.has_value())
        return;

    const PendingClipboardAction action = _pendingClipboardAction;
    const size_t emitterIndex = _pendingClipboardEmitterIndex.value();
    const optional<size_t> moduleIndex = _pendingClipboardModuleIndex;

    switch (action)
    {
    case PendingClipboardAction::PasteEmitterAfter:
        Execute_AuthoringEdit(
            "Paste Emitter",
            [this, emitterIndex]
            {
                Paste_EmitterAfter(emitterIndex);
            }
        );
        break;

    case PendingClipboardAction::PasteEmitterToEnd:
        Execute_AuthoringEdit(
            "Paste Emitter",
            [this]
            {
                Paste_EmitterToEnd();
            }
        );
        break;

    case PendingClipboardAction::PasteModule:
        Execute_AuthoringEdit(
            "Paste Module",
            [this, emitterIndex]
            {
                Paste_ModuleInto(emitterIndex);
            }
        );
        break;

    case PendingClipboardAction::PasteModuleValues:
        if (moduleIndex.has_value())
        {
            Execute_AuthoringEdit(
                "Paste Module Values",
                [this, emitterIndex, moduleIndex]
                {
                    Paste_ModuleValues(emitterIndex, moduleIndex.value());
                }
            );
        }
        break;

    case PendingClipboardAction::None:
    default:
        break;
    }

    _pendingClipboardAction = PendingClipboardAction::None;
    _pendingClipboardEmitterIndex.reset();
    _pendingClipboardModuleIndex.reset();
}

void Emitter_View::Queue_ClipboardAction(
    PendingClipboardAction action,
    size_t emitterIndex,
    optional<size_t> moduleIndex)
{
    _pendingClipboardAction = action;
    _pendingClipboardEmitterIndex = emitterIndex;
    _pendingClipboardModuleIndex = moduleIndex;
}
NS_END
