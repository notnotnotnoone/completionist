#pragma once
#include <string>
#include <windows.h>

namespace renderer {
bool renderFixture(const std::wstring& outputPath, UINT dpi, bool dark);
bool renderFixtureMatrix(const std::wstring& outputDirectory);
}
