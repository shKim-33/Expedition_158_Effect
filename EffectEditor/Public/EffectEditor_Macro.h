#pragma once

namespace EffectEditor
{
#define EDITOR GET_SINGLE(EffectEditorInstance)

#define NOTIFY(...) EffectEditorInstance::GetInstance()->Get_Notification()->Add_Notification(__VA_ARGS__)
#define NOTIFY_WARN(...) EffectEditorInstance::GetInstance()->Get_Notification()->Add_Notification_With_Type(NotifyType::Warning, __VA_ARGS__)

#define U8(str) (const char*)u8##str
}
