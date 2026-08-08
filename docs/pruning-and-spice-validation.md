# Pruning and SPICE Validation Plan

This document records the implemented post-evaluation pruning policy and tracks
the separate work of validating numerical models against SPICE. It is not an
assertion that the current model outputs are SPICE-validated.

## Current status

- Exhaustive exploration and optional Pareto-frontier filtering are supported.
- Structurally valid candidates are canonicalized and deduplicated by their
  complete modeled-input identity before bank construction. This removes
  equivalent inactive-option settings, but it is not performance pruning.
- Constrained optimization reuses compact metrics from the exhaustive pass and
  reconstructs only the unique winning organizations; it does not model the
  entire design space a second time.
- Parallel exploration uses fixed C++ worker threads. Workers only mutate their
  own result/count/accounting slots and the output slot for their assigned outer
  geometry. After every worker is joined, the main thread merges those slots in
  canonical order and resolves exact metric ties by canonical candidate identity.
  The validated configuration and model inputs are read-only during this pass.
- `exploration.enable_pruning: true` is supported for
  `optimization.target: Exploration`. It applies constraints and then retains a
  deterministic Pareto frontier without skipping bank evaluations.
- Configuration and model-file loaders reject non-finite and physically invalid
  inputs with field-specific errors.
- Numerical golden baselines for CAM technologies and organizations are deferred
  until the corresponding models have been checked against SPICE.

## Pruning

Post-evaluation pruning uses exact comparisons over read, write, and search
latency; read, write, and search dynamic energy; area; and leakage.
Metric-equivalent candidates retain the lowest canonical identity. Focused
tests cover dominance, trade-offs, equality, constraints, fixed spaces, empty
frontiers, exhaustive parity, and deterministic parallel output.

True runtime pruning remains deferred. It requires proven model-specific bounds
that can reject a branch before constructing every bank in that branch.

## SPICE validation

### Establish reference simulations

For each supported cell technology and sensing organization, record:

- device models and model versions;
- process node, voltage, and temperature;
- cell and peripheral sizing;
- extracted or assumed parasitics;
- stimulus, initial conditions, and measurement definitions;
- simulator name, version, and solver settings; and
- source netlists, scripts, raw results, and provenance.

The initial matrix should cover SRAM/BCAM, FeFET TCAM and MCAM, MRAM, PCM, and
ReRAM configurations, including each sensing mode used by shipped examples.

### Correlate EvaCAM metrics

Compare SPICE and EvaCAM at matching operating points for:

- matchline and search latency;
- sense margin and sense-amplifier latency;
- read and search dynamic energy;
- set/reset latency and energy where the write model is supported;
- leakage; and
- peripheral contributions when they can be isolated consistently.

Define the measurement window and equation for every metric before comparing
values. Record absolute and relative error, but do not establish pass tolerances
until the expected modeling fidelity has been reviewed.

### Resolve discrepancies

For each discrepancy, determine whether it comes from a model equation, unit
conversion, parameter mapping, topology mismatch, omitted parasitic, or a SPICE
measurement definition. Model changes should cite the corresponding validation
case and preserve unrelated supported configurations.

### Add validation-backed tests

After a model and operating point are accepted:

- add a compact, versioned reference fixture containing the approved metrics and
  provenance;
- test discrete organization choices exactly;
- test floating-point metrics using an explicitly justified tolerance;
- add invariant/property checks that do not assume unverified numerical values;
  and
- retain the original SPICE artifacts outside generated build output.

Golden tests must identify which metrics are SPICE-correlated. Unvalidated
metrics must not be presented as validated merely because they appear in the
same EvaCAM result file.

## Completion criteria

Post-evaluation Pareto pruning is complete. Runtime search-space pruning remains
future work and must preserve exhaustive results for every supported model.

SPICE validation is complete per model only when the reference setup is
reproducible, metric definitions and tolerances are approved, discrepancies are
resolved or documented, and validation-backed tests are part of the automated
suite.
