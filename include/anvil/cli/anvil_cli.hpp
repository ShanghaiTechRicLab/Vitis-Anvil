#pragma once
#include <argparse.hpp>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace anvil::cli {

using Parser = argparse::ArgumentParser;

inline void parse_or_exit(argparse::ArgumentParser& p, int argc, char* argv[]) {
    try {
        p.parse_args(argc, argv);
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << "\n" << p;
        std::exit(1);
    }
}

}  // namespace anvil::cli
