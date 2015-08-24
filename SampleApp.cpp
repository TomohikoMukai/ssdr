#include "SampleApp.h"
#include <algorithm>

#ifndef ELOG
#define ELOG( x, ... );  \
    fprintf( stderr, "[File:%s, Line:%d] "x"\n", __FILE__, __LINE__, ##__VA_ARGS__ ); \
    assert( false );
#endif

#define SAFE_RELEASE(p) if (p != nullptr) { p->Release(); p = nullptr; }

SampleApp* SampleApp::s_pThis = nullptr;

SampleApp::SampleApp(const Config& config)
    : hInstance(nullptr),
    hMainWnd(nullptr),
    driverType(D3D_DRIVER_TYPE_NULL),
    featureLevel(D3D_FEATURE_LEVEL_11_0),
    multiSampleCount(config.multiSampleCount),
    multiSampleQuality(config.multiSampleQuality),
    swapChainCount(config.swapChainCount),
    swapChainFormat(config.swapChainFormat),
    depthStencilFormat(config.depthStencilFormat),
    d3dDevice(nullptr),
    d3dDeviceContext(nullptr),
    swapChain(nullptr),
    renderTargetView(nullptr),
    depthStencilView(nullptr),
    renderTargetTexture(nullptr),
    depthStencilTexture(nullptr),
    renderTargetShaderResourceView(nullptr),
    depthStencilShaderResourceView(nullptr),
    wndWidth(config.width),
    wndHeight(config.height),
    wndTitle(config.title)
{
    clearColor[0] = config.clearColorR;
    clearColor[1] = config.clearColorG;
    clearColor[2] = config.clearColorB;
    clearColor[3] = config.clearColorA;

    aspectRatio = static_cast<float>(wndWidth) / static_cast<float>(wndHeight);
}


SampleApp::~SampleApp()
{
    TerminateApp();
}

bool SampleApp::OpenWindow()
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    s_pThis = this;

    WNDCLASSEX wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = wndTitle.c_str();
    wc.hIconSm = LoadIcon(hInst, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        ELOG("Error : RegisterClassEx() Failed.\n");
        return false;
    }
    hInstance = hInst;
    RECT rc = { 0, 0, wndWidth, wndHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    hMainWnd = CreateWindow(
        wndTitle.c_str(),
        wndTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        nullptr,
        hInst,
        nullptr
        );
    if (hMainWnd == NULL)
    {
        ELOG("Error : CreateWindow() Failed.");
        return false;
    }
    ShowWindow(hMainWnd, SW_SHOWNORMAL);
    return true;
}

void SampleApp::CloseWindow()
{
    if (hInstance != NULL)
    {
        UnregisterClass(wndTitle.c_str(), hInstance);
    }
    wndTitle.clear();
    s_pThis = nullptr;
}

bool SampleApp::InitializeD3D11()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(hMainWnd, &rc);
    UINT w = rc.right - rc.left;
    UINT h = rc.bottom - rc.top;
    wndWidth = w;
    wndHeight = h;

    aspectRatio = static_cast<float>(w) / static_cast<float>(h);

    UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTytpes = sizeof(driverTypes) / sizeof(driverTypes[0]);

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = sizeof(featureLevels) / sizeof(featureLevels[0]);

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(DXGI_SWAP_CHAIN_DESC));
    sd.BufferCount = swapChainCount;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = swapChainFormat;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    sd.OutputWindow = hMainWnd;
    sd.SampleDesc.Count = multiSampleCount;
    sd.SampleDesc.Quality = multiSampleQuality;
    sd.Windowed = TRUE;

    for (UINT idx = 0; idx < numDriverTytpes; ++idx)
    {
        driverType = driverTypes[idx];
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            driverType,
            NULL,
            createDeviceFlags,
            featureLevels,
            numFeatureLevels,
            D3D11_SDK_VERSION,
            &sd,
            &swapChain,
            &d3dDevice,
            &featureLevel,
            &d3dDeviceContext
            );

        if (SUCCEEDED(hr))
        {
            break;
        }
    }
    if (FAILED(hr))
    {
        ELOG("Error : D3D11CreateDeviceAndSwapChain() Failed.");
        return false;
    }
    hr = d3dDevice->CheckMultisampleQualityLevels(swapChainFormat, multiSampleCount, &multiSampleMaxQuality);
    if (FAILED(hr))
    {
        ELOG("Error : D3D11Device::CheckMultiSampleQualityLevels() Failed.");
        return false;
    }
    if (!CreateDefaultRenderTarget())
    {
        ELOG("Error : CreateDefaultRenderTarget() Failed.");
        return false;
    }
    if (!CreateDefaultDepthStencil())
    {
        ELOG("Error : CreateDefaultDepthStencil() Failed.");
        return false;
    }

    d3dDeviceContext->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(w);
    vp.Height = static_cast<float>(h);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    d3dDeviceContext->RSSetViewports(1, &vp);
    return OnInit();
}

void SampleApp::TerminateD3D11()
{
    if (d3dDeviceContext != nullptr)
    {
        d3dDeviceContext->ClearState();
        d3dDeviceContext->Flush();
    }
    OnTerminate();
    ReleaseDefaultRenderTarget();
    ReleaseDefaultDepthStencil();
    SAFE_RELEASE(swapChain);
    SAFE_RELEASE(d3dDeviceContext);
    SAFE_RELEASE(d3dDevice);
}

bool SampleApp::InitializeApp()
{
    if (!OpenWindow())
    {
        ELOG("Error : InitWnd() Failed.");
        return false;
    }
    if (!InitializeD3D11())
    {
        ELOG("Error : InitD3D11() Failed.");
        return false;
    }

    LARGE_INTEGER pcf;
    QueryPerformanceFrequency(&pcf);
    counterPerFrame = pcf.QuadPart / 30;
    QueryPerformanceCounter(&prevCounter);

    return true;
}

void SampleApp::TerminateApp()
{
    TerminateD3D11();
    CloseWindow();
}

bool SampleApp::CreateDefaultRenderTarget()
{
    HRESULT hr = S_OK;
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&renderTargetTexture));
    if (FAILED(hr))
    {
        ELOG("Error : IDXGISwapChain::GetBuffer() Failed");
        return false;
    }
    hr = d3dDevice->CreateRenderTargetView(renderTargetTexture, nullptr, &renderTargetView);
    if (FAILED(hr))
    {
        ELOG("Error : ID3D11Device::CreateRenderTargetView() Failed.");
        return false;
    }
    hr = d3dDevice->CreateShaderResourceView(renderTargetTexture, nullptr, &renderTargetShaderResourceView);
    if (FAILED(hr))
    {
        ELOG("Error : ID3D11Device::CreateShaderResourceView() Failed.");
        return false;
    }
    return true;
}

void SampleApp::ReleaseDefaultRenderTarget()
{
    SAFE_RELEASE(renderTargetShaderResourceView);
    SAFE_RELEASE(renderTargetView);
    SAFE_RELEASE(renderTargetTexture);
}

bool SampleApp::CreateDefaultDepthStencil()
{
    HRESULT hr = S_OK;

    DXGI_FORMAT textureFormat = depthStencilFormat;
    DXGI_FORMAT resourceFormat = depthStencilFormat;

    switch (depthStencilFormat)
    {
    case DXGI_FORMAT_D16_UNORM:
        textureFormat = DXGI_FORMAT_R16_TYPELESS;
        resourceFormat = DXGI_FORMAT_R16_UNORM;
        break;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        textureFormat = DXGI_FORMAT_R24G8_TYPELESS;
        resourceFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        break;
    case DXGI_FORMAT_D32_FLOAT:
        textureFormat = DXGI_FORMAT_R32_TYPELESS;
        resourceFormat = DXGI_FORMAT_R32_FLOAT;
        break;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        textureFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
        resourceFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        break;
    }

    D3D11_TEXTURE2D_DESC td;
    ZeroMemory(&td, sizeof(D3D11_TEXTURE2D_DESC));
    td.Width = wndWidth;
    td.Height = wndHeight;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = textureFormat;
    td.SampleDesc.Count = multiSampleCount;
    td.SampleDesc.Quality = multiSampleQuality;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;

    hr = d3dDevice->CreateTexture2D(&td, nullptr, &depthStencilTexture);
    if (FAILED(hr))
    {
        ELOG("Error : ID3D11Device::CreateTexture2D() Failed.");
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
    ZeroMemory(&dsvd, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
    dsvd.Format = depthStencilFormat;
    if (multiSampleCount == 0)
    {
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvd.Texture2D.MipSlice = 0;
    }
    else
    {
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
    }

    hr = d3dDevice->CreateDepthStencilView(depthStencilTexture, &dsvd, &depthStencilView);
    if (FAILED(hr))
    {
        ELOG("Error : ID3D11Device::CreateDepthStencilView() Failed.");
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
    ZeroMemory(&srvd, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    srvd.Format = resourceFormat;

    if (multiSampleCount == 0)
    {
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MostDetailedMip = 0;
        srvd.Texture2D.MipLevels = 1;
    }
    else
    {
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
    }

    hr = d3dDevice->CreateShaderResourceView(depthStencilTexture, &srvd, &depthStencilShaderResourceView);
    if (FAILED(hr))
    {
        ELOG("Error : ID3D11Device::CreateShaderResourceView() Failed.");
        return false;
    }
    return true;
}

void SampleApp::ReleaseDefaultDepthStencil()
{
    SAFE_RELEASE(depthStencilShaderResourceView);
    SAFE_RELEASE(depthStencilView);
    SAFE_RELEASE(depthStencilTexture);
}

bool SampleApp::OnInit()
{
    bool retval = true;
    std::for_each(object.begin(), object.end(), [&](Object::SharedPtr o)
    {
        if (!o->OnInit(d3dDevice, d3dDeviceContext, wndWidth, wndHeight))
        {
            ELOG("Error : Object::OnInit() Failed.");
            retval = false;
        }
    });
    return retval;
}

void SampleApp::OnTerminate()
{
    std::for_each(object.begin(), object.end(), [&](Object::SharedPtr o)
    {
        o->OnDestroy();
        o.reset();
    });
}

void SampleApp::OnRender()
{
    assert(renderTargetView != nullptr);
    assert(depthStencilView != nullptr);

    d3dDeviceContext->ClearRenderTargetView(renderTargetView, clearColor);
    d3dDeviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    std::for_each(object.begin(), object.end(), [&](Object::SharedPtr o)
    {
        o->OnRender(d3dDevice, d3dDeviceContext);
    });
    swapChain->Present(0, 0);
}

void SampleApp::OnResize(const UINT w, const UINT h)
{
    if (swapChain && d3dDeviceContext)
    {
        wndWidth = w;
        wndHeight = h;
        aspectRatio = static_cast<float>(w) / static_cast<float>(h);

        ID3D11RenderTargetView* pNull = nullptr;
        d3dDeviceContext->OMSetRenderTargets(1, &pNull, nullptr);
        ReleaseDefaultRenderTarget();
        ReleaseDefaultDepthStencil();

        HRESULT hr = S_OK;
        hr = swapChain->ResizeBuffers(swapChainCount, 0, 0, swapChainFormat, 0);
        if (FAILED(hr))
        {
            ELOG("Error : IDXGISwapChain::ResizeBuffer() Failed.");
        }

        if (!CreateDefaultRenderTarget())
        {
            ELOG("Error : CreateDefaultRenderTarget() Failed.");
        }
        if (!CreateDefaultDepthStencil())
        {
            ELOG("Error : CreateDefaultDepthStencil() Failed.");
        }
        d3dDeviceContext->OMSetRenderTargets(1, &renderTargetView, depthStencilView);

        D3D11_VIEWPORT vp;
        vp.Width = static_cast<float>(w);
        vp.Height = static_cast<float>(h);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        d3dDeviceContext->RSSetViewports(1, &vp);
    }
    std::for_each(object.begin(), object.end(), [&](Object::SharedPtr o)
    {
        o->OnResize(d3dDevice, d3dDeviceContext, w, h);
    });
}

void SampleApp::OnUpdate(LONGLONG elapsed)
{
    std::for_each(object.begin(), object.end(), [&](Object::SharedPtr o)
    {
        o->OnUpdate(d3dDevice, d3dDeviceContext, static_cast<float>(elapsed) / static_cast<float>(counterPerFrame * 30));
    });
}

int SampleApp::MainLoop()
{
    MSG msg = { 0 };

    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            LARGE_INTEGER pc;
            QueryPerformanceCounter(&pc);
            if (pc.QuadPart - prevCounter.QuadPart >= counterPerFrame)
            {
                LONGLONG elapsed = pc.QuadPart - prevCounter.QuadPart;
                prevCounter = pc;
                OnUpdate(elapsed);
                OnRender();
            }
        }
    }

    return (int)msg.wParam;
}

int SampleApp::Run()
{
    int retcode = -1;
    if (InitializeApp())
    {
        retcode = MainLoop();
    }
    TerminateApp();
    return retcode;
}

LRESULT CALLBACK SampleApp::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (msg)
    {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        s_pThis->OnResize(static_cast<UINT>(LOWORD(lp)), static_cast<UINT>(HIWORD(lp)));
        break;
    default:
        break;
    }
    return DefWindowProc(hWnd, msg, wp, lp);
}

void SampleApp::AddObject(Object::SharedPtr obj)
{
    if (std::find(object.begin(), object.end(), obj) == object.end())
    {
        object.push_back(obj);
    }
}
