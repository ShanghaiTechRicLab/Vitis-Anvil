#pragma once
// Dataflow wrappers over hlslib Simulation.h.
//
// Important hlslib v1.4.6 rule: pass stream variables directly to
// ANVIL_DATAFLOW_FUNCTION. Do not wrap hlslib::Stream arguments in std::ref.
// In simulation mode hlslib uses passed_by<Arg> keyed on the callee function
// signature to preserve reference parameters; std::ref at the call site can
// double-wrap and fail to bind.

#include <hlslib/xilinx/Simulation.h>

#define ANVIL_DATAFLOW_INIT       HLSLIB_DATAFLOW_INIT
#define ANVIL_DATAFLOW_FUNCTION   HLSLIB_DATAFLOW_FUNCTION
#define ANVIL_DATAFLOW_FINALIZE   HLSLIB_DATAFLOW_FINALIZE
