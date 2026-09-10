#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL ComputeStructuredBuffer final : public Base
{
public:
    ComputeStructuredBuffer(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~ComputeStructuredBuffer() override;

public:
    HRESULT Initialize(
        uint32 elementSize,
        uint32 elementCount,
        const void* initialData = nullptr,
        uint32 extraBindFlags = 0);

public: //## Behavior::DataTransfer
    HRESULT Update_Data(const void* data, uint32 elementCount);
    HRESULT Copy_From(ID3D11Resource* source);
    HRESULT Read_Data(void* outData, uint32 elementCount);

public: //## Accessors::Resource
    Buffer* Get_Buffer() const { return _buffer.Get(); }
    ShaderResourceView* Get_SRV() const { return _srv.Get(); }
    UnorderedAccessView* Get_UAV() const { return _uav.Get(); }

public: //## Accessors::Layout
    uint32 Get_ElementSize() const { return _elementSize; }
    uint32 Get_ElementCount() const { return _elementCount; }

private: //## Data::Device
    ComPtr<Device> _device{};
    ComPtr<Context> _context{};

private: //## Data::Resource
    ComPtr<Buffer> _buffer{};
    ComPtr<ShaderResourceView> _srv{};
    ComPtr<UnorderedAccessView> _uav{};
    ComPtr<Buffer> _readbackBuffer{};

private: //## Data::Layout
    uint32 _elementSize{ 0 };
    uint32 _elementCount{ 0 };
    uint32 _extraBindFlags{ 0 };

private: //## Helper::Setup
    HRESULT Ready_Views();

public:
    static Shared<ComputeStructuredBuffer> Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        uint32 elementSize,
        uint32 elementCount,
        const void* initialData = nullptr,
        uint32 extraBindFlags = 0);
    void Free() override;
};

NS_END
