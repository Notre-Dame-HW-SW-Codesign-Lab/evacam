# Python API

EvaCAM provides Python bindings through the `evacam_py` module. The Python API wraps the C++ match-evaluation interface and returns the same timing, energy, and sensing metrics as `EvaCAM_Match`.

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

## EvaCAMMatch

Create a matcher from a system config file:

```python
matcher = evacam_py.EvaCAMMatch(
    "config/2FeFET_TCAM/2FeFET_TCAM_match_system_config.yaml"
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
- TCAM `BE`: not implemented
- TCAM `TH`: not implemented
- MCAM binary vector paths: not implemented
- ACAM requires range/value inputs

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

## Mismatch-Count Array Evaluation

For TCAM exact match, use `evaluate_array(mismatch_counts)` to evaluate many rows from known mismatch counts.

```python
mismatch_counts = [0, 1, 2, 8]
results = matcher.evaluate_array(mismatch_counts)
```

This is equivalent to calling `evaluate_mismatches()` for each count.

Rules are the same as scalar mismatch-count evaluation:

- TCAM only
- `search_function: EX` only
- each count must be in range

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
    "config/2FeFET_TCAM/2FeFET_TCAM_match_system_config.yaml"
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
