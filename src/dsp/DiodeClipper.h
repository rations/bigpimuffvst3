// BigBubbleMuff — WDF antiparallel-diode clipper (one Big Muff clipping stage).
// Copyright (C) 2026  BigBubbleMuff contributors. GPL-3.0-or-later (see COPYING).
//
// Models the diode-pair feedback clipper of a Big Muff gain stage as a Wave
// Digital Filter: a resistive voltage source (the stage's output impedance)
// driving a capacitor (the 470pF feedback cap, C11/C12) in parallel with the
// antiparallel diode pair (D1/D2). The cap gives the frequency-dependent
// "muff" rolloff; the diodes give the soft clipping / sustain. The diode model
// is Werner et al.'s improved WDF diode (chowdsp DiodePairT).
//
// This is the Phase A nonlinearity. Phase B replaces the linearised BJT feeding
// this clipper with a joint BJT+diode R-type root for full circuit fidelity.
#pragma once

#include "dsp/Netlist.h"

#include <chowdsp_wdf/chowdsp_wdf.h>

namespace bbm {

class DiodeClipper {
public:
  DiodeClipper()
      : vs_(netlist::kFbR), cap_(netlist::kFbC, 48000.0f), parallel_(vs_, cap_),
        diodes_(parallel_, netlist::kDiodeIs, netlist::kDiodeVt, netlist::kDiodeN) {}

  // WDF members hold references to sibling members; the object cannot be copied
  // or moved once built. Store via pointer/in-place construction.
  DiodeClipper(const DiodeClipper &) = delete;
  DiodeClipper &operator=(const DiodeClipper &) = delete;

  void prepare(double sampleRate) {
    // Propagates the new capacitor impedance through the whole tree.
    cap_.prepare(static_cast<float>(sampleRate));
    reset();
  }

  void reset() {
    cap_.reset();
    vs_.setVoltage(0.0f);
  }

  // Processes one sample (an already-amplified stage voltage) and returns the
  // clipped voltage across the diodes. Real-time-safe, no allocation.
  inline float process(float driveVolts) noexcept {
    vs_.setVoltage(driveVolts);
    diodes_.incident(parallel_.reflected());
    const float out = chowdsp::wdft::voltage<float>(cap_);
    parallel_.incident(diodes_.reflected());
    return out;
  }

private:
  using SourceT = chowdsp::wdft::ResistiveVoltageSourceT<float>;
  using CapT = chowdsp::wdft::CapacitorT<float>;
  using ParallelT = chowdsp::wdft::WDFParallelT<float, SourceT, CapT>;
  using DiodesT = chowdsp::wdft::DiodePairT<float, ParallelT>;

  SourceT vs_;
  CapT cap_;
  ParallelT parallel_;
  DiodesT diodes_;
};

} // namespace bbm
