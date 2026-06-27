# BigBubbleMuff — transcribed netlist

Source: the "V7/V8 Tall Font & Bubble Font Russian" Big Muff Pi schematic traced
by **Kit Rae** ([BigMuffPage](https://www.bigmuffpage.com)), cross-checked against
the project owner's reference notes. The schematic image and those notes are
copyrighted by their authors and are kept locally only — they are **not**
redistributed in this repository. This file (factual component values) is the
**single source of truth** for the WDF model. The schematic
is the canonical four-NPN Big Muff Pi; component roles below follow from that
topology.

> Confidence: values were read from the schematic. High-confidence values are the
> ones corroborated by the reference notes and the well-documented Big Muff
> circuit. Where a designator↔role assignment is less certain it is marked
> **(reading)**. Verify against a high-resolution copy before relying on exact
> assignments for Phase B.

## Version-defining traits
- Feedback caps **C10 = C11 = C12 = 470 pF** (single caps) — the Bubble-Font change
  from the Tall Font's 2×1 nF (= 500 pF). This is the main sonic differentiator.
- Diodes **D1, D2 = KD522B** in hardware ≈ **1N914 / 1N4148** (shown on schematic).
- Transistors: Russian **3102EM**, or **2N549C / 547C** (NPN, silicon, TO-92).
- Emitter resistors **390 Ω** (rarely 430 Ω on early black-box V7).
- **R8 = 20 k** usually (rarely 39 k).
- Tone caps may vary slightly; C9 occasionally **6n8** (0.0068 µF) in series.

## Power / input
| Ref | Value  | Role                                   |
|-----|--------|----------------------------------------|
| —   | 9 V    | Supply (battery / DC jack)             |
| C14 | 22 µF  | Supply decoupling                      |
| R1  | 1.5 k  | LED series resistor (off-board)        |
| C1  | 1 µF   | Input coupling                         |
| R2  | 39 k   | Input series / pulldown                |

## Stage 1 — Q4 input booster (no clipping)
| Ref | Value  | Role                                        |
|-----|--------|---------------------------------------------|
| Q4  | 3102EM / 549C / 547C | NPN common-emitter             |
| R9  | 470 k  | Collector→base feedback bias                |
| R14 | 100 k  | Base bias to ground                         |
| R13 | 12 k   | Collector load                              |
| C4  | 0.1 µF | Output coupling toward Sustain pot **(reading)** |

## SUSTAIN / DISTORTION control
| Ref | Value  | Role                                        |
|-----|--------|---------------------------------------------|
| R24 | 100 k  | Sustain pot — variable divider Q4 → clip 1  |
| R19 | 10 k   | Series resistor in the Sustain network      |

## Stage 2 — Q3 first clipping stage
| Ref | Value   | Role                                       |
|-----|---------|--------------------------------------------|
| Q3  | 3102EM / 549C / 547C | NPN common-emitter            |
| C6  | 0.047 µF| Input coupling                             |
| R17 | 470 k   | Collector→base feedback                    |
| C12 | 470 pF  | Feedback cap (HF rolloff) — **Bubble-Font** |
| D1  | 1N914 ×2| Antiparallel diode pair (feedback clipping)|
| R20 | 100 k   | Base bias to ground                        |
| R18 | 12 k    | Collector load **(reading)**               |
| R22 | 390 Ω   | Emitter resistor                           |

## Stage 3 — Q2 second clipping stage
| Ref | Value   | Role                                       |
|-----|---------|--------------------------------------------|
| Q2  | 3102EM / 549C / 547C | NPN common-emitter            |
| C7  | 0.047 µF| Input coupling                             |
| R15 | 470 k   | Collector→base feedback                    |
| C11 | 470 pF  | Feedback cap (HF rolloff) — **Bubble-Font** |
| D2  | 1N914 ×2| Antiparallel diode pair (feedback clipping)|
| R16 | 100 k   | Base bias to ground                        |
| R12 | 12 k    | Collector load                             |
| R21 | 390 Ω   | Emitter resistor                           |
| C5  | 0.1 µF  | Inter-stage coupling **(reading)**         |
| C9  | 0.1 µF  | Coupling into tone stack (rarely 6n8)      |

## TONE control (passive tone stack)
| Ref | Value    | Role                                      |
|-----|----------|-------------------------------------------|
| R23 | 100 k    | Tone pot — bass↔treble blend at wiper     |
| C8  | 0.0039 µF| Treble (high-pass) branch cap             |
| R5  | 22 k     | Bass (low-pass) branch resistor           |
| R11 | 12 k     | Tone-stack series resistor **(reading)**  |
| R8  | 20 k     | Tone-stack shunt to ground (mid scoop)    |

## Stage 4 — Q1 output recovery
| Ref | Value  | Role                                        |
|-----|--------|---------------------------------------------|
| Q1  | 3102EM / 549C / 547C | NPN common-emitter             |
| R7  | 470 k  | Collector→base feedback bias                |
| R3  | 100 k  | Base bias to ground                         |
| R25 | 100 k  | Bias **(reading)**                          |
| R6  | 10 k   | Collector load                              |
| R10 | 390 Ω  | Emitter resistor                            |
| R4  | 2.7 k  | Emitter/output network                      |
| C13 | 1 µF   | Coupling                                    |
| C2  | 1 µF   | Output coupling                             |
| C3  | 0.1 µF | Output network                              |

## VOLUME control
| Ref | Value  | Role                                        |
|-----|--------|---------------------------------------------|
| R26 | 100 k  | Volume pot — output divider                 |

---

### Open verification items
- `(reading)` rows: confirm designator↔role against a high-res schematic.
- Confirm Q4 emitter resistor value (input stage emitter degeneration).
- Confirm exact tone-stack node connectivity (R5/R8/C8/C9 around the R23 wiper)
  before Phase B's joint-nonlinearity R-type adaptor is derived.
