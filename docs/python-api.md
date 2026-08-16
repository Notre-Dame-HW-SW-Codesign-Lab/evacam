# Python API

EvaCAM provides Python bindings through the `evacam_py` module. The Python API can run the simulator and return structured full-run results, and it also wraps the C++ match-evaluation interface for per-query match modeling.

## Build

Run the Python binding test from the repository root. This builds the extension module before running the test:

```bash
make test-pybind-match
```

The built extension is written to the repository root with the Python ABI suffix, for example `evacam_py.cpython-312-x86_64-linux-gnu.so`.

## Import

Run Python from the repository root, or make sure the built extension is on `PYTHONPATH`.

```python
import evacam_py
```

The package also ships the default v2 config library used by installed Python
workflows:

```python
import evacam

config_lib = evacam.config_lib_path()
```

`config_lib` points at the installed `config/lib` defaults, including
technology, sensing, and sense-amplifier YAML files.

## Full Simulator Run

Use `evacam_py.run()` to execute the same simulator path used by the CLI and return raw SI-valued metrics to Python.

```python
result = evacam_py.run(
    "config/2FeFET_TCAM/2FeFET_TCAM.config.yaml",
    threads=1,
    output_yaml_path=None,
    write_yaml=False,
    stdout=False,
    verbose=False,
    variation_plots=False,
)
```

Signature:

```python
evacam_py.run(
    config_path: str,
    *,
    threads: int = 1,
    output_yaml_path: str | None = None,
    write_yaml: bool = False,
    stdout: bool = False,
    verbose: bool = False,
    variation_plots: bool = False,
) -> EvaCAMRunResult
```

Arguments:

- `config_path`: required system YAML path.
- `threads`: number of exploration threads. Values less than or equal to zero fall back to one thread.
- `output_yaml_path`: optional YAML output path. If omitted with `write_yaml=True`, EvaCAM uses the same default output path logic as the CLI.
- `write_yaml`: when true, writes the existing EvaCAM results YAML format.
- `stdout`: when false, suppresses C++ stdout from the run. This defaults to false for Python embedding and modeling workflows.
- `verbose`: enables the existing EvaCAM verbose logger. If `stdout=False`, verbose output is still suppressed from stdout.
- `variation_plots`: enables Monte Carlo variation histogram SVG generation when YAML/sample files are written.

`run()` releases the Python GIL while EvaCAM is modeling. Independent calls may
therefore execute concurrently from different Python threads. Each call owns
its stdout policy: a quiet call does not redirect or suppress output from a
simultaneous call with `stdout=True`. When concurrent calls write files, give
them distinct output paths; EvaCAM rejects simultaneous ownership of the same
result path.
Configuration parsing is briefly serialized because the linked `yaml-cpp`
decoder uses shared process state; exploration and result generation remain
concurrent after each configuration has loaded.

The return object has:

- `num_solutions`: number of valid solutions found.
- `exploration_csv_path`: path to the exploration CSV for full-exploration runs, or an empty string.
- `output_yaml_path`: YAML path used when `write_yaml=True`, or an empty string.
- `best_results`: dictionary keyed by optimization target name.

Example:

```python
result = evacam_py.run(
    "config/2FeFET_TCAM/2FeFET_TCAM.config.yaml",
    write_yaml=True,
    output_yaml_path="results/python_run.yaml",
)

search = result.best_results["SearchLatency"]
print(search.summary["timing.search_latency_s"])
print(search.summary["energy.search_dynamic_j"])
print(search.breakdown["search_latency.matchline_s"])
print(search.geometry["bit_serial_width"])
```

`EvaCAMDesignResult` fields:

- `optimization_target`: target name, for example `SearchLatency`.
- `summary`: raw SI-valued top-level metrics keyed by dotted names.
- `breakdown`: raw SI-valued component breakdown metrics keyed by dotted names.
- `geometry`: selected raw geometry and design settings.
- `variation`: Monte Carlo summary and sample data when variation is enabled.

## EvaCAMMatch

Create a matcher from a run config file:

```python
matcher = evacam_py.EvaCAMMatch(
    "config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml"
)
```

## Word Width

`word_width()` returns the configured word width in bits.

```python
width = matcher.word_width()
```

## Match Results

Evaluation methods return `EvaCAMMatchResult`.

Fields:

- `hit`: boolean match result
- `search_latency`: search latency in seconds
- `search_dynamic_energy`: search dynamic energy in joules
- `matchline_delay`: matchline delay in seconds
- `sense_margin`: sense margin in volts

Example:

```python
result = matcher.evaluate_mismatches(1)

print(result.hit)
print(result.search_latency)
print(result.search_dynamic_energy)
print(result.matchline_delay)
print(result.sense_margin)
```

## Binary TCAM Vector Evaluation

Use `evaluate_vector(stored, query)` to evaluate a concrete stored word and query word.

```python
stored = [0] * matcher.word_width()
query = [0] * matcher.word_width()

result = matcher.evaluate_vector(stored, query)
```

For TCAM, `stored` may contain:

- `0`
- `1`
- `-1` wildcard

`query` must contain only:

- `0`
- `1`

The vector length must equal `matcher.word_width()`.

Current support:

- TCAM `EX`: supported
- TCAM `BE`: supported through array evaluation
- TCAM `TH`: use explicit threshold evaluation with `max_mismatches`
- MCAM `EX`: supported for integer vectors and arrays; symbols must be in `0..num_resistance_state-1`
- MCAM `BE` and `TH`: not implemented
- ACAM requires range/value inputs

For an eight-state MCAM configuration:

```python
stored = [index % 8 for index in range(matcher.word_width())]
query = stored.copy()

exact = matcher.evaluate_vector(stored, query)
assert exact.hit

query[0] = (query[0] + 1) % 8
miss = matcher.evaluate_vector(stored, query)
assert not miss.hit

rows = matcher.evaluate_array([stored, query], stored)
```

MCAM exact results include query-specific searchline energy and matchline
delay. The semantic hit remains strict element-wise equality even when sampled
state variation is enabled.

## Threshold TCAM Evaluation

Use `evaluate_threshold(stored, query, max_mismatches)` when the hit rule should accept up to an inclusive mismatch count.

```python
stored = [0] * matcher.word_width()
query = [0] * matcher.word_width()

result = matcher.evaluate_threshold(stored, query, max_mismatches=2)
```

`max_mismatches=2` means `0`, `1`, or `2` mismatches are hits. The returned timing, energy, matchline delay, and sense margin describe the actual stored/query mismatch count; only `hit` is decided by the threshold rule.

The method is TCAM-only. `max_mismatches` must be between `0` and `matcher.word_width()`, inclusive. EvaCAM also checks the modeled sense margin at the boundary between `max_mismatches` and `max_mismatches + 1`; if that boundary cannot be sensed, the method raises `RuntimeError`. `max_mismatches == matcher.word_width()` accepts every mismatch count and has no miss boundary to check.

The same operation is available from mismatch counts:

```python
result = matcher.evaluate_threshold(1, max_mismatches=2)
```

For `search_function: TH`, plain `evaluate_vector(stored, query)` requires an explicit threshold and raises `RuntimeError`; call `evaluate_threshold(...)` instead.

## Mismatch-Count Evaluation

For TCAM exact match, use `evaluate_mismatches(mismatches)` to evaluate directly from a mismatch count instead of passing stored/query vectors.

```python
result = matcher.evaluate_mismatches(3)
```

This is equivalent to evaluating a TCAM exact-match vector pair with three non-wildcard mismatches.

Rules:

- only valid for TCAM
- only valid for `search_function: EX`
- mismatch count must be between `0` and `matcher.word_width()`, inclusive

Invalid values raise `ValueError`:

```python
matcher.evaluate_mismatches(-1)
matcher.evaluate_mismatches(width + 1)
```

## Array Evaluation

Use `evaluate_array(stored_rows, query)` to evaluate many stored rows against one query.

```python
rows = [
    [0] * matcher.word_width(),
    [1] + [0] * (matcher.word_width() - 1),
]
query = [0] * matcher.word_width()

results = matcher.evaluate_array(rows, query)
```

`results` is a list of `EvaCAMMatchResult`, one per stored row.

For `search_function: EX`, each row is evaluated independently with exact-match hit semantics.

For `search_function: BE`, the full array is evaluated as one best-match operation:

- the row or rows with the minimum mismatch count are returned with `hit=True`
- all tied best rows are hits
- non-best rows are returned with `hit=False`
- no priority selector or multiple-match resolver is applied

EvaCAM validates that the modeled sense margin can distinguish the best mismatch class from the next mismatch class. If the best mismatch count or best-vs-next boundary is beyond the detectable range, `evaluate_array(...)` raises `RuntimeError`.

## Mismatch-Count Array Evaluation

Use `evaluate_array(mismatch_counts)` to evaluate many rows from known mismatch counts.

```python
mismatch_counts = [0, 1, 2, 8]
results = matcher.evaluate_array(mismatch_counts)
```

For `search_function: EX`, this is equivalent to calling `evaluate_mismatches()` for each count.

For `search_function: BE`, this applies the same best-match array semantics described above, but skips stored/query vector mismatch counting.

Rules:

- TCAM only
- supported for `search_function: EX` and `BE`
- each count must be in range
- the count array must not be empty for `BE`

## ACAM Range/Value Inputs

ACAM vector evaluation uses ranges for stored values and scalar query values.

```python
stored = [(0.0, 1.0)] * matcher.word_width()
query = [0.5] * matcher.word_width()

result = matcher.evaluate_vector(stored, query)
```

ACAM exact, best, and threshold evaluation paths are currently not implemented and raise `RuntimeError`.

## Error Handling

The bindings translate standard C++ exceptions into Python exceptions:

- `std::invalid_argument` becomes `ValueError`
- `std::runtime_error` becomes `RuntimeError`

Common examples:

```python
matcher.evaluate_vector([0], [0])
matcher.evaluate_vector(stored, [2] * width)
matcher.evaluate_mismatches(width + 1)
```

## Minimal Example

```python
import evacam_py

matcher = evacam_py.EvaCAMMatch(
    "config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml"
)

width = matcher.word_width()
stored = [0] * width
query = [0] * width
query[0] = 1

by_vector = matcher.evaluate_vector(stored, query)
by_count = matcher.evaluate_mismatches(1)

assert by_vector.hit == by_count.hit
assert by_vector.matchline_delay == by_count.matchline_delay

print(f"hit: {by_count.hit}")
print(f"matchline delay: {by_count.matchline_delay} s")
```
