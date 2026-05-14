#include <anvil/runtime/xrt_context.hpp>
#include <anvil/runtime/kernel_handle.hpp>

namespace anvil::runtime {

XrtContext::XrtContext(unsigned device_index, const std::filesystem::path& xclbin_path) {
    device_ = xrt::device(device_index);
    xclbin_ = xrt::xclbin(xclbin_path.string());
    uuid_ = device_.load_xclbin(xclbin_);
}

KernelHandle XrtContext::GetKernel(const std::string& cu_name) {
    return KernelHandle{xrt::kernel(device_, uuid_, cu_name)};
}

}  // namespace anvil::runtime
