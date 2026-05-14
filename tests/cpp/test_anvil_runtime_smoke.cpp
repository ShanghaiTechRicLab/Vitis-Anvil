#include <catch_amalgamated.hpp>
#include <anvil/runtime/xrt_context.hpp>
#include <anvil/runtime/kernel_handle.hpp>
#include <type_traits>

TEST_CASE("anvil::runtime headers compile", "[runtime][smoke]") {
    static_assert(std::is_class_v<anvil::runtime::XrtContext>);
    static_assert(sizeof(anvil::runtime::KernelHandle) > 0);
    SUCCEED("runtime headers compile and types are accessible");
}
