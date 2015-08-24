#ifndef SAMPLE_APP_H
#define SAMPLE_APP_H
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include "Object.h"

class SampleApp
{
public:
    struct Config
    {
        UINT swapChainCount;
        DXGI_FORMAT swapChainFormat;
        DXGI_FORMAT depthStencilFormat;
        UINT multiSampleCount;
        UINT multiSampleQuality;
        UINT width;
        UINT height;
        wchar_t* title;
        FLOAT clearColorR;
        FLOAT clearColorG;
        FLOAT clearColorB;
        FLOAT clearColorA;

        Config()
            : swapChainCount(2),
            swapChainFormat(DXGI_FORMAT_R8G8B8A8_UNORM),
            depthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT),
            multiSampleCount(1),
            multiSampleQuality(0),
            width(1280),
            height(720),
            title(L"BasicApp"),
            clearColorR(0.392f),
            clearColorG(0.584f),
            clearColorB(0.929f),
            clearColorA(1.0f)
        {
        }
    };

//
// field
protected:
    static const UINT SWAPCHAIN_BUFFER_COUNT = 2;
    static const DXGI_FORMAT SWAPCHAIN_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
    static const DXGI_FORMAT DEPTHSTENCIL_TEXTURE_FORMAT = DXGI_FORMAT_R24G8_TYPELESS;
    static const DXGI_FORMAT DEPTHSTENCIL_VIEW_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;
    static const DXGI_FORMAT DEPTHSTENCIL_RESOURCE_FORMAT = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

    static SampleApp* s_pThis;
    HINSTANCE hInstance;
    HWND  hMainWnd;
    D3D_DRIVER_TYPE driverType;
    D3D_FEATURE_LEVEL featureLevel;
    UINT multiSampleCount;
    UINT multiSampleQuality;
    UINT multiSampleMaxQuality;
    UINT swapChainCount;
    DXGI_FORMAT swapChainFormat;
    DXGI_FORMAT depthStencilFormat;
    ID3D11Device* d3dDevice;
    ID3D11DeviceContext* d3dDeviceContext;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;
    ID3D11DepthStencilView* depthStencilView;
    ID3D11Texture2D* renderTargetTexture;
    ID3D11Texture2D* depthStencilTexture;
    ID3D11ShaderResourceView* renderTargetShaderResourceView;
    ID3D11ShaderResourceView* depthStencilShaderResourceView;
    FLOAT clearColor[4];
    UINT wndWidth;
    UINT wndHeight;
    FLOAT aspectRatio;
    std::wstring wndTitle;
    LARGE_INTEGER prevCounter;
    LONGLONG counterPerFrame;
    std::vector<Object::SharedPtr> object;

public:
    bool InitializeApp();
    void TerminateApp();
    bool OpenWindow();
    void CloseWindow();
    bool InitializeD3D11();
    void TerminateD3D11();
    void OnTerminate();
    int  MainLoop();
    bool OnInit();
    void OnRender();
    void OnResize(const UINT w, const UINT h);
    void OnUpdate(LONGLONG elapsed);
    void AddObject(Object::SharedPtr obj);

public:
    bool CreateDefaultRenderTarget();
    bool CreateDefaultDepthStencil();
    void ReleaseDefaultRenderTarget();
    void ReleaseDefaultDepthStencil();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

public:
    SampleApp(const Config& config);
    virtual ~SampleApp();

    int Run();
};

#endif //SAMPLE_APP_H
