#pragma once
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_bo.h>
#include <xrt/experimental/xrt_xclbin.h>
#include <filesystem>
#include <string>

namespace anvil::runtime {

class KernelHandle;

class XrtContext {
 public:
    XrtContext(unsigned device_index, const std::filesystem::path& xclbin_path);

    KernelHandle GetKernel(const std::string& cu_name);

    const xrt::device& device() const noexcept { return device_; }
    const xrt::uuid&   uuid()   const noexcept { return uuid_; }

 private:
    xrt::device device_{};
    xrt::xclbin xclbin_{};
    xrt::uuid uuid_{};
};

}  // namespace anvil::runtime
