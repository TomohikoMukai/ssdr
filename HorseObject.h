#ifndef HORSE_OBJECT_H
#define HORSE_OBJECT_H
#pragma once

#include "Object.h"
#include "RigidTransform.h"

class HorseObject : public Object
{
public:
    using SharedPtr = std::shared_ptr<HorseObject>;
    using UniquePtr = std::unique_ptr<HorseObject>;

protected:
    struct ConstantBufferPerFrame
    {
        DirectX::XMFLOAT4X4A view;
        DirectX::XMFLOAT4X4A proj;
        DirectX::XMFLOAT4A lightColor;
    };
    static const int NumMaxSkinningMatrix = 100;
    struct ConstantBufferSkinningMatrix
    {
        DirectX::XMFLOAT4X4A pallet[NumMaxSkinningMatrix];
    };
    struct ConstantBufferPerObj
    {
        DirectX::XMFLOAT4X4A world;
        DirectX::XMFLOAT3A localEyePos;
        DirectX::XMFLOAT3A localLightPos;
        DirectX::XMFLOAT4A ambientColor;
        float specExpon;
    };
    struct CustomVertex
    {
        static const int NumInfluences = 4;
        DirectX::XMFLOAT3A position;
        DirectX::XMFLOAT3A normal;
        float weight[NumInfluences];
        unsigned int indices[NumInfluences];
        DirectX::XMFLOAT4A color;
    };
    static D3D11_INPUT_ELEMENT_DESC customVertexLayout[5];


public:
    static SharedPtr CreateInstance()
    {
        SharedPtr r(new HorseObject());
        r->self = r;
        return r;
    }
    virtual ~HorseObject();

public:
    bool OnInit(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const UINT width, const UINT height) override;
    void OnResize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const UINT width, const UINT height) override;
    void OnUpdate(ID3D11Device* device, ID3D11DeviceContext* deviceContext, float elapsed);
    void OnRender(ID3D11Device* device, ID3D11DeviceContext* deviceContext) override;
    void OnDestroy() override;

protected:
    HRESULT InitShader(ID3D11Device* device);
    HRESULT LoadModel(ID3D11Device* device, const wchar_t* filePath, const DirectX::XMFLOAT4A& color);
    bool LoadAnim(const wchar_t* filePath);
    void ColorVerticesByError(int frame, float upperBound);

private:
    ID3D11Buffer* constantBufferPerFrame;
    ID3D11Buffer* constantBufferPerObj;
    ID3D11Buffer* constantBufferSkinningBone;
    ConstantBufferPerFrame perFrameData;
    DirectX::XMFLOAT3A lightPosition;
    ID3D11InputLayout* inputLayout;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11BlendState* alphaBlendState;

    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    unsigned long numVertices;
    unsigned long numFaces;

    CustomVertex* vertexBufferCPU;
    CustomVertex* srcVertexBufferCPU;
    std::vector<DirectX::XMFLOAT3A> vertexAnim;
    std::vector<RigidTransform> boneAnim;
    unsigned long numFrames;
    unsigned long frame;
    int numBones;

protected:
    HorseObject();
};

#endif //HORSE_OBJECT_H
