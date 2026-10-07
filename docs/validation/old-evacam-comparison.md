# Why the named CAM results differ from older EvaCAM

> Historical audit before the repairs. See [implemented fixes and updated results](named-cam-fixes.md).
> For the controlled comparison after the analytical circuit changes, see
> [matched public and current executables](matched-legacy-evacam.md).

Audit date: 2026-10-07. **The discrepancies combine inherited example
configurations, real migration regressions, and changes to the circuit model.**
Restoring every old number would restore known bugs too. Repair configuration
and geometry semantics first, then validate individual circuit paths before
fitting whole-chip results.

This follows the [paper comparison](named-cam-papers.md). Experiments ran in
temporary copies. Production code, the normal executable, and canonical
configs were not changed. Numerical differences below apply to the specified
inputs and versions; they are not device accuracy bounds.

## Versions and comparison method

| Version | Source and qualification |
| --- | --- |
| Public upstream | Pristine source from local upstream commit `3289a855c7ad4b74edd60920a9c5c6dc2edcad9b`, dated 2025-04-06, remote `https://github.com/eva-cam/EvaCAM.git`. Extracted committed source to avoid the old checkout's local modifications. |
| Local HV snapshot | `/home/jbech002/Research/Eva-CAM_HV_Nov_25`, compiled from source in a temporary copy. A later local model, not a claimed publication release. |
| Current | Working tree based on `2c88007cf833f8639897d86222265cf850b30378`, including pre-existing modifications. The normal binary is identical to the paper audit's binary. |

The named `.cfg`/`.cell` examples are **untracked additions in the upstream
checkout**, not files shipped in the examined upstream commit. Thus these
are old-code runs with local legacy examples, not the authors' exact
validation setups. That upstream commit contains the root two-FeFET example.

Upstream builds with `g++ -std=c++11`; the modern default language mode
conflicts with its global enum named `data`. Output-only instrumentation
records actual bank search latency and energy. Both old versions' console
summaries label **read latency** as “Search Latency.” Missing relative cell
references were corrected only in the temporary copies.

## Confirmed migration regressions

### Current sensing became voltage sensing

Commit `1d634ac` (2026-07-10) removed `amplifier_type: nvsim_cur` from the
ReRAM 4T2R, 3T1R, and 2.5T1R architecture files and replaced it with references
to `nvsim_vol.sense_amp.yaml`. When `sensing_mode` is absent,
[`ResolveSensingNode`](../../src/config/EvaCamYamlLoader.cpp) takes the mode
from that library file's name. This changed behavior, not just filenames.

Changing **only** the selection back to `nvsim_cur`, with the normal current
executable and shipped geometry, produces:

| Config | Current ns | Restored sensing ns | Current pJ | Restored sensing pJ |
| --- | ---: | ---: | ---: | ---: |
| ReRAM-4T2R-VLSIC14 | 0.188338 | 1.258 | 0.733 | 10.961 |
| ReRAM-3T1R-ISSCC15 | 0.542886 | 1.073 | 42.285 | 183.700 |
| ReRAM-2.5T1R-ISSCC16 | 0.612547 | 1.233 | 542.955 | 1785 |

Current sensing adds the library's characterized current-to-voltage converter
latency and energy. This explains much of the unusually fast current ReRAM
output. Restoring the selection still uses a generic NVSim sense amplifier,
not the papers' specialized circuits.

**Action:** restore the intended modes explicitly; test the resolved mode
and its converter contribution before fitting any device parameters.

### Technology migration lost the requested physical feature size

Before `1d634ac`, `Technology::Initialize(requestedNode, ...)` stored
`featureSize = requestedNode * 1e-9` even when electrical parameters came
from a nearby table bucket. The current
[`TechnologyLoader.cpp`](../../src/config/TechnologyLoader.cpp) builds from
the bucket's specification. Interpolation changes electrical parameters but
leaves that bucket's physical feature size.

| Requested legacy node | Current physical feature size | Old physical feature size |
| ---: | ---: | ---: |
| 28 nm | 22 nm | 28 nm |
| 40 nm | 32 nm | 40 nm |
| 140 nm | 120 nm | 140 nm |
| 180 nm | 120 nm | 180 nm |

This changes dimensions, transistor sizing, parasitics, and sensing-library
threshold selection. Preserving requested dimensions in a diagnostic moves
the shipped SRAM example from **272,798.901 to 441,889.129 µm²**. It also
makes the large MRAM6 example infeasible under the current sensing check.
Electrical tables and interpolation coefficients were unchanged; this is
not a fully calibrated process model.

**Action:** distinguish requested node, table selection, and physical feature
size; preserve their intended meanings and emit them in results. Test
intermediate nodes as well as exact table nodes. Review old interpolation
rules independently rather than assuming all legacy coefficients are correct.

### The TCAM geometry path conflicts with the new axis rotation

The old main program passes `capacity_bits / word_width` as the bank's
`blockSize`. Electrical rows therefore represent word bits and columns
represent entries. The current
[`EvaCamExplorer.cpp`](../../src/app/EvaCamExplorer.cpp) retains that
calculation for ordinary TCAMs. Commit `e0d3f64` (2026-08-26) also introduced
`std::swap(numRow, numColumn)` in
[`CAM_SubArray.cpp`](../../src/cam/CAM_SubArray.cpp), expecting incoming rows
to represent entries. Explicit-dimension inputs and MCAMs follow the newer
convention; ordinary legacy-shaped TCAM inputs do not.

This explains the reported 256 × 64 local subarray for the logical
64-entry × 256-bit ISSCC16 example. Correcting the explorer's incoming
geometry changes its energy from **542.955 to 160.522 pJ** and latency from
**0.612547 to 0.697847 ns**, without changing sensing. Explicitly requesting
64 × 256 gives 160.523 pJ / 0.698668 ns.

**Action:** use one geometry convention across explorer, bank, subarray,
comparison width, and output. Test rectangular arrays against equivalent
explicit-dimension inputs. Do not remove the swap globally: newer callers
already need it. PCM and SRAM partition settings also need migration;
changing the explorer formula alone makes their current inputs infeasible.

## Older defects and changes to the circuit model

### Printed latency and search schedules are different

Upstream ASPDAC12 prints **1.283 ns**, but its actual bank search latency is
**2.991 ns**. HV prints about **1.524 ns**, but actual search latency is
**12.455 ns**. The current
[`ResultConsole.cpp`](../../src/model/ResultConsole.cpp) correctly prints the
search field. Old console text is therefore not a directly comparable target.

The ASPDAC12 runs also use different serial comparison widths: 16 bits in
upstream, 8 in HV, and 72 in one current step. Current
[`ReadMemorySection`](../../src/config/ConfigSectionReaders.cpp) initializes
comparison width to the full word. Upstream additionally truncates the
serial-step count with integer division for 72/16.

**Action:** compare identical schedules and actual search fields. Specify
comparison width explicitly for serial designs and define how incomplete
final steps are counted.

### Public upstream has a dimensionally invalid ReRAM equation

Its current-read time constant contains
`wire_resistance * (line_capacitance + BitSerialWidth)`. Nearby exploratory
equations include a `0.6e-13` capacitance factor, but the result-producing
equation omits it. Adding a dimensionless count to capacitance yields
approximately **165 seconds**, **4443 seconds**, and **78617 seconds** for
the three named ReRAM examples. HV had already replaced this branch with
the generalized matchline model that led to the current implementation.

**Action:** do not use those outputs as golden targets or restore this
branch. Keep dimensional and independent circuit checks.

### Matchline capacitance needs a physical accounting review

Current [`MatchlineTau`](../../src/cam/CAM_SubArray.cpp) includes line
capacitance in the wire-resistance term, but not the cell-resistance term.
The latter includes cell-access, mux, precharger, and sensing capacitance.
HV already had this structure; public upstream included line capacitance
in the cell-resistance term too.

The discharge path must also drain the wire capacitance. Adding **wire
capacitance only**, reconstructed using the existing wire model, changes the
small MRAM4 diagnostic from **0.743150 to 0.905485 ns**, and the square
FeFET example from **0.357621 to 0.768525 ns**. These are sensitivity
experiments, not validated corrections.

Blindly adding all of `Col.cap` is unsafe: it also contains device loads
that overlap `capCellAccess`. In addition,
[`CAM_Line.cpp`](../../src/cam/CAM_Line.cpp) computes the diode gate term as
`gateCap * numCell * (numCell * numCmos)`, which scales quadratically with
line length. That behavior predates this checkout. A broad full-`Col.cap`
addition makes small MRAM4 2.951 ns, close to upstream's 2.912 ns, but may
double count devices and inherit that quadratic term. Agreement alone does
not justify that formula.

**Action:** separate wire and individual device-terminal loads, count each
once, and compare the RC network against an independent transient solver
or SPICE. Check linear scaling of repeated identical cell loads.

### The NVM discharge flag no longer reaches the model

HV reads the cell-level `isNVMdischarge` flag. The current equations still
consult `cell.isNVMdischarge`, but it defaults to false and no production
input path sets it. The memory-device schema accepts
`match.is_nvm_discharge` without loading it. Port flags are parsed into a
different member that these equations do not read. The v2 migration bypassed
the old `ReadMatchSection`, later removed in `51d92b7`.

In the small-MRAM4 diagnostic, setting both accepted flag locations to true
produces identical results. Multiplying both MTJ resistances by 100 leaves
the **223.518 ps matchline delay** unchanged, although total latency changes
through peripheral sizing. This demonstrates a disconnected control, not
that every MRAM topology should put its NVM in the discharge path. The old
public model constructed resistance from the access-device type; HV/current
construct it from ports and discharge participation.

**Action:** define whether discharge participation belongs to the cell or
port, connect the selected input to the appropriate equations, and reject
accepted-but-ignored settings. Check the schematic before deciding whether
the NVM should carry matchline current or only control a CMOS path.

### Some current differences improve correctness

HV used `exp(-referDelay * tau)` for all-match decay, where an RC model
requires a time divided by a time constant. It also used different load
inventories for match and mismatch states. Current code uses
`exp(-referDelay / tau)` and shared loads. Old acceptance of the large
MRAM4 array therefore does not prove its 500 mV sensing requirement was
achievable. Validate the margin waveform rather than weakening the threshold.

Old HV bank search energy also starts from one subarray's energy without
consistently multiplying by all searched subarrays. Current
[`BankWithHtree.cpp`](../../src/model/BankWithHtree.cpp) explicitly includes
the bank/mat/subarray count. The old SRAM result, **1.086 pJ**, and current
**240.557 pJ** cannot be interpreted as a physical energy regression without
reconciling that accounting boundary. The old smaller number is not inherently
more accurate.

## Mismatches already present in the old inputs

- **ReRAM VLSI14:** 40 nm, 128 B, and 16-bit words instead of the paper's
  180 nm, 128-entry × 32-bit macro.
- **MRAM VLSI11:** 40 nm system and 256 × 256 instead of the small 90 nm chip.
- **MRAM VLSI12:** 256 × 256 instead of the 64 × 32 validation instance.
- **FeFET DATE21:** 350 F² cell area, inconsistent with the paper's 0.36 µm²
  estimate at 45 nm.
- **PCM:** the `JSSC11` name and mixed cell/system node labels.
- **ASPDAC12:** `RealCapacity (KB): 9`. The current byte-valued `9kb`
  preserves a legacy field; this is not solely a new lowercase-unit parser
  mistake. Old code uses it in an ad hoc geometry override, so changing it
  to 1152 B in isolation is not a safe migration.
- **ReRAM VLSI21:** missing matchline topology. Public upstream reports no
  matchline too; current validation catches it earlier.

The old filenames are evidence of design inspiration, not exact reproduction.
Publication details and primary references are in the
[paper audit](named-cam-papers.md).

## Comparisons after restoring selected old settings

These use the previous audit's separate geometry variants. They restore
selected semantics, not full paper-specific operating conditions or sensing.

| Example | Previous comparison ns | Diagnostic ns | Context |
| --- | ---: | ---: | --- |
| ReRAM VLSI14, nominal 180 nm, 128 × 32 | 1.209 | 1.777 | Restore current sensing and requested feature size; paper 1.2 ns |
| ReRAM ISSCC15, single 64 × 64 block | 0.544482 | 1.075 | Restore current sensing; paper 0.96 ns for two-block macro |
| ReRAM ISSCC16, explicit 64 × 256 | 0.698668 | 1.319 | Restore current sensing; paper headline 1 ns |
| MRAM VLSI12, 90 nm, 64 × 32 | 0.743150 | 2.912 in public upstream | Different resistance/load model; paper reference 2.5 ns |
| MRAM VLSI11, 90 nm, 64 × 32 | 0.808143 | 0.745350 in public upstream | Much of gap to paper's 0.29 ns predates current implementation |

The shipped FeFET run is **0.481134 ns / 2.46547 pJ** in public upstream,
**0.288803 ns / 3.19551 pJ** in HV, and **0.357621 ns / 3.503 pJ** now.
HV already normalizes to about 0.780 fJ/bit/search versus the paper's 0.195;
recent changes do not explain the whole energy gap.

## Recommended order of work

1. **Repair configuration semantics.** Restore intended sensing modes,
   physical feature size, rectangular TCAM geometry, and explicit serial
   schedules. Test resolved physical quantities and equivalent inputs.
2. **Audit the shared matchline circuit.** Resolve discharge participation,
   capacitance accounting, diode scaling, and sensing-time/margin equations
   against small independent circuit references. Keep valid rejection behavior.
3. **Create separate paper-reproduction fixtures.** Record process, voltage,
   temperature, organization, cell footprint, sensing, activity, encoding,
   and included peripherals. Keep exploration examples separate. Start with
   FeFET DATE21 and one ReRAM macro; add diode MRAM after checking that path.
4. **Re-establish device accuracy claims.** Use SPICE or measured quantities
   as physical references. Treat old-code agreement as a separate software
   continuity check. Do not fit resistance or energy constants merely to
   recover a headline number.

The semantic regressions warrant fixes. The circuit findings warrant an
independent model audit. Neither calls for dropping all non-flash support
or rolling the entire tool back.

## Reproduction evidence

- [Upstream source/config provenance](../../output/validation/old-cam-comparison/baseline.json) and [HV source hashes](../../output/validation/old-cam-comparison/hv-source.json).
- [Upstream actual search metrics](../../output/validation/old-cam-comparison/instrumented/runs.json) and [HV actual search metrics](../../output/validation/old-cam-comparison/hv/runs.json).
- [Fifty current-code ablations](../../output/validation/old-cam-comparison/ablations/runs.json).
- [Sensing-only comparisons](../../output/validation/old-cam-comparison/sensing/runs.json).
- [Geometry variants with restored sensing and feature size](../../output/validation/old-cam-comparison/paper-sensitivities/runs.json).
- [NVM flag/resistance sensitivity](../../output/validation/old-cam-comparison/mram-resistance/runs.json) and [wire-only sensitivity](../../output/validation/old-cam-comparison/wire-only-cap/runs.json).
- [Build commands, switches, and source hashes](../../output/validation/old-cam-comparison/provenance.json).

Logs, result YAMLs, diagnostic patches, and inputs accompany the manifests.
All 50 geometry/node ablations were replayed against the final diagnostic
binary with identical exit status, latency, and energy. With switches
disabled it reproduces the prior audit's eight numerical results and two
failures. No production patch or production test-suite run was part of this
investigation.
