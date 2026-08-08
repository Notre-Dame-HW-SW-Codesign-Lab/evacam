# Pruning and SPICE Validation Plan

This document tracks two pieces of deferred EvaCAM work: defining a real
exploration-pruning policy and validating numerical models against SPICE. It is
not an assertion that the current model outputs are SPICE-validated.

## Current status

- Exhaustive exploration is the supported behavior.
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
- `exploration.enable_pruning: true` is rejected because pruning semantics have
  not yet been agreed upon.
- Configuration and model-file loaders reject non-finite and physically invalid
  inputs with field-specific errors.
- Numerical golden baselines for CAM technologies and organizations are deferred
  until the corresponding models have been checked against SPICE.

## Pruning

### Define the contract

Before enabling pruning, document and approve:

- whether pruning means Pareto-frontier filtering, constraint-based early exit,
  or both;
- which metrics participate in dominance comparisons;
- how metric equality and floating-point tolerances are handled;
- whether constraints are applied before or after dominance checks;
- how equivalent organizations are deduplicated;
- whether a pruned exploration CSV contains all valid candidates or only retained
  candidates; and
- the required behavior for fixed-organization and single-target runs.

### Implement the selected policy

Keep exhaustive enumeration as the reference path. Add pruning as an explicit
stage with independently tracked counts for:

1. candidates evaluated;
2. candidates passing structural and numerical validation;
3. candidates passing user constraints; and
4. candidates retained after pruning.

Parallel execution must produce deterministic retained candidates and counts.
With pruning disabled, candidate enumeration, selected results, console output,
and result files must remain unchanged.

### Test pruning

Use a small, deterministic exploration space containing dominated candidates,
non-dominated candidates, equal-metric duplicates, and candidates rejected by
constraints. Assert the exact count at every stage and the retained design
identities. Also compare pruning-disabled results with the exhaustive reference
path.

Do not remove the current rejection of `enable_pruning: true` until the policy,
implementation, reporting, and regression tests are complete.

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

Pruning is complete when its semantics are documented, enabled, deterministic,
counted, and regression-tested without changing pruning-disabled results.

SPICE validation is complete per model only when the reference setup is
reproducible, metric definitions and tolerances are approved, discrepancies are
resolved or documented, and validation-backed tests are part of the automated
suite.
