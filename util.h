#ifndef UTIL_H
#define UTIL_H
#pragma once

#include <DirectXMath.h>
#include <vector>

extern bool LoadObjFile(std::vector<DirectX::XMFLOAT3A>& position, std::vector<DWORD>& index, const std::wstring& filePath, float scale = 1.0f);
extern bool ComputeNormal(std::vector<DirectX::XMFLOAT3A>& normal, const std::vector<DirectX::XMFLOAT3A>& position, const std::vector<DWORD>& index);

#endif //UTIL_H
