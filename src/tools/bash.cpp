#include "bash.hpp"
#include <string>
#include <sstream>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Windows implementation — temporary .ps1 file + WaitForSingleObject (PowerShell)
// ---------------------------------------------------------------------------
#ifdef _WIN32

#include <windows.h>
#include <thread>
#include <fstream>

static std::string run_win(const std::string& command, int timeout_ms) {
    // 1. Create temporary .ps1 file — PowerShell execution (stable quoting, Unix alias support)
    char tmpdir[MAX_PATH] = {}, tmpfile_base[MAX_PATH] = {};
    GetTempPathA(sizeof(tmpdir), tmpdir);
    GetTempFileNameA(tmpdir, "pyx_", 0, tmpfile_base);
    std::string ps1 = std::string(tmpfile_base) + ".ps1";
    {
        std::ofstream f(ps1);
        // Set UTF-8 output + command + propagate exit code
        f << "$OutputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8\n"
          << command << "\n"
          << "exit $LASTEXITCODE\n";
    }

    // 2. Create pipe (merges stdout + stderr)
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        std::remove(ps1.c_str());
        std::remove(tmpfile_base);
        return "Error: CreatePipe failed";
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    // 3. Launch process via CreateProcess
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::string cmd_line =
        "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""
        + ps1 + "\"";
    BOOL ok = CreateProcessA(
        nullptr, &cmd_line[0], nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );
    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        std::remove(ps1.c_str());
        std::remove(tmpfile_base);
        return "Error: CreateProcess failed (code " +
               std::to_string(GetLastError()) + ")";
    }

    // 4. Reader thread (drain pipe in separate thread — prevents deadlock)
    std::string output;
    std::thread reader([&]() {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(hRead, buf, sizeof(buf), &n, nullptr) && n > 0)
            output.append(buf, n);
    });

    // 5. Wait with timeout
    bool timed_out = false;
    DWORD wait_ms = (timeout_ms <= 0) ? INFINITE : static_cast<DWORD>(timeout_ms);
    if (WaitForSingleObject(pi.hProcess, wait_ms) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        timed_out = true;
    }
    reader.join();
    CloseHandle(hRead);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // 6. Clean up temporary files
    std::remove(ps1.c_str());
    std::remove(tmpfile_base);

    if (timed_out)
        output += "\n[Timed out after " + std::to_string(timeout_ms) + "ms]";
    else if (exit_code != 0)
        output += "\n[Exit code: " + std::to_string(exit_code) + "]";

    if (output.empty()) return "(no output)";

    // UTF-8 safety: replace invalid bytes with '?'
    std::string safe;
    safe.reserve(output.size());
    for (size_t i = 0; i < output.size(); ) {
        unsigned char c = static_cast<unsigned char>(output[i]);
        if (c < 0x80) {
            safe += output[i++];
        } else if ((c & 0xE0) == 0xC0 && i+1 < output.size() && (static_cast<unsigned char>(output[i+1]) & 0xC0) == 0x80) {
            safe += output[i++]; safe += output[i++];
        } else if ((c & 0xF0) == 0xE0 && i+2 < output.size() && (static_cast<unsigned char>(output[i+1]) & 0xC0) == 0x80 && (static_cast<unsigned char>(output[i+2]) & 0xC0) == 0x80) {
            safe += output[i++]; safe += output[i++]; safe += output[i++];
        } else if ((c & 0xF8) == 0xF0 && i+3 < output.size()) {
            safe += output[i++]; safe += output[i++]; safe += output[i++]; safe += output[i++];
        } else {
            safe += '?'; i++;  // invalid byte substitution
        }
    }
    return safe;
}

// ---------------------------------------------------------------------------
// Unix implementation — fork + select (timeout) + SIGKILL
// ---------------------------------------------------------------------------
#else

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/select.h>
#include <chrono>
#include <climits>

static std::string run_unix(const std::string& command, int timeout_ms) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return "Error: pipe() failed";

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return "Error: fork() failed";
    }

    if (pid == 0) {
        // child: become process group leader (for killpg)
        setpgid(0, 0);
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    close(pipefd[1]);
    std::string output;
    char buf[4096];
    bool timed_out = false;

    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : INT_MAX);

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (timeout_ms > 0 && now >= deadline) {
            timed_out = true;
            break;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(pipefd[0], &rfds);

        struct timeval tv{};
        if (timeout_ms > 0) {
            auto remain_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                 deadline - now).count();
            tv.tv_sec  = remain_us / 1000000;
            tv.tv_usec = static_cast<int>(remain_us % 1000000);
        }

        int r = select(pipefd[0] + 1, &rfds, nullptr, nullptr,
                       timeout_ms > 0 ? &tv : nullptr);
        if (r < 0) break;        // EINTR etc. — exit loop
        if (r == 0) { timed_out = true; break; }

        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n <= 0) break;
        output.append(buf, static_cast<size_t>(n));
    }
    close(pipefd[0]);

    if (timed_out) {
        killpg(pid, SIGKILL);  // kill entire process group
        output += "\n[Timed out after " + std::to_string(timeout_ms) + "ms]";
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (!timed_out && WIFEXITED(status) && WEXITSTATUS(status) != 0)
        output += "\n[Exit code: " + std::to_string(WEXITSTATUS(status)) + "]";

    return output.empty() ? "(no output)" : output;
}

#endif

// ---------------------------------------------------------------------------
// BashTool implementation
// ---------------------------------------------------------------------------

namespace openrein {

std::string BashTool::description() const {
    return
        "Executes a shell command and returns combined stdout+stderr.\n"
        "Windows: runs via PowerShell (temporary .ps1) — supports Unix aliases like ls/cat, UTF-8 output.\n"
        "Unix: runs via /bin/sh -c (select-based timeout, SIGKILL termination).";
}

json BashTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "Shell command to execute"}
            }},
            {"description", {
                {"type", "string"},
                {"description", "Command description (optional, ignored)"}
            }},
            {"timeout", {
                {"type", "integer"},
                {"description", "Timeout in milliseconds. Default: 120000 (2 min). 0 means unlimited."},
                {"default", 120000}
            }},
            {"run_in_background", {
                {"type", "boolean"},
                {"description", "Run in background — not currently supported (ignored)"}
            }}
        }},
        {"required", {"command"}}
    };
}

std::string BashTool::call(const json& input) const {
    const std::string command = input["command"].get<std::string>();
    const int timeout_ms      = input.value("timeout", 120000);  // default 2 minutes
#ifdef _WIN32
    return run_win(command, timeout_ms);
#else
    return run_unix(command, timeout_ms);
#endif
}

} // namespace openrein
