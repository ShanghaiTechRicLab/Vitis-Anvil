#include <catch_amalgamated.hpp>
#include <anvil/runtime/xrt_context.hpp>
#include <anvil/runtime/kernel_handle.hpp>
#include <anvil/runtime/xrt_buffer.hpp>
#include <type_traits>

TEST_CASE("anvil::runtime headers compile", "[runtime][smoke]") {
    static_assert(std::is_class_v<anvil::runtime::XrtContext>);
    static_assert(sizeof(anvil::runtime::KernelHandle) > 0);
    SUCCEED("runtime headers compile and types are accessible");
}


TEST_CASE("anvil::runtime::XrtBuffer header compiles", "[runtime][smoke]") {
    static_assert(sizeof(anvil::runtime::XrtBuffer<float>) > 0);
    SUCCEED("XrtBuffer<float> header compiles");
}
