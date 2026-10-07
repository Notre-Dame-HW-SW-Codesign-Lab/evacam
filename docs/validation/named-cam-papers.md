# Paper comparisons for the named CAM configurations

For the subsequent original-DATE-2022 reconstruction, full-paper evidence and
additional circuit/accounting corrections, see [the original validation audit](original-evacam-validation.md).
The numerical tables below describe the earlier checkpoint.

> Historical audit before the repairs. See [implemented fixes and updated results](named-cam-fixes.md).

Audit date: 2026-10-06. The ten configurations named after conferences or
journals are **not a currently reproduced validation suite**. Eight produce
numerical results; one reports no valid solution, and one fails input
validation. Several differ substantially from the experiments named in their
filenames. Historical NVSim-CAM and Eva-CAM publications do contain silicon
and SPICE validation, but those results do not establish the accuracy of this
checkout and its present YAML inputs.

Follow-up on 2026-10-07: the [old-code investigation](old-evacam-comparison.md)
identifies sensing-mode and feature-size migration regressions, a conflicting
TCAM geometry convention, and inherited circuit limitations. In particular,
restoring current sensing and requested physical feature size changes the
ReRAM2014 diagnostic from 1.209 ns to 1.777 ns. The close original comparison
must not be treated as calibration. The ASPDAC12 capacity discrepancy also
predates YAML: its old input already used `RealCapacity (KB): 9`.

This comparison covers FeFET, MRAM, PCM, ReRAM, and SRAM CAMs with venue/year
names. Generic examples and the separately documented NAND CAMs are outside
its scope. Canonical configurations and production code were not changed.

## Current shipped results

Results below are fresh runs of `./EvaCAM -t 1 -o <result> <config>` after
`make -j4`. Latency is `summary.timing.search_latency`, energy is
`summary.power.search_dynamic_energy`, and area is the model's total area.
Units are normalized from the rounded YAML output. These are model results,
not measured device performance.

| Configuration | Logical entries × bits | System node | Area µm² | Search ns | Search pJ | Status |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| FeFET-2Fe1T-DATE-2021 | 64 × 64 | 45 nm | 7,336.676 | 0.357621 | 3.503 | Runs |
| MRAM-2T2R-ASPDAC12 | 128 × 72 requested | 140 nm | 70,399.317 | 1.244 | 59.948 | Runs with inconsistent capacity metadata |
| MRAM-4T2R-VLSIC12 | 256 × 256 | 90 nm | — | — | — | `no_valid_solutions` |
| MRAM-6T2R-VLSIC11 | 256 × 256 | 40 nm | 135,608.836 | 13.992 | 7,222 | Runs |
| PCM-2T2R-JSSC11 | 16,384 × 64 | 90 nm | 2,682,000 | 1.059 | 16,191 | Runs |
| ReRAM-2.5T1R-ISSCC16 | 64 × 256 | 65 nm | 43,918.170 | 0.612547 | 542.955 | Runs |
| ReRAM-3T1R-ISSCC15 | 64 × 64 | 90 nm | 21,142.955 | 0.542886 | 42.285 | Runs |
| ReRAM-4T2R-VLSIC14 | 64 × 16 | 40 nm | 1,247.166 | 0.188338 | 0.733 | Runs |
| SRAM-16T-ESSCIRC15 | 8,192 × 32 | 28 nm | 272,798.901 | 0.279113 | 240.557 | Runs |
| ReRAM-2T2R-VLSI21 | 256 × 256 | 40 nm | — | — | — | Missing CAM matchline port |

The VLSI21 top-level input is under `config/examples/`; the other nine are
under their corresponding `config/<name>/` directories. The MRAM VLSI12 run
exits with code zero despite returning `no_valid_solutions`; exit status alone
would incorrectly count it as a successful numerical comparison.

The logical geometry does not always equal the modeled local subarray shape.
For example, the ISSCC16 result reports a `256x64` subarray despite a logical
256-bit word, and PCM reports `64x512`. The explicit-dimension experiments
below remove some of this ambiguity. In this checkout,
`organization.subarray.dimensions` means stored entries × columns; the
electrical model subsequently rotates its internal axes. See
[`CAM_SubArray.cpp`](../../src/cam/CAM_SubArray.cpp) and
[`InputRuleValidator.cpp`](../../src/config/InputRuleValidator.cpp).

## Papers and comparison limits

### FeFET DATE 2021

Yu Qian et al., *Energy-Aware Designs of Ferroelectric Ternary Content
Addressable Memory*, DATE 2021, pp. 1090–1095,
[conference paper, Table I and Section IV-A](https://past.date-conference.com/proceedings-archive/2021/pdf/1478.pdf),
DOI `10.23919/DATE51398.2021.9474234`.

The 2FeFET-1T column reports **252.8 ps**, **0.195 fJ/bit/search**, and
**0.36 µm²/cell**, from circuit simulation using 45 nm PTM MOSFETs. The
current run gives **357.621 ps** and **0.8552 fJ/logical-bit/search**
(`3.503 pJ / 4096 bits`): numerical gaps of +41.5% and 4.39×, respectively.
Its cell footprint is 350 F², or **0.70875 µm²** at 45 nm, already 1.97× the
paper's estimate. Array dimensions, activity, peripheral boundaries, and the
paper's worst-case one-bit-mismatch timing must be reconciled before these
gaps can be called model errors. Section IV-A does not provide an explicit
array dimension alongside Table I. The cell area is a supplied input, not an
independent layout prediction.

### MRAM ASP-DAC 2012

Shoun Matsunaga et al., *Implementation of a Perpendicular MTJ-Based
Read-Disturb-Tolerant 2T-2R Nonvolatile TCAM Based on a Reversed Current
Reading Scheme*, ASP-DAC 2012, pp. 475–476,
[official abstract D1-5](https://www.aspdac.com/aspdac2012/archive/program/D1_abst.html),
DOI `10.1109/ASPDAC.2012.6164998`.

The fabricated design has **128 words × 72 bits**, 140 nm CMOS, and
**2.5 µm²/cell**. It searches words in parallel and bits serially, with
activity gating. Neither an absolute search latency nor absolute search
energy is supplied in the accessible conference abstract, so no numerical
accuracy score is assigned.

The current result reports one comparison step spanning 72 columns. Its
`physical_capacity: 9kb` is parsed as **9 KiB**, because this parser treats
`kb` as a byte unit, whereas the logical capacity is 1152 B. The resulting
allocated capacity is 73,728 bits while the physical cell count is 9,216.
The diagnostic variant resolves capacity and shape, but still does not
reproduce the paper's bit-serial operation. See
[`YamlUnitParsers.cpp`](../../src/input/YamlUnitParsers.cpp).

### MRAM VLSI Circuits 2012

Shoun Matsunaga et al., *A 3.14 µm² 4T-2MTJ-cell fully parallel TCAM based on
nonvolatile logic-in-memory architecture*, VLSI Circuits 2012,
[publisher record](https://doi.org/10.1109/VLSIC.2012.6243781).
The authors' institution confirms the
[90 nm fabricated prototype](https://www.wpi-aimr.tohoku.ac.jp/en/achievements/press/2012/20120611_000290.html).

The numerical reference used here is explicitly the reproduction of that
chip in [NVSim-CAM Table 2](https://miglopst.github.io/files/li_iccad2016.pdf):
**64 entries × 32 bits**, **2.50 ns**, and **17,118.95 µm² excluding blank
area**. The original full paper was not available during this audit. At the
smaller geometry the current model runs, but predicts **0.743150 ns** and
**28,412.419 µm²**: gaps of **−70.3%** and **+66.0%**. Sensing and area
boundaries remain unmatched. The shipped larger geometry has no valid
solution with its configured 500 mV minimum sensing margin.

### MRAM VLSI Circuits 2011

Shoun Matsunaga et al., *Fully Parallel 6T-2MTJ Nonvolatile TCAM with
Single-Transistor-Based Self Match-Line Discharge Control*, VLSI Circuits 2011,
[author presentation, slides 11 and 19](https://www.csis.tohoku.ac.jp/files/Matsunaga_2011_SymposiumonVLSI_Circuits.pdf).

The fabricated **64-word × 32-bit**, **90 nm**, **1.2 V** design reports
**0.29 ns** match delay and **10.35 µm²/cell**. The shipped input instead uses
256 × 256 and a 40 nm system node despite its 90 nm cell label. A 64 × 32,
90 nm diagnostic gives **0.808143 ns**, a **+178.7%** gap. Its 10.3494 µm²
cell footprint follows directly from the configured 1277.7 F² area; that
agreement does not validate the circuit model.

The presentation's **1.04 fJ/bit/search** result belongs to a separate HSPICE
experiment: 256 words × 144 bits, three matchline segments, and 2.8% cell
activity. It must not be paired with the small prototype or our variant's
41.624 pJ to form a measured energy error.

### PCM with the JSSC11 filename

The matching publication is Jing Li et al., *1 Mb 0.41 µm² 2T-2R Cell
Nonvolatile TCAM With Two-Bit Encoding and Clocked Self-Referenced Sensing*,
**JSSC 2014**, 49(4), pp. 896–907,
[author publication page](https://li.seas.upenn.edu/publication/li-2014-jssc/),
DOI `10.1109/JSSC.2013.2292055`. The filename's 2011 date is inconsistent
with this publication.

The fabricated 1 Mbit, 90 nm PCM design reports **1.9 ns** match delay at
nominal conditions. The shipped model's **1.059 ns** is 44.3% lower, but its
generic NVSim sensing configuration, encoding, and modeled subarrays do not
establish the same measurement boundary. Its cell YAML also says 45 nm while
the system says 90 nm. The published **0.41 µm²/cell** is a cell metric and
cannot validate the model's **2.682 mm² total area**. No comparable measured
whole-search energy was extracted from the accessible author abstract.

### ReRAM ISSCC 2016

Chien-Chen Lin et al., *A 256b-wordlength ReRAM-based TCAM with 1ns
search-time and 14× improvement in wordlength-energyefficiency-density
product using 2.5T1R cell*, ISSCC 2016, pp. 136–137,
[author institution abstract](https://scholar.nycu.edu.tw/en/publications/a-256b-wordlength-reram-based-tcam-with-1ns-search-time-and-14-im/),
DOI `10.1109/ISSCC.2016.7417944`.

The published **1 ns for 256-bit words** compares with 0.612547 ns shipped
and **0.698668 ns** when a 64-entry × 256-column subarray is explicitly
requested. The latter is 30.1% lower. The paper uses a region-splitter sense
amplifier; this config references `nvsim_vol.sense_amp.yaml`. The variant
retains the shipped capacity, voltages, and 350 K temperature; it is a
wordlength check, not a reconstruction of the full chip. The abstract does
not establish an absolute energy or macro-area reference for our outputs.

### ReRAM ISSCC 2015

Meng-Fan Chang et al., *A 3T1R nonvolatile TCAM using MLC ReRAM with
Sub-1ns search time*, ISSCC 2015, pp. 318–319,
[author institution abstract](https://scholar.nycu.edu.tw/en/publications/a-3t1r-nonvolatile-tcam-using-mlc-reram-with-sub-1ns-search-time/),
DOI `10.1109/ISSCC.2015.7063054`.

The fabricated macro comprises **2 × 64 × 64 bits in 90 nm**, with
**0.96 ns** search delay for a 64-bit word. The shipped configuration has
only 4096 logical bits and partitions across two active mats, reporting
64 × 32 local subarrays. A single explicit **64 × 64 block** predicts
**0.544482 ns**, 43.3% lower. It represents one block, not the two-block
macro. Whole-chip area and energy therefore remain incomparable, and the
paper's sensing and voltage-divider circuitry still need matching.

### ReRAM VLSI Circuits 2014

Li-Yang Huang et al., *ReRAM-based 4T2R Nonvolatile TCAM with 7× NVM-Stress
Reduction, and 4× Improvement in Speed-WordLength-Capacity for Normally-Off
Instant-On Filter-Based Search Engines Used in Big-Data Processing*,
VLSI Circuits 2014,
[official program, paper 12.2](https://archive.vlsisymposium.org/14web/wp-content/uploads/2013/06/Circ-14-program.pdf),
DOI `10.1109/VLSIC.2014.6858404`.

The fabricated macro is **128 × 32 bits in 180 nm**, with **1.2 ns** search
delay. The shipped config is 64 × 16 logical bits in 40 nm. Changing the
nominal node and explicitly setting 128 × 32 produces **1.209 ns**, just
**+0.75%** from the paper. **This is not evidence of 1% model accuracy.**
The legacy technology loader starts from the 120 nm bucket for a 180 nm
request and interpolates electrical parameters without updating its feature
size. The input node, physical dimensions, sensing, and operating conditions
are therefore not fully matched. No measured area or energy reference was
established; historical simulator projections are not measured ground truth.

### ReRAM VLSI 2021

The likely match is Haitong Li et al., *One-shot learning with
memory-augmented neural networks using a 64-kbit, 118 GOPS/W RRAM-based
non-volatile associative memory*, VLSI Technology 2021. The authors' expanded
[SAPIENS paper](https://engineering.purdue.edu/~haitongl/assets/pdf/2022_TED_SAPIENS_final.pdf)
identifies that conference predecessor in reference 8 and describes a
64-kbit, 40 nm chip operating at 200 MHz. This mapping is inferred from the
venue, capacity, node, and Eva-CAM's citation chain; the config supplies no
explicit paper citation.

SAPIENS performs **L1-distance computation and prediction**, so a clock period
cannot automatically serve as an exact-TCAM search delay. The shipped config
currently fails with `cell.ports.column must define at least one CAM matchline
port`. Relabeling a bitline would not establish the correct circuit model.
There is no current numerical comparison for this config.

### SRAM ESSCIRC 2015

The likely match is Alexander Fritsch et al., *A 4GHz, low latency TCAM in
14nm SOI FinFET technology using a high performance current sense amplifier
for AC current surge reduction*, ESSCIRC 2015, pp. 343–346,
[conference proceedings contents](https://www.proceedings.com/content/028/028141webtoc.pdf),
DOI `10.1109/ESSCIRC.2015.7313897`.

This attribution is tentative: the venue matches, but the input supplies no
explicit citation and uses **28 nm**, whereas the paper is **14 nm SOI
FinFET**. Its published 4 GHz operating frequency is not an interchangeable
measurement of the model's 0.279113 ns search latency. The primary full paper
was unavailable, so no absolute delay, energy, or area accuracy score is
assigned. The shipped energy normalizes to 0.9177 fJ/logical-bit/search, but
this value alone establishes no agreement with the chip.

## Separate geometry and nominal process experiments

These variants preserve the existing electrical/device parameters, sensing
models, temperature, and optimization target. They change only the listed
geometry, nominal node, or capacity fields, plus path resolution. They are
diagnostics rather than calibrated paper replicas. No parameter was fitted
to a target result.

| Variant | Changes from shipped config | Area µm² | Search ns | Search pJ | Paper ns | Numerical latency gap |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| MRAM-6T2R-VLSIC11 | 90 nm system/cell; 256 B; 32-bit words; explicit 64 × 32 | 48,470.996 | 0.808143 | 41.624 | 0.29 | +178.7% |
| MRAM-4T2R-VLSIC12 | 256 B; 32-bit words; explicit 64 × 32 | 28,412.419 | 0.743150 | 39.301 | 2.50 | −70.3% |
| ReRAM-4T2R-VLSIC14 | Nominal 180 nm system/cell; 512 B; 32-bit words; explicit 128 × 32 | 49,904.052 | 1.209 | 64.284 | 1.20 | +0.75% |
| ReRAM-3T1R-ISSCC15 | Single mat; 512 B; explicit 64 × 64 | 20,197.953 | 0.544482 | 21.677 | 0.96 | −43.3% |
| ReRAM-2.5T1R-ISSCC16 | Explicit 64 × 256 | 71,699.509 | 0.698668 | 160.523 | 1.00 | −30.1% |
| MRAM-2T2R-ASPDAC12 | Explicit 128 × 72; physical capacity 1152 B | 69,933.395 | 1.596 | 94.535 | — | — |

The signed gap is `(model / paper - 1) × 100`, computed from the printed
results. It is not a controlled model-error estimate. Initial attempts to
model the ISSCC15 two-block macro with two active mats failed the explicit
word-width validator; their errors and inputs are preserved in the variant
manifest. The successful single-block run is the one used above.

The node-bucketing issue also affects requests such as 140, 40, and 28 nm
when using the legacy technology file. See
[`TechnologyLoader.cpp`](../../src/config/TechnologyLoader.cpp),
[`Technology.cpp`](../../src/technology/Technology.cpp), and
[`cmos.legacy.yaml`](../../config/lib/technology/cmos.legacy.yaml).
Consequently, merely aligning the node text in YAML is insufficient for a
physical comparison.

## What the historical validation establishes

The [NVSim-CAM ICCAD 2016 paper](https://miglopst.github.io/files/li_iccad2016.pdf),
Table 2, compares fabricated MRAM, ReRAM, and PCM with its then-current
model. It reports actual/projected search times of **2.50/2.571 ns**,
**1.20/1.14 ns**, and **1.90/1.85 ns**, respectively. Its energy rows contain
simulator predictions with no measured energy values. Section 4.3 explains
that custom sense-amplifier characterization improves agreement; matching
the cell family alone is insufficient.

The [Eva-CAM DATE 2022 paper](https://past.date-conference.com/proceedings-archive/2022/pdf/0130.pdf),
Tables I–II, additionally reports chip comparisons and 2FeFET SPICE
comparisons. Examples are **270 versus 268.5 pJ** for its RRAM comparison,
**1.9 versus 2.1 ns** for PCM, and **2.5 versus 2.72** for MRAM. The MRAM
table labels this last row in ps; that appears to be a unit typo given the
2.5 ns reference in NVSim-CAM. Its two-FeFET SPICE validation concerns a
different topology from the 2FeFET-1T DATE21 input.

Thus the earlier blanket claim that this model family lacked physical
validation would be incorrect. The narrower supported conclusion is that
**the current named configurations do not reproduce that historical
validation**, and this audit establishes no per-device accuracy bound.

## Reproduction artifacts

The build was already up to date under `make -j4`. The working tree contained
pre-existing modifications, so HEAD alone does not identify these results.
HEAD was `2c88007cf833f8639897d86222265cf850b30378`; the executable SHA-256 was
`ddbac4d3dbc44ad45ab5b3cc706057299e9ef72a6c2d20b360b83da8b569fb5f`.

- [Run commands and exit codes](../../output/validation/named-cam-papers/runs.json).
- [Raw logs and result YAMLs](../../output/validation/named-cam-papers/).
- [Variant inputs, commands, and before/after records](../../output/validation/named-cam-papers/variants/changes.json).
- [Machine-readable result summary](data/named-cam-papers.results.json).
- [Source and input hashes](../../output/validation/named-cam-papers/provenance.json).

Each raw log/result uses the corresponding configuration name. Replaying a
command uses the current referenced configuration files; compare provenance
hashes first if the working tree has changed. The variant YAMLs contain
absolute paths for this workspace. No production tests were added or run
for this research-only comparison.

The next validation work should resolve the geometry/capacity discrepancies,
technology sizing, and the two failing examples, then create fixtures with
paper-specific sensing, activity, voltages, and measurement boundaries.
The close ReRAM2014 latency is a useful point to investigate, but the MRAM,
FeFET, and other ReRAM gaps preclude a general accuracy claim today.
