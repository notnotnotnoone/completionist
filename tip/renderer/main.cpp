#define NOMINMAX
#include <windows.h>
#include "fixture.h"
#include "fault_dialogs.h"
#include "live_fixture.h"
#include "production_service.h"
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (!renderer::suppressFaultDialogsForCurrentProcess()) return ERROR_FUNCTION_FAILED;
    int count = 0;
    LPWSTR* args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args) return ERROR_INVALID_PARAMETER;
    int result = ERROR_SUCCESS;
    if (count == 3 && lstrcmpW(args[1], L"--fixture-matrix") == 0) {
        if (!renderer::renderFixtureMatrix(args[2])) result = ERROR_GEN_FAILURE;
    } else if (count == 2 && lstrcmpW(args[1], L"--live") == 0) {
        result = renderer::runLiveFixture();
    } else if (count == 2 && lstrcmpW(args[1], L"--serve") == 0) {
        result = renderer::runProductionService();
    } else if (count == 4 && lstrcmpW(args[1], L"--serve") == 0 && lstrcmpW(args[2], L"--capture-dir") == 0) {
        result = renderer::runProductionService(args[3]);
    } else if (count == 4 && lstrcmpW(args[1], L"--review") == 0 && lstrcmpW(args[2], L"--capture-dir") == 0) {
        result = renderer::runMaterialReview(args[3]);
    } else if (count != 1) {
        result = ERROR_INVALID_PARAMETER;
    }
    LocalFree(args);
    return result;
}
