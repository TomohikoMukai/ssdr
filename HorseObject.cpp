#include "HorseObject.h"
#include <d3dcompiler.h>
#include "util.h"
#include "SSDR.h"

using namespace DirectX;

HRESULT CompileShaderFromFile(std::wstring fileName, char* entryPoint, char* shaderModel, ID3DBlob*& blob)
{
    HRESULT hr = S_OK;
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#else
    dwShaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompileFromFile(
        fileName.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        shaderModel,
        dwShaderFlags,
        0,
        &blob,
        &errorBlob
        );
    if (FAILED(hr))
    {
        if (errorBlob != nullptr)
        {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
    }
    if (errorBlob != nullptr)
    {
        errorBlob->Release();
    }
    return hr;
}

D3D11_INPUT_ELEMENT_DESC HorseObject::customVertexLayout[5] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

HorseObject::HorseObject()
    : constantBufferPerFrame(nullptr),
    constantBufferSkinningBone(nullptr),
    constantBufferPerObj(nullptr),
    lightPosition(5.0f, 5.0f, 0.0f),
    inputLayout(nullptr),
    vertexShader(nullptr), pixelShader(nullptr), alphaBlendState(nullptr),
    vertexBuffer(nullptr), indexBuffer(nullptr), numVertices(0), numFaces(0),
    vertexBufferCPU(nullptr), srcVertexBufferCPU(nullptr), numFrames(0), frame(0)
{
}

HorseObject::~HorseObject()
{
}

HRESULT HorseObject::InitShader(ID3D11Device* device)
{
    HRESULT hr = S_OK;

    // alpha blend state
    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    for (int i = 0; i < 8; ++i)
    {
        blendDesc.RenderTarget[i].BlendEnable = TRUE;
        blendDesc.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    hr = device->CreateBlendState(&blendDesc, &alphaBlendState);
    if (FAILED(hr))
    {
        return hr;
    }

    // per-frame constant buffer
    D3D11_BUFFER_DESC bufferDesc;
    ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
    bufferDesc.ByteWidth = sizeof(ConstantBufferPerFrame);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    hr = device->CreateBuffer(&bufferDesc, nullptr, &constantBufferPerFrame);
    if (FAILED(hr))
    {
        assert(false && "ID3D11Device::CreateBuffer() Failed.");
        return false;
    }

    // matrix palette
    ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
    bufferDesc.ByteWidth = sizeof(ConstantBufferSkinningMatrix);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    hr = device->CreateBuffer(&bufferDesc, nullptr, &constantBufferSkinningBone);
    if (FAILED(hr))
    {
        assert(false && "ID3D11Device::CreateBuffer() Failed.");
        return hr;
    }

    // per-object constant buffer
    ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
    bufferDesc.ByteWidth = sizeof(ConstantBufferPerObj);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    hr = device->CreateBuffer(&bufferDesc, nullptr, &constantBufferPerObj);
    if (FAILED(hr))
    {
        assert(false && "ID3D11Device::CreateBuffer() Failed.");
        return hr;
    }

    // vertex shader
    ID3DBlob* blob = nullptr;
    hr = CompileShaderFromFile(L"./ssdr.fx", "LambertSkinVS", "vs_5_0", blob);
    if (FAILED(hr))
    {
        if (blob != nullptr)
        {
            blob->Release();
        }
        return hr;
    }
    hr = device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vertexShader);
    if (FAILED(hr))
    {
        assert(false && "CompileShaderFromFile() Failed");
        return false;
    }

    // vertex layout
    UINT numElements = ARRAYSIZE(customVertexLayout);
    hr = device->CreateInputLayout(customVertexLayout, numElements, blob->GetBufferPointer(), blob->GetBufferSize(), &inputLayout);
    if (blob != nullptr)
    {
        blob->Release();
    }
    if (FAILED(hr))
    {
        assert(false && "ID3D11Device::CreateInputLayout() Failed.");
        return false;
    }

    // pixel shader
    hr = CompileShaderFromFile(L"./ssdr.fx", "LambertPS", "ps_5_0", blob);
    if (FAILED(hr))
    {
        if (blob != nullptr)
        {
            blob->Release();
        }
        return hr;
    }
    hr = device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &pixelShader);
    if (FAILED(hr))
    {
        assert(false && "ID3D11Device::CreateVertexShader() Failed.");
        if (blob != nullptr)
        {
            blob->Release();
        }
        return hr;
    }
    if (blob != nullptr)
    {
        blob->Release();
    }
    return hr;
}

HRESULT HorseObject::LoadModel(ID3D11Device* device, const wchar_t* filePath, const XMFLOAT4A& color)
{
    HRESULT hr = S_OK;

    std::vector<XMFLOAT3A> position, normal;
    std::vector<DWORD> index;
    LoadObjFile(position, index, filePath, 1.0f);
    ComputeNormal(normal, position, index);

    numVertices = static_cast<DWORD>(position.size());
    numFaces = static_cast<DWORD>(index.size() / 3);

    D3D11_BUFFER_DESC bd;
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&bd, sizeof(D3D11_BUFFER_DESC));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(CustomVertex) * numVertices;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
    vertexBufferCPU = new CustomVertex[numVertices];
    ZeroMemory(vertexBufferCPU, sizeof(CustomVertex) * numVertices);
    srcVertexBufferCPU = new CustomVertex[numVertices];
    ZeroMemory(srcVertexBufferCPU, sizeof(CustomVertex) * numVertices);
    initData.pSysMem = vertexBufferCPU;
    for (unsigned long v = 0; v < numVertices; ++v)
    {
        vertexBufferCPU[v].position = position[v];
        vertexBufferCPU[v].normal = normal[v];
        srcVertexBufferCPU[v].normal = normal[v];
        for (int i = 0; i < CustomVertex::NumInfluences; ++i)
        {
            vertexBufferCPU[v].indices[i] = 0;
            vertexBufferCPU[v].weight[i] = 0;
            srcVertexBufferCPU[v].indices[i] = 0;
            srcVertexBufferCPU[v].weight[i] = 0;
        }
        vertexBufferCPU[v].color = color;
        srcVertexBufferCPU[v].color = XMFLOAT4A(color.x, color.y, color.z, 0.5f);
        vertexBufferCPU[v].weight[0] = 1.0f;
    }

    hr = device->CreateBuffer(&bd, &initData, &vertexBuffer);
    if (FAILED(hr))
    {
        assert(false && "ID3D11Device::CreateBuffer() Failed.");
        return false;
    }

    ZeroMemory(&bd, sizeof(D3D11_BUFFER_DESC));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(DWORD)* numFaces * 3;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;

    ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
    std::unique_ptr<DWORD[]> indexBuf(new DWORD[numFaces * 3]);
    initData.pSysMem = indexBuf.get();
    for (unsigned long v = 0; v < numFaces * 3; ++v)
    {
        indexBuf[v] = index[v];
    }
    hr = device->CreateBuffer(&bd, &initData, &indexBuffer);
    if (FAILED(hr))
    {
        assert(false && "ID3D11Device::CreateBuffer() Failed.");
        return false;
    }

    return hr;
}

bool HorseObject::LoadAnim(const wchar_t* filePath)
{
    bool retval = true;
    wchar_t pathBuf[1024];
    numFrames = 1;
    for (;; ++numFrames)
    {
        swprintf_s(pathBuf, filePath, numFrames);
        if (GetFileAttributesW(pathBuf) == -1)
        {
            --numFrames;
            break;
        }
    }
    vertexAnim.resize(numFrames * numVertices);

    std::vector<XMFLOAT3A> position, normal;
    std::vector<DWORD> index;
    for (unsigned long f = 0; f < numFrames; ++f)
    {
        swprintf_s(pathBuf, filePath, f + 1);
        retval = LoadObjFile(position, index, pathBuf, 1.0f);
        if (!retval)
        {
            break;
        }
        for (unsigned long v = 0; v < numVertices; ++v)
        {
            vertexAnim[f * numVertices + v] = position[v];
        }
    }
    if (!retval)
    {
        vertexAnim.clear();
    }
    return retval;
}

bool HorseObject::OnInit(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const UINT width, const UINT height)
{
    HRESULT hr = InitShader(device);
    if (FAILED(hr))
    {
        assert(false && "HorseObject::InitShader() Failed.");
        return false;
    }

    // projection matrix
    FLOAT aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.001f, 1000.0f);
    XMStoreFloat4x4A(&perFrameData.proj, XMMatrixTranspose(projMatrix));

    LoadModel(device, L"./data/horse-gallop-reference.obj", XMFLOAT4A(0.5f, 0.5f, 0.5f, 1.0f));
    LoadAnim(L"./data/horse-gallop-%02d.obj");

    // SSDR
    //
    SSDR::Input ssdrIn;
    ssdrIn.numVertices = numVertices;
    ssdrIn.numExamples = numFrames;
    ssdrIn.bindModel.resize(numVertices);
    for (unsigned long v = 0; v < numVertices; ++v)
    {
        ssdrIn.bindModel[v] = vertexBufferCPU[v].position;
    }
    ssdrIn.sample.resize(numFrames * numVertices);
    for (unsigned long s = 0; s < numFrames; ++s)
    {
        for (unsigned long v = 0; v < numVertices; ++v)
        {
            ssdrIn.sample[s * numVertices + v] = vertexAnim[s * numVertices + v];
        }
    }

    SSDR::Parameter ssdrParam;
    ssdrParam.numIndices = CustomVertex::NumInfluences;
    ssdrParam.numMinBones = 16;
    ssdrParam.numMaxIterations = 30;

    SSDR::Output ssdrOut;
    SSDR::Decompose(ssdrOut, ssdrIn, ssdrParam);

    for (unsigned long v = 0; v < numVertices; ++v)
    {
        vertexBufferCPU[v].position = ssdrIn.bindModel[v];
        for (int i = 0; i < CustomVertex::NumInfluences; ++i)
        {
            vertexBufferCPU[v].indices[i] = ssdrOut.index[v * ssdrParam.numIndices + i];
            vertexBufferCPU[v].weight[i] = ssdrOut.weight[v * ssdrParam.numIndices + i];
        }
    }
    deviceContext->UpdateSubresource(vertexBuffer, 0, nullptr, vertexBufferCPU, 0, 0);

    numBones = ssdrOut.numBones;
    boneAnim.resize(ssdrIn.numExamples * ssdrOut.numBones);
    for (int s = 0; s < ssdrIn.numExamples; ++s)
    {
        for (int b = 0; b < ssdrOut.numBones; ++b)
        {
            boneAnim[s * ssdrOut.numBones + b] = ssdrOut.boneTrans[s * ssdrOut.numBones + b];
        }
    }

    fprintf(stderr, "%d\n", numBones);

    return Object::OnInit(device, deviceContext, width, height);
}

void HorseObject::OnResize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const UINT width, const UINT height)
{
    FLOAT aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.001f, 1000.0f);
    XMStoreFloat4x4A(&perFrameData.proj, XMMatrixTranspose(projMatrix));
}

void HorseObject::ColorVerticesByError(int frame, float upperBound)
{
    for (int v = 0; v < numVertices; ++v)
    {
        XMVECTOR pos = XMVectorZero();
        for (long i = 0; i < CustomVertex::NumInfluences; ++i)
        {
            const RigidTransform& rt = boneAnim[numBones * frame + vertexBufferCPU[v].indices[i]];
            XMVECTOR xv = XMLoadFloat3A(&vertexBufferCPU[v].position);
            pos += vertexBufferCPU[v].weight[i] * rt.TransformCoord(xv);
        }
        XMVECTOR err = XMVector3Length(pos - XMLoadFloat3A(&vertexAnim[numVertices * frame + v]));
        float l = XMVectorGetX(err) / upperBound;
        if (l > 1.0f) l = 1.0f;
        vertexBufferCPU[v].color = XMFLOAT4A(1.0f, 1.0f - l, 1.0f - l, vertexBufferCPU[v].color.w);
    }
}

void HorseObject::OnUpdate(ID3D11Device* device, ID3D11DeviceContext* deviceContext, float elapsed)
{
    frame = (frame + 1) % numFrames;

    // per-frame constant buffer
    //
    // view matrix
    XMVECTOR cameraPos = XMVectorSet(1.3f, 0.5f, 0, 1.0f);
    XMVECTOR cameraTgt = XMVectorSet(0, 0.5f, 0, 1.0f);
    XMVECTOR cameraUp = XMVectorSet(0, 1.0f, 0, 1.0f);
    XMMATRIX viewMatrix = XMMatrixLookAtLH(cameraPos, cameraTgt, cameraUp);
    XMStoreFloat4x4A(&perFrameData.view, XMMatrixTranspose(viewMatrix));
    // light color
    perFrameData.lightColor = XMFLOAT4A(1.0f, 1.0f, 1.0f, 1.0f);
    deviceContext->UpdateSubresource(constantBufferPerFrame, 0, nullptr, &perFrameData, 0, 0);

    ConstantBufferSkinningMatrix cbs;
    for (int i = 0; i < numBones; ++i)
    {
        XMFLOAT4X4A m = boneAnim[frame * numBones + i].ToMatrix4x4();
        XMStoreFloat4x4A(cbs.pallet + i, XMMatrixTranspose(XMLoadFloat4x4A(&m)));
    }
    XMStoreFloat4x4A(cbs.pallet + numBones, XMMatrixIdentity());
    deviceContext->UpdateSubresource(constantBufferSkinningBone, 0, nullptr, &cbs, 0, 0);

    ColorVerticesByError(frame, 0.05f);

    ConstantBufferPerObj cbo;
    XMMATRIX invModelTransform = XMMatrixIdentity();
    XMStoreFloat4x4(&cbo.world, XMMatrixTranspose(invModelTransform));
    XMStoreFloat3A(&cbo.localEyePos, XMVector3TransformCoord(cameraPos, invModelTransform));
    XMStoreFloat3A(&cbo.localLightPos, XMVector3TransformCoord(XMVectorSetW(XMLoadFloat3A(&lightPosition), 1.0f), invModelTransform));
    cbo.ambientColor = XMFLOAT4A(0.4f, 0.4f, 0.4f, 1.0f);
    cbo.specExpon = 1.0f;
    deviceContext->UpdateSubresource(constantBufferPerObj, 0, nullptr, &cbo, 0, 0);

    for (unsigned long v = 0; v < numVertices; ++v)
    {
        srcVertexBufferCPU[v].position = vertexAnim[frame * numVertices + v];
        srcVertexBufferCPU[v].indices[0] = numBones;
        srcVertexBufferCPU[v].weight[0] = 1.0;
    }

    Object::OnUpdate(device, deviceContext, elapsed);
}

void HorseObject::OnRender(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    // per-frame constant buffer
    deviceContext->VSSetConstantBuffers(0, 1, &constantBufferPerFrame);
    deviceContext->PSSetConstantBuffers(0, 1, &constantBufferPerFrame);

    // matrix palette
    deviceContext->VSSetConstantBuffers(2, 1, &constantBufferSkinningBone);
    deviceContext->PSSetConstantBuffers(2, 1, &constantBufferSkinningBone);

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    deviceContext->OMSetBlendState(alphaBlendState, blendFactor, 0xffffffff);
    deviceContext->IASetInputLayout(inputLayout);
    deviceContext->VSSetShader(vertexShader, nullptr, 0);
    deviceContext->GSSetShader(nullptr, nullptr, 0);
    deviceContext->PSSetShader(pixelShader, nullptr, 0);

    if (vertexBuffer != nullptr && indexBuffer != nullptr)
    {
        // per-object constant buffer
        deviceContext->VSSetConstantBuffers(1, 1, &constantBufferPerObj);
        deviceContext->PSSetConstantBuffers(1, 1, &constantBufferPerObj);

        // vertex & index buffer
        UINT stride = sizeof(CustomVertex);
        UINT offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        deviceContext->UpdateSubresource(vertexBuffer, 0, nullptr, vertexBufferCPU, 0, 0);
        deviceContext->DrawIndexed(numFaces * 3, 0, 0);

        //deviceContext->UpdateSubresource(vertexBuffer, 0, nullptr, srcVertexBufferCPU, 0, 0);
        //deviceContext->DrawIndexed(numFaces * 3, 0, 0);
    }

    Object::OnRender(device, deviceContext);
}

void HorseObject::OnDestroy()
{
    if (vertexBufferCPU != nullptr)
    {
        delete[] vertexBufferCPU;
        vertexBufferCPU = nullptr;
    }
    if (vertexBuffer != nullptr)
    {
        vertexBuffer->Release();
        vertexBuffer = nullptr;
    }
    if (indexBuffer != nullptr)
    {
        indexBuffer->Release();
        indexBuffer = nullptr;
    }
    if (constantBufferPerFrame != nullptr)
    {
        constantBufferPerFrame->Release();
        constantBufferPerFrame = nullptr;
    }
    if (constantBufferPerObj  != nullptr)
    {
        constantBufferPerObj->Release();
        constantBufferPerObj = nullptr;
    }
    if (constantBufferSkinningBone != nullptr)
    {
        constantBufferSkinningBone->Release();
        constantBufferSkinningBone = nullptr;
    }
    if (inputLayout != nullptr)
    {
        inputLayout->Release();
        inputLayout = nullptr;
    }
    if (vertexShader != nullptr)
    {
        vertexShader->Release();
        vertexShader = nullptr;
    }
    if (pixelShader != nullptr)
    {
        pixelShader->Release();
        pixelShader = nullptr;
    }
    if (alphaBlendState != nullptr)
    {
        alphaBlendState->Release();
        alphaBlendState = nullptr;
    }
    Object::OnDestroy();
}
