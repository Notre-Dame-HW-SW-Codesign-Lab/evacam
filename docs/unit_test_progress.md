# Dedicated Unit Test Plan

## Goal

Add a focused, traceable unit test for every maintained production function and
method that does not already have one. Preserve the existing regression and
end-to-end tests, but do not treat incidental execution by those tests as a
substitute for a dedicated unit test.

This is a point-in-time plan based on the current contents of `include/`,
`src/`, `bindings/`, `evacam/`, `scripts/`, `tests/`, the `Makefile`, and
`.github/workflows/cpp-tests.yml`.

## Definition of “dedicated”

A callable is covered by a dedicated test when all of the following are true:

- The test case or table row names the callable and states the behavior it is
  checking.
- The test invokes that callable directly where practical. For a private
  helper, it drives the smallest public operation that uniquely exercises the
  helper and documents that mapping in the inventory described below.
- It checks an observable result, state transition, exception, or output. Merely
  constructing an object or reaching a line is not sufficient.
- It includes the normal case and all meaningful boundary/error branches.
- It is deterministic and does not depend on an existing `results/` file,
  network access, wall-clock timing, or test execution order.

One test source may cover a cohesive class or module; “dedicated” does not mean
one executable or one source file per function. Table-driven tests may cover
families of trivial accessors or unit conversion functions, provided every
callable has its own named row and assertion.

## Scope

The inventory must include:

- All implemented functions and methods in `src/`, including anonymous-
  namespace and file-local helpers.
- All inline production functions and methods in `include/`, including
  `Logger`, YAML helper templates, technology getters, and unit formatters.
- Public API behavior exposed through `bindings/EvaCAM_Pybind.cpp` and
  `evacam/__init__.py`.
- Reusable functions and methods in `scripts/*.py`.
- Hand-written constructors, destructors, move operations, and copy operations
  when they own resources or establish observable invariants.
- The `EvaCAM` command entry point and Python script entry points as boundary
  tests, even though they are not isolated unit-test candidates.

The following may be marked exempt, with a reason in the inventory:

- Compiler-generated or explicitly defaulted special members with no custom
  behavior.
- Pure-virtual declarations, because the concrete overrides are tested.
- Data-only structs with no behavior.
- Generated technology table data in `TechnologyTable*.inc`.
- Test helpers and functions under `tests/`.

No other callable should be silently omitted. If an internal function is hard
to reach, refactor it behind a small internal header or test it through a
uniquely observable public behavior. Do not use `#define private public`.

## Current baseline

The repository currently has good focused coverage in these areas:

- YAML node helpers, quantities, enum parsing, schema checks, and a substantial
  portion of cell, memory-device, sense-amplifier, technology, and top-level
  configuration loading.
- Input validation error cases.
- `CliOptionsParser`, `IntValueDomain`, `ExplorationSpec`,
  `ExplorationSpaceResolver`, `OutputPathBuilder`, and `VariationSampler`.
- A subset of formula helpers, wire process lookup, repeated-wire calculations,
  and wire copy behavior.
- High-level matching behavior through `EvaCAM_Match` and its Python binding.
- Monte Carlo, corner variation, H-tree routing, decoder behavior, and exhaustive
  search as regression/integration tests.

These tests should be retained. During the audit, each existing assertion must
be mapped to the callable it intentionally tests. Broad tests such as
`MatchTest.cpp`, `MatDecoderRegressionTest.cpp`, and
`HtreeRoutingRegressionTest.cpp` should not automatically mark every transitively
called method as covered.

The largest obvious dedicated-test gaps are the circuit/CAM component classes,
model aggregation classes, result/output formatting, application orchestration,
private parsing/normalization helpers, and Python script helpers.

## Phase 1: Create a callable-to-test inventory

**Status:** Implemented. The generated inventory is
`tests/unit_test_inventory.tsv`; methodology and current counts are documented
in `tests/unit_test_inventory.md`. Use `make unit-test-inventory` to regenerate
it and `make check-unit-test-inventory` to detect drift.

Create `tests/unit_test_inventory.tsv` (or an equivalently reviewable Markdown
table) with these columns:

1. Production file and line.
2. Qualified callable/signature.
3. Visibility: public, protected, private, inline, or file-local.
4. Status: `covered`, `missing`, or `exempt`.
5. Dedicated test case and test target.
6. Behaviors/branches checked.
7. Exemption or refactor note.

Generate the initial symbol list from the compiler AST or a C++-aware indexer;
do not rely only on a regular expression because this code contains overloads,
templates, inline methods, and multiline signatures. Add the Python inventory
with `ast`. Review the generated list manually for overloads, constructors,
destructors, lambdas used as behavior, and entry points.

Make this inventory the source of truth. Code coverage is a useful cross-check,
but 100% function coverage alone does not prove that each callable has a
meaningful assertion.

## Phase 2: Add common test infrastructure

**Status:** Implemented. Shared C++ helpers and explicit model builders live in
`tests/TestSupport.h` and `tests/TestModelBuilders.h`, with their focused checks
in `tests/TestSupportTest.cpp`. Use `make test-unit` for focused tests and
`make test-regression` for the slower regression and boundary tests.

Before adding many tests, introduce small shared test support under `tests/`:

- A temporary-directory/file fixture that cleans up by RAII, so new tests do
  not add more fixed `tests/tmp_*` paths or collide when run concurrently.
- `AssertNear` helpers supporting absolute and relative tolerances, plus helpers
  for finite/non-negative physical values.
- Minimal builders for `EvaCamConfig`, `Technology`, `MemCell`, `Wire`, and
  initialized CAM components. Use explicit values so tests do not depend on a
  large shipped config unless the config loader itself is under test.
- Stream-capture helpers for `PrintProperty`, logger, console, CSV, and YAML
  tests.
- Exception helpers that verify both exception type and the important part of
  the diagnostic.

Keep the existing assert-driven C++ style. Test helpers should fail loudly when
their fixture is invalid and should not reproduce production formulas to derive
expected values. Prefer hand-computed fixtures, invariants, and small reference
values.

Add a `make test-unit` aggregate that runs every focused unit-test target. Keep
slow regression/end-to-end tests in their existing targets, and provide a
separate `make test-regression` aggregate if useful. Each new executable must be
placed in `test-bin/`, its dependency file in `obj/tests/`, and its target added
to `.github/workflows/cpp-tests.yml` as required by the repository guidelines.

## Phase 3: Close gaps in pure helpers, configuration, and input

**Status:** Implemented. Dedicated targets cover the configuration helpers and
validators, technology and variation loading, output locking, YAML primitives
and units, physical-domain validation, and the cell/device, sense-amplifier, and
technology YAML loader branches listed below. File-local helpers are tied to
named public-behavior cases in the generated inventory. The unreachable legacy
cell-loader branch was removed; cell loading and validation now consistently
require the supported schema-based cell and referenced memory-device format.

Implement these first because they are fast and establish reusable fixtures.

### Configuration and exploration

- `DerivedValueHelpers`: every capacity/block-size decision and every predicate,
  including fixed dimensions, real capacity, DSE, and CSV-output choices.
- `ConfigNormalizer`: ordinary defaults, CACTI assumptions, explicit geometry,
  idempotence, and conflicting domains.
- `ConfigSectionReaders`: give each exported section reader a minimal valid node,
  missing/optional fields, aliases where supported, and an unknown-key case.
  Existing whole-file parser tests count only where they isolate a reader.
- `EvaCamConfig`: construction defaults, `ReadConfigFromFile`, deep-exploration
  toggling, `BuildResultLimits`, and `ApplyResultLimits` for null, finite, and
  multiple-result cases.
- `EvaCamConfigValidator` and `InputRuleValidator`: map every validation rule and
  accepted boundary, not only failures. Split large scenario tests into named
  rule-level cases while retaining the broad validation regression.
- `TechnologyLoader`: exact-node loading, legacy node bucketing, interpolation,
  updated versus legacy libraries, unavailable nodes, and cell loading.
- `VariationConfigBuilder`: nominal, Monte Carlo, cell/effective granularity,
  deterministic corner counts, default seed behavior, and invalid values.
- `OutputFileLock`: acquire, contention, move construction/assignment, release,
  destructor cleanup, stale/error paths, and `IsHeld`. Use a per-test temporary
  directory and a child process only for behavior that genuinely requires a
  separate process.

### YAML and physical-domain helpers

- Complete direct coverage of every `YamlHelpers` function/template, every enum
  mapping, all index and optional readers, bool-key lookup, schema matching, and
  integer range/overflow behavior.
- Cover every unit table in `YamlUnitParsers`, case sensitivity, whitespace,
  scalar versus string nodes, wrong dimensions, unknown suffixes, non-finite
  values, and unitless policies.
- Give `ValidateMemCell` and `ValidateTechnology` one valid fixture plus a named
  case for each checked field/table.
- Complete loader branch coverage for legacy and v2 cell/device files, custom
  and default sense amplifiers, and technology tables. Test path resolution
  from the owning file rather than the process working directory.

## Phase 4: Test technology and circuit primitives

### Technology and memory cell

- `Technology`: initialization from a spec, all inline getters, current-table
  expansion/interpolation at endpoints and between points, invalid alpha/spec
  handling, and printed properties.
- `MemCell`: constructor invariants, YAML delegation, memristance interpolation,
  read power, every write-energy mode, disabled operations, invalid inputs, and
  printed output.

### Formula and wire primitives

- Complete `formula.cpp` coverage for gate area/capacitance, drain capacitance,
  leakage, resistance, transconductance, Horowitz delay, and wire resistance/
  capacitance. For each, cover planar/FinFET paths, NMOS/PMOS paths, zero/invalid
  dimensions, supported temperature boundaries, and representative reference
  values.
- `Wire`: every initialization mode, passive/repeated/low-swing calculations,
  optimal and penalized repeaters, no-repeater behavior, invalid state guards,
  latency/power accumulation, copy semantics, and printing.
- `WireFactory`: local/global defaults, every configured wire/repeater type,
  low-swing flags, and preservation of configuration/technology ownership.

### Standalone circuit components

Add a focused test source for each of `FunctionUnit`, `BasicDecoder`, `Mux`,
`OutputDriver`, `Precharger`, `PredecodeBlock`, `RowDecoder`, and `SenseAmp`.
For every concrete class, test:

- constructor state and calls made before initialization;
- valid initialization plus important topology/options;
- `CalculateArea`, `CalculateRC`, `CalculateLatency`, and `CalculatePower`
  independently where the API permits;
- repeated calculation (no accidental accumulation unless intended);
- zero/minimum and representative larger sizes;
- invalid inputs and unsupported combinations; and
- `PrintProperty` output containing the component identity and key values.

Use invariants such as finite positive area, monotonic load behavior, and energy
scaling in addition to a few stable numeric reference points. Avoid snapshots of
every floating-point digit.

## Phase 5: Test CAM components and model composition

### CAM building blocks

Create cohesive class-level tests for `CAM_BasicEncoder`, `CAM_Encoder`,
`CAM_InputEncoder`, `CAM_BasicMMR`, `CAM_MMR`, `CAM_DataBuffer`,
`CAM_LevelShifter`, `CAM_OutputAccumulator`, `CAM_PriorityEncoder`,
`CAM_SenseAmp`, and `CAM_Line`. Exercise each constructor, initializer,
calculation method, custom-design path, supported optimization mode, output
method, and initialization/error guard.

### `CAM_SubArray`

Split this large surface into focused test files rather than one monolithic
test:

- initialization/topology and component selection for SRAM, resistive CAM, and
  MCAM;
- binary mismatch counting and exact/threshold result metrics;
- effective matchline and MCAM state resistance calculations;
- searchline energy, time constants, Horowitz delay, and sense margin;
- nominal, effective Monte Carlo, cell Monte Carlo, and corner resistance
  samples, including deterministic seed/stream behavior;
- timing/power variation summaries, percentiles/min/max/mean, single-point
  summaries, empty/invalid inputs, and sample read energy; and
- area, latency, power, and printed breakdown behavior.

Private file-local helpers such as corner selection, seed mixing, metric summary
construction, and CAM-model classification need explicit inventory mappings.
If public behavior cannot isolate one, move the pure calculation into a small
`detail` module with a narrow internal header and test it directly.

### Mat, bank, and result objects

- `Mat`: address-bit splitting, predecoder traversal, initialization variants,
  invalid configurations, area/RC/latency/power aggregation, and printing.
- `Bank`: initialization, binary match/evaluate delegation, all breakdown sums,
  and printed output.
- `BankWithoutHtree`: initialization and all three calculation stages for small
  and multi-mat layouts, including invalid capacity/routing cases.
- `BankWithHtree`: every routing-state transition and invalidation reason,
  horizontal/vertical bit accounting, wire-area model, level aggregation,
  first/extra/paired level reduction, final routing, and all calculation stages.
  Keep the current H-tree regression as a final reference test.
- `BankFactory`: concrete type selection and successful initialization for each
  supported routing mode, plus rejected modes/configurations.
- `Result`: initialization/reset, every optimization comparison (including ties
  and invalid candidates), CSV serialization, wire snapshots, and console
  printing.

## Phase 6: Test output and application services

### Output serialization

- Give every `UnitFormatter` conversion a named row covering zero, unit
  thresholds on both sides, negative values if allowed, and very large values.
- Test `ResultsYaml` helpers through exact parsed YAML structure rather than
  whitespace snapshots: single result, multiple results, no solutions,
  assumptions, breakdowns, all optimization targets/roadmaps, safe zero
  denominators, variation summaries, and finite formatting.
- Test `VariationSamplesCsv` header/order, numeric fields, CSV quoting, nominal,
  Monte Carlo, corner labels, empty samples, and multi-sample output.
- Test `EvaCamResultExtractor` field-by-field for complete, partially populated,
  no-solution, and variation results, including safe ratios and optional units.
- Capture and test `EvaCamConfigPrinter`, `EvaCamOutput` console summaries, YAML
  writes, sample-file writes, histogram command quoting/failure, and quiet/
  verbose behavior.

### Application orchestration

- `Logger`: enabled/disabled output, `Log`, `Verbose`, stream manipulators, move
  behavior, one-time flush, and concurrent complete-line writes.
- `EvaCamContextBuilder`: missing/unreadable inputs, CLI overrides, thread and
  output options, loaded technology/cell ownership, and normalized/validated
  configuration.
- `EvaCamExplorer`: initialization, wire candidates, best-result buffers,
  unconstrained and constrained passes, candidate validity, capacity checks,
  every constraint, tie/update/merge behavior, CSV buffering, zero solutions,
  and deterministic one-thread versus multi-thread results. Extract pure
  candidate/constraint decisions if private state prevents focused tests.
- `EvaCAM_Match`: retain the broad tests, then add named cases for every public
  overload and each validation rule across EX/BE/TH and supported BCAM/TCAM/MCAM
  paths. Cover move behavior and initialization failures. Map private LUT,
  mismatch, sense-margin, validation, and wire-construction helpers to those
  cases.
- `RunEvaCam`: option translation, quiet/stdout restoration, output override,
  success/no-solution/error behavior, and returned DTO consistency.
- `EvaCamOutput`, `ExtractEvaCamRunResult`, and the `EvaCAM` executable: add small
  boundary tests for exit codes, diagnostics, quiet mode, and output paths while
  keeping detailed logic in unit tests below the entry point.

## Phase 7: Python package, bindings, and scripts

Use Python's standard `unittest` unless the project intentionally adopts
`pytest`; avoid adding a dependency solely for this work.

- `evacam.config_lib_path`: returned type/path, installed-package resource
  behavior, and required bundled files.
- Pybind API: constructor/errors, every overload, DTO field exposure, argument
  conversion errors, move/lifetime behavior visible to Python, `run`, `match`,
  and quiet/output options. Add the currently omitted `test-pybind-match` target
  to the CI matrix.
- Migration/synchronization/generation scripts: directly test parsing,
  transformation, reference resolution, idempotence, atomic writes, discovery,
  and error handling with temporary directory trees.
- Sweep/analysis/table scripts: test CLI value parsing, spec generation, corner
  selection, resume/completion logic, derived metrics, ranking, percent helpers,
  CSV/report generation, and subprocess failure propagation without launching a
  full sweep.
- Plotting scripts: test row/column parsing, metric discovery, ordering, bounds,
  labels/formatting, default paths, empty/malformed inputs, and one headless
  smoke render per plotting entry point using Matplotlib's `Agg` backend.
- `scripts/test_variation_normality.py`: despite its filename, treat its reusable
  readers/statistical decisions as production script code and unit test them;
  keep any expensive statistical smoke check separate.

Mock only process execution and plotting/display boundaries. Prefer real
temporary files for CSV/YAML transformations so quoting and path behavior remain
covered.

## Phase 8: Coverage, CI, and completion gate

Add an optional coverage build using GCC coverage flags and `gcovr` or an
equivalent tool. Exclude `tests/`, generated `.inc` data, and third-party headers.
Use branch coverage to find untested conditions, but review misses against the
callable inventory rather than chasing a percentage alone.

Completion requires all of the following:

- Every in-scope callable has an inventory row marked `covered`, with a test
  case/row name, or `exempt`, with a reviewed reason.
- Every non-trivial callable has normal, boundary, and error-path assertions as
  applicable.
- `make test-unit` passes from a clean build.
- Existing focused and regression targets still pass; run `make test` for the
  repository's valgrind check after runtime/ownership changes.
- Every new target is present in `.github/workflows/cpp-tests.yml`, and the
  Python binding match test is no longer omitted.
- Tests pass independently and in parallel without fixed temporary-file
  collisions or dependence on test order.
- A coverage report shows no unexplained function miss. Any unreachable branch
  is documented and either removed or explicitly exempted.
- The PR description lists the inventory change, test targets run, any
  production refactors made for testability, and any remaining reviewed
  exemptions.

## Suggested implementation order

Deliver this in small reviewable changes rather than one very large PR:

1. Inventory, shared fixtures, `test-unit`, and coverage plumbing.
2. Pure helpers, configuration, YAML/unit parsers, and technology/memory cell.
3. Formula, wire, and standalone circuit classes.
4. CAM building blocks and `CAM_SubArray` calculations/variation.
5. Mat/bank/result/factory composition.
6. Output, result extraction, logger, run/context/explorer/match services.
7. Python package, binding, and script tests.
8. Final inventory audit, branch-gap cleanup, full regression run, valgrind, and
   CI matrix verification.

At the end of each step, update the inventory in the same change as the tests.
This prevents new production callables from being lost during the longer
coverage effort and makes progress measurable without weakening the final
“every function and method” goal.
