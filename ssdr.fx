cbuffer CBPerFrame : register(b0)
{
    matrix viewMatrix;
    matrix projectionMatrix;
    float4 lightColor;
};

cbuffer CBPerObj : register(b1)
{
    matrix worldMatrix;
    float3 localEyePos;
    float3 localLightPos;
    float4 ambientColor;
    float  specExpon;
};

cbuffer CBSkinningMatrix : register(b2)
{
    matrix pallet[100];
}

struct VertexData
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 weight   : BLENDWEIGHT;
    uint4  index    : BLENDINDICES;
    float4 color    : COLOR;
};

struct LambertData
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float3 viewVec  : TEXCOORD1;
    float3 lightVec : TEXCOORD2;
    float4 color    : COLOR;
};

float4 BlendPosition(float4 weight, uint4 index, float3 p)
{
    float4 v = float4(p.xyz, 1.0f);
    float4 output = weight.x * mul(v, pallet[index.x]);
    output += weight.y * mul(v, pallet[index.y]);
    output += weight.z * mul(v, pallet[index.z]);
    output += weight.w * mul(v, pallet[index.w]);
    return float4(output.xyz, 1.0f);
}

float3 BlendNormal(float4 weight, uint4 index, float3 n)
{
    float4 v = float4(n.xyz, 0);
    float4 output = weight.x * mul(v, pallet[index.x]);
    output += weight.y * mul(v, pallet[index.y]);
    output += weight.z * mul(v, pallet[index.z]);
    output += weight.w * mul(v, pallet[index.w]);
    return output.xyz;
}

LambertData LambertSkinVS(VertexData input)
{
    LambertData output = (LambertData)0;
    output.position = BlendPosition(input.weight, input.index, input.position);
    output.position = mul(output.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    output.normal = BlendNormal(input.weight, input.index, input.normal.xyz);
    output.viewVec = normalize(localEyePos - input.position);
    output.lightVec = normalize(localLightPos - input.position);
    output.color = input.color;
    return output;
}

float4 LambertPS(LambertData input) : SV_TARGET0
{
    float3 hn = normalize(input.viewVec + input.lightVec);
    float3 diffContrib = (lightColor.xyz * input.color.xyz) * max(dot(input.lightVec, input.normal), 0);
    return float4(ambientColor + diffContrib, input.color.w);
}
