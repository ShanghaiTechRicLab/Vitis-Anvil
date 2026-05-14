#pragma once
#include <xrt/xrt_kernel.h>
#include <cstddef>
#include <utility>

namespace anvil::runtime {

class KernelHandle {
 public:
    explicit KernelHandle(xrt::kernel k) : kernel_(std::move(k)) {}

    std::size_t MemGroupId(int arg_index) const {
        return kernel_.group_id(arg_index);
    }

    template <typename... Args>
    void operator()(Args&&... args) {
        auto run = kernel_(std::forward<Args>(args)...);
        run.wait();
    }

    template <typename... Args>
    xrt::run Launch(Args&&... args) {
        return kernel_(std::forward<Args>(args)...);
    }

    xrt::kernel& kernel() noexcept { return kernel_; }
    const xrt::kernel& kernel() const noexcept { return kernel_; }

 private:
    xrt::kernel kernel_;
};

}  // namespace anvil::runtime
