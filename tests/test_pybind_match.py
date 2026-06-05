#!/usr/bin/env python3

import math
import pathlib
import sys


def mismatched_query(width, mismatches, offset=0):
    query = [0] * width
    for index in range(mismatches):
        query[(offset + index) % width] = 1
    return query


def assert_close(lhs, rhs):
    assert lhs.hit == rhs.hit
    assert math.isclose(lhs.search_latency, rhs.search_latency, rel_tol=0.0, abs_tol=1e-18)
    assert math.isclose(lhs.search_dynamic_energy, rhs.search_dynamic_energy, rel_tol=0.0, abs_tol=1e-18)
    assert math.isclose(lhs.matchline_delay, rhs.matchline_delay, rel_tol=0.0, abs_tol=1e-18)
    assert math.isclose(lhs.sense_margin, rhs.sense_margin, rel_tol=0.0, abs_tol=1e-18)


def assert_raises(expected_exception, callback):
    try:
        callback()
    except expected_exception:
        return
    raise AssertionError(f"expected {expected_exception.__name__}")


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_pybind_match.py <config.yaml>")

    repo_root = pathlib.Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repo_root))

    import evacam_py

    matcher = evacam_py.EvaCAMMatch(sys.argv[1])
    width = matcher.word_width()
    assert width > 0

    stored = [0] * width
    first_match = matcher.evaluate(stored, stored)
    second_match = matcher.evaluate(stored, stored)
    assert first_match.hit
    assert matcher.match(stored, stored)
    assert_close(first_match, second_match)

    previous_delay = first_match.matchline_delay
    previous_sense_margin = first_match.sense_margin
    batch_rows = [stored]
    expected_batch_results = [first_match]
    for mismatches in range(1, width + 1):
        query = mismatched_query(width, mismatches)
        shifted_query = mismatched_query(width, mismatches, offset=7)

        result = matcher.evaluate(stored, query)
        repeated_result = matcher.evaluate(stored, query)
        shifted_result = matcher.evaluate(stored, shifted_query)

        assert not result.hit
        assert not matcher.match(stored, query)
        assert_close(result, repeated_result)
        assert_close(result, shifted_result)
        assert result.matchline_delay <= previous_delay
        if mismatches == 1:
            assert math.isclose(result.sense_margin, first_match.sense_margin, rel_tol=0.0, abs_tol=1e-18)
        else:
            assert result.sense_margin <= previous_sense_margin
        previous_delay = result.matchline_delay
        previous_sense_margin = result.sense_margin
        batch_rows.append(query)
        expected_batch_results.append(result)

    batch_results = matcher.evaluate_rows(batch_rows, stored)
    assert len(batch_results) == len(expected_batch_results)
    for actual, expected in zip(batch_results, expected_batch_results):
        assert_close(actual, expected)

    assert_raises(ValueError, lambda: matcher.evaluate(stored[:-1], stored))
    assert_raises(ValueError, lambda: matcher.evaluate(stored, stored + [0]))
    assert_raises(ValueError, lambda: matcher.evaluate(stored, [2] * width))
    assert_raises(ValueError, lambda: matcher.evaluate_rows([stored[:-1]], stored))
    assert_raises(ValueError, lambda: matcher.evaluate_rows([stored], stored + [0]))
    assert_raises(ValueError, lambda: matcher.evaluate_rows([[2] * width], stored))

    print("Pybind match test passed")


if __name__ == "__main__":
    main()
