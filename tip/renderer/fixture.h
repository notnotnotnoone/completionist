#pragma once
#include <string>
#include <windows.h>
#include <d3d11.h>

namespace renderer {
bool saveTexturePng(const std::wstring& path, ID3D11DeviceContext* context, ID3D11Texture2D* texture);
bool saveTextureCropPng(const std::wstring& path, ID3D11DeviceContext* context, ID3D11Texture2D* texture, const RECT& crop);
bool renderFixture(const std::wstring& outputPath, UINT dpi, bool dark, int fontSizePoints = 12);
bool renderFixtureMatrix(const std::wstring& outputDirectory);
}
