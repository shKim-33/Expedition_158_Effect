#pragma once

#ifdef new
#pragma push_macro("new")
#undef new
#define EFFECTEDITOR_RESTORE_NEW_MACRO
#endif

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <implot.h>

#include <ImGuizmo.h>
#include <imgui-node-editor/imgui_node_editor.h>

#ifdef EFFECTEDITOR_RESTORE_NEW_MACRO
#pragma pop_macro("new")
#undef EFFECTEDITOR_RESTORE_NEW_MACRO
#endif
