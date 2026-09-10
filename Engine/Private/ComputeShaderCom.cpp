#include "ComputeShaderCom.h"

ComputeShaderCom::ComputeShaderCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context }
{
}

ComputeShaderCom::ComputeShaderCom(const ComputeShaderCom& prototype)
    : Component{ prototype }
    , _computeShader{ prototype._computeShader }
{
}

ComputeShaderCom::~ComputeShaderCom()
{
    Free();
}

HRESULT ComputeShaderCom::Initialize_Prototype(const wchar_t* shaderFilePath, const char* entryPoint)
{
    if (nullptr == shaderFilePath || nullptr == entryPoint || '\0' == *entryPoint)
    {
        LOG_ERROR("ComputeShaderCom requires shader path and entry point.");
        return E_FAIL;
    }

    uint32 hlslFlag = 0;

#ifdef _DEBUG
    hlslFlag |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
    hlslFlag |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
#endif

    ComPtr<ID3DBlob> shaderBlob{};
    ComPtr<ID3DBlob> errorBlob{};

    HRESULT hr = D3DCompileFromFile(
        shaderFilePath,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        "cs_5_0",
        hlslFlag,
        0,
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );
    if (FAILED(hr))
    {
        if (errorBlob)
            LOG_ERROR("Compute shader compile failed: {}", static_cast<const char*>(errorBlob->GetBufferPointer()));

        LOG_ERROR("Failed to compile ComputeShaderCom: path='{}', entry='{}'", String::ToString(shaderFilePath), entryPoint);
        return E_FAIL;
    }

    hr = _device->CreateComputeShader(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        nullptr,
        _computeShader.GetAddressOf()
    );
    CHECK_FAILED(hr, E_FAIL);

    return S_OK;
}

HRESULT ComputeShaderCom::Initialize(void* arg)
{
    _boundSRVSlots.clear();
    _boundUAVSlots.clear();
    _boundConstantBufferSlots.clear();

    return S_OK;
}

HRESULT ComputeShaderCom::Bind_SRV(uint32 slot, ShaderResourceView* srv)
{
    if (slot >= kSrvSlotCount)
    {
        LOG_WARN("ComputeShaderCom SRV slot out of range. slot={}, max={}", slot, kSrvSlotCount - 1);
        return E_FAIL;
    }

    _context->CSSetShaderResources(slot, 1, &srv);

    if (nullptr != srv)
        Track_Slot(_boundSRVSlots, slot);

    return S_OK;
}

HRESULT ComputeShaderCom::Bind_UAV(uint32 slot, UnorderedAccessView* uav)
{
    if (slot >= kUavSlotCount)
    {
        LOG_WARN("ComputeShaderCom UAV slot out of range. slot={}, max={}", slot, kUavSlotCount - 1);
        return E_FAIL;
    }

    _context->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr);

    if (nullptr != uav)
        Track_Slot(_boundUAVSlots, slot);

    return S_OK;
}

HRESULT ComputeShaderCom::Bind_ConstantBuffer(uint32 slot, Buffer* buffer)
{
    if (slot >= kConstantBufferSlotCount)
    {
        LOG_WARN("ComputeShaderCom constant buffer slot out of range. slot={}, max={}", slot, kConstantBufferSlotCount - 1);
        return E_FAIL;
    }

    _context->CSSetConstantBuffers(slot, 1, &buffer);

    if (nullptr != buffer)
        Track_Slot(_boundConstantBufferSlots, slot);

    return S_OK;
}

HRESULT ComputeShaderCom::Dispatch(uint32 groupX, uint32 groupY, uint32 groupZ)
{
    CHECK_NULL(_computeShader, E_FAIL);

    if (groupX == 0 || groupY == 0 || groupZ == 0)
    {
        LOG_WARN("ComputeShaderCom dispatch failed: invalid group count. x={}, y={}, z={}", groupX, groupY, groupZ);
        return E_FAIL;
    }

    _context->CSSetShader(_computeShader.Get(), nullptr, 0);
    _context->Dispatch(groupX, groupY, groupZ);
    Clear_Bindings();

    return S_OK;
}

void ComputeShaderCom::Clear_Bindings()
{
    CHECK_NULL(_context);

    ShaderResourceView* nullSRV = nullptr;
    for (const auto slot : _boundSRVSlots)
        _context->CSSetShaderResources(slot, 1, &nullSRV);

    UnorderedAccessView* nullUAV = nullptr;
    for (const uint32 slot : _boundUAVSlots)
        _context->CSSetUnorderedAccessViews(slot, 1, &nullUAV, nullptr);

    Buffer* nullBuffer = nullptr;
    for (const uint32 slot : _boundConstantBufferSlots)
        _context->CSSetConstantBuffers(slot, 1, &nullBuffer);

    _context->CSSetShader(nullptr, nullptr, 0);

    _boundSRVSlots.clear();
    _boundUAVSlots.clear();
    _boundConstantBufferSlots.clear();
}

void ComputeShaderCom::Track_Slot(vector<uint32>& slots, uint32 slot)
{
    if (slots.end() == ranges::find(slots, slot))
        slots.push_back(slot);
}

Shared<ComputeShaderCom> ComputeShaderCom::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    const wchar_t* shaderFilePath,
    const char* entryPoint)
{
    auto instance = make_shared<ComputeShaderCom>(device, context);

    if (FAILED(instance->Initialize_Prototype(shaderFilePath, entryPoint)))
    {
        LOG_CRITICAL("Failed to Create : ComputeShaderCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> ComputeShaderCom::Clone(void* arg)
{
    auto instance = make_shared<ComputeShaderCom>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeShaderCom");
        MSG_BOX("Failed to Clone : ComputeShaderCom");
        return nullptr;
    }

    return instance;
}

void ComputeShaderCom::Free()
{
    Clear_Bindings();

    __super::Free();

    _computeShader.Reset();
}
