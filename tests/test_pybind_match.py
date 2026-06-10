#!/usr/bin/env python3

import math
import pathlib
import sys
import tempfile


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


def assert_raises_message(expected_exception, expected_message, callback):
    try:
        callback()
    except expected_exception as error:
        assert expected_message in str(error), str(error)
        return
    raise AssertionError(f"expected {expected_exception.__name__}: {expected_message}")


def write_config_with_search_function(source_config, search_function, tmp_dir):
    text = source_config.read_text()
    for token in ("EX", "BE", "TH"):
        text = text.replace(f"search_function: {token}", f"search_function: {search_function}")
    path = pathlib.Path(tmp_dir) / f"{source_config.stem}_{search_function}.yaml"
    path.write_text(text)
    return path


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
    first_match = matcher.evaluate_vector(stored, stored)
    second_match = matcher.evaluate_vector(stored, stored)
    assert first_match.hit
    assert not hasattr(matcher, "match")
    assert_close(first_match, second_match)

    wildcard_stored = stored.copy()
    wildcard_stored[0] = -1
    wildcard_query = stored.copy()
    wildcard_query[0] = 1
    wildcard_match = matcher.evaluate_vector(wildcard_stored, wildcard_query)
    assert wildcard_match.hit
    assert_close(first_match, wildcard_match)

    previous_delay = first_match.matchline_delay
    previous_sense_margin = first_match.sense_margin
    batch_rows = [stored, wildcard_stored]
    expected_batch_results = [first_match, wildcard_match]
    mismatch_counts = [0]
    expected_mismatch_results = [first_match]
    for mismatches in range(1, width + 1):
        query = mismatched_query(width, mismatches)
        shifted_query = mismatched_query(width, mismatches, offset=7)

        result = matcher.evaluate_vector(stored, query)
        mismatch_result = matcher.evaluate_mismatches(mismatches)
        repeated_result = matcher.evaluate_vector(stored, query)
        shifted_result = matcher.evaluate_vector(stored, shifted_query)

        assert not result.hit
        assert_close(result, mismatch_result)
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
        mismatch_counts.append(mismatches)
        expected_mismatch_results.append(result)

    batch_results = matcher.evaluate_array(batch_rows, stored)
    assert len(batch_results) == len(expected_batch_results)
    for actual, expected in zip(batch_results, expected_batch_results):
        assert_close(actual, expected)

    mismatch_batch_results = matcher.evaluate_array(mismatch_counts)
    assert len(mismatch_batch_results) == len(expected_mismatch_results)
    for actual, expected in zip(mismatch_batch_results, expected_mismatch_results):
        assert_close(actual, expected)

    assert_raises(ValueError, lambda: matcher.evaluate_mismatches(-1))
    assert_raises(ValueError, lambda: matcher.evaluate_mismatches(width + 1))
    assert_raises(ValueError, lambda: matcher.evaluate_array([0, width + 1]))
    assert_raises(ValueError, lambda: matcher.evaluate_vector(stored[:-1], stored))
    assert_raises(ValueError, lambda: matcher.evaluate_vector(stored, stored + [0]))
    assert_raises(ValueError, lambda: matcher.evaluate_vector(stored, [2] * width))
    assert_raises(ValueError, lambda: matcher.evaluate_vector(stored, [-1] * width))
    assert_raises(ValueError, lambda: matcher.evaluate_array([stored[:-1]], stored))
    assert_raises(ValueError, lambda: matcher.evaluate_array([stored], stored + [0]))
    assert_raises(ValueError, lambda: matcher.evaluate_array([[2] * width], stored))
    assert_raises(
        ValueError,
        lambda: matcher.evaluate_vector([(0.0, 1.0)] * width, [0.5] * width),
    )
    assert_raises(
        ValueError,
        lambda: matcher.evaluate_array([[(0.0, 1.0)] * width], [0.5] * width),
    )

    tcam_be_matcher = evacam_py.EvaCAMMatch(
        str(repo_root / "config/2FeFET_TCAM/2FeFET_TCAM_system_config.yaml")
    )
    tcam_be_width = tcam_be_matcher.word_width()
    tcam_be_stored = [0] * tcam_be_width
    assert_raises_message(
        RuntimeError,
        "best TCAM vector evaluation is not implemented",
        lambda: tcam_be_matcher.evaluate_vector(tcam_be_stored, tcam_be_stored),
    )
    assert_raises_message(
        RuntimeError,
        "best TCAM vector evaluation is not implemented",
        lambda: tcam_be_matcher.evaluate_array([tcam_be_stored], tcam_be_stored),
    )
    assert_raises_message(
        RuntimeError,
        "mismatch-count evaluation currently supports exact search only",
        lambda: tcam_be_matcher.evaluate_mismatches(0),
    )
    assert_raises_message(
        RuntimeError,
        "mismatch-count evaluation currently supports exact search only",
        lambda: tcam_be_matcher.evaluate_array([0]),
    )

    with tempfile.TemporaryDirectory() as tmp_dir:
        tcam_source = repo_root / "config/2FeFET_TCAM/2FeFET_TCAM_match_system_config.yaml"
        mcam_source = repo_root / "config/2FeFET_MCAM/2FeFET_MCAM_system_config.yaml"

        tcam_th_matcher = evacam_py.EvaCAMMatch(
            str(write_config_with_search_function(tcam_source, "TH", tmp_dir))
        )
        tcam_th_width = tcam_th_matcher.word_width()
        tcam_th_stored = [0] * tcam_th_width
        assert_raises_message(
            RuntimeError,
            "threshold TCAM vector evaluation is not implemented",
            lambda: tcam_th_matcher.evaluate_vector(tcam_th_stored, tcam_th_stored),
        )

        mcam_ex_matcher = evacam_py.EvaCAMMatch(
            str(write_config_with_search_function(mcam_source, "EX", tmp_dir))
        )
        mcam_ex_width = mcam_ex_matcher.word_width()
        mcam_ex_stored = [0] * mcam_ex_width
        assert_raises_message(
            RuntimeError,
            "exact MCAM vector evaluation is not implemented",
            lambda: mcam_ex_matcher.evaluate_vector(mcam_ex_stored, mcam_ex_stored),
        )
        assert_raises_message(
            ValueError,
            "mismatch-count evaluation is only valid for TCAM",
            lambda: mcam_ex_matcher.evaluate_mismatches(0),
        )

        mcam_be_matcher = evacam_py.EvaCAMMatch(
            str(write_config_with_search_function(mcam_source, "BE", tmp_dir))
        )
        mcam_be_width = mcam_be_matcher.word_width()
        mcam_be_stored = [0] * mcam_be_width
        assert_raises_message(
            RuntimeError,
            "best MCAM vector evaluation is not implemented",
            lambda: mcam_be_matcher.evaluate_vector(mcam_be_stored, mcam_be_stored),
        )

        mcam_th_matcher = evacam_py.EvaCAMMatch(
            str(write_config_with_search_function(mcam_source, "TH", tmp_dir))
        )
        mcam_th_width = mcam_th_matcher.word_width()
        mcam_th_stored = [0] * mcam_th_width
        assert_raises_message(
            RuntimeError,
            "threshold MCAM vector evaluation is not implemented",
            lambda: mcam_th_matcher.evaluate_vector(mcam_th_stored, mcam_th_stored),
        )

    print("Pybind match test passed")


if __name__ == "__main__":
    main()
