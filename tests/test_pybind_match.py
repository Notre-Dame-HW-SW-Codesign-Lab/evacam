#!/usr/bin/env python3

import gc
import math
import pathlib
import sys
import tempfile

import yaml


def mismatched_query(width, mismatches, offset=0):
    query = [0] * width
    for index in range(mismatches):
        query[(offset + index) % width] = 1
    return query


def assert_close(lhs, rhs):
    assert lhs.hit == rhs.hit
    assert_same_metrics(lhs, rhs)


def assert_same_metrics(lhs, rhs):
    assert math.isclose(lhs.search_latency, rhs.search_latency, rel_tol=0.0, abs_tol=1e-18)
    assert math.isclose(lhs.search_dynamic_energy, rhs.search_dynamic_energy, rel_tol=0.0, abs_tol=1e-18)
    assert math.isclose(lhs.matchline_delay, rhs.matchline_delay, rel_tol=0.0, abs_tol=1e-18)
    assert math.isclose(lhs.sense_margin, rhs.sense_margin, rel_tol=0.0, abs_tol=1e-18)
    assert math.isclose(
        lhs.required_sense_margin, rhs.required_sense_margin,
        rel_tol=0.0, abs_tol=1e-18,
    )
    assert math.isclose(
        lhs.sense_margin_slack, rhs.sense_margin_slack,
        rel_tol=0.0, abs_tol=1e-18,
    )
    assert lhs.sense_margin_pass == rhs.sense_margin_pass
    assert lhs.sense_margin_applicable == rhs.sense_margin_applicable
    assert math.isclose(
        lhs.squared_euclidean_distance,
        rhs.squared_euclidean_distance,
        rel_tol=0.0,
        abs_tol=1e-18,
    )
    assert math.isclose(
        lhs.matchline_conductance, rhs.matchline_conductance,
        rel_tol=0.0, abs_tol=1e-18,
    )
    assert math.isclose(
        lhs.matchline_voltage, rhs.matchline_voltage,
        rel_tol=0.0, abs_tol=1e-18,
    )


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


def write_config_with_search_function(
    source_config,
    search_function,
    tmp_dir,
    cell_override=None,
    strict_sensing=False,
):
    tool = yaml.safe_load(source_config.read_text())
    architecture_source = (source_config.parent / tool["architecture"]).resolve()
    text = architecture_source.read_text()
    for token in ("EX", "BE", "TH"):
        text = text.replace(f"search_function: {token}", f"search_function: {search_function}")
    sensing = yaml.safe_load(architecture_source.read_text()).get("sensing")
    if sensing:
        sensing_path = (architecture_source.parent / sensing).resolve()
        if strict_sensing:
            sensing_text = sensing_path.read_text().replace(
                "strict_sense_margin: false", "strict_sense_margin: true"
            )
            sense_amp = (sensing_path.parent / "../lib/sense_amp/nvsim_vol.sense_amp.yaml").resolve()
            sensing_text = sensing_text.replace(
                "sense_amplifier: ../lib/sense_amp/nvsim_vol.sense_amp.yaml",
                f"sense_amplifier: {sense_amp}",
            )
            sensing_path = pathlib.Path(tmp_dir) / "mcam_strict.sensing.yaml"
            sensing_path.write_text(sensing_text)
        text = text.replace(f"sensing: {sensing}", f"sensing: {sensing_path}")
    architecture_path = pathlib.Path(tmp_dir) / f"{source_config.stem}_{search_function}_architecture.yaml"
    architecture_path.write_text(text)

    tool["architecture"] = str(architecture_path)
    if cell_override is None:
        source_cell = (source_config.parent / tool["cell"]).resolve()
        tool["cell"] = str(source_cell)
    else:
        tool["cell"] = str(cell_override)
    tool["technology"] = str((source_config.parent / tool["technology"]).resolve())
    tool_path = pathlib.Path(tmp_dir) / f"{source_config.stem}_{search_function}_tool.yaml"
    tool_path.write_text(yaml.safe_dump(tool, sort_keys=False))
    return tool_path


def write_high_sense_threshold_config(source_config, sense_voltage, tmp_dir, search_function="TH"):
    repo_root = pathlib.Path(__file__).resolve().parents[1]
    source_cell = repo_root / "config/2FeFET_TCAM/2FeFET_TCAM.cell.yaml"
    source_memory_device = repo_root / "config/2FeFET_TCAM/2FeFET_TCAM.memory_device.yaml"
    memory_device_text = source_memory_device.read_text()
    memory_device_text = memory_device_text.replace(
        "min_sense_voltage: 70mV",
        f"min_sense_voltage: {sense_voltage:.12e}V",
    )
    memory_device_path = pathlib.Path(tmp_dir) / "high_sense.memory_device.yaml"
    memory_device_path.write_text(memory_device_text)

    cell_text = source_cell.read_text()
    cell_text = cell_text.replace(
        "memory_device: ./2FeFET_TCAM.memory_device.yaml",
        f"memory_device: {memory_device_path}",
    )
    cell_path = pathlib.Path(tmp_dir) / "high_sense.cell.yaml"
    cell_path.write_text(cell_text)

    return write_config_with_search_function(
        source_config,
        search_function,
        tmp_dir,
        cell_override=cell_path,
    )


def test_pybind_match_module(config_path):
    repo_root = pathlib.Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repo_root))

    import evacam_py

    # pybind constructor dispatch and native construction failures remain visible
    # as normal Python exceptions.
    assert_raises(TypeError, lambda: evacam_py.EvaCAMMatch())
    assert_raises(TypeError, lambda: evacam_py.EvaCAMMatch(42))
    assert_raises(RuntimeError, lambda: evacam_py.EvaCAMMatch("does-not-exist.yaml"))

    matcher = evacam_py.EvaCAMMatch(config_path)
    width = matcher.word_width()
    assert width > 0
    assert_raises_message(
        RuntimeError,
        "vector_dimensions is only defined for MCAM",
        matcher.vector_dimensions,
    )

    stored = [0] * width
    first_match = matcher.evaluate_vector(stored, stored)
    second_match = matcher.evaluate_vector(stored, stored)
    assert first_match.hit
    assert not hasattr(matcher, "match")
    assert_close(first_match, second_match)
    for field in (
        "hit",
        "search_latency",
        "search_dynamic_energy",
        "matchline_delay",
        "sense_margin",
    ):
        assert hasattr(first_match, field)
    assert_raises(AttributeError, lambda: setattr(first_match, "hit", False))

    # Result DTOs are returned by value: they stay valid after the matcher is
    # reclaimed and retain their scalar data.
    retained_result = matcher.evaluate_mismatches(0)
    retained_latency = retained_result.search_latency
    del matcher
    gc.collect()
    assert retained_result.hit
    assert retained_result.search_latency == retained_latency
    matcher = evacam_py.EvaCAMMatch(sys.argv[1])

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

    for mismatches in range(0, min(3, width) + 1):
        query = mismatched_query(width, mismatches)
        threshold_by_vector = matcher.evaluate_threshold(stored, query, 2)
        threshold_by_count = matcher.evaluate_threshold(mismatches, 2)
        assert threshold_by_vector.hit == (mismatches <= 2)
        assert_close(threshold_by_vector, threshold_by_count)
        assert_same_metrics(threshold_by_vector, expected_mismatch_results[mismatches])

    exact_threshold_match = matcher.evaluate_threshold(stored, stored, 0)
    assert exact_threshold_match.hit
    exact_threshold_miss = matcher.evaluate_threshold(stored, mismatched_query(width, 1), 0)
    assert not exact_threshold_miss.hit
    all_threshold = matcher.evaluate_threshold(width, width)
    assert all_threshold.hit

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
    assert_raises(ValueError, lambda: matcher.evaluate_threshold(stored, stored, -1))
    assert_raises(ValueError, lambda: matcher.evaluate_threshold(stored, stored, width + 1))
    assert_raises(ValueError, lambda: matcher.evaluate_threshold(-1, 0))
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
    # Incompatible Python containers fail during binding conversion rather
    # than reaching C++.
    assert_raises(TypeError, lambda: matcher.evaluate_mismatches("0"))
    assert_raises(TypeError, lambda: matcher.evaluate_threshold(stored, stored, "0"))
    assert_raises(TypeError, lambda: matcher.evaluate_vector([object()] * width, stored))
    assert_raises(TypeError, lambda: matcher.evaluate_array(object()))

    tcam_be_matcher = evacam_py.EvaCAMMatch(
        str(repo_root / "config/2FeFET_TCAM/2FeFET_TCAM.config.yaml")
    )
    tcam_be_width = tcam_be_matcher.word_width()
    tcam_be_query = [0] * tcam_be_width
    tcam_be_rows = [
        mismatched_query(tcam_be_width, 2),
        tcam_be_query,
        mismatched_query(tcam_be_width, 1),
        tcam_be_query,
    ]
    tcam_be_counts = [2, 0, 1, 0]
    tcam_be_vector_results = tcam_be_matcher.evaluate_array(tcam_be_rows, tcam_be_query)
    tcam_be_count_results = tcam_be_matcher.evaluate_array(tcam_be_counts)
    assert len(tcam_be_vector_results) == len(tcam_be_counts)
    assert len(tcam_be_count_results) == len(tcam_be_counts)
    for actual_by_vector, actual_by_count, mismatches in zip(
        tcam_be_vector_results,
        tcam_be_count_results,
        tcam_be_counts,
    ):
        expected_hit = mismatches == 0
        assert actual_by_vector.hit == expected_hit
        assert actual_by_count.hit == expected_hit
        assert_close(actual_by_vector, actual_by_count)
        assert_same_metrics(actual_by_vector, expected_mismatch_results[mismatches])

    tied_best_results = tcam_be_matcher.evaluate_array([1, 1, 2])
    assert tied_best_results[0].hit
    assert tied_best_results[1].hit
    assert not tied_best_results[2].hit

    assert_raises_message(
        RuntimeError,
        "best TCAM vector evaluation requires evaluate_array",
        lambda: tcam_be_matcher.evaluate_vector(tcam_be_query, tcam_be_query),
    )
    assert_raises_message(
        ValueError,
        "mismatch-count array must not be empty",
        lambda: tcam_be_matcher.evaluate_array([]),
    )
    assert_raises(ValueError, lambda: tcam_be_matcher.evaluate_array([0, tcam_be_width + 1]))
    assert_raises_message(
        RuntimeError,
        "mismatch-count evaluation currently supports exact search only",
        lambda: tcam_be_matcher.evaluate_mismatches(0),
    )

    with tempfile.TemporaryDirectory() as tmp_dir:
        tcam_source = repo_root / "config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml"
        mcam_source = repo_root / "config/2FeFET_MCAM/2FeFET_MCAM.config.yaml"

        tcam_th_matcher = evacam_py.EvaCAMMatch(
            str(write_config_with_search_function(tcam_source, "TH", tmp_dir))
        )
        tcam_th_width = tcam_th_matcher.word_width()
        tcam_th_stored = [0] * tcam_th_width
        assert_raises_message(
            RuntimeError,
            "threshold TCAM vector evaluation requires evaluate_threshold",
            lambda: tcam_th_matcher.evaluate_vector(tcam_th_stored, tcam_th_stored),
        )
        assert tcam_th_matcher.evaluate_threshold(tcam_th_stored, tcam_th_stored, 0).hit
        assert not tcam_th_matcher.evaluate_threshold(
            tcam_th_stored,
            mismatched_query(tcam_th_width, 1),
            0,
        ).hit
        assert tcam_th_matcher.evaluate_threshold(
            tcam_th_stored,
            mismatched_query(tcam_th_width, 1),
            1,
        ).hit
        assert tcam_th_matcher.evaluate_threshold(1, 1).hit

        if expected_mismatch_results[2].sense_margin < expected_mismatch_results[1].sense_margin:
            guarded_sense_voltage = (
                expected_mismatch_results[1].sense_margin
                + expected_mismatch_results[2].sense_margin
            ) / 2.0
            high_sense_matcher = evacam_py.EvaCAMMatch(
                str(write_high_sense_threshold_config(tcam_source, guarded_sense_voltage, tmp_dir))
            )
            assert high_sense_matcher.evaluate_threshold(0, 0).hit
            assert_raises_message(
                RuntimeError,
                "sense-margin capability",
                lambda: high_sense_matcher.evaluate_threshold(1, 1),
            )

            high_sense_best_path = write_high_sense_threshold_config(
                tcam_source,
                guarded_sense_voltage,
                tmp_dir,
                search_function="BE",
            )
            high_sense_best_matcher = evacam_py.EvaCAMMatch(str(high_sense_best_path))
            assert high_sense_best_matcher.evaluate_array([0, 1])[0].hit
            high_sense_tied = high_sense_best_matcher.evaluate_array([1, 1])
            assert high_sense_tied[0].hit
            assert high_sense_tied[1].hit
            assert_raises_message(
                RuntimeError,
                "best-match boundary",
                lambda: high_sense_best_matcher.evaluate_array([1, 2]),
            )
            assert_raises_message(
                RuntimeError,
                "best match exceeds",
                lambda: high_sense_best_matcher.evaluate_array([2, 3]),
            )

        mcam_ex_matcher = evacam_py.EvaCAMMatch(
            str(write_config_with_search_function(mcam_source, "EX", tmp_dir))
        )
        assert_raises_message(
            RuntimeError,
            "word_width is not defined for MCAM",
            mcam_ex_matcher.word_width,
        )
        assert mcam_ex_matcher.vector_dimensions() == 64
        assert mcam_ex_matcher.storage_width_bits() == 192
        assert_raises_message(
            RuntimeError,
            "logical_word_width_bits is not defined for MCAM",
            mcam_ex_matcher.logical_word_width_bits,
        )
        assert mcam_ex_matcher.bits_per_symbol() == 3
        mcam_ex_width = mcam_ex_matcher.symbol_width()
        assert mcam_ex_width == 64
        mcam_ex_stored = [index % 8 for index in range(mcam_ex_width)]
        mcam_ex_query = list(mcam_ex_stored)
        mcam_ex_query[0] = (mcam_ex_query[0] + 1) % 8
        mcam_exact = mcam_ex_matcher.evaluate_vector(mcam_ex_stored, mcam_ex_stored)
        mcam_miss = mcam_ex_matcher.evaluate_vector(mcam_ex_stored, mcam_ex_query)
        assert mcam_exact.hit
        assert not mcam_miss.hit
        assert mcam_exact.squared_euclidean_distance == 0
        assert mcam_miss.squared_euclidean_distance == 1
        assert mcam_exact.matchline_conductance < mcam_miss.matchline_conductance
        assert mcam_exact.matchline_voltage > mcam_miss.matchline_voltage
        assert mcam_exact.sense_margin_applicable
        assert not mcam_exact.sense_margin_pass
        assert math.isclose(mcam_exact.required_sense_margin, 0.07)
        assert mcam_exact.sense_margin_slack < 0
        assert mcam_ex_matcher.evaluate_symbols(
            mcam_ex_stored, mcam_ex_stored
        ).hit
        logical_bits = [0] * mcam_ex_matcher.storage_width_bits()
        assert mcam_ex_matcher.evaluate_bits(logical_bits, logical_bits).hit
        for result in (mcam_exact, mcam_miss):
            assert math.isfinite(result.search_latency) and result.search_latency > 0
            assert math.isfinite(result.search_dynamic_energy) and result.search_dynamic_energy > 0
            assert math.isfinite(result.matchline_delay) and result.matchline_delay > 0
            assert math.isfinite(result.sense_margin) and result.sense_margin >= 0
            assert math.isfinite(result.required_sense_margin)
            assert math.isfinite(result.sense_margin_slack)
            assert math.isfinite(result.squared_euclidean_distance)
            assert math.isfinite(result.matchline_conductance) and result.matchline_conductance > 0
            assert math.isfinite(result.matchline_voltage) and result.matchline_voltage >= 0
        one_large_delta = [0] * mcam_ex_width
        one_large_delta[0] = 7
        two_small_deltas = [0] * mcam_ex_width
        two_small_deltas[0] = 1
        two_small_deltas[1] = 1
        large_distance = mcam_ex_matcher.evaluate_distance(
            [0] * mcam_ex_width, one_large_delta
        )
        small_distance = mcam_ex_matcher.evaluate_distance(
            [0] * mcam_ex_width, two_small_deltas
        )
        assert large_distance.squared_euclidean_distance == 49
        assert small_distance.squared_euclidean_distance == 2
        assert small_distance.matchline_conductance < large_distance.matchline_conductance
        one_delta_four = [0] * mcam_ex_width
        one_delta_four[0] = 4
        detectable = mcam_ex_matcher.evaluate_distance(
            [0] * mcam_ex_width, one_delta_four
        )
        assert detectable.squared_euclidean_distance == 16
        assert detectable.sense_margin_pass
        assert detectable.sense_margin_slack > 0
        mcam_rows = mcam_ex_matcher.evaluate_array(
            [mcam_ex_stored, mcam_ex_query], mcam_ex_stored
        )
        assert mcam_rows[0].hit
        assert not mcam_rows[1].hit
        invalid_mcam = list(mcam_ex_stored)
        invalid_mcam[0] = -1
        assert_raises_message(
            ValueError,
            "between 0 and 7",
            lambda: mcam_ex_matcher.evaluate_vector(invalid_mcam, mcam_ex_stored),
        )
        assert_raises_message(
            ValueError,
            "mismatch-count evaluation is only valid for TCAM",
            lambda: mcam_ex_matcher.evaluate_mismatches(0),
        )
        assert_raises_message(
            RuntimeError,
            "configured bank is invalid",
            lambda: evacam_py.EvaCAMMatch(
                str(write_config_with_search_function(
                    mcam_source, "EX", tmp_dir, strict_sensing=True
                ))
            ),
        )

        mcam_be_matcher = evacam_py.EvaCAMMatch(
            str(write_config_with_search_function(mcam_source, "BE", tmp_dir))
        )
        mcam_be_width = mcam_be_matcher.symbol_width()
        mcam_be_stored = [0] * mcam_be_width
        mcam_be_far = list(mcam_be_stored)
        mcam_be_far[0] = 7
        mcam_be_near = list(mcam_be_stored)
        mcam_be_near[0] = 1
        mcam_be_near[1] = 1
        mcam_best = mcam_be_matcher.evaluate_array(
            [mcam_be_far, mcam_be_near], mcam_be_stored
        )
        assert not mcam_best[0].hit
        assert mcam_best[1].hit
        assert all(result.sense_margin_pass for result in mcam_best)
        mcam_be_medium = list(mcam_be_stored)
        mcam_be_medium[0] = 2
        close_best = mcam_be_matcher.evaluate_array(
            [mcam_be_far, mcam_be_medium, mcam_be_near], mcam_be_stored
        )
        assert close_best[2].hit
        assert all(not result.sense_margin_pass for result in close_best)
        assert all(result.sense_margin_slack < 0 for result in close_best)

        knn_rows = [
            mcam_be_far,
            mcam_be_near,
            mcam_be_stored,
            mcam_be_medium,
        ]
        one_neighbor = mcam_be_matcher.evaluate_knn(
            knn_rows, mcam_be_stored, k=1
        )
        best_from_array = mcam_be_matcher.evaluate_array(
            knn_rows, mcam_be_stored
        )
        assert len(one_neighbor) == len(knn_rows)
        for knn_result, best_result in zip(one_neighbor, best_from_array):
            assert_same_metrics(knn_result, best_result)
            assert knn_result.hit == best_result.hit

        two_neighbors = mcam_be_matcher.evaluate_knn(
            knn_rows, mcam_be_stored, k=2
        )
        assert [result.hit for result in two_neighbors] == [False, True, True, False]
        assert [result.squared_euclidean_distance for result in two_neighbors] == [
            49,
            2,
            0,
            4,
        ]
        assert all(result.sense_margin_applicable for result in two_neighbors)
        assert all(
            math.isclose(result.sense_margin, two_neighbors[0].sense_margin)
            for result in two_neighbors
        )

        tied_neighbors = mcam_be_matcher.evaluate_knn(
            [
                mcam_be_medium,
                mcam_be_near,
                mcam_be_stored,
                list(mcam_be_near),
                mcam_be_far,
            ],
            mcam_be_stored,
            k=2,
        )
        assert [result.hit for result in tied_neighbors] == [
            False,
            True,
            True,
            True,
            False,
        ]

        every_neighbor = mcam_be_matcher.evaluate_knn(
            knn_rows, mcam_be_stored, k=len(knn_rows)
        )
        assert all(result.hit for result in every_neighbor)
        assert all(not result.sense_margin_applicable for result in every_neighbor)
        assert all(result.sense_margin_pass for result in every_neighbor)
        assert_raises_message(
            ValueError,
            "k must be between 1",
            lambda: mcam_be_matcher.evaluate_knn(
                knn_rows, mcam_be_stored, k=0
            ),
        )
        assert_raises_message(
            ValueError,
            "k must be between 1",
            lambda: mcam_be_matcher.evaluate_knn(
                knn_rows, mcam_be_stored, k=len(knn_rows) + 1
            ),
        )
        assert_raises_message(
            ValueError,
            "must not be empty",
            lambda: mcam_be_matcher.evaluate_knn([], mcam_be_stored, k=1),
        )
        assert_raises_message(
            ValueError,
            "only valid for MCAM",
            lambda: matcher.evaluate_knn([stored], stored, k=1),
        )
        assert_raises_message(
            RuntimeError,
            "requires evaluate_array",
            lambda: mcam_be_matcher.evaluate_vector(mcam_be_stored, mcam_be_stored),
        )

        mcam_th_matcher = evacam_py.EvaCAMMatch(
            str(write_config_with_search_function(mcam_source, "TH", tmp_dir))
        )
        mcam_th_width = mcam_th_matcher.symbol_width()
        mcam_th_stored = [0] * mcam_th_width
        mcam_th_near = list(mcam_th_stored)
        mcam_th_near[0] = 1
        mcam_th_near[1] = 1
        mcam_th_far = list(mcam_th_stored)
        mcam_th_far[0] = 2
        assert mcam_th_matcher.evaluate_distance_threshold(
            mcam_th_near, mcam_th_stored, 2
        ).hit
        assert not mcam_th_matcher.evaluate_distance_threshold(
            mcam_th_far, mcam_th_stored, 2
        ).hit
        interior_query = [3] * mcam_th_width
        edge_boundary = mcam_th_matcher.evaluate_distance_threshold(
            mcam_th_stored, mcam_th_stored, 128
        )
        interior_boundary = mcam_th_matcher.evaluate_distance_threshold(
            interior_query, interior_query, 128
        )
        assert not math.isclose(
            edge_boundary.sense_margin,
            interior_boundary.sense_margin,
            rel_tol=0.0,
            abs_tol=1e-6,
        )
        edge_still_has_boundary = mcam_th_matcher.evaluate_distance_threshold(
            mcam_th_stored, mcam_th_stored, 1024
        )
        interior_accepts_every_reachable_vector = (
            mcam_th_matcher.evaluate_distance_threshold(
                interior_query, interior_query, 1024
            )
        )
        assert edge_still_has_boundary.sense_margin_applicable
        assert not interior_accepts_every_reachable_vector.sense_margin_applicable
        assert interior_accepts_every_reachable_vector.sense_margin_pass
        assert_raises_message(
            RuntimeError,
            "requires evaluate_distance_threshold",
            lambda: mcam_th_matcher.evaluate_vector(mcam_th_stored, mcam_th_stored),
        )

    print("Pybind match test passed")


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: test_pybind_match.py <config.yaml>")
    test_pybind_match_module(sys.argv[1])


if __name__ == "__main__":
    main()
