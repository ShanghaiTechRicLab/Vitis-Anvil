#pragma once
#include <indicators.hpp>
#include <cstddef>
#include <string>

namespace anvil::progress {

class Bar {
 public:
    Bar(const std::string& desc, std::size_t total)
        : total_(total), count_(0) {
        using namespace indicators;
        bar_.set_option(option::BarWidth{40});
        bar_.set_option(option::Start{"["});
        bar_.set_option(option::Fill{"="});
        bar_.set_option(option::Lead{">"});
        bar_.set_option(option::Remainder{" "});
        bar_.set_option(option::End{"]"});
        bar_.set_option(option::PrefixText{desc + " "});
        bar_.set_option(option::ShowElapsedTime{true});
        bar_.set_option(option::MaxProgress{total});
    }

    void Tick() {
        if (count_ < total_) ++count_;
        bar_.set_progress(count_);
    }

    void Done() { bar_.mark_as_completed(); }

 private:
    indicators::ProgressBar bar_;
    std::size_t total_;
    std::size_t count_;
};

}  // namespace anvil::progress
