#include "ComputeStructuredBuffer.h"

ComputeStructuredBuffer::ComputeStructuredBuffer(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

ComputeStructuredBuffer::~ComputeStructuredBuffer()
{
    Free();
}

HRESULT ComputeStructuredBuffer::Initialize(uint32 elementSize, uint32 elementCount, const void* initialData, uint32 extraBindFlags)
{
    if (elementSize == 0 || elementCount == 0)
    {
        LOG_ERROR("Failed to initialize ComputeStructuredBuffer. elementSize={}, elementCount={}", elementSize, elementCount);
        return E_FAIL;
    }

    _elementSize = elementSize;
    _elementCount = elementCount;
    _extraBindFlags = extraBindFlags;

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = _elementSize * _elementCount;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | _extraBindFlags;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = _elementSize;

    D3D11_SUBRESOURCE_DATA initialSubResource{};
    initialSubResource.pSysMem = initialData;

    const D3D11_SUBRESOURCE_DATA* initialDataDesc =
        initialData != nullptr ? &initialSubResource : nullptr;

    CHECK_FAILED(_device->CreateBuffer(&bufferDesc, initialDataDesc, _buffer.GetAddressOf()), E_FAIL);
    CHECK_FAILED(Ready_Views(), E_FAIL);

    return S_OK;
}

HRESULT ComputeStructuredBuffer::Update_Data(const void* data, uint32 elementCount)
{
    if (nullptr == data || nullptr == _buffer || elementCount == 0 || elementCount > _elementCount)
        return E_FAIL;

    // D3D11_BOX는 월드 좌표/도형이 아니라 GPU resource 내부의 부분 갱신 범위다.
    // StructuredBuffer는 1D buffer이므로 left/right만 byte offset 범위로 쓰고,
    // top/bottom, front/back은 1칸짜리 영역으로 고정한다.
    // right는 마지막 byte index가 아니라 갱신 범위 끝 다음 byte offset이다.
    D3D11_BOX updateBox{};
    updateBox.left = 0;
    updateBox.right = _elementSize * elementCount;
    updateBox.top = 0;
    updateBox.bottom = 1;
    updateBox.front = 0;
    updateBox.back = 1;

    _context->UpdateSubresource(_buffer.Get(), 0, &updateBox, data, 0, 0);

    return S_OK;
}

HRESULT ComputeStructuredBuffer::Copy_From(ID3D11Resource* source)
{
    if (nullptr == source || nullptr == _buffer)
        return E_FAIL;

    _context->CopyResource(_buffer.Get(), source);

    return S_OK;
}

HRESULT ComputeStructuredBuffer::Read_Data(void* outData, uint32 elementCount)
{
    if (outData == nullptr || _buffer == nullptr || elementCount == 0 || elementCount > _elementCount)
        return E_FAIL;

    if (_readbackBuffer == nullptr)
    {
        D3D11_BUFFER_DESC readbackDesc{};
        readbackDesc.ByteWidth = _elementSize * _elementCount;
        readbackDesc.Usage = D3D11_USAGE_STAGING;
        readbackDesc.BindFlags = 0;
        readbackDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        readbackDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        readbackDesc.StructureByteStride = _elementSize;

        CHECK_FAILED(_device->CreateBuffer(&readbackDesc, nullptr, _readbackBuffer.GetAddressOf()), E_FAIL);
    }

    _context->CopyResource(_readbackBuffer.Get(), _buffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    CHECK_FAILED(_context->Map(_readbackBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped), E_FAIL);

    memcpy(outData, mapped.pData, static_cast<size_t>(_elementSize) * elementCount);

    _context->Unmap(_readbackBuffer.Get(), 0);
    return S_OK;
}

HRESULT ComputeStructuredBuffer::Ready_Views()
{
    // StructuredBuffer view는 typed format이 없으므로 DXGI_FORMAT_UNKNOWN을 사용한다.
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = _elementCount;

    CHECK_FAILED(_device->CreateShaderResourceView(_buffer.Get(), &srvDesc, _srv.GetAddressOf()), E_FAIL);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = _elementCount;

    CHECK_FAILED(_device->CreateUnorderedAccessView(_buffer.Get(), &uavDesc, _uav.GetAddressOf()), E_FAIL);

    return S_OK;
}

Shared<ComputeStructuredBuffer> ComputeStructuredBuffer::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    uint32 elementSize,
    uint32 elementCount,
    const void* initialData,
    uint32 extraBindFlags)
{
    auto instance = make_shared<ComputeStructuredBuffer>(device, context);

    if (FAILED(instance->Initialize(elementSize, elementCount, initialData, extraBindFlags)))
    {
        LOG_CRITICAL("Failed to Create : ComputeStructuredBuffer");
        return nullptr;
    }

    return instance;
}

void ComputeStructuredBuffer::Free()
{
    _readbackBuffer.Reset();
    _uav.Reset();
    _srv.Reset();
    _buffer.Reset();

    _context.Reset();
    _device.Reset();

    __super::Free();
}
