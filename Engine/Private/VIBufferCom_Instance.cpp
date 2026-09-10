#include "VIBufferCom_Instance.h"

VIBufferCom_Instance::VIBufferCom_Instance(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBuffer{ device, context }
{
}

VIBufferCom_Instance::VIBufferCom_Instance(const VIBufferCom_Instance& prototype)
    : VIBuffer{ prototype }
    , _instanceStride{ prototype._instanceStride }
    , _instanceCount{ prototype._instanceCount }
    , _verticesPerInstance{ prototype._verticesPerInstance }
{
}

HRESULT VIBufferCom_Instance::Initialize_Prototype()
{
    return S_OK;
}

HRESULT VIBufferCom_Instance::Initialize(void* arg)
{
    return S_OK;
}

HRESULT VIBufferCom_Instance::Bind_Resources()
{
    if (nullptr == _vb.Get() || nullptr == _instanceVB.Get())
    {
        LOG_WARN("VIBufferCom_Instance requires vertex buffer and instance buffer before binding.");
        return E_FAIL;
    }

    Buffer* vertexBuffers[] = {
        _vb.Get(),
        _instanceVB.Get(),
    };

    const uint32 vertexStrides[] = {
        _vertexStride,
        _instanceStride,
    };

    const uint32 offsets[] = {
        0,
        0,
    };

    _context->IASetVertexBuffers(0, _numVertexBuffers, vertexBuffers, vertexStrides, offsets);

    if (nullptr != _ib.Get() && _numIndices > 0)
        _context->IASetIndexBuffer(_ib.Get(), _indexStride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
    else
        _context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);

    _context->IASetPrimitiveTopology(_primitiveType);

    return S_OK;
}

HRESULT VIBufferCom_Instance::Render()
{
    if (_instanceCount == 0)
        return E_FAIL;

    if (nullptr != _ib.Get() && _numIndices > 0)
    {
        _context->DrawIndexedInstanced(_numIndices, _instanceCount, 0, 0, 0);
        return S_OK;
    }

    if (_verticesPerInstance == 0)
    {
        LOG_WARN("VIBufferCom_Instance requires verticesPerInstance for non-indexed instanced draw.");
        return E_FAIL;
    }

    _context->DrawInstanced(_verticesPerInstance, _instanceCount, 0, 0);

    return S_OK;
}

void VIBufferCom_Instance::Free()
{
    __super::Free();
}
