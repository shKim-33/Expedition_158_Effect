#pragma once

#include "Base.h"

NS_BEGIN(EffectEditor)

class ImGui_Manager final : public Base
{
public:
    ImGui_Manager() = default;
    ~ImGui_Manager() override;

public:
    HRESULT Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Update(float timeDelta);
    void Render();

private: //## Helper::ImGuiLifecycle
    void ShutDown();
    void ImGuiStyleSetting();
    static string Get_ClientResourcePath(const wchar_t* relativePath);

public:
    static Unique<ImGui_Manager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;
};

NS_END
