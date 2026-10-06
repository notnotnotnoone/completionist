#pragma once
#include <string>

namespace renderer {
int runProductionService(const std::wstring& screenshotDirectory = {});
int runMaterialReview(const std::wstring& screenshotDirectory);
}
