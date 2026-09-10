#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBufferCom_Instance abstract : public VIBuffer
{
public:
    VIBufferCom_Instance(const ComPtr<Device>& device, const ComPtr<Context>& context);
    VIBufferCom_Instance(const VIBufferCom_Instance& prototype);
    ~VIBufferCom_Instance() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Bind_Resources() override;
    HRESULT Render() override;

protected: //## Data::GPU
    ComPtr<Buffer> _instanceVB{};

protected: //## Data::InstanceDraw
    uint32 _instanceStride{};
    uint32 _instanceCount{};
    uint32 _verticesPerInstance{};

public:
    Shared<Component> Clone(void* arg) override = 0;
    void Free() override;
};

NS_END
