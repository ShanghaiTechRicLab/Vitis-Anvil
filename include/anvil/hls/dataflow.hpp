#pragma once
// Dataflow wrappers over hlslib Simulation.h.
//
// Important hlslib v1.4.6 rule: pass stream variables directly to
// ANVIL_DATAFLOW_FUNCTION. Do not wrap hlslib::Stream arguments in std::ref.
// In simulation mode hlslib uses passed_by<Arg> keyed on the callee function
// signature to preserve reference parameters; std::ref at the call site can
// double-wrap and fail to bind.

#include <hlslib/xilinx/Simulation.h>

#ifdef HLSLIB_SYNTHESIS
// hlslib's synthesis-mode dataflow helpers intentionally collapse to plain
// sequential function calls. That is fine for leaf functions, but not for this
// project's bounded-stream Load/Compute/Store pipelines: without an enclosing
// HLS dataflow region, synthesis emits sequential producers/consumers and the
// first producer can fill its finite stream before the consumer ever starts.
//
// Keep hlslib's threaded simulation behavior below, but make synthesis an
// actual Vitis HLS dataflow region.
#define ANVIL_DATAFLOW_INIT()     _Pragma("HLS dataflow")
#define ANVIL_DATAFLOW_FUNCTION(func, ...) func(__VA_ARGS__)
#define ANVIL_DATAFLOW_FINALIZE()
#else
#define ANVIL_DATAFLOW_INIT       HLSLIB_DATAFLOW_INIT
#define ANVIL_DATAFLOW_FUNCTION   HLSLIB_DATAFLOW_FUNCTION
#define ANVIL_DATAFLOW_FINALIZE   HLSLIB_DATAFLOW_FINALIZE
#endif
