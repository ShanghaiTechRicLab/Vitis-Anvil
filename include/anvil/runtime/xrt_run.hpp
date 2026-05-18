#pragma once

#include <xrt/xrt_kernel.h>

#include <exception>
#include <string>

namespace anvil::runtime {

inline const char* RunStateName(ert_cmd_state state) {
    switch (state) {
        case ERT_CMD_STATE_NEW: return "NEW";
        case ERT_CMD_STATE_QUEUED: return "QUEUED";
        case ERT_CMD_STATE_RUNNING: return "RUNNING";
        case ERT_CMD_STATE_COMPLETED: return "COMPLETED";
        case ERT_CMD_STATE_ERROR: return "ERROR";
        case ERT_CMD_STATE_ABORT: return "ABORT";
        case ERT_CMD_STATE_SUBMITTED: return "SUBMITTED";
        case ERT_CMD_STATE_TIMEOUT: return "TIMEOUT";
        case ERT_CMD_STATE_NORESPONSE: return "NORESPONSE";
        case ERT_CMD_STATE_SKERROR: return "SKERROR";
        case ERT_CMD_STATE_SKCRASHED: return "SKCRASHED";
        default: return "UNKNOWN";
    }
}

inline ert_cmd_state WaitRun(xrt::run& run, int timeout_ms) {
    return timeout_ms == 0 ? run.wait() : run.wait(static_cast<unsigned int>(timeout_ms));
}

inline bool TryAbortRun(xrt::run& run, std::string* error = nullptr) noexcept {
    try {
        run.abort();
        return true;
    } catch (const std::exception& e) {
        if (error != nullptr) {
            *error = e.what();
        }
        return false;
    } catch (...) {
        if (error != nullptr) {
            *error = "unknown exception";
        }
        return false;
    }
}

}  // namespace anvil::runtime
