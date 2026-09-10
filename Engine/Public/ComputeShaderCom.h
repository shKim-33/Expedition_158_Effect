#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL ComputeShaderCom final : public Component
{
public:
    ComputeShaderCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ComputeShaderCom(const ComputeShaderCom& prototype);
    ~ComputeShaderCom() override;

public:
    HRESULT Initialize_Prototype(const wchar_t* shaderFilePath, const char* entryPoint);
    HRESULT Initialize(void* arg) override;

public: //## Behavior::Binding
    HRESULT Bind_SRV(uint32 slot, ShaderResourceView* srv);
    HRESULT Bind_UAV(uint32 slot, UnorderedAccessView* uav);
    HRESULT Bind_ConstantBuffer(uint32 slot, Buffer* buffer);
    HRESULT Dispatch(uint32 groupX, uint32 groupY, uint32 groupZ);
    void Clear_Bindings();

private: //## Static::D3DSlot
    static constexpr uint32 kSrvSlotCount{ D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT };
    static constexpr uint32 kUavSlotCount{ D3D11_PS_CS_UAV_REGISTER_COUNT };
    static constexpr uint32 kConstantBufferSlotCount{ D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT };

private: //## Data::Shader
    ComPtr<ComputeShader> _computeShader{};

private: //## Data::TrackedBindings
    vector<uint32> _boundSRVSlots{};
    vector<uint32> _boundUAVSlots{};
    vector<uint32> _boundConstantBufferSlots{};

private: //## Helper::Binding
    static void Track_Slot(vector<uint32>& slots, uint32 slot);

public:
    static Shared<ComputeShaderCom> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wchar_t* shaderFilePath,
        const char* entryPoint);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
