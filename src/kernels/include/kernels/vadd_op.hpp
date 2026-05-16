#pragma once

namespace kernels {

struct VaddOp {
  float operator()(float a, float b) const { return a + b; }
};

}  // namespace kernels
