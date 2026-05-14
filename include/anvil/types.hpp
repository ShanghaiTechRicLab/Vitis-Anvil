#pragma once
// Common type aliases shared across gold / hls_model / runtime / kernel layers.
// Phase 0+1 keeps this empty — saxpy uses plain float.
// Future phases (Phase 2 vectorization) will introduce ElemPack, FixedT, etc.

namespace anvil {}
