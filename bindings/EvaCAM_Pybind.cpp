#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "EvaCAM_Match.h"
#include "EvaCAMMatchResult.h"
#include "EvaCamRun.h"
#include "EvaCamRunResult.h"

namespace py = pybind11;

PYBIND11_MODULE(evacam_py, module) {
    module.doc() = "Python bindings for EvaCAM run and match evaluation";

    py::class_<EvaCAMMatchResult>(module, "EvaCAMMatchResult")
        .def_readonly("hit", &EvaCAMMatchResult::hit)
        .def_readonly("search_latency", &EvaCAMMatchResult::searchLatency)
        .def_readonly("search_dynamic_energy", &EvaCAMMatchResult::searchDynamicEnergy)
        .def_readonly("matchline_delay", &EvaCAMMatchResult::matchlineDelay)
        .def_readonly("sense_margin", &EvaCAMMatchResult::senseMargin);

    py::class_<EvaCAM_Match>(module, "EvaCAMMatch")
        .def(py::init<const std::string &>(), py::arg("config_path"))
        .def("evaluate_vector",
                py::overload_cast<const std::vector<int>&, const std::vector<int>&>(
                        &EvaCAM_Match::evaluate_vector, py::const_),
                py::arg("stored"), py::arg("query"))
        .def("evaluate_vector",
                py::overload_cast<
                        const std::vector<std::pair<double, double>>&,
                        const std::vector<double>&>(
                        &EvaCAM_Match::evaluate_vector, py::const_),
                py::arg("stored"), py::arg("query"))
        .def("evaluate_mismatches",
                &EvaCAM_Match::evaluate_mismatches,
                py::arg("mismatches"))
        .def("evaluate_threshold",
                py::overload_cast<const std::vector<int>&, const std::vector<int>&, int>(
                        &EvaCAM_Match::evaluate_threshold, py::const_),
                py::arg("stored"), py::arg("query"), py::arg("max_mismatches"))
        .def("evaluate_threshold",
                py::overload_cast<int, int>(
                        &EvaCAM_Match::evaluate_threshold, py::const_),
                py::arg("mismatches"), py::arg("max_mismatches"))
        .def("evaluate_array",
                py::overload_cast<const std::vector<int>&>(
                        &EvaCAM_Match::evaluate_array, py::const_),
                py::arg("mismatch_counts"))
        .def("evaluate_array",
                py::overload_cast<
                        const std::vector<std::vector<int>>&,
                        const std::vector<int>&>(
                        &EvaCAM_Match::evaluate_array, py::const_),
                py::arg("stored_rows"), py::arg("query"))
        .def("evaluate_array",
                py::overload_cast<
                        const std::vector<std::vector<std::pair<double, double>>>&,
                        const std::vector<double>&>(
                        &EvaCAM_Match::evaluate_array, py::const_),
                py::arg("stored_rows"), py::arg("query"))
        .def("word_width", &EvaCAM_Match::word_width);

    py::class_<EvaCamMetricStatsDto>(module, "EvaCAMMetricStats")
        .def_readonly("available", &EvaCamMetricStatsDto::available)
        .def_readonly("nominal", &EvaCamMetricStatsDto::nominal)
        .def_readonly("sample", &EvaCamMetricStatsDto::sample)
        .def_readonly("mean", &EvaCamMetricStatsDto::mean)
        .def_readonly("stddev", &EvaCamMetricStatsDto::stddev)
        .def_readonly("min", &EvaCamMetricStatsDto::min)
        .def_readonly("max", &EvaCamMetricStatsDto::max)
        .def_readonly("p95", &EvaCamMetricStatsDto::p95);

    py::class_<EvaCamVariationSampleDto>(module, "EvaCAMVariationSample")
        .def_readonly("sample", &EvaCamVariationSampleDto::sample)
        .def_readonly("corner_label", &EvaCamVariationSampleDto::cornerLabel)
        .def_readonly("memory_device_res_on_corner", &EvaCamVariationSampleDto::memoryDeviceResOnCorner)
        .def_readonly("memory_device_res_off_corner", &EvaCamVariationSampleDto::memoryDeviceResOffCorner)
        .def_readonly("matchline_delay", &EvaCamVariationSampleDto::matchlineDelay)
        .def_readonly("search_latency", &EvaCamVariationSampleDto::searchLatency)
        .def_readonly("search_dynamic_energy", &EvaCamVariationSampleDto::searchDynamicEnergy)
        .def_readonly("exact_match_sense_margin", &EvaCamVariationSampleDto::senseMargin)
        .def_readonly("reference_delay", &EvaCamVariationSampleDto::referenceDelay);
    module.attr("EvaCAMMonteCarloSample") = module.attr("EvaCAMVariationSample");

    py::class_<EvaCamVariationDto>(module, "EvaCAMVariation")
        .def_readonly("enabled", &EvaCamVariationDto::enabled)
        .def_readonly("mode", &EvaCamVariationDto::mode)
        .def_readonly("samples", &EvaCamVariationDto::samples)
        .def_readonly("matchline_delay", &EvaCamVariationDto::matchlineDelay)
        .def_readonly("search_latency", &EvaCamVariationDto::searchLatency)
        .def_readonly("search_dynamic_energy", &EvaCamVariationDto::searchDynamicEnergy)
        .def_readonly("exact_match_sense_margin", &EvaCamVariationDto::senseMargin)
        .def_readonly("sample_data", &EvaCamVariationDto::sampleData);

    py::class_<EvaCamDesignResultDto>(module, "EvaCAMDesignResult")
        .def_readonly("optimization_target", &EvaCamDesignResultDto::optimizationTarget)
        .def_readonly("summary", &EvaCamDesignResultDto::summary)
        .def_readonly("breakdown", &EvaCamDesignResultDto::breakdown)
        .def_readonly("geometry", &EvaCamDesignResultDto::geometry)
        .def_readonly("variation", &EvaCamDesignResultDto::variation);

    py::class_<EvaCamRunResultDto>(module, "EvaCAMRunResult")
        .def_readonly("num_solutions", &EvaCamRunResultDto::numSolutions)
        .def_readonly("exploration_csv_path", &EvaCamRunResultDto::explorationCsvPath)
        .def_readonly("output_yaml_path", &EvaCamRunResultDto::outputYamlPath)
        .def_readonly("best_results", &EvaCamRunResultDto::bestResults);

    module.def("run",
            [](const std::string &configPath,
                    int threads,
                    py::object outputYamlPath,
                    bool writeYaml,
                    bool stdoutOutput,
                    bool verbose,
                    bool variationPlots) {
                EvaCamRunOptions options;
                options.configPath = configPath;
                options.threads = threads;
                if (!outputYamlPath.is_none()) {
                    options.outputYamlPath = outputYamlPath.cast<std::string>();
                }
                options.writeYaml = writeYaml;
                options.stdoutOutput = stdoutOutput;
                options.verbose = verbose;
                options.variationPlots = variationPlots;

                py::gil_scoped_release release;
                return RunEvaCam(options);
            },
            py::arg("config_path"),
            py::kw_only(),
            py::arg("threads") = 1,
            py::arg("output_yaml_path") = py::none(),
            py::arg("write_yaml") = false,
            py::arg("stdout") = false,
            py::arg("verbose") = false,
            py::arg("variation_plots") = false);
}
