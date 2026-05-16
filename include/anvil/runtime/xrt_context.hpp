#pragma once
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_bo.h>
#include <xrt/xrt_uuid.h>

#ifndef ANVIL_XRT_HAS_EXPERIMENTAL_XCLBIN
#define ANVIL_XRT_HAS_EXPERIMENTAL_XCLBIN 0
#endif

#if ANVIL_XRT_HAS_EXPERIMENTAL_XCLBIN
#include <xrt/experimental/xrt_xclbin.h>
#endif

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
#if ANVIL_XRT_HAS_EXPERIMENTAL_XCLBIN
    xrt::xclbin xclbin_{};
#endif
    xrt::uuid uuid_{};
};

}  // namespace anvil::runtime
