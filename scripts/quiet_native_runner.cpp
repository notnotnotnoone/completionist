#define NOMINMAX
#include <windows.h>
#include <werapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr UINT kErrorMode = SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                           SEM_NOOPENFILEERRORBOX;
constexpr DWORD kLogLimit = 1024 * 1024;
constexpr char kTruncationMarker[] = "\r\n[quiet runner: output limit reached; remainder discarded]\r\n";
constexpr DWORD kLogPayloadLimit = kLogLimit - static_cast<DWORD>(sizeof(kTruncationMarker) - 1);
constexpr DWORD kDefaultTimeoutMs = 120'000;
constexpr DWORD kMaximumTimeoutMs = 600'000;

struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    explicit Handle(HANDLE handle) : value(handle) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value(other.value) { other.value = nullptr; }
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
            value = other.value;
            other.value = nullptr;
        }
        return *this;
    }
    HANDLE get() const { return value; }
    HANDLE* put() { return &value; }
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};

struct Target {
    const wchar_t* name;
    const wchar_t* folder;
};

constexpr std::array<Target, 5> kTargets{{
    {L"tests.exe", L"host-tests"},
    {L"test_popup_layout.exe", L"host-tests"},
    {L"test_session.exe", L"renderer-tests"},
    {L"test_capture_policy.exe", L"renderer-tests"},
    {L"test_blur.exe", L"renderer-tests"},
}};

struct RunResult {
    DWORD exitCode = ERROR_GEN_FAILURE;
    bool timedOut = false;
};

UINT gInheritedErrorMode = 0;

std::filesystem::path ModulePath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) return {};
    return std::filesystem::path(std::wstring(buffer.data(), length));
}

std::filesystem::path WorkspaceRoot() {
    const auto module = ModulePath();
    if (module.empty()) return {};
    // Build output is scripts/out/quiet_native_runner.exe.
    const auto root = module.parent_path().parent_path().parent_path();
    std::error_code error;
    const auto canonical = std::filesystem::canonical(root, error);
    if (error || !std::filesystem::is_directory(canonical)) return {};
    return canonical;
}

bool ConfigureFaultSuppression() {
    SetErrorMode(GetErrorMode() | kErrorMode);
    if ((GetErrorMode() & kErrorMode) != kErrorMode) return false;
    return SUCCEEDED(WerSetFlags(WER_FAULT_REPORTING_NO_UI));
}

std::wstring Quote(const std::wstring& value) {
    std::wstring quoted = L"\"";
    unsigned slashes = 0;
    for (wchar_t character : value) {
        if (character == L'\\') {
            ++slashes;
        } else if (character == L'\"') {
            quoted.append(slashes * 2 + 1, L'\\');
            quoted.push_back(character);
            slashes = 0;
        } else {
            quoted.append(slashes, L'\\');
            slashes = 0;
            quoted.push_back(character);
        }
    }
    quoted.append(slashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

void DrainBounded(HANDLE pipe, Handle output, std::atomic_bool* logOk) {
    std::array<char, 8192> buffer{};
    DWORD kept = 0;
    bool canWrite = true;
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            if (GetLastError() != ERROR_BROKEN_PIPE) logOk->store(false);
            break;
        }
        if (!read) break;
        if (!canWrite || kept >= kLogPayloadLimit) continue;
        const DWORD amount = std::min<DWORD>(read, kLogPayloadLimit - kept);
        DWORD written = 0;
        if (amount && WriteFile(output.get(), buffer.data(), amount, &written, nullptr) && written == amount) {
            kept += written;
        } else {
            canWrite = false;
            logOk->store(false);
        }
    }
    if (canWrite && kept == kLogPayloadLimit) {
        DWORD written = 0;
        if (!WriteFile(output.get(), kTruncationMarker,
                       static_cast<DWORD>(sizeof(kTruncationMarker) - 1), &written, nullptr) ||
            written != sizeof(kTruncationMarker) - 1) logOk->store(false);
    }
}

bool CreateJob(Handle& job) {
    job = Handle(CreateJobObjectW(nullptr, nullptr));
    if (!job) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                             JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    return SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                   &limits, sizeof(limits)) != FALSE;
}

bool Launch(const std::filesystem::path& executable, const std::wstring& childArguments,
            DWORD timeoutMs, const std::filesystem::path& logBase, RunResult* result) {
    if (!ConfigureFaultSuppression()) return false;

    const auto stdoutPath = std::filesystem::path(logBase.wstring() + L".stdout.log");
    const auto stderrPath = std::filesystem::path(logBase.wstring() + L".stderr.log");
    Handle stdoutLog(CreateFileW(stdoutPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!stdoutLog) {
        std::wcerr << L"unable to create log " << stdoutPath.wstring() << L" error=" << GetLastError() << L"\n";
        return false;
    }
    Handle stderrLog(CreateFileW(stderrPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!stderrLog) {
        std::wcerr << L"unable to create log " << stderrPath.wstring() << L" error=" << GetLastError() << L"\n";
        return false;
    }

    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    Handle stdoutRead, stdoutWrite, stderrRead, stderrWrite;
    if (!CreatePipe(stdoutRead.put(), stdoutWrite.put(), &security, 0) ||
        !CreatePipe(stderrRead.put(), stderrWrite.put(), &security, 0) ||
        !SetHandleInformation(stdoutRead.get(), HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(stderrRead.get(), HANDLE_FLAG_INHERIT, 0)) return false;
    Handle nullInput(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!nullInput) return false;

    Handle job;
    if (!CreateJob(job)) return false;

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nullInput.get();
    startup.hStdOutput = stdoutWrite.get();
    startup.hStdError = stderrWrite.get();
    PROCESS_INFORMATION process{};
    const std::wstring commandLine = Quote(executable.native()) + childArguments;
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');
    if (!CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr,
                        executable.parent_path().c_str(), &startup, &process)) return false;
    Handle processHandle(process.hProcess), threadHandle(process.hThread);
    stdoutWrite = Handle();
    stderrWrite = Handle();

    if (!AssignProcessToJobObject(job.get(), processHandle.get())) {
        TerminateProcess(processHandle.get(), ERROR_PROCESS_ABORTED);
        WaitForSingleObject(processHandle.get(), INFINITE);
        return false;
    }
    std::atomic_bool stdoutLogOk{true};
    std::atomic_bool stderrLogOk{true};
    std::thread stdoutDrain(DrainBounded, stdoutRead.get(), std::move(stdoutLog), &stdoutLogOk);
    std::thread stderrDrain(DrainBounded, stderrRead.get(), std::move(stderrLog), &stderrLogOk);
    const DWORD previousSuspendCount = ResumeThread(threadHandle.get());
    if (previousSuspendCount == static_cast<DWORD>(-1)) {
        TerminateJobObject(job.get(), ERROR_PROCESS_ABORTED);
        stdoutDrain.join();
        stderrDrain.join();
        return false;
    }

    const DWORD wait = WaitForSingleObject(processHandle.get(), timeoutMs);
    if (wait == WAIT_TIMEOUT) {
        result->timedOut = true;
        TerminateJobObject(job.get(), ERROR_TIMEOUT);
    } else if (wait != WAIT_OBJECT_0) {
        TerminateJobObject(job.get(), ERROR_PROCESS_ABORTED);
    }
    if (wait == WAIT_OBJECT_0 && !GetExitCodeProcess(processHandle.get(), &result->exitCode))
        result->exitCode = ERROR_GEN_FAILURE;
    if (wait == WAIT_TIMEOUT) result->exitCode = ERROR_TIMEOUT;

    job = Handle(); // Closing this job terminates only members assigned to this run.
    stdoutDrain.join();
    stderrDrain.join();
    stdoutRead = Handle();
    stderrRead = Handle();
    if (!stdoutLogOk.load() || !stderrLogOk.load()) {
        std::wcerr << L"log write or pipe drain failed for " << logBase.wstring() << L"\n";
        return false;
    }
    return wait == WAIT_OBJECT_0 || wait == WAIT_TIMEOUT;
}

const Target* FindTarget(const std::wstring& name) {
    const auto found = std::find_if(kTargets.begin(), kTargets.end(), [&](const Target& target) {
        return name == target.name;
    });
    return found == kTargets.end() ? nullptr : &*found;
}

bool ParseTimeout(const wchar_t* input, DWORD* timeout) {
    if (!input || !*input) return false;
    unsigned long value = 0;
    for (const wchar_t* digit = input; *digit; ++digit) {
        if (*digit < L'0' || *digit > L'9') return false;
        const unsigned long next = static_cast<unsigned>(*digit - L'0');
        if (value > (kMaximumTimeoutMs - next) / 10) return false;
        value = value * 10 + next;
    }
    if (value < 100 || value > kMaximumTimeoutMs) return false;
    *timeout = static_cast<DWORD>(value);
    return true;
}

int RunTarget(const std::wstring& targetName, DWORD timeout) {
    const Target* target = FindTarget(targetName);
    if (!target) return 2;
    const auto root = WorkspaceRoot();
    if (root.empty()) return 2;
    const auto stage = root / L"tip" / L"out" / L"task9-review";
    const auto targetPath = stage / target->folder / target->name;
    std::error_code error;
    const auto canonicalStage = std::filesystem::canonical(stage, error);
    if (error) return 2;
    const auto canonicalTarget = std::filesystem::canonical(targetPath, error);
    if (error || canonicalTarget.parent_path() != canonicalStage / target->folder ||
        !std::filesystem::is_regular_file(canonicalTarget)) return 2;

    const auto logDirectory = stage / L"native-test-logs";
    if (!CreateDirectoryW(logDirectory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 2;
    const auto logBase = logDirectory / target->name;
    RunResult result{};
    if (!Launch(canonicalTarget, L"", timeout, logBase, &result)) return 2;
    if (result.timedOut) {
        std::wcerr << targetName << L" timed out after " << timeout << L" ms; owned job terminated.\n";
        return 124;
    }
    std::wcout << targetName << L" exit=" << result.exitCode << L" logs="
               << logBase.wstring() << L".{stdout,stderr}.log\n";
    return static_cast<int>(result.exitCode);
}

int Fixture(const std::wstring& name) {
    if (name == L"flags") {
        std::wcout << L"inherited_error_mode=0x" << std::hex << gInheritedErrorMode << L"\n";
        return (gInheritedErrorMode & kErrorMode) == kErrorMode ? 0 : 10;
    }
    if (name == L"crash") {
        RaiseException(0xE0424242, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        return 11;
    }
    if (name == L"timeout") {
        Sleep(5000);
        return 12;
    }
    return 2;
}

int SelfCheck() {
    const auto module = ModulePath();
    if (module.empty()) return 2;
    const auto root = WorkspaceRoot();
    const auto logDirectory = root / L"tip" / L"out" / L"task9-review" / L"native-test-logs";
    if (!CreateDirectoryW(logDirectory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 2;
    const std::array<std::wstring, 3> fixtures{L"flags", L"crash", L"timeout"};
    for (const auto& fixture : fixtures) {
        RunResult result{};
        const DWORD timeout = fixture == L"timeout" ? 100 : 10'000;
        const auto base = logDirectory / (L"self-check-" + fixture);
        if (!Launch(module, L" --fixture " + fixture, timeout, base, &result)) {
            std::wcerr << L"self-check " << fixture << L" failed to launch safely.\n";
            return 2;
        }
        if (fixture == L"flags" && (result.timedOut || result.exitCode != 0)) return 1;
        if (fixture == L"crash" && (result.timedOut || result.exitCode == 0)) return 1;
        if (fixture == L"timeout" && !result.timedOut) return 1;
        std::wcout << L"self-check " << fixture << L" ok; logs=" << base.wstring() << L".*.log\n";
    }
    return 0;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    gInheritedErrorMode = GetErrorMode();
    if (!ConfigureFaultSuppression()) {
        std::wcerr << L"quiet native runner: could not suppress Windows fault UI.\n";
        return 2;
    }
    if (argc == 3 && std::wstring(argv[1]) == L"--fixture") return Fixture(argv[2]);
    if (argc == 2 && std::wstring(argv[1]) == L"--self-check") return SelfCheck();
    if (argc < 2 || argc > 3) {
        std::wcerr << L"usage: quiet_native_runner <allowlisted-test.exe> [timeout-ms]\n";
        return 2;
    }
    DWORD timeout = kDefaultTimeoutMs;
    if (argc == 3 && !ParseTimeout(argv[2], &timeout)) return 2;
    return RunTarget(argv[1], timeout);
}
