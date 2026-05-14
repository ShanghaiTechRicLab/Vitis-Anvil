#pragma once

#include <anvil/runtime/kernel_handle.hpp>
#include <anvil/runtime/xrt_context.hpp>

#include <xrt/xrt_bo.h>

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace anvil::runtime {

enum class SyncDirection { HostToDevice, DeviceToHost };

template <typename T>
class XrtBuffer {
 public:
    XrtBuffer(XrtContext& ctx, const KernelHandle& kernel, int arg_index, std::size_t n_elem)
        : n_elem_(n_elem),
          bytes_(CheckedBytes(n_elem)),
          bo_(ctx.device(), bytes_, XCL_BO_FLAGS_NONE, static_cast<int>(kernel.MemGroupId(arg_index))) {
        host_ptr_ = bo_.map<T*>();
    }

    T* host() noexcept { return host_ptr_; }
    const T* host() const noexcept { return host_ptr_; }

    xrt::bo& bo() noexcept { return bo_; }
    const xrt::bo& bo() const noexcept { return bo_; }

    void Sync(SyncDirection dir) {
        if (dir == SyncDirection::HostToDevice) {
            bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        } else {
            bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        }
    }

    std::size_t Size() const noexcept { return n_elem_; }
    std::size_t Bytes() const noexcept { return bytes_; }

 private:
    static std::size_t CheckedBytes(std::size_t n_elem) {
        if (n_elem == 0) {
            throw std::invalid_argument("XrtBuffer requires at least one element");
        }
        if (n_elem > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::overflow_error("XrtBuffer byte size overflow");
        }
        return n_elem * sizeof(T);
    }

    std::size_t n_elem_ = 0;
    std::size_t bytes_ = 0;
    xrt::bo bo_{};
    T* host_ptr_ = nullptr;
};

}  // namespace anvil::runtime
