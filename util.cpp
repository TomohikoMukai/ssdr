#include "util.h"
#include <cstdio>

static const char* delim = " /\t\n";
static char* context = nullptr;

using namespace DirectX;

bool ReadVector3(XMFLOAT3& v, char* buf)
{
    // local definition
    //
#define READ_VECTOR3_TOKEN(ELEMENT) tok = strtok_s(buf, delim, &context); \
    if (tok == nullptr) { return false; } \
    v.ELEMENT = static_cast<float>(std::atof(tok));
    //
    char* tok = nullptr;
    READ_VECTOR3_TOKEN(x);
    READ_VECTOR3_TOKEN(y);
    READ_VECTOR3_TOKEN(z);
    return true;
}

bool LoadObjFile(std::vector<XMFLOAT3A>& position, std::vector<DWORD>& index, const std::wstring& filePath, float scale)
{
    char buf[1024];
    char* tok = nullptr;

    FILE* fin = nullptr;
    _wfopen_s(&fin, filePath.data(), L"r");
    if (fin == nullptr)
    {
        return false;
    }

    position.clear();
    index.clear();

    while (std::fgets(buf, 1024, fin) != nullptr)
    {
        tok = strtok_s(buf, delim, &context);
        if (tok == nullptr)
        {
        }
        else if (tok[0] == '#')
        {
            continue;
        }
        else if (strcmp(tok, "v") == 0)
        {
            XMFLOAT3 p;
            if (!ReadVector3(p, NULL))
            {
                break;
            }
            position.push_back(XMFLOAT3A(p.x * scale, p.y * scale, p.z * scale));
        }
        else if (strcmp(tok, "vt") == 0)
        {
            continue;
        }
        else if (strcmp(tok, "f") == 0)
        {
            std::vector<unsigned int > tmp;
            while (tok = strtok_s(NULL, delim, &context), tok != nullptr)
            {
                tmp.push_back(static_cast<unsigned int >(atol(tok) - 1));
            }
            switch (tmp.size())
            {
            case 6:
                index.push_back(tmp[0]);
                index.push_back(tmp[2]);
                index.push_back(tmp[4]);
                break;
            default:
                fclose(fin);
                position.clear();
                index.clear();
                return false;
                break;
            }
        }
        else if (strcmp(tok, "g") == 0)
        {
            continue;
        }
        else
        {
            continue;
        }
    }
    fclose(fin);
    return true;
}

bool ComputeNormal(std::vector<XMFLOAT3A>& normal, const std::vector<XMFLOAT3A>& position, const std::vector<DWORD>& index)
{
    if (position.empty() || index.empty())
    {
        return false;
    }
    std::vector<XMFLOAT3A> nv(position.size(), XMFLOAT3A(0, 0, 0));
    for (auto it = index.begin(); it != index.end(); it += 3)
    {
        const XMVECTOR v0 = XMLoadFloat3A(&position[*it]);
        const XMVECTOR v1 = XMLoadFloat3A(&position[*(it + 1)]);
        const XMVECTOR v2 = XMLoadFloat3A(&position[*(it + 2)]);

        XMVECTOR n = XMVector3Cross(XMVectorSubtract(v1, v0), XMVectorSubtract(v2, v0));
        n = XMVector3Normalize(n);

        XMStoreFloat3A(&nv[*it], XMVectorAdd(XMLoadFloat3A(&nv[*it]), n));
        XMStoreFloat3A(&nv[*(it + 1)], XMVectorAdd(XMLoadFloat3A(&nv[*(it + 1)]), n));
        XMStoreFloat3A(&nv[*(it + 2)], XMVectorAdd(XMLoadFloat3A(&nv[*(it + 2)]), n));
    }
    normal.resize(position.size());
    for (size_t v = 0; v < position.size(); ++v)
    {
        XMVECTOR n = XMLoadFloat3A(&nv[v]);
        n = XMVector3Normalize(n);
        XMFLOAT3A na;
        XMStoreFloat3A(&na, n);
        normal[v] = na;
    }
    return true;
}
