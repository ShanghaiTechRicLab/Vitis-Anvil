#pragma once
#include <catch_amalgamated.hpp>
#include <cstddef>
#include <initializer_list>

#define ANVIL_TEST_CASE(name, tags) TEST_CASE(name, tags)

#define ANVIL_PARITY_CASE(name, tags, ...) \
    TEST_CASE(name, tags) { \
        for (std::size_t n : std::initializer_list<std::size_t>{__VA_ARGS__})

#define ANVIL_PARITY_CASE_END }

#define ANVIL_APPROX_CASE(name, tags, tol_val) \
    TEST_CASE(name, tags) { \
        const double tol = (tol_val);

#define ANVIL_APPROX_CASE_END }
